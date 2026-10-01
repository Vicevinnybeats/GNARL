#!/usr/bin/env python3
"""The AI button's generator (ui/src/generate.ts, docs/design/phase4-01-generator.md).

Generates patches from fixed seeds through tools/generate_patches.mjs and
renders each with the desktop renderer at 140 BPM. Each is a variation of a
base patch (Ref Wob 1-6, Vinny Bass 2), so it is measured against its base
on the sound matcher's own measure (tools/match.py: a smoothed log-mel
spectrogram of the growl band) - a variation should stay a neighbour of
the sound it came from. With --targets DIR (the matcher's target wobs,
which never leave a session's scratch space) it also reports how close the
variations come to the references.

  python3 tests/test_generate.py [--seeds N] [--targets DIR]   (needs librosa, Node 22.18+)
"""
import argparse, glob, json, os, re, subprocess, sys, tempfile
import numpy as np, soundfile as sf

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import match  # noqa: E402

RENDER = match.RENDER
BPM = 140
failures = 0


def check(ok, message):
    global failures
    print(('PASS ' if ok else 'FAIL ') + message, flush=True)
    failures += 0 if ok else 1


def generate(out, first, count):
    lines = subprocess.run(['node', os.path.join(ROOT, 'tools/generate_patches.mjs'), out, str(first), str(count)],
                           check=True, capture_output=True, text=True).stdout.splitlines()
    return [json.loads(l) for l in lines]


def render(patch, note, wav):
    r = subprocess.run([RENDER, '--headless', '-o', wav, '-l', '2', '-m', note, '-b', str(BPM), '--bits', '32', patch],
                       capture_output=True)
    if r.returncode != 0:
        return None
    x, _ = sf.read(wav)
    return x


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--seeds', type=int, default=24)
    ap.add_argument('--targets', help='a directory of target-*.wav (tools/match.py), to report closeness')
    args = ap.parse_args()
    L = int(1.25 * match.SR)
    with tempfile.TemporaryDirectory() as tmp:
        wav = os.path.join(tmp, 'o.wav')
        made = generate(os.path.join(tmp, 'a'), 1, args.seeds)
        generate(os.path.join(tmp, 'b'), 1, args.seeds)
        same = all(open(os.path.join(tmp, 'a', f'{g["seed"]}.vital')).read() ==
                   open(os.path.join(tmp, 'b', f'{g["seed"]}.vital')).read() for g in made)
        check(same, f'a seed always makes the same patch ({args.seeds} seeds, twice)')
        check(len({g['name'] for g in made}) == len(made), 'every patch has its own name')

        base_features = {}
        targets = {}
        if args.targets:
            for f in sorted(glob.glob(os.path.join(args.targets, 'target-*.wav'))):
                x, _ = sf.read(f)
                n = min(len(x), L)
                targets[os.path.basename(f)[7:-4]] = (match.features(x, n), n)
        peaks, near, loaded, finite, closest = [], [], 0, True, {t: [] for t in targets}
        for g in made:
            patch = os.path.join(tmp, 'a', f'{g["seed"]}.vital')
            base = re.search(r'from (.+?)[,.]', g['about']).group(1)
            loudest, ok, f1 = -999.0, True, None
            for note in ['D1', 'F1', 'D#2', 'F2']:
                x = render(patch, note, wav)
                if x is None:
                    ok = False
                    break
                finite = finite and bool(np.isfinite(x).all())
                loudest = max(loudest, 20 * np.log10(np.abs(x).max() + 1e-12))
                if note == 'F1':
                    f1 = x.mean(1)
            loaded += ok
            peaks.append(loudest)
            if f1 is None:
                continue
            if base not in base_features:
                bx = render(os.path.join(ROOT, 'presets', f'{base}.vital'), 'F1', wav)
                base_features[base] = match.features(bx.mean(1), L)
            d = match.distance(match.features(f1, L), base_features[base])
            near.append(d)
            for t, (feat, n) in targets.items():
                closest[t].append(match.distance(match.features(f1, n), feat))
            print(f'     {g["seed"]:3d} {g["name"]:20s} peak {loudest:6.1f} dBFS  {d:4.2f} dB from {base}')
        check(loaded == len(made), f'the renderer loads every generated patch ({loaded}/{len(made)})')
        check(finite, 'every render is finite')
        check(max(peaks) <= -1.5, f'the loudest, D1 to F2, peaks at {max(peaks):.1f} dBFS (at most -1.5)')
        check(float(np.median(near)) <= 3.0,
              f'a variation stays near its base: median {np.median(near):.2f} dB, most {max(near):.2f} '
              f'(a different patch is 5-6 dB away)')
        for t, ds in closest.items():
            print(f'     closest variation to {t}: {min(ds):.2f} dB, median {np.median(ds):.2f}')
    print(f'\n{failures} failure(s)')
    sys.exit(1 if failures else 0)


main()
