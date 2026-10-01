#!/usr/bin/env python3
"""The sound matcher (docs/design/phase4-02-matcher.md).

Searches GNARL settings for the patch whose render looks most like a target
wob: osc 1 through one of GNARL's tables (ui/src/wavetables.ts) moved by
LFO 1, a warp, filter 1, drive, fold, OTT and a shelf. Each candidate is
rendered with the desktop renderer and compared with the target on the
whole picture over time - a log-mel spectrogram of the growl band (100 Hz
to 8 kHz), each normalised to its own loudest cell, 50 dB deep - not on two
summary numbers.

The target audio stays where it is given; nothing of it is written out but
the distance. What comes out is a patch: settings, found by search.

  python3 tools/match.py target.wav --midi 29 --out patch.vital [--tables DIR]
          [--random 360] [--generations 40] [--children 24] [--seed 1]

--tables is a directory of the tables' JSON (written by
tools/generate_patches.mjs --tables DIR). Needs librosa.
"""
import argparse, json, math, os, random, subprocess, sys, tempfile
from multiprocessing import Pool

import numpy as np
import soundfile as sf
import librosa
from scipy.ndimage import uniform_filter1d

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RENDER = os.path.join(ROOT, 'headless/builds/linux/build/gnarl-render')
BASE = os.path.join(ROOT, 'presets', 'Yoi Talk.vital')
BPM = 140
SR = 44100
# The growl band, as every wob measurement here: the sub under 100 Hz is a
# separate voice (mono_sub, Riddim Sub) and would dominate any distance.
FMIN, FMAX, MELS = 100, 8000, 24
HOP = 441  # 10 ms
SMOOTH = 8  # frames: 80 ms
FLOOR_DB = -50.0

# The search space. Continuous genes are 0..1; choice genes are indices.
CHOICES = {
    'table': None,  # filled from the tables directory
    'rate': [(1, 7), (1, 8), (1, 9), (3, 9), (1, 6)],  # 1/2, 1/4, 1/8, 1/8T, 1/1
    'loop': [0, 1],
    'double': [0, 1],
    'warp': [0, 1, 2, 4, 5],  # none, sync, formant, bend, squeeze
    'filter': ['off', 'analog', 'dirty', 'formant', 'comb'],
}
CONTINUOUS = ['wt_start', 'wt_depth', 'peak', 'peak2', 'dip', 'lfo_start', 'curve', 'warp_amount',
              'warp_depth', 'cutoff', 'resonance', 'formant_x', 'formant_y', 'filter_depth', 'drive',
              'fold', 'ott', 'shelf', 'unison', 'detune', 'decay', 'sustain']


def features(x, length):
    """Log-mel of the first `length` samples, power averaged over 80 ms,
    normalised to the loudest cell and floored. The averaging is in power, not
    in dB: unison voices beating a few times a second cut notches that a
    shorter window reads as a different sound (tools/match.py's sensitivity
    test in phase4-02-matcher.md)."""
    x = np.asarray(x[:length], dtype=np.float64)
    if len(x) < length:
        x = np.pad(x, (0, length - len(x)))
    m = librosa.feature.melspectrogram(y=x, sr=SR, n_fft=2048, hop_length=HOP, n_mels=MELS, fmin=FMIN, fmax=FMAX,
                                       power=2.0)
    m = np.maximum(uniform_filter1d(m, SMOOTH, axis=1), 0)
    db = 10 * np.log10(m + 1e-20)
    db -= db.max()
    return np.maximum(db, FLOOR_DB)


def distance(a, b):
    """Mean absolute difference, dB per mel cell."""
    return float(np.mean(np.abs(a - b)))


def random_genes(rng):
    g = {k: rng.random() for k in CONTINUOUS}
    for k, options in CHOICES.items():
        g[k] = rng.randrange(len(options))
    return g


def mutate(g, rng, sigma):
    child = dict(g)
    for k in CONTINUOUS:
        if rng.random() < 0.35:
            child[k] = min(1.0, max(0.0, g[k] + rng.gauss(0, sigma)))
    for k, options in CHOICES.items():
        if rng.random() < (0.03 if k == 'table' else 0.12):
            child[k] = rng.randrange(len(options))
    return child


def lerp(a, b, t):
    return a + (b - a) * t


def route(settings, slot, source, destination, amount):
    settings['modulations'][slot - 1] = {'source': source, 'destination': destination}
    settings[f'modulation_{slot}_amount'] = amount
    settings[f'modulation_{slot}_bipolar'] = 0.0
    settings[f'modulation_{slot}_bypass'] = 0.0
    settings[f'modulation_{slot}_power'] = 0.0
    settings[f'modulation_{slot}_stereo'] = 0.0


