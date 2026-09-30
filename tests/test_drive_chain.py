#!/usr/bin/env python3
"""The drive chain: FOLD and CRUSH after Vital's drive stage.

    python3 tests/test_drive_chain.py [path/to/gnarl-render]

docs/design/phase2-07-drive-chain.md. Every check renders and measures;
exit status is the number of failures.

1. Off changes nothing: fold and crush off renders bit-identically to the
   same patch without their keys - with the drive stage on and with it off.
2. FOLD is Vital's own fold: FOLD alone (drive stage off) is bit-identical to
   Vital's single stage set to the same fold type and drive, for sine and
   linear. So the fold stage cannot have drifted from the processor it reuses.
3. FOLD MIX 0 is bit-identical to FOLD off.
4. The chain is a chain: drive + fold differs from either stage alone by
   more than -20 dB, so both act on the signal.
5. CRUSH bits: at 1x oversampling (the decimator then passes samples
   through), 3 bits leaves at most 2^3 + 1 distinct output values in the
   sustained note; with CRUSH off there are thousands.
6. CRUSH rate: at 50% the hold is 64^0.5 = 8 samples; the median run of
   identical output samples is 8.
7. Block size, at blocks 32 and 512:
   a. CRUSH alone is exact (differs by less than -200 dB): its hold counter
      and ramps carry across blocks.
   b. FOLD alone is no worse than Vital's own fold stage, which differs by
      about -113 dB (its drive smoothing rounds differently per block). A
      single chain render with CRUSH after FOLD was the first form of this
      check, and it was unsound: 5-bit quantisation turns those -113 dB
      differences into whole-step flips (-27 dB) wherever a sample sits on
      a step boundary, so it passed or failed by where the steps fell.
"""
import json, os, subprocess, sys, tempfile
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'tools'))
import wavio  # noqa: E402

RENDER = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'headless/builds/linux/build/gnarl-render')
TMP = tempfile.mkdtemp(prefix='gnarl-drive-')
FAILS = []
WINDOW = (0.5, 1.5)
# Vital's Distortion::Type indices.
LINEAR_FOLD, SIN_FOLD, HARD_CLIP = 2, 3, 1


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
    x, sr = wavio.read(out)
    return x[:, 0], sr, out


def same_file(a, b):
    return open(a, 'rb').read() == open(b, 'rb').read()


def window(x, sr):
    return x[int(WINDOW[0] * sr):int(WINDOW[1] * sr)]


def db_diff(a, b):
    n = min(len(a), len(b))
    return 20 * np.log10(max(np.sqrt(np.mean((a[:n] - b[:n]) ** 2)), 1e-300) / np.sqrt(np.mean(a[:n] ** 2)))


