#!/usr/bin/env python3
"""DRUMS' kits, held to how they were fitted (docs/design/phase4-06-drums-riddimize.md).

    python3 tests/test_drum_prints.py

1. tools/drum_prints.py (which fingerprints the tracks) and tools/
   fit_drums.mjs (which fingerprints GNARL's drums while fitting) compute the
   same fingerprint of the same sound - or a fit would chase a different
   measure than the tracks were taken with.
2. Every kit in ui/src/drums/kits.json still sounds as it did when fitted:
   its kick and snare, rendered by today's drums.ts, fingerprint the same as
   then (the stored fingerprint of the fitted sound, not of any track). A
   change to the synth that moves a fitted kit fails here, and the kits are
   fitted again.
"""

import json
import os
import subprocess
import sys

import numpy as np

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import drum_prints  # noqa: E402

failures = []


def check(ok, message):
    print(('PASS ' if ok else 'FAIL ') + message)
    if not ok:
        failures.append(message)


SCRIPT = r'''
import { kickVoice, layeredSnare, random, KITS } from '%s/ui/src/drums/drums.ts';
import { fingerprint } from '%s/tools/fit_drums.mjs';
const kick = { pitchStart: 160, pitchEnd: 48, sweepMs: 35, decayMs: 240, click: 0.35, drive: 0.5 };
const snare = { tone: 210, noiseHz: 3200, decayMs: 150, body: 0.5, drive: 0.4, noiseQ: 1, crack: 1, kickLayer: 0.5 };
const k = kickVoice(kick, 44100, random(7));
const s = layeredSnare(snare, kick, 44100, random(7));
const kits = KITS.map((kit) => ({ name: kit.name,
  kick: fingerprint(kickVoice(kit.kick, 44100, random(7))),
  snare: fingerprint(layeredSnare(kit.snare, kit.kick, 44100, random(7))),
  stored: kit.prints ?? null }));
console.log(JSON.stringify({ k: Array.from(k), kF: fingerprint(k), s: Array.from(s), sF: fingerprint(s), kits }));
''' % (ROOT, ROOT)

out = json.loads(subprocess.run(['node', '--input-type=module', '-e', SCRIPT], check=True,
                                capture_output=True, text=True, cwd=ROOT).stdout)


def db_apart(a, b):
    a, b = np.array(a), np.array(b)
    seen = (a > a.max() * 1e-5) | (b > b.max() * 1e-5)
    return float(np.max(np.abs(10 * np.log10((a[seen] + 1e-30) / (b[seen] + 1e-30)))))


# 1. One sound, both fingerprints. The Python one takes a hit inside a longer
#    signal; nothing before it, so nothing is taken off.
for name, x, js in (('kick', out['k'], out['kF']), ('layered snare', out['s'], out['sF'])):
    pad = 4410
    signal = np.concatenate([np.zeros(pad), np.array(x), np.zeros(20000)])
    F, n = drum_prints.fingerprint(signal, 44100, [pad])
    apart = db_apart(F, js)
    check(n == 1 and apart < 1e-6, f'drum_prints.py and fit_drums.mjs fingerprint a {name} the same (largest cell {apart:.2e} dB apart)')

# 2. The kits as fitted.
kits = json.load(open(os.path.join(ROOT, 'ui', 'src', 'drums', 'kits.json')))['kits']
check(len(out['kits']) == len(kits) >= 10, f'{len(out["kits"])} kits fitted to the producer\'s tracks')
for kit in out['kits']:
    stored = kit['stored']
    if stored is None:
        check(False, f'{kit["name"]}: kits.json lacks the fitted sound\'s fingerprint (fit again)')
        continue
    worst = max(db_apart(kit['kick'], stored['kick']), db_apart(kit['snare'], stored['snare']))
    check(worst < 0.01, f'{kit["name"]}: kick and snare sound as fitted (largest cell {worst:.3f} dB apart)')

print(f'\n{len(failures)} failure(s)')
sys.exit(len(failures))
