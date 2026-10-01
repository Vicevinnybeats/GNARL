#!/usr/bin/env python3
"""The FM knob and the FOLD warp mode.

    python3 tests/test_fm.py [path/to/gnarl-render]

docs/design/phase2-08-panel-controls.md. Every check renders and measures;
exit status is the number of failures.

1. Zero changes nothing: osc_N_fm_amount 0 renders bit-identically to the
   same patch without the keys.
2. It IS Vital's FM: osc 1 with no warp and FM a is bit-identical to Vital's
   own warp mode "FM <- Osc 2" at amount a - the same law (amount squared),
   the same phase arithmetic, the same oscillator.
3. It runs alongside a warp mode: FORMANT plus FM differs from FORMANT
   alone and from FM alone by more than -20 dB each.
4. An oscillator that is off modulates nothing: FM from osc 2 while osc 2 is
   off is bit-identical to no FM.
5. Two oscillators FM-ing each other still sound (the lower one hears the
   other's previous block rather than waiting forever).
6. Block size: FM is no worse at blocks 32 vs 512 than Vital's FM warp.
7. FOLD off (the default) is bit-identical to a patch without osc_N_fold.
8. FOLD skips the warp: FOLD on with FORMANT selected and WARP 0 is
   bit-identical to no warp at all.
9. FOLD folds: on a pure sine (osc 1's spectral low pass at 0), WARP 0.8
   puts the 3rd harmonic above -30 dBc where the sine has none.
10. Block size: FOLD is no worse than Vital's FM warp.
"""

import json, os, subprocess, sys, tempfile
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'tools'))
import wavio  # noqa: E402

RENDER = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'headless/builds/linux/build/gnarl-render')
TMP = tempfile.mkdtemp(prefix='gnarl-fm-')
FAILS = []
# Vital's SynthOscillator::DistortionType indices.
NONE, FORMANT, FM_OSC_A = 0, 2, 7


def check(ok, msg):
    print(('PASS ' if ok else 'FAIL ') + msg)
    if not ok:
        FAILS.append(msg)


