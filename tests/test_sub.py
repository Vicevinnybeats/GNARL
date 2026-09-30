#!/usr/bin/env python3
"""Mono sub tests, run against the real engine through gnarl-render.

    python3 tests/test_sub.py [path/to/gnarl-render] [--plugin PROBE GNARL.so]

docs/design/phase2-05-mono-sub.md. Every check renders and measures; exit
status is the number of failures.

The sub is measured as a RESIDUAL: the render with it on minus the same
patch with it off. It is summed after the effect chain, and the renderer is
deterministic, so the residual is the sub alone - even under heavy FX.

1. Off changes nothing: mono_sub_on = 0 renders bit-identically to the same
   patch with every mono_sub_* key removed (a patch from before the sub).
2. Pitch: C2 at -1 octave is 32.70 Hz, at -2 octaves 16.35 Hz, within 0.5%.
3. Purity at drive 0: 2nd to 5th harmonics each below -80 dBc
   (Blackman-Harris, whose sidelobes sit at -92 dB). Vital's polynomial
   sine (futils::sin1) measured -64 dBc at the 5th harmonic - 164 Hz for a
   C1 sub, inside the growl's body - which is why the sub uses sinf.
4. Clean under the FX: the same with distortion at full drive and OTT on,
   and the level exactly the clean sub's. At volume -12 dB: the engine's
   output clamp (+-2.1, SoundEngine) is the last stage, and at 0 dB a
   +30 dB-driven growl plus the sub reaches it - 122 clipped samples put
   the residual's harmonics at -78 dBc. That is the host's safety net,
   not the sub's path, so the check keeps below it and asserts it did.
5. Mono: left minus right of the residual is exactly zero.
6. Level: at sustain 1 and volume 0 dB the residual's peak is level^2,
   within 0.1 dB.
7. Drive: at 100% the 3rd harmonic is above -20 dBc and the RMS within
   0.5 dB of drive 0 - DRIVE is tone, not volume.
8. Block size: the residual at blocks 32 and 512 differs by less than
   -100 dB.
9. With --plugin, through the VST3 as a host drives it: a note starting
   100 samples into a block gives the same sub as one starting at 0,
   shifted by 100 samples, within -100 dB. The renderer always starts
   notes on a block boundary, so only this catches a phase reset at the
   block start instead of the note's sample.
"""
import json, os, subprocess, sys, tempfile
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'tools'))
import wavio  # noqa: E402

ARGS = sys.argv[1:]
PLUGIN = None
if '--plugin' in ARGS:
    i = ARGS.index('--plugin')
    PLUGIN = (ARGS[i + 1], ARGS[i + 2])
    del ARGS[i:i + 3]
RENDER = ARGS[0] if ARGS else os.path.join(HERE, '..', 'headless/builds/linux/build/gnarl-render')
TMP = tempfile.mkdtemp(prefix='gnarl-sub-')
FAILS = []
LAST_PEAK = 0.0
SECONDS = 2
# The sustained middle of the note: past the attack, clear of the release
# and of the renderer's end fade.
WINDOW = (0.5, 1.5)
C1_HZ = 440 * 2 ** ((24 - 69) / 12)


def check(ok, msg):
    print(('PASS ' if ok else 'FAIL ') + msg)
    if not ok:
        FAILS.append(msg)


