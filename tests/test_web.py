#!/usr/bin/env python3
"""The browser build of the engine (wasm/, docs/design/phase2-09-mobile.md)
against the desktop build.

    python3 tests/test_web.py [path/to/gnarl-render]

Needs wasm/build/gnarl.wasm (wasm/build.sh) and node. Renders the same notes
with tools/web_render.mjs and with gnarl-render, and measures the difference.
Exit status: the number of failures.

The two are the same C++ source compiled for two targets, so they should
agree to float rounding, but not bit for bit: the browser build uses kissfft
where the desktop uses JUCE's FFT, and libc++ where the desktop uses
libstdc++. Random oscillator phase is therefore off in every comparison -
std::uniform_real_distribution is implementation-defined, so the two
libraries draw different phases from the same seed. The checks:

1. The init patch: the same to -100 dB.
2. Each GNARL feature on its own (FM, osc FOLD, the formant filter, the
   wobble, the mono sub, TUBE, CRUSH, OTT, reverb, unison): the same to
   -75 dB. CRUSH and OTT sit nearest: a quantiser and a level detector
   turn rounding into steps.
3. A second oscillator an octave up drifts in pitch by float rounding: the
   difference grows steadily through the note. It must stay under -45 dB
   over 2 s, i.e. a pitch error well under a hundredth of a cent.
4. Blocks of 32 vs 128 in the browser build differ no more than in the
   desktop build (+1 dB), measured without chorus and CRUSH: Vital's chorus
   is block-size dependent in both builds, and a crusher amplifies rounding.
5. The pitch wheel bends by the bend range (2 semitones) and the mod wheel
   drives a matrix route, as in the plugin; at 0 the mod wheel's route
   changes nothing.
6. Speed: a heavy patch (two unison oscillators, every effect) renders
   faster than real time with room to spare (printed; asserted > 1.5x).
7. Presets (the mobile version's save and load): a patch with its own
   wavetable (two keyframes, sine to square), a drawn LFO routed to the
   wavetable position and a drawn wobble, saved by the desktop, loads in the
   browser: within 0.05 dB of gain and -35 dB after matching it with
   spectral morphing, which rounds through the maths library (see the
   check); -100 dB with time-domain morphing. Saved again by the browser, it loads
   in the desktop renderer and sounds as the original (-100 dB). A file that
   is not a patch, one from a newer GNARL and one from an older version are
   refused with their own answers, and the engine still plays the init patch
   bit for bit afterwards.
"""

import json, os, subprocess, sys, tempfile
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import wavio  # noqa: E402

RENDER = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'headless/builds/linux/build/gnarl-render')
WEB = os.path.join(ROOT, 'tools/web_render.mjs')
TMP = tempfile.mkdtemp(prefix='gnarl-web-')
FAILS = []


def check(ok, msg):
    print(('PASS ' if ok else 'FAIL ') + msg)
    if not ok:
        FAILS.append(msg)


def patch(base, name, **overrides):
    d = json.loads(json.dumps(base))
    s = d['settings']
    for k, v in overrides.items():
        if k == 'modulations':
            s['modulations'] = v
            continue
        if k not in s:
            raise KeyError(k)
        s[k] = float(v)
    path = os.path.join(TMP, name + '.vital')
    json.dump(d, open(path, 'w'))
    return path