def render(base, name, block=64, drop=(), **overrides):
    d = json.loads(json.dumps(base))
    s = d['settings']
    for k in drop:
        s.pop(k)
    for k, v in overrides.items():
        if k not in s:
            raise KeyError(k)
        s[k] = float(v)
    path = os.path.join(TMP, name + '.vital')
    json.dump(d, open(path, 'w'))
    out = f'{path}.{block}.wav'
    subprocess.run([RENDER, '--headless', '-o', out, '-l', '2', '-m', 'C2', '--bits', '32', '--block', str(block),
                    path], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    x, _ = wavio.read(out)
    return x, out


def same(a, b):
    return open(a, 'rb').read() == open(b, 'rb').read()


def diff_db(a, b):
    n = min(len(a), len(b))
    return 20 * np.log10(max(np.sqrt(np.mean((a[:n] - b[:n]) ** 2)), 1e-300) / np.sqrt(np.mean(a[:n] ** 2)))


def blocks_db(base, name, **settings):
    a, _ = render(base, name, block=32, **settings)
    b, _ = render(base, name, block=512, **settings)
    n = min(len(a), len(b))
    return 20 * np.log10(max(np.max(np.abs(a[:n] - b[:n])), 1e-300) / np.max(np.abs(a[:n])))


def main():
    init = os.path.join(TMP, 'init.vital')
    subprocess.run([RENDER, '--headless', '--save', init, '-o', os.path.join(TMP, 'x.wav'), '-l', '0.1'],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    base = json.load(open(init))
    # Both oscillators on, deterministic, osc 2 an octave up so the FM is plain.
    base['settings'].update(osc_1_random_phase=0.0, osc_2_random_phase=0.0, osc_2_on=1.0, osc_2_transpose=12.0,
                            env_1_sustain=1.0)
    keys = [k for k in base['settings'] if k.endswith('_fm_amount')]
    check(sorted(keys) == ['osc_1_fm_amount', 'osc_2_fm_amount', 'osc_3_fm_amount'], f'parameters saved: {keys}')

    # 1. Zero changes nothing.
    _, a = render(base, 'zero')
    _, b = render(base, 'nokeys', drop=keys)
    check(same(a, b), 'FM 0 is bit-identical to a patch without the keys')

    # 2. It is Vital's FM.
    for amount in (0.3, 0.7):
        _, ours = render(base, f'ours{amount}', osc_1_distortion_type=NONE, osc_1_fm_amount=amount)
        _, vitals = render(base, f'vital{amount}', osc_1_distortion_type=FM_OSC_A, osc_1_distortion_amount=amount)
        check(same(ours, vitals), f'FM {amount} is bit-identical to Vital\'s "FM <- Osc 2" warp at {amount}')

    # 3. Alongside a warp mode.
    both, _ = render(base, 'both', osc_1_distortion_type=FORMANT, osc_1_distortion_amount=0.6, osc_1_fm_amount=0.5)
    warp, _ = render(base, 'warp', osc_1_distortion_type=FORMANT, osc_1_distortion_amount=0.6)
    fm, _ = render(base, 'fm', osc_1_distortion_type=NONE, osc_1_fm_amount=0.5)
    d1, d2 = diff_db(both, warp), diff_db(both, fm)
    check(d1 > -20 and d2 > -20, f'FORMANT + FM differs from FORMANT alone by {d1:.1f} dB, from FM alone by {d2:.1f} dB')

    # 4. No FM from an oscillator that is off.
    _, a = render(base, 'off_fm', osc_2_on=0, osc_1_fm_amount=0.8)
    _, b = render(base, 'off_nofm', osc_2_on=0)
    check(same(a, b), 'FM from osc 2 while osc 2 is off is bit-identical to no FM')

    # 5. Mutual FM still sounds.
    mutual, _ = render(base, 'mutual', osc_1_fm_amount=0.5, osc_2_fm_amount=0.5)
    level = 20 * np.log10(np.sqrt(np.mean(mutual[22050:] ** 2)))
    check(np.isfinite(mutual).all() and level > -40, f'osc 1 and osc 2 FM-ing each other: {level:.1f} dBFS RMS')

    # 6. Block size.
    ours = blocks_db(base, 'blk_ours', osc_1_distortion_type=NONE, osc_1_fm_amount=0.5)
    vitals = blocks_db(base, 'blk_vital', osc_1_distortion_type=FM_OSC_A, osc_1_distortion_amount=0.5)
    check(ours <= vitals + 1.0, f'FM, blocks 32 vs 512: {ours:.1f} dB (Vital\'s FM warp: {vitals:.1f} dB)')

    # 7-10. FOLD.
    fold_keys = [k for k in base['settings'] if k.startswith('osc_') and k.endswith('_fold')]
    _, a = render(base, 'fold_default')
    _, b = render(base, 'fold_nokeys', drop=fold_keys)
    check(len(fold_keys) == 3 and same(a, b), f'FOLD off is bit-identical to no FOLD keys ({fold_keys})')
    _, a = render(base, 'fold_formant0', osc_1_fold=1, osc_1_distortion_type=FORMANT, osc_1_distortion_amount=0)
    _, b = render(base, 'fold_none0', osc_1_distortion_type=NONE, osc_1_distortion_amount=0)
    check(same(a, b), 'FOLD on, FORMANT selected, WARP 0: bit-identical to no warp')
    sine = dict(osc_2_on=0, osc_1_spectral_morph_type=7, osc_1_spectral_morph_amount=0, osc_1_distortion_type=NONE)

    def third(x):
        w = x[22050:66150, 0]
        n = len(w)
        t = np.arange(n) / (n - 1)
        bh = 0.35875 - 0.48829 * np.cos(2 * np.pi * t) + 0.14128 * np.cos(4 * np.pi * t) - 0.01168 * np.cos(6 * np.pi * t)
        spec = np.abs(np.fft.rfft(w * bh, 1 << 20))
        f = np.fft.rfftfreq(1 << 20, 1 / 44100)
        f0 = 440 * 2 ** ((36 - 69) / 12)
        h1 = spec[(f > f0 - 3) & (f < f0 + 3)].max()
        return 20 * np.log10(spec[(f > 3 * f0 - 3) & (f < 3 * f0 + 3)].max() / h1)
    plain, _ = render(base, 'sine', **sine)
    folded, _ = render(base, 'sine_fold', osc_1_fold=1, osc_1_distortion_amount=0.8, **sine)
    h_plain, h_fold = third(plain), third(folded)
    check(h_fold > -30 and h_plain < -80, f'FOLD at WARP 0.8: 3rd harmonic {h_fold:.1f} dBc (plain sine {h_plain:.1f})')
    ours = blocks_db(base, 'blk_fold', osc_1_fold=1, osc_1_distortion_amount=0.8)
    check(ours <= vitals + 1.0, f'FOLD, blocks 32 vs 512: {ours:.1f} dB (Vital\'s FM warp: {vitals:.1f} dB)')

    print(f'\n{len(FAILS)} failure(s)')
    return len(FAILS)


if __name__ == '__main__':
    sys.exit(main())
