#!/usr/bin/env python3
"""Wobble macro tests, run against the real engine through gnarl-render.

    python3 tests/test_wobble.py [path/to/gnarl-render]

Every check renders audio and measures it; none asserts that code was
called. Exit status is the number of failures.

1. Zero depth changes nothing: depth 0 renders bit-identically to the same
   patch with the wobble keys removed entirely.
2. Each rate lands where it should at 140 BPM: the peak of the output
   envelope's modulation spectrum within 1% of 2.333 / 4.667 / 7.000 /
   9.333 Hz. Envelope hop 128 samples (344 frames/s, Nyquist 172 Hz), so it
   cannot alias the way an 8192-sample hop once did; Blackman-Harris window.
3. Rate follows tempo: 1/8 at 100 BPM is 3.333 Hz.
4. Transport lock: the same note started half a period later in the bar has
   its wobble shifted by half a period. A NEGATIVE CONTROL runs the same
   measurement on a note-triggered LFO, which must NOT shift - otherwise the
   test could pass on anything.
5. A wobble depth is EXACTLY a matrix connection of that amount: bit-identical
   to "wobble -> filter_1_cutoff" through the matrix, at blocks 32 and 128.
6. Block size: 32 vs 128 is no worse than Vital's OWN LFO on the same
   destination. Not an absolute -100 dB: that was the first threshold, set
   before measuring, and Vital's LFO itself measures about -64 dB here - the
   engine interpolates control values across each block, so no modulated
   patch is block-size exact. The wobble is held to the engine's own figure.
7. The vowel route (wobble_amount_formant, phase2-06-vowel-filter.md) is
   EXACTLY a matrix connection to filter_1_formant_x, with filter 1 in
   Vital's formant model, at blocks 32 and 128.
8. And it does something: the render differs from depth 0 by more than
   -20 dB, where check 7 alone would pass for a route that moved nothing
   in both renders.

Oscillator random phase is switched off in every patch, so two renders
differ only by what the test changes.
"""
import json, os, subprocess, sys, tempfile
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'tools'))
import wavio  # noqa: E402

RENDER = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'headless/builds/linux/build/gnarl-render')
TMP = tempfile.mkdtemp(prefix='gnarl-wobble-')
FAILS = []


def check(ok, msg):
    print(('PASS ' if ok else 'FAIL ') + msg)
    if not ok:
        FAILS.append(msg)