def main():
    init = os.path.join(TMP, 'init.vital')
    subprocess.run([RENDER, '--headless', '--save', init, '-o', os.path.join(TMP, 'x.wav'), '-l', '0.1'],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    base = json.load(open(init))
    base['settings'].update(osc_1_random_phase=0.0, env_1_sustain=1.0)
    chain_keys = [k for k in base['settings'] if k.startswith(('distortion_fold_', 'distortion_crush_'))]
    check(len(chain_keys) == 7, f'the seven chain parameters are saved: {sorted(chain_keys)}')

    # 1. Off changes nothing.
    for on in (0, 1):
        _, _, a = render(base, f'off{on}', distortion_on=on, distortion_type=HARD_CLIP, distortion_drive=12)
        _, _, b = render(base, f'old{on}', drop=chain_keys, distortion_on=on, distortion_type=HARD_CLIP,
                         distortion_drive=12)
        check(same_file(a, b), f'drive stage {"on" if on else "off"}, fold and crush off: bit-identical to no chain keys')

    # 2. FOLD is Vital's fold.
    for label, fold_type, vital_type in (('sine', 0, SIN_FOLD), ('linear', 1, LINEAR_FOLD)):
        _, _, ours = render(base, f'fold_{label}', distortion_on=0, distortion_fold_on=1,
                            distortion_fold_type=fold_type, distortion_fold_drive=9)
        _, _, vitals = render(base, f'vital_{label}', distortion_on=1, distortion_type=vital_type, distortion_drive=9)
        check(same_file(ours, vitals), f'{label} FOLD alone is bit-identical to Vital\'s single {label} fold stage')

    # 3. FOLD MIX 0 is FOLD off.
    _, _, mixed = render(base, 'foldmix0', distortion_on=1, distortion_type=HARD_CLIP, distortion_drive=12,
                         distortion_fold_on=1, distortion_fold_drive=9, distortion_fold_mix=0)
    _, _, plain = render(base, 'foldoff', distortion_on=1, distortion_type=HARD_CLIP, distortion_drive=12)
    check(same_file(mixed, plain), 'FOLD MIX 0 is bit-identical to FOLD off')

    # 4. Both stages act.
    drive, _, _ = render(base, 'c_drive', distortion_on=1, distortion_type=HARD_CLIP, distortion_drive=12)
    fold, _, _ = render(base, 'c_fold', distortion_on=0, distortion_fold_on=1, distortion_fold_drive=9)
    both, _, _ = render(base, 'c_both', distortion_on=1, distortion_type=HARD_CLIP, distortion_drive=12,
                        distortion_fold_on=1, distortion_fold_drive=9)
    d1, d2 = db_diff(both, drive), db_diff(both, fold)
    check(d1 > -20 and d2 > -20, f'drive + fold differs from drive alone by {d1:.1f} dB, from fold alone by {d2:.1f} dB')

    # 5. CRUSH bits, at 1x oversampling so the output is the crushed samples.
    crushed, sr, _ = render(base, 'bits3', oversampling=0, distortion_crush_on=1, distortion_crush_bits=3)
    clean, _, _ = render(base, 'bitsoff', oversampling=0)
    levels = len(np.unique(np.round(window(crushed, sr), 7)))
    clean_levels = len(np.unique(np.round(window(clean, sr), 7)))
    check(levels <= 9 and clean_levels > 1000, f'3 bits: {levels} distinct output values (clean: {clean_levels})')

    # 6. CRUSH rate: 64^0.5 = 8-sample hold.
    held, sr, _ = render(base, 'rate50', oversampling=0, distortion_crush_on=1, distortion_crush_bits=16,
                         distortion_crush_rate=0.5)
    w = window(held, sr)
    edges = np.flatnonzero(np.diff(w) != 0)
    runs = np.diff(edges)
    median = float(np.median(runs)) if len(runs) else 0.0
    check(median == 8, f'rate 50%: median run of held samples {median:g} (expected 8)')

    # 7. Block size.
    def blocks_db(name, **settings):
        a, _, _ = render(base, name, block=32, **settings)
        b, _, _ = render(base, name, block=512, **settings)
        n = min(len(a), len(b))
        return 20 * np.log10(max(np.max(np.abs(a[:n] - b[:n])), 1e-300) / np.max(np.abs(a[:n])))

    crush_db = blocks_db('blk_crush', distortion_crush_on=1, distortion_crush_bits=5, distortion_crush_rate=0.3)
    check(crush_db < -200, f'CRUSH alone, blocks 32 vs 512: {crush_db:.1f} dB')
    for label, fold_type, vital_type in (('sine', 0, SIN_FOLD), ('linear', 1, LINEAR_FOLD)):
        ours = blocks_db(f'blk_{label}', distortion_on=0, distortion_fold_on=1, distortion_fold_type=fold_type,
                         distortion_fold_drive=9)
        vitals = blocks_db(f'blk_vital_{label}', distortion_on=1, distortion_type=vital_type, distortion_drive=9)
        check(ours <= vitals + 1.0, f'{label} FOLD alone, blocks 32 vs 512: {ours:.1f} dB '
              f'(Vital\'s own {label} fold: {vitals:.1f} dB; may not be worse)')

    print(f'\n{len(FAILS)} failure(s)')
    return len(FAILS)


if __name__ == '__main__':
    sys.exit(main())
