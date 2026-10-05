#!/usr/bin/env python3
"""Fingerprint the kick and snare of riddim tracks, for fitting DRUMS' synth
to them (tools/fit_drums.mjs; docs/design/phase4-06-drums-riddimize.md).

    python3 tools/drum_prints.py track1.mp3 track2.mp3 ... --json /tmp/prints.json

A fingerprint is a hit's power in 20 log bands (30 Hz - 16 kHz) over 8 time
frames (0-10, 10-25, 25-50, 50-80, 80-120, 120-180, 180-260, 260-360 ms),
averaged over every kick (beat 1) and every snare (beat 3) of a track's
drops, with the 40 ms before each hit taken off band by band - what was
already sounding, the bass above all. The bar is lined up by
tools/measure_drums.py; a drop whose bar it could not line up (a kick on
beat 1 in under 60% of its bars) is left out.

The output is a measurement of copyrighted tracks, kept out of the
repository like the tracks themselves (CLAUDE.md section 7); what is
committed is fit_drums.mjs's answer, GNARL's own synth settings.
tests/test_drum_prints.py holds this fingerprint and fit_drums.mjs's to the
same numbers.
"""

import argparse
import json
import os
import re
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import measure  # noqa: E402
import measure_drums  # noqa: E402

# As fit_drums.mjs: these must stay the same in both.
EDGES = np.geomspace(30, 16000, 21)
TIMES = [0, 0.01, 0.025, 0.05, 0.08, 0.12, 0.18, 0.26, 0.36]
NFFT = 8192
MIN_KICK_ON_ONE = 0.6


def bands(seg, sr):
    w = np.hanning(len(seg))
    spec = np.abs(np.fft.rfft(seg * w, n=NFFT)) ** 2 / (w ** 2).sum()
    f = np.fft.rfftfreq(NFFT, 1 / sr)
    return np.array([spec[(f >= lo) & (f < hi)].sum() for lo, hi in zip(EDGES[:-1], EDGES[1:])])


def fingerprint(x, sr, ats, take_off=True):
    """Mean of each hit's band x frame power at sample offsets `ats`, less the 40 ms before it."""
    F = np.zeros((20, 8))
    n = 0
    pre_len = int(0.045 * sr) - int(0.005 * sr)
    for at in ats:
        if at - int(0.05 * sr) < 0 or at + int(0.4 * sr) > len(x):
            continue
        base = bands(x[at - int(0.045 * sr): at - int(0.005 * sr)], sr) if take_off else np.zeros(20)
        for k in range(8):
            seg = x[at + int(TIMES[k] * sr): at + int(TIMES[k + 1] * sr)]
            F[:, k] += np.maximum(bands(seg, sr) * (pre_len / len(seg)) - base, 0)
        n += 1
    return F / max(n, 1), n


def short(name):
    name = re.sub(r'\.(mp3|wav|flac)$', '', name, flags=re.I)
    name = re.sub(r'_?(free|freebie|free_download|direct_download|free_dl)\b.*$', '', name, flags=re.I)
    return re.sub(r'[_\s]+', ' ', name).strip()[:32]


def track(path):
    r = measure_drums.measure_track(path)
    x, sr = measure.read_audio(path)
    mid = x.mean(axis=1) if x.ndim > 1 else x
    bar = 4 * 60 / r['bpm']
    kick, snare, nk, ns = np.zeros((20, 8)), np.zeros((20, 8)), 0, 0
    for d in r['drops']:
        if d['patterns']['kick'][0] < MIN_KICK_ON_ONE:
            continue
        t0 = d['at_seconds']
        k, a = fingerprint(mid, sr, [int((t0 + b * bar) * sr) for b in range(d['bars'])])
        s, c = fingerprint(mid, sr, [int((t0 + (b + 0.5) * bar) * sr) for b in range(d['bars'])])
        kick, snare, nk, ns = kick + k * a, snare + s * c, nk + a, ns + c
    if not nk:
        return None
    return {'name': short(re.sub(r'^[0-9a-f]{8}-', '', os.path.basename(path))), 'bpm': r['bpm'],
            'hits': [nk, ns], 'kick': (kick / nk).tolist(), 'snare': (snare / ns).tolist()}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('tracks', nargs='+')
    ap.add_argument('--json', required=True)
    args = ap.parse_args()
    out, seen = [], set()
    for path in args.tracks:
        t = track(path)
        if t is None or t['name'] in seen:
            print(f'{os.path.basename(path)}: skipped (no bar lined up, or a copy)')
            continue
        seen.add(t['name'])
        out.append(t)
        print(f"{t['name']}: {t['hits'][0]} kicks, {t['hits'][1]} snares at {t['bpm']} bpm")
    with open(args.json, 'w') as f:
        json.dump({'tracks': out}, f)


if __name__ == '__main__':
    main()