def build(g, base, tables, name='Matched'):
    """Genes -> .vital patch dict."""
    patch = json.loads(json.dumps(base))
    s = patch['settings']
    for i in range(len(s['modulations'])):
        s['modulations'][i] = {'source': '', 'destination': ''}
        s[f'modulation_{i + 1}_amount'] = 0.0
    table = CHOICES['table'][g['table']]
    s['wavetables'][0] = tables[table]

    sync, tempo = CHOICES['rate'][g['rate']]
    loop = CHOICES['loop'][g['loop']]
    s.update(lfo_1_sync=sync, lfo_1_tempo=tempo, lfo_1_sync_type=0 if loop else 2)
    # LFO 1 over one wob, Vital's points: x, then y with 0 at the TOP.
    peak = lerp(0.05, 0.95, g['peak'])
    start_y = lerp(0.0, 1.0, g['lfo_start'])
    if CHOICES['double'][g['double']]:
        p1 = lerp(0.05, 0.45, g['peak'])
        p2 = lerp(0.55, 0.95, g['peak2'])
        points = [[0, start_y], [p1, 0], [(p1 + p2) / 2, lerp(0.3, 1, g['dip'])], [p2, 0], [1, 1]]
    else:
        points = [[0, start_y], [peak, 0], [1, 1]]
    curve = lerp(-4, 4, g['curve'])
    s['lfos'][0] = {'name': 'Matched', 'num_points': len(points), 'points': [v for p in points for v in p],
                    'powers': [curve] * len(points), 'smooth': False}

    s.update(
        osc_1_wave_frame=lerp(0, 256, g['wt_start']),
        osc_1_unison_voices=round(lerp(1, 6, g['unison'])), osc_1_unison_detune=lerp(0, 2.5, g['detune']),
        osc_1_stereo_spread=0.5, osc_1_random_phase=0,
        osc_1_distortion_type=CHOICES['warp'][g['warp']], osc_1_distortion_amount=g['warp_amount'],
        env_1_attack=0, env_1_hold=0, env_1_decay=lerp(0.3, 1.6, g['decay']), env_1_sustain=g['sustain'],
        env_1_release=0.2,
        distortion_on=1, distortion_type=0, distortion_drive=lerp(0, 30, g['drive']), distortion_mix=1,
        distortion_fold_on=1 if g['fold'] > 0.15 else 0, distortion_fold_drive=lerp(0, 12, g['fold']),
        distortion_crush_on=0,
        compressor_on=1, compressor_mix=g['ott'],
        eq_on=1, eq_low_gain=0, eq_band_gain=0, eq_high_gain=lerp(-6, 10, g['shelf']), eq_high_cutoff=88,
        flanger_on=0, phaser_on=0, chorus_on=0, reverb_on=0, delay_on=0, filter_2_on=0,
        volume=4300,
    )
    route(s, 1, 'lfo_1', 'osc_1_wave_frame', lerp(-1, 1, g['wt_depth']))
    if s['osc_1_distortion_type']:
        route(s, 2, 'lfo_1', 'osc_1_distortion_amount', lerp(-1, 1, g['warp_depth']))

    kind = CHOICES['filter'][g['filter']]
    s['filter_1_on'] = 0 if kind == 'off' else 1
    if kind in ('analog', 'dirty', 'comb'):
        s.update(filter_1_model={'analog': 0, 'dirty': 1, 'comb': 6}[kind], filter_1_style=0,
                 filter_1_cutoff=lerp(30, 130, g['cutoff']), filter_1_resonance=lerp(0, 0.9, g['resonance']),
                 filter_1_drive=0, filter_1_mix=1)
        route(s, 3, 'lfo_1', 'filter_1_cutoff', lerp(-1, 1, g['filter_depth']))
    elif kind == 'formant':
        s.update(filter_1_model=5, filter_1_style=0, filter_1_formant_x=g['formant_x'],
                 filter_1_formant_y=g['formant_y'], filter_1_formant_resonance=lerp(0.3, 1, g['resonance']),
                 filter_1_mix=1)
        route(s, 3, 'lfo_1', 'filter_1_formant_x', lerp(-1, 1, g['filter_depth']))
    patch['preset_name'] = name
    patch['author'] = 'GNARL matcher'
    return patch


