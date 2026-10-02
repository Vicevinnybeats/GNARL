#!/usr/bin/env python3
"""The AI button's generator (ui/src/generate.ts, docs/design/phase4-01-generator.md,
phase4-05-riddim-recipe.md).

Generates patches from fixed seeds through tools/generate_patches.mjs and
renders each with the desktop renderer at 140 BPM. Each is a variation of
Vinny Bass 2, the producer's own patch, and must stay a WOB: rendered at
the producer's note (D#3 in FL, MIDI 39) for 16 beats, tools/measure.py
must find beat-locked movement in the growl band, as deep as 0.4 (Vinny
Bass 2: 0.72; the matcher's Sig Wob 1: none found), at a riddim rate, and
no more of its energy above 5 kHz than Vinny Bass 2 has plus a little -
added highs are added screech. It is also measured against its base on
the sound matcher's measure (tools/match.py). With --targets DIR (the matcher's target wobs,
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


# The rates a riddim wob takes, per beat: 1/2, 1/4, 1/8, 1/8T (3), 1/16, and
# the 1/4T half-time triplet (1.5). LFO 4 bends LFO 1's speed within the bar,
# so a triplet measures 3.2-3.3.
RIDDIM_RATES = (0.5, 1.0, 1.5, 2.0, 3.0, 4.0)


def wob(patch, tmp):
    """(growl movement depth, its rate per beat, % of energy above 5 kHz) at D#3 FL."""
    wav, js = os.path.join(tmp, 'wob.wav'), os.path.join(tmp, 'wob.json')
    subprocess.run([RENDER, '--headless', '-o', wav, '-l', '16', '-m', 'D#2', '-b', str(BPM), patch],
                   check=True, capture_output=True)
    subprocess.run([sys.executable, os.path.join(ROOT, 'tools/measure.py'), wav, '--bpm', str(BPM),
                    '--from', '0.2', '--to', '6.5', '--json', js], check=True, capture_output=True)
    g = json.load(open(js))['modulation']['growl_band']
    x, sr = sf.read(wav)
    seg = x.mean(1)[int(0.2 * sr):int(6.5 * sr)]
    f = np.fft.rfftfreq(len(seg), 1 / sr)
    p = np.abs(np.fft.rfft(seg * np.blackman(len(seg)))) ** 2
    high = 100 * p[f > 5000].sum() / p.sum()
    if not g or not g.get('rate'):
        return 0.0, None, high
    return g['depth'], g['rate']['per_beat'], high


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
        # Every variation is a wob: it moves, at a riddim rate, no harsher
        # than the patch it came from.
        base_depth, base_rate, base_high = wob(os.path.join(ROOT, 'presets', 'Vinny Bass 2.vital'), tmp)
        print(f'     Vinny Bass 2: movement {base_depth:.2f} at {base_rate}/beat, {base_high:.1f}% above 5 kHz')
        wobs = [(g['name'],) + wob(os.path.join(tmp, 'a', f'{g["seed"]}.vital'), tmp) for g in made]
        still = [n for n, d, _, _ in wobs if d < 0.4]
        offbeat = [n for n, d, r, _ in wobs if r is None or min(abs(r - x) for x in RIDDIM_RATES) > 0.35]
        harsh = [n for n, _, _, h in wobs if h > base_high + 4]
        check(not still, f'every variation wobs (movement at least 0.4): '
              f'{min(d for _, d, _, _ in wobs):.2f}-{max(d for _, d, _, _ in wobs):.2f}; too still: {still}')
        check(not offbeat, f'at a riddim rate ({sorted({r for *_, r, _ in wobs if r})}); off the grid: {offbeat}')
        check(not harsh, f'no harsher than Vinny Bass 2 + 4 points above 5 kHz '
              f'(most {max(h for *_, h in wobs):.1f}%); harsher: {harsh}')
        check(loaded == len(made), f'the renderer loads every generated patch ({loaded}/{len(made)})')
        check(finite, 'every render is finite')
        check(max(peaks) <= -1.5, f'the loudest, D1 to F2, peaks at {max(peaks):.1f} dBFS (at most -1.5)')
        check(float(np.median(near)) <= 3.0,
              f'a variation stays near its base: median {np.median(near):.2f} dB, most {max(near):.2f} '
              f'(a different patch is 5-6 dB away)')
        # Chains of picks (the AI's pick-the-best mode): twelve rounds deep,
        # the level must neither creep up nor fade out.
        chain_peaks = []
        for chain_seed in (11, 22, 33, 44, 55):
            out = os.path.join(tmp, f'chain{chain_seed}')
            subprocess.run(['node', os.path.join(ROOT, 'tools/generate_patches.mjs'), '--chain', out, str(chain_seed), '12'],
                           check=True, capture_output=True)
            for round_ in range(1, 13):
                x = render(os.path.join(out, f'{round_}.vital'), 'D#2', wav)
                chain_peaks.append(20 * np.log10(np.abs(x).max() + 1e-12) if x is not None else -999)
        check(max(chain_peaks) <= -1.5 and min(chain_peaks) >= -18,
              f'twelve picks deep, five chains, the level stays between {min(chain_peaks):.1f} and '
              f'{max(chain_peaks):.1f} dBFS at D#2 (at most -1.5, at least -18)')
        for t, ds in closest.items():
            print(f'     closest variation to {t}: {min(ds):.2f} dB, median {np.median(ds):.2f}')
    print(f'\n{failures} failure(s)')
    sys.exit(1 if failures else 0)


main()