def native(path, note='C2', seconds=2, block=128):
    out = f'{path}.native{block}.wav'
    subprocess.run([RENDER, '--headless', '-o', out, '-l', str(seconds), '-m', note, '-b', '140', '--bits', '32',
                    '--block', str(block), path], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return wavio.read(out)[0]


def web(path, note='C2', seconds=2, block=128, by_name=False):
    out = f'{path}.web{block}.wav'
    run = subprocess.run(['node', WEB, path, '-o', out, '-l', str(seconds), '-m', note, '-b', '140',
                          '--block', str(block)] + (['--by-name'] if by_name else []),
                         check=True, capture_output=True, text=True)
    return wavio.read(out)[0], run.stdout.strip()


def rel_db(a, b):
    n = min(len(a), len(b))
    rms = lambda x: np.sqrt(np.mean(x ** 2))  # noqa: E731
    return 20 * np.log10(max(rms(a[:n] - b[:n]), 1e-30) / rms(a[:n]))


def peak_db(a, b):
    n = min(len(a), len(b))
    return 20 * np.log10(max(np.max(np.abs(a[:n] - b[:n])), 1e-30) / np.max(np.abs(a[:n])))


def f0(x, lo, hi):
    """The strongest line between lo and hi Hz, Blackman-Harris windowed,
    with parabolic interpolation: enough to tell 2 semitones apart."""
    w = x[22050:66150, 0]
    n = len(w)
    t = np.arange(n) / (n - 1)
    bh = 0.35875 - 0.48829 * np.cos(2 * np.pi * t) + 0.14128 * np.cos(4 * np.pi * t) - 0.01168 * np.cos(6 * np.pi * t)
    size = 1 << 20
    spec = np.abs(np.fft.rfft(w * bh, size))
    freqs = np.fft.rfftfreq(size, 1 / 44100)
    band = np.where((freqs > lo) & (freqs < hi))[0]
    k = band[np.argmax(spec[band])]
    a, b, c = np.log(spec[k - 1:k + 2])
    return freqs[k] + 0.5 * (a - c) / (a - 2 * b + c) * (freqs[1] - freqs[0])


def main():
    if not os.path.exists(os.path.join(ROOT, 'wasm/build/gnarl.wasm')):
        print('FAIL wasm/build/gnarl.wasm missing: run wasm/build.sh')
        return 1
    init = os.path.join(TMP, 'init.vital')
    subprocess.run([RENDER, '--headless', '--save', init, '-o', os.path.join(TMP, 'x.wav'), '-l', '0.1'],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    base = json.load(open(init))
    base['settings'].update(osc_1_random_phase=0.0, osc_2_random_phase=0.0, osc_3_random_phase=0.0)

    # 1. The init patch.
    p = patch(base, 'init')
    d = rel_db(native(p), web(p)[0])
    check(d < -100, f'init patch: browser vs desktop {d:.1f} dB')

    # 2. Each feature on its own.
    features = {
        'unison 7': dict(osc_1_unison_voices=7, osc_1_unison_detune=3.0),
        'FM knob': dict(osc_2_on=1, osc_1_fm_amount=0.4),
        'FORMANT warp': dict(osc_1_distortion_type=2, osc_1_distortion_amount=0.5),
        'osc FOLD': dict(osc_1_fold=1, osc_1_distortion_amount=0.6),
        'formant filter': dict(filter_1_on=1, filter_1_model=5, filter_1_formant_x=0.3),
        'wobble on cutoff 1/8T': dict(filter_1_on=1, wobble_amount_cutoff=0.6, wobble_rate=2),
        'mono sub': dict(mono_sub_on=1, mono_sub_level=0.7),
        'TUBE': dict(distortion_on=1, distortion_tube=1, distortion_drive=12),
        'CRUSH': dict(distortion_crush_on=1, distortion_crush_bits=10),
        'OTT': dict(compressor_on=1),
        'reverb': dict(reverb_on=1),
    }
    for name, overrides in features.items():
        p = patch(base, name.replace(' ', '_').replace('/', ''), **overrides)
        d = rel_db(native(p, 'F1'), web(p, 'F1')[0])
        check(d < -75, f'{name}: browser vs desktop {d:.1f} dB')

    # 3. An octave-up second oscillator: a pitch drift, growing.
    p = patch(base, 'osc2up', osc_2_on=1, osc_2_transpose=12)
    a, b = native(p, 'F1'), web(p, 'F1')[0]
    d = rel_db(a, b)
    early, late = rel_db(a[:22050], b[:22050]), rel_db(a[-44100:-22050], b[-44100:-22050])
    check(d < -45 and late > early,
          f'osc 2 an octave up: {d:.1f} dB over 2 s, growing from {early:.1f} to {late:.1f} dB (a rounding drift)')

    # 4. Block size.
    heavy = patch(base, 'heavy', osc_1_unison_voices=7, osc_1_unison_detune=3.0, osc_2_on=1, osc_2_unison_voices=5,
                  osc_2_transpose=12, osc_1_fm_amount=0.4, osc_1_distortion_type=2, osc_1_distortion_amount=0.5,
                  osc_2_fold=1, osc_2_distortion_amount=0.6, filter_1_on=1, filter_1_model=5, filter_1_formant_x=0.3,
                  wobble_amount_cutoff=0.6, wobble_amount_wave_frame=0.5, wobble_amount_formant=0.5, wobble_rate=2,
                  mono_sub_on=1, mono_sub_level=0.7, distortion_on=1, distortion_tube=1, distortion_drive=12,
                  distortion_fold_on=1, distortion_crush_on=1, distortion_crush_bits=10, compressor_on=1, reverb_on=1,
                  chorus_on=1, volume=3000)
    # Vital's chorus depends on the block size by itself (-12 dB here, in
    # both builds; docs/design/phase2-09-mobile.md), and the crusher turns
    # rounding into steps (CLAUDE.md section 5): the check runs without them.
    web_all = peak_db(web(heavy, 'F1', block=32)[0], web(heavy, 'F1', block=128)[0])
    native_all = peak_db(native(heavy, 'F1', block=32), native(heavy, 'F1', block=128))
    smooth = patch(json.load(open(heavy)), 'heavy_smooth', distortion_crush_on=0, chorus_on=0)
    web_smooth = peak_db(web(smooth, 'F1', block=32)[0], web(smooth, 'F1', block=128)[0])
    native_smooth = peak_db(native(smooth, 'F1', block=32), native(smooth, 'F1', block=128))
    check(web_smooth <= native_smooth + 1.0,
          f'blocks 32 vs 128, heavy patch without chorus and CRUSH: browser {web_smooth:.1f} dB, desktop '
          f'{native_smooth:.1f} dB (with them: {web_all:.1f} / {native_all:.1f})')

    # 5. The wheels.
    # Set as the panel sets it (by name), not loaded: Vital's loader restores
    # the mod wheel from a preset but not the pitch wheel, and so does ours.
    plain = web(patch(base, 'wheel0'), 'C2', by_name=True)[0]
    bent = web(patch(base, 'wheel1', pitch_wheel=1.0), 'C2', by_name=True)[0]
    f_plain, f_bent = f0(plain, 50, 80), f0(bent, 55, 90)
    semis = 12 * np.log2(f_bent / f_plain)
    check(abs(semis - 2.0) < 0.01, f'pitch wheel up: {f_plain:.3f} -> {f_bent:.3f} Hz, {semis:.4f} semitones (range 2)')
    route = [{'source': 'mod_wheel', 'destination': 'filter_1_cutoff'}]
    filtered = dict(filter_1_on=1, modulation_1_amount=-0.25, modulations=route)
    mod0, _ = web(patch(base, 'mod0', **filtered))
    mod1, _ = web(patch(base, 'mod1', mod_wheel=1.0, **filtered))
    unrouted, _ = web(patch(base, 'nomod', filter_1_on=1))
    same = np.array_equal(unrouted, mod0)

    def h5(x):
        """5th harmonic re the fundamental at C2, Blackman-Harris windowed (an
        unwindowed centroid reads the leakage floor and went the wrong way)."""
        w = x[22050:66150, 0]
        t = np.arange(len(w)) / (len(w) - 1)
        bh = 0.35875 - 0.48829 * np.cos(2 * np.pi * t) + 0.14128 * np.cos(4 * np.pi * t) - 0.01168 * np.cos(6 * np.pi * t)
        spec = np.abs(np.fft.rfft(w * bh, 1 << 20))
        freqs = np.fft.rfftfreq(1 << 20, 1 / 44100)
        line = lambda k: spec[(freqs > k * 65.406 - 3) & (freqs < k * 65.406 + 3)].max()  # noqa: E731
        return 20 * np.log10(line(5) / line(1))

    def level(x):
        return 20 * np.log10(np.sqrt(np.mean(x[22050:66150] ** 2)))
    h0, h1 = h5(unrouted), h5(mod1)
    check(same and h1 < h0 - 20 and level(mod1) > -45,
          f'mod wheel -> cutoff: at 0 the route is bit-identical to none ({same}); at 1 the 5th harmonic falls '
          f'{h0:.1f} -> {h1:.1f} dBc at {level(mod1):.1f} dBFS (darker, still sounding)')

    # 7. Presets.
    import base64
    rich = json.loads(json.dumps(base))
    rs = rich['settings']
    n = np.arange(2048)
    sine = np.sin(2 * np.pi * n / 2048).astype('<f4')
    square = np.where(n < 1024, 0.9, -0.9).astype('<f4')
    keyframes = [{'position': 0, 'wave_data': base64.b64encode(sine.tobytes()).decode()},
                 {'position': 256, 'wave_data': base64.b64encode(square.tobytes()).decode()}]
    rs['wavetables'][0]['groups'][0]['components'][0]['keyframes'] = keyframes
    rs['wavetables'][0]['name'] = 'Sine to square'
    rs['lfos'][0] = {'name': 'Steps', 'num_points': 5, 'points': [0, 1, 0.25, 0.2, 0.5, 0.8, 0.75, 0, 1, 1],
                     'powers': [0, 0, 0, 0, 0], 'smooth': False}
    rs['modulations'][0] = {'source': 'lfo_1', 'destination': 'osc_1_wave_frame'}
    rs['modulation_1_amount'] = 0.7
    rs['wobble_shape'] = {'name': 'Drawn', 'num_points': 5, 'points': [0, 0, 0.25, 1, 0.5, 0.3, 0.75, 0.9, 1, 0],
                          'powers': [0, 0, 0, 0, 0], 'smooth': False}
    rs['wobble_amount_cutoff'] = 0.5
    rs['filter_1_on'] = 1.0
    rich_path = os.path.join(TMP, 'rich.vital')
    json.dump(rich, open(rich_path, 'w'))
    # Through the desktop's own loader and saver first, so the file is one the
    # plugin wrote, not one this test wrote.
    desktop_saved = os.path.join(TMP, 'rich_desktop.vital')
    subprocess.run([RENDER, '--headless', '--save', desktop_saved, '-o', os.path.join(TMP, 'y.wav'), '-l', '0.1',
                    rich_path], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    a_native = native(desktop_saved, 'C2')
    b_web = web(desktop_saved, 'C2')[0]
    plain_init = native(patch(base, 'init_again'), 'C2')
    # The keyframes morph SPECTRALLY (Vital's default), and a sine keyframe's
    # silent bins have phases that are FFT rounding noise: the browser's
    # maths library (musl) rounds atan2/sin/cos differently from glibc, the
    # loudest morphed frame lands a hair differently, and the whole table's
    # normalisation with it - one uniform gain. Swept by the LFO through the
    # morphed frames, a few frames also differ in shape by the same cause:
    # -41 dB once the gain is matched (docs/design/phase2-09-mobile.md). A
    # static keyframe position differs by the gain alone.
    n = min(len(a_native), len(b_web))
    gain = float(np.sum(a_native[:n] * b_web[:n]) / np.sum(b_web[:n] ** 2))
    d_shape = rel_db(a_native, gain * b_web)
    d_rich = rel_db(plain_init, a_native)
    check(abs(20 * np.log10(gain)) < 0.05 and d_shape < -35 and d_rich > -20,
          f'a desktop-saved patch with its own wavetable, LFO, route and wobble loads in the browser: gain '
          f'{20 * np.log10(gain):+.3f} dB, then {d_shape:.1f} dB (and it is not the init sound: {d_rich:.1f} dB from it)')
    timed = json.load(open(desktop_saved))
    timed['settings']['wavetables'][0]['groups'][0]['components'][0]['interpolation_style'] = 0
    timed_path = os.path.join(TMP, 'rich_time.vital')
    json.dump(timed, open(timed_path, 'w'))
    d_time = rel_db(native(timed_path, 'C2'), web(timed_path, 'C2')[0])
    check(d_time < -100, f'the same patch morphing in the time domain: browser vs desktop {d_time:.1f} dB, no gain to match')

    web_saved = os.path.join(TMP, 'rich_web.vital')
    subprocess.run(['node', WEB, desktop_saved, '-o', os.path.join(TMP, 'z.wav'), '-l', '0.1', '--save', web_saved],
                   check=True, capture_output=True)
    saved = json.load(open(web_saved))
    c_native = native(web_saved, 'C2')
    d_back = rel_db(a_native, c_native)
    check(d_back < -100 and saved['synth_version'] == base['synth_version'] and 'wobble_shape' in saved['settings'],
          f'saved by the browser, it loads in the desktop renderer and sounds as the original: {d_back:.1f} dB '
          f'(version {saved["synth_version"]})')

    probe = """
const fs = require('fs');
const create = new Function(fs.readFileSync(process.argv[1], 'utf8') + '; return createGnarlEngine;')();
const e = create(fs.readFileSync(process.argv[2]), 44100);
const answers = [e.load('not json {'), e.load('{"a":1}'), e.load(JSON.stringify({synth_version: '9.0.0', settings: {}})),
  e.load(JSON.stringify({synth_version: '0.9.0', settings: {}}))];
e.reset();
let sum = 0;
e.note(36, true);
for (let i = 0; i < 200; i += 1) for (const v of e.process(128)) sum += v * v;
console.log(JSON.stringify({ answers, sum, name: e.presetName() }));
"""
    after = subprocess.run(['node', '-e', probe, os.path.join(ROOT, 'ui/src/web/engine-core.js'),
                            os.path.join(ROOT, 'wasm/build/gnarl.wasm')], capture_output=True, text=True, check=True)
    fresh = subprocess.run(['node', '-e', probe.replace("const answers = [", "const answers = []; [", 1),
                            os.path.join(ROOT, 'ui/src/web/engine-core.js'), os.path.join(ROOT, 'wasm/build/gnarl.wasm')],
                           capture_output=True, text=True, check=True)
    got, clean = json.loads(after.stdout), json.loads(fresh.stdout)
    check(got['answers'] == [1, 1, 2, 3] and got['sum'] == clean['sum'] and got['sum'] > 0,
          f'refused: garbage, not a patch, newer, older -> {got["answers"]}; afterwards the init patch plays '
          f'bit-identically to a fresh engine ({got["sum"] == clean["sum"]})')

    # 6. Speed.
    _, speed = web(heavy, 'F1', seconds=4)
    factor = float(speed.split(':')[1].split('x')[0])
    check(factor > 1.5, f'heavy patch in the browser build: {factor:.1f}x real time on this machine')

    print(f'\n{len(FAILS)} failure(s)')
    return len(FAILS)


if __name__ == '__main__':
    sys.exit(main())
