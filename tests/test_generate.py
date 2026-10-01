#!/usr/bin/env python3
"""The AI button's generator (ui/src/generate.ts, docs/design/phase4-01-generator.md).

Generates patches from fixed seeds, renders each with the desktop renderer,
and measures what the button makes against the references' wobs, as the
style presets were (phase2-12-presets.md): the growl band 150 Hz - 6 kHz,
F1 at 140 BPM, the median spectral centroid within 20 dB of the loudest.
The references' medians are 1874-2233 Hz.

  python3 tests/test_generate.py [--seeds N]      (needs librosa, Node 22.18+)
"""
import argparse, json, os, subprocess, sys, tempfile
import numpy as np, soundfile as sf, librosa
from scipy import signal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RENDER = os.path.join(ROOT, 'headless/builds/linux/build/gnarl-render')
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
    r = subprocess.run([RENDER, '--headless', '-o', wav, '-l', '1', '-m', note, '-b', str(BPM), '--bits', '32', patch],
                       capture_output=True)
    if r.returncode != 0:
        return None
    x, sr = sf.read(wav)
    return x, sr

def centroid(x, sr):
    m = x.mean(1)[:int(2.5 * 60 / BPM * sr)]
    g = signal.sosfiltfilt(signal.butter(4, [150, 6000], 'bandpass', fs=sr, output='sos'), m)
    lev = 20 * np.log10(librosa.feature.rms(y=g, frame_length=1024, hop_length=256)[0] + 1e-9)
    c = librosa.feature.spectral_centroid(y=g, sr=sr, n_fft=2048, hop_length=256)[0]
    loud = lev > lev.max() - 20
    return float(np.median(c[loud][4:]))  # past the onset's click

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--seeds', type=int, default=24)
    args = ap.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        made = generate(os.path.join(tmp, 'a'), 1, args.seeds)
        generate(os.path.join(tmp, 'b'), 1, args.seeds)
        same = all(open(os.path.join(tmp, 'a', f'{g["seed"]}.vital')).read() ==
                   open(os.path.join(tmp, 'b', f'{g["seed"]}.vital')).read() for g in made)
        check(same, f'a seed always makes the same patch ({args.seeds} seeds, twice)')
        check(len({g['name'] for g in made}) == len(made), 'every patch has its own name')

        peaks, cents, loaded, finite = [], [], 0, True
        for g in made:
            patch = os.path.join(tmp, 'a', f'{g["seed"]}.vital')
            loudest, ok = -999.0, True
            for note in ['D1', 'F1', 'F2']:
                got = render(patch, note, os.path.join(tmp, 'o.wav'))
                if got is None:
                    ok = False
                    break
                x, sr = got
                finite = finite and bool(np.isfinite(x).all())
                loudest = max(loudest, 20 * np.log10(np.abs(x).max() + 1e-12))
                if note == 'F1':
                    cents.append(centroid(x, sr))
            loaded += ok
            peaks.append(loudest)
            print(f'     {g["seed"]:3d} {g["name"]:22s} peak {loudest:6.1f} dBFS  centroid {cents[-1] if cents else 0:6.0f} Hz')
        C = np.array(cents)
        inside = float(((C >= 1600) & (C <= 2600)).mean())
        check(loaded == len(made), f'the renderer loads every generated patch ({loaded}/{len(made)})')
        check(finite, 'every render is finite')
        check(max(peaks) <= -1.5, f'the loudest, D1 to F2, peaks at {max(peaks):.1f} dBFS (at most -1.5)')
        check(1900 <= np.median(C) <= 2400, f'the median centroid is {np.median(C):.0f} Hz (the references: 1874-2233)')
        check(inside >= 0.8, f'{inside * 100:.0f}% centre at 1.6-2.6 kHz (at least 80%)')
    print(f'\n{failures} failure(s)')
    sys.exit(1 if failures else 0)

main()