def render(patch_path, block=64):
    out = patch_path + f'.{block}.wav'
    subprocess.run([RENDER, '--headless', '-o', out, '-l', str(SECONDS), '-m', 'C2', '-b', '140',
                    '--bits', '32', '--block', str(block), patch_path],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    x, sr = wavio.read(out)
    return x, sr, out


def init_preset():
    path = os.path.join(TMP, 'init.vital')
    subprocess.run([RENDER, '--headless', '--save', path, '-o', os.path.join(TMP, 'x.wav'), '-l', '0.1'],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return json.load(open(path))


def patch(base, name, drop=(), **overrides):
    d = json.loads(json.dumps(base))
    s = d['settings']
    for k in drop:
        s.pop(k)
    for k, v in overrides.items():
        if k not in s:
            raise KeyError(f'unknown parameter {k}')
        s[k] = float(v)
    path = os.path.join(TMP, name + '.vital')
    json.dump(d, open(path, 'w'))
    return path


def blackman_harris(n):
    t = np.arange(n) / (n - 1)
    return (0.35875 - 0.48829 * np.cos(2 * np.pi * t) + 0.14128 * np.cos(4 * np.pi * t)
            - 0.01168 * np.cos(6 * np.pi * t))


def window(x, sr):
    return x[int(WINDOW[0] * sr):int(WINDOW[1] * sr)]


def spectrum(x, sr):
    n = 1 << 20  # zero-padded: 0.04 Hz bins at 44.1 kHz
    mag = np.abs(np.fft.rfft(x * blackman_harris(len(x)), n))
    return mag, np.fft.rfftfreq(n, 1 / sr)


def fundamental(x, sr):
    mag, f = spectrum(x, sr)
    lo = np.searchsorted(f, 8.0)
    i = lo + int(np.argmax(mag[lo:]))
    a, b, c = np.log(mag[i - 1:i + 2] + 1e-30)
    return f[i] + 0.5 * (a - c) / (a - 2 * b + c) * (f[1] - f[0])


def harmonics_dbc(x, sr, f0, count=5):
    """Level of harmonics 2..count relative to the fundamental, in dB."""
    mag, f = spectrum(x, sr)
    width = 1.5 * (f[1] - f[0]) * 64  # a few main-lobe widths either side

    def peak(freq):
        sel = (f > freq - width) & (f < freq + width)
        return mag[sel].max()

    ref = peak(f0)
    return [20 * np.log10(peak(k * f0) / ref) for k in range(2, count + 1)]


def db(x):
    return 20 * np.log10(max(x, 1e-300))


def plugin_offset(probe, plugin):
    """Check 9: the phase resets at the note's own sample, mid-block."""
    listing = subprocess.run([probe, plugin, '--params'], check=True, capture_output=True, text=True).stdout
    ids = {}
    for line in listing.splitlines():
        parts = line.split(None, 2)
        if len(parts) == 3 and parts[1].startswith('id='):
            ids[parts[2].strip()] = int(parts[1][3:])
    on_id = ids['Mono Sub On']

    def run(sub_on, offset):
        out = os.path.join(TMP, f'probe_{sub_on}_{offset}.f32')
        subprocess.run([probe, plugin, '--render', out, f'{on_id}={sub_on},offset={offset}'],
                       check=True, stdout=subprocess.DEVNULL)
        return np.fromfile(out, dtype=np.float32).reshape(-1, 2)[:, 0].astype(np.float64)

    shift = 100
    at_0 = run(1, 0) - run(0, 0)
    at_100 = run(1, shift) - run(0, shift)
    sr = 48000  # the probe's rate
    a = at_0[int(0.5 * sr):int(1.5 * sr)]
    b = at_100[int(0.5 * sr) + shift:int(1.5 * sr) + shift]
    diff = db(np.max(np.abs(a - b))) - db(np.max(np.abs(a)))
    check(diff < -100, f'plugin: note at sample {shift} of a block = note at 0, shifted: {diff:.1f} dB')


def main():
    base = init_preset()
    # Oscillator random phase off: two renders differ only by what the test
    # changes. Volume 0 dB, so the residual's level is the sub's own.
    common = dict(osc_1_random_phase=0, osc_2_random_phase=0, osc_3_random_phase=0, volume=6400)
    sub_keys = [k for k in base['settings'] if k.startswith('mono_sub_')]
    check(sorted(sub_keys) == ['mono_sub_drive', 'mono_sub_level', 'mono_sub_octave', 'mono_sub_on'],
          f'the four parameters are saved ({sub_keys})')

    # 1. Off changes nothing.
    off, sr, off_path = render(patch(base, 'off', **common))
    old, _, old_path = render(patch(base, 'old', drop=sub_keys, **common))
    check(open(off_path, 'rb').read() == open(old_path, 'rb').read(),
          'off renders bit-identically to a patch without the mono_sub keys')

    def residual(name, block=64, **extra):
        settings = dict(common, **extra)
        p_on = patch(base, name + '_on', mono_sub_on=1, **settings)
        p_off = patch(base, name + '_off', mono_sub_on=0, **settings)
        on, sr, _ = render(p_on, block)
        ref, _, _ = render(p_off, block)
        global LAST_PEAK
        LAST_PEAK = np.max(np.abs(on))
        return on - ref, sr

    # 2. Pitch.
    r1, sr = residual('oct1', mono_sub_octave=1)
    r2, _ = residual('oct2', mono_sub_octave=2)
    f1 = fundamental(window(r1[:, 0], sr), sr)
    f2 = fundamental(window(r2[:, 0], sr), sr)
    check(abs(f1 / C1_HZ - 1) < 0.005, f'C2 at -1 octave: {f1:.3f} Hz (C1 = {C1_HZ:.3f})')
    check(abs(f2 / (C1_HZ / 2) - 1) < 0.005, f'C2 at -2 octaves: {f2:.3f} Hz (C0 = {C1_HZ / 2:.3f})')

    # 3. Purity.
    h = harmonics_dbc(window(r1[:, 0], sr), sr, C1_HZ)
    check(max(h) < -80, 'drive 0 harmonics 2-5: ' + ', '.join(f'{v:.1f}' for v in h) + ' dBc')

    # 4. Clean under the FX. Distortion at full drive and OTT on the growl.
    # volume (80 - 12)^2 = 4624 is -12 dB (Vital's volume is sqrt(v) - 80 dB).
    fx, _ = residual('fx', distortion_on=1, distortion_drive=30, compressor_on=1, volume=4624)
    check(LAST_PEAK < 2.0, f'under the FX the output peaks at {LAST_PEAK:.2f}, below the 2.1 clamp')
    hx = harmonics_dbc(window(fx[:, 0], sr), sr, C1_HZ)
    check(max(hx) < -80, 'under distortion + OTT, harmonics 2-5: ' + ', '.join(f'{v:.1f}' for v in hx) + ' dBc')
    level_diff = db(np.sqrt(np.mean(window(fx[:, 0], sr) ** 2))) - db(np.sqrt(np.mean(window(r1[:, 0], sr) ** 2)))
    check(abs(level_diff + 12) < 0.01, f'under the FX its RMS is {level_diff:+.4f} dB (volume -12 dB)')

    # 5. Mono.
    side = np.max(np.abs(r1[:, 0] - r1[:, 1]))
    check(side == 0.0, f'left minus right of the sub: max {side:g}')

    # 6. Level: default level 0.707 is amplitude 0.5.
    peak = np.max(np.abs(window(r1[:, 0], sr)))
    check(abs(db(peak) - db(0.5)) < 0.1, f'peak {db(peak):.2f} dB, expected {db(0.5):.2f} dB (0.707^2)')

    # 7. Drive.
    rd, _ = residual('drive', mono_sub_drive=1)
    hd = harmonics_dbc(window(rd[:, 0], sr), sr, C1_HZ)
    rms_diff = db(np.sqrt(np.mean(window(rd[:, 0], sr) ** 2))) - db(np.sqrt(np.mean(window(r1[:, 0], sr) ** 2)))
    check(hd[1] > -20, f'drive 100%: 3rd harmonic {hd[1]:.1f} dBc')
    check(abs(rms_diff) < 0.5, f'drive 100%: RMS {rms_diff:+.2f} dB against drive 0')

    # 8. Block size.
    b32, _ = residual('b32', block=32)
    b512, _ = residual('b512', block=512)
    n = min(len(b32), len(b512))
    diff = db(np.max(np.abs(b32[:n] - b512[:n]))) - db(np.max(np.abs(b32[:n])))
    check(diff < -100, f'blocks 32 vs 512: {diff:.1f} dB')

    if PLUGIN:
        plugin_offset(*PLUGIN)

    print(f'\n{len(FAILS)} failure(s)')
    return len(FAILS)


if __name__ == '__main__':
    sys.exit(main())