def render(patch, out, seconds, note='C2', bpm=140, block=64, start=0.0):
    subprocess.run([RENDER, '--headless', '-o', out, '-l', str(seconds), '-m', note, '-b', str(bpm),
                    '--bits', '32', '--block', str(block), '--start', repr(start), patch],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    x, sr = wavio.read(out)
    return x.mean(axis=1), sr


def init_preset():
    path = os.path.join(TMP, 'init.vital')
    subprocess.run([RENDER, '--headless', '--save', path, '-o', os.path.join(TMP, 'x.wav'), '-l', '0.1'],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return json.load(open(path))


def patch(base, name, drop=(), modulation=None, **overrides):
    d = json.loads(json.dumps(base))
    s = d['settings']
    for k in drop:
        s.pop(k, None)
    for k, v in overrides.items():
        if k not in s:
            raise KeyError(f'unknown parameter {k}')
        s[k] = float(v)
    if modulation:
        s['modulations'][0] = {'source': modulation[0], 'destination': modulation[1]}
        s['modulation_1_amount'] = float(modulation[2])
    path = os.path.join(TMP, name + '.vital')
    json.dump(d, open(path, 'w'))
    return path


def blackman_harris(n):
    a = [0.35875, 0.48829, 0.14128, 0.01168]
    k = np.arange(n) / (n - 1)
    return a[0] - a[1] * np.cos(2 * np.pi * k) + a[2] * np.cos(4 * np.pi * k) - a[3] * np.cos(6 * np.pi * k)


def envelope(x, sr, start, stop, hop=128):
    m = x[int(start * sr):int(stop * sr)]
    frames = len(m) // hop
    return np.sqrt((m[:frames * hop].reshape(frames, hop) ** 2).mean(axis=1)), sr / hop


def filter_envelope(x, sr, start, stop):
    # A window of two periods of C2 (65.4 Hz): an envelope of the FILTER
    # SWEEP, not of the sawtooth. A short window mostly measures the
    # oscillator's own waveform, which is how an earlier lag test found no
    # lag it could never have seen.
    w = 1356
    m = x[int(start * sr):int(stop * sr)]
    c = np.cumsum(np.concatenate([[0.0], m ** 2]))
    return np.sqrt((c[w:] - c[:-w]) / w)


def envelope_lag(a, b, sr, period):
    # Circular lag of b against a, in fractions of a wobble period.
    ea, eb = filter_envelope(a, sr, 0.5, 2.5), filter_envelope(b, sr, 0.5, 2.5)
    step = 8
    n = int(period * sr)
    errs = [(k, np.abs(np.roll(eb, -k)[n:-n] - ea[n:-n]).mean()) for k in range(0, n, step)]
    k = min(errs, key=lambda e: e[1])[0]
    return k / n


def modulation_peak(x, sr):
    env, fps = envelope(x, sr, 0.5, 7.5)
    env = env - env.mean()
    n = 1 << 16
    spec = np.abs(np.fft.rfft(env * blackman_harris(len(env)), n))
    f = np.fft.rfftfreq(n, 1 / fps)
    band = (f > 0.5) & (f < 25)
    return f[band][np.argmax(spec[band])]


def main():
    base = init_preset()
    wobble = dict(env_1_attack=0, env_1_sustain=1, filter_1_on=1, filter_1_cutoff=40, osc_1_random_phase=0)

    # 1. zero depth
    a, _ = render(patch(base, 'zero', **wobble), os.path.join(TMP, 'zero.wav'), 3)
    keys = [k for k in base['settings'] if k.startswith('wobble')]
    b, _ = render(patch(base, 'nokeys', drop=keys, **wobble), os.path.join(TMP, 'nokeys.wav'), 3)
    check(len(a) == len(b) and np.array_equal(a, b), 'zero depth is bit-identical to a patch with no wobble keys')

    # 2. rates at 140 BPM
    for index, (label, cycles) in enumerate([('1/4', 1), ('1/8', 2), ('1/8T', 3), ('1/16', 4)]):
        x, sr = render(patch(base, f'rate{index}', wobble_amount_cutoff=0.6, wobble_rate=index, **wobble),
                       os.path.join(TMP, f'rate{index}.wav'), 8)
        expect = 140 / 60 * cycles
        got = modulation_peak(x, sr)
        check(abs(got - expect) / expect < 0.01, f'{label:5} at 140 BPM: {got:.3f} Hz, expected {expect:.3f}')

    # 3. tempo follows the host
    x, sr = render(patch(base, 'tempo100', wobble_amount_cutoff=0.6, wobble_rate=1, **wobble),
                   os.path.join(TMP, 'tempo100.wav'), 8, bpm=100)
    got = modulation_peak(x, sr)
    check(abs(got - 100 / 60 * 2) / (100 / 60 * 2) < 0.01, f'1/8 at 100 BPM: {got:.3f} Hz, expected 3.333')

    # 4. transport lock, with a negative control
    period = 60 / 140                                  # 1/4 at 140 BPM: one beat
    locked = patch(base, 'lock', wobble_amount_cutoff=0.6, wobble_rate=0, **wobble)
    a, sr = render(locked, os.path.join(TMP, 'lock0.wav'), 3)
    b, _ = render(locked, os.path.join(TMP, 'lockhalf.wav'), 3, start=period / 2)
    lag = envelope_lag(a, b, sr, period)
    check(abs(lag - 0.5) < 0.03, f'transport lock: note started half a beat later -> wobble shifted {lag:.3f} periods (expect 0.5)')
    trig = patch(base, 'trig', lfo_1_sync=1, lfo_1_tempo=8, lfo_1_sync_type=0,
                 modulation=('lfo_1', 'filter_1_cutoff', 0.6), **wobble)
    a, _ = render(trig, os.path.join(TMP, 'trig0.wav'), 3)
    b, _ = render(trig, os.path.join(TMP, 'trighalf.wav'), 3, start=period / 2)
    lag = envelope_lag(a, b, sr, period)
    check(min(lag, 1 - lag) < 0.03, f'negative control: note-triggered LFO shifted {lag:.3f} periods (expect 0)')

    # 5. a depth is exactly a matrix connection
    route = patch(base, 'route', wobble_amount_cutoff=0.6, wobble_rate=2, **wobble)
    matrix = patch(base, 'matrix', wobble_rate=2, modulation=('wobble', 'filter_1_cutoff', 0.6), **wobble)
    for block in (32, 128):
        x, _ = render(route, os.path.join(TMP, f'route{block}.wav'), 3, block=block)
        y, _ = render(matrix, os.path.join(TMP, f'matrix{block}.wav'), 3, block=block)
        check(np.array_equal(x, y), f'block {block}: wobble depth 0.6 is bit-identical to a 0.6 matrix connection')

    # 6. block size, relative to Vital's own LFO on the same destination
    def block_db(p, tag):
        x32, _ = render(p, os.path.join(TMP, f'{tag}32.wav'), 3, block=32)
        x128, _ = render(p, os.path.join(TMP, f'{tag}128.wav'), 3, block=128)
        return 20 * np.log10(np.abs(x32 - x128).max() / np.abs(x32).max())
    lfo = patch(base, 'lfo', lfo_1_sync=3, lfo_1_tempo=9, lfo_1_sync_type=1,
                modulation=('lfo_1', 'filter_1_cutoff', 0.6), **wobble)
    w_db, l_db = block_db(route, 'bw'), block_db(lfo, 'bl')
    check(w_db <= l_db + 1.0, f'block 32 vs 128: wobble {w_db:.1f} dB, Vital LFO {l_db:.1f} dB (wobble may not be worse)')

    # 7. the vowel route is exactly a matrix connection. Formant model (5),
    # with a saw rich enough in harmonics for the formants to shape.
    vowel = dict(wobble, filter_1_model=5, filter_1_formant_x=0.3)
    route = patch(base, 'vroute', wobble_amount_formant=0.6, wobble_rate=2, **vowel)
    matrix = patch(base, 'vmatrix', wobble_rate=2, modulation=('wobble', 'filter_1_formant_x', 0.6), **vowel)
    for block in (32, 128):
        x, _ = render(route, os.path.join(TMP, f'vroute{block}.wav'), 3, block=block)
        y, _ = render(matrix, os.path.join(TMP, f'vmatrix{block}.wav'), 3, block=block)
        check(np.array_equal(x, y), f'block {block}: vowel depth 0.6 is bit-identical to a 0.6 matrix connection')

    # 8. ... and moves the sound.
    still, _ = render(patch(base, 'vstill', wobble_rate=2, **vowel), os.path.join(TMP, 'vstill.wav'), 3)
    moved, _ = render(route, os.path.join(TMP, 'vmoved.wav'), 3)
    n = min(len(still), len(moved))
    change = 20 * np.log10(np.sqrt(np.mean((moved[:n] - still[:n]) ** 2)) / np.sqrt(np.mean(still[:n] ** 2)))
    check(change > -20, f'vowel depth 0.6 changes the sound by {change:.1f} dB against depth 0')

    print(f'\n{len(FAILS)} failure(s)')
    return len(FAILS)


if __name__ == '__main__':
    sys.exit(main())
