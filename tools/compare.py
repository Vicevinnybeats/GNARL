#!/usr/bin/env python3
"""Compare a GNARL patch (or render) with a reference, in numbers.

    python3 tools/compare.py REFERENCE TARGET [--bpm 140] [--note C1] [--seconds 8]

REFERENCE and TARGET are each a measurement JSON (tools/measure.py --json),
a WAV, or a .vital patch, which is rendered with gnarl-render first. The
usual loop (docs/design/phase3-01-measure.md):

    python3 tools/measure.py reference.wav --bpm 140 --from 61 --to 69 --json refs/drop.json
    python3 tools/compare.py refs/drop.json my_patch.vital --bpm 140 --note F1

It prints each measurement side by side with the difference, and says which
is further from the reference. It never says a patch "sounds close": that
is the producer's call (CLAUDE.md §7), and these are the numbers to make it
with.
"""
import argparse, json, math, os, subprocess, sys, tempfile

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import measure  # noqa: E402
import wavio  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_RENDER = os.path.join(HERE, '..', 'headless/builds/linux/build/gnarl-render')


def load(path, args):
    if path.endswith('.json'):
        return json.load(open(path))
    if path.endswith('.vital'):
        out = os.path.join(tempfile.mkdtemp(prefix='gnarl-compare-'), 'render.wav')
        subprocess.run([args.render, '--headless', '-o', out, '-l', str(args.seconds), '-m', args.note,
                        '-b', str(args.bpm or 120), '--bits', '32', path],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        path = out
        x, sr = wavio.read(path)
        # Leave the attack and the release out: compare the held sound.
        x = x[int(0.5 * sr):int((args.seconds - 0.5) * sr)]
    else:
        x, sr = wavio.read(path)
    return measure.measure(x, sr, args.bpm)


def spectrum_similarity(a, b):
    """Correlation of two modulation spectra (dB on the same grid), -1..1."""
    a, b = np.asarray(a, float), np.asarray(b, float)
    n = min(len(a), len(b))
    if n < 3 or np.std(a[:n]) == 0 or np.std(b[:n]) == 0:
        return float('nan')
    return float(np.corrcoef(a[:n], b[:n])[0, 1])


def rate_of(section):
    r = section.get('rate') if section else None
    if not r:
        return None
    return r.get('per_beat', r['hz'])


def rows(ref, tgt):
    out = [('loudness', ref['loudness_lufs'], tgt['loudness_lufs'], 'LUFS'),
           ('crest', ref['crest_db'], tgt['crest_db'], 'dB')]
    for band in ref['bands']:
        r, t = ref['bands'][band], tgt['bands'][band]
        out.append((f'{band} share', 100 * r, 100 * t, '%'))
    for band, r in ref['stereo']['side_to_mid_db'].items():
        out.append((f'{band} side/mid', max(r, -60), max(tgt['stereo']['side_to_mid_db'][band], -60), 'dB'))
    out.append(('centroid', ref['brightness']['centroid_mean_hz'], tgt['brightness']['centroid_mean_hz'], 'Hz'))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('reference')
    ap.add_argument('target')
    ap.add_argument('--bpm', type=float)
    ap.add_argument('--note', default='C1', help='for a .vital: the note to render')
    ap.add_argument('--seconds', type=float, default=8.0, help='for a .vital: how long to render')
    ap.add_argument('--render', default=DEFAULT_RENDER)
    args = ap.parse_args()

    ref, tgt = load(args.reference, args), load(args.target, args)
    print(f'{"":18} {"reference":>11} {"target":>11} {"difference":>12}')
    for name, r, t, unit in rows(ref, tgt):
        if unit == 'Hz':
            diff = f'{12 * math.log2(t / r):+.1f} st' if r > 0 and t > 0 else 'n/a'
        else:
            diff = f'{t - r:+.1f} {unit}'
        print(f'{name:18} {r:11.1f} {t:11.1f} {diff:>12}')

    print()
    for label, rs, ts in (('level wobble', ref['modulation']['growl_band'], tgt['modulation']['growl_band']),
                          ('brightness wobble', ref['brightness'].get('movement'), tgt['brightness'].get('movement'))):
        if not rs or not ts:
            print(f'{label:18} not measurable in one of them')
            continue
        unit = '/beat' if args.bpm else ' Hz'
        rr, tr = rate_of(rs), rate_of(ts)
        same = 'same rate' if rr and tr and abs(rr - tr) / rr < 0.02 else 'DIFFERENT rate'
        sim = spectrum_similarity(rs['spectrum_db'], ts['spectrum_db'])
        print(f'{label:18} rate {rr}{unit} vs {tr}{unit} ({same}); depth {rs["depth"]:.3f} vs {ts["depth"]:.3f}; '
              f'spectrum correlation {sim:.2f}')


if __name__ == '__main__':
    main()