def render_features(args):
    """Worker: render genes at the target's note, return (distance, peak)."""
    g, base, tables, target, length, midi, workdir = args
    fd, path = tempfile.mkstemp(suffix='.vital', dir=workdir)
    os.close(fd)
    wav = path[:-6] + '.wav'
    try:
        with open(path, 'w') as f:
            json.dump(build(g, base, tables), f)
        r = subprocess.run([RENDER, '--headless', '-o', wav, '-l', '1', '-m', midi_name(midi), '-b', str(BPM),
                            '--bits', '32', path], capture_output=True)
        if r.returncode != 0:
            return math.inf, 0.0
        x, sr = sf.read(wav)
        if sr != SR or not np.isfinite(x).all():
            return math.inf, 0.0
        mono = x.mean(1)
        if np.abs(mono).max() < 1e-4:
            return math.inf, 0.0
        return distance(features(mono, length), target), float(np.abs(x).max())
    finally:
        for p in (path, wav):
            if os.path.exists(p):
                os.remove(p)


def midi_name(midi):
    names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
    return f'{names[midi % 12]}{midi // 12 - 1}'  # the renderer's naming: MIDI 60 = C4


def load_tables(directory):
    tables = {}
    for f in sorted(os.listdir(directory)):
        if f.endswith('.json'):
            tables[f[:-5]] = json.load(open(os.path.join(directory, f)))
    CHOICES['table'] = sorted(tables)
    return tables


def search(target_audio, midi, tables, base, n_random=360, generations=40, children=24, seed=1, log=print):
    length = min(len(target_audio), int(1.25 * SR))
    target = features(target_audio, length)
    rng = random.Random(seed)
    pool = Pool(os.cpu_count() or 2)
    with tempfile.TemporaryDirectory() as workdir:
        def score(genes):
            return pool.map(render_features, [(g, base, tables, target, length, midi, workdir) for g in genes])

        # The table is the largest single choice: every table gets an equal
        # share of the random round, so none is missed by chance.
        population = [random_genes(rng) for _ in range(n_random)]
        for i, g in enumerate(population):
            g['table'] = i % len(CHOICES['table'])
        scored = sorted(zip([d for d, _ in score(population)], range(n_random), population), key=lambda t: t[0])
        log(f'random {n_random}: best {scored[0][0]:.2f} dB, median {scored[len(scored) // 2][0]:.2f} dB')
        # Parents: the best two of each of the six best tables, so a table
        # whose first random tries were unlucky is not dropped for good.
        everything = [(d, g) for d, _, g in scored]

        def parents():
            best = {}
            for d, g in sorted(everything, key=lambda t: t[0]):
                best.setdefault(g['table'], [])
                if len(best[g['table']]) < 2:
                    best[g['table']].append((d, g))
            tables_ranked = sorted(best.values(), key=lambda v: v[0][0])[:6]
            return [p for v in tables_ranked for p in v]

        for gen in range(generations):
            sigma = 0.25 * (1 - gen / max(1, generations)) + 0.03
            pool_parents = parents()
            kids = [mutate(pool_parents[rng.randrange(len(pool_parents))][1], rng, sigma) for _ in range(children)]
            results = score(kids)
            everything.extend((d, g) for (d, _), g in zip(results, kids))
            if gen % 5 == 4 or gen == generations - 1:
                log(f'generation {gen + 1}: best {min(everything, key=lambda t: t[0])[0]:.2f} dB')
        elite = sorted(everything, key=lambda t: t[0])[:8]
    pool.close()
    return elite


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('target')
    ap.add_argument('--midi', type=int, required=True, help="the target's note, MIDI (F1 = 29)")
    ap.add_argument('--out', required=True)
    ap.add_argument('--tables', required=True)
    ap.add_argument('--name', default='Matched')
    ap.add_argument('--random', type=int, default=360)
    ap.add_argument('--generations', type=int, default=40)
    ap.add_argument('--children', type=int, default=24)
    ap.add_argument('--seed', type=int, default=1)
    args = ap.parse_args()
    x, sr = sf.read(args.target)
    if x.ndim > 1:
        x = x.mean(1)
    if sr != SR:
        x = librosa.resample(x, orig_sr=sr, target_sr=SR)
    tables = load_tables(args.tables)
    base = json.load(open(BASE))
    elite = search(x, args.midi, tables, base, args.random, args.generations, args.children, args.seed)
    best_d, best = elite[0]
    with open(args.out, 'w') as f:
        json.dump(build(best, base, tables, args.name), f)
    json.dump({'distance_db': best_d, 'genes': best,
               'table': CHOICES['table'][best['table']], 'filter': CHOICES['filter'][best['filter']]},
              open(args.out + '.json', 'w'), indent=1)
    print(f'best {best_d:.2f} dB -> {args.out}')


if __name__ == '__main__':
    sys.exit(main())
