#!/usr/bin/env python3
"""tools/isolate.py: measuring a wobble UNDER DRUMS (docs/design/phase3-02-isolate.md).

    python3 tests/test_isolate.py      # needs librosa; skips without it

A synthetic growl - a saw at 87 Hz whose brightness and level open and
close three times a beat (1/8T at 140 bpm) - under a synthetic riddim kit:
a kick every beat, a snare on 2 and 4, hats on every 1/16, at four times
the growl's level. No recording is used, so the right answer is known.

1. The problem is real: measured as it is, the growl band does NOT read 3
   per beat (the drums win). If this ever passes, the test kit is too quiet
   to prove anything.
2. --isolate hpss reads 3 per beat, within 0.05.
3. --bpm auto reads 140 within 1 bpm.
"""
import os, sys
import numpy as np
from scipy import signal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'tools'))
try:
    import librosa  # noqa: F401
except ImportError:
    print('SKIP: librosa is not installed (pip install librosa)')
    sys.exit(0)
import isolate  # noqa: E402
import measure  # noqa: E402

SR = 44100
BPM = 140
BEAT = 60 / BPM
SECONDS = 6
FAILS = []


def check(ok, msg):
    print(('PASS ' if ok else 'FAIL ') + msg)
    if not ok:
        FAILS.append(msg)


def growl():
    t = np.arange(int(SECONDS * SR)) / SR
    saw = 2 * ((87.3 * t) % 1) - 1
    lfo = 0.5 - 0.5 * np.cos(2 * np.pi * 3 / BEAT * t)  # 3 per beat
    # A swept low-pass, block by block: brightness and level both move.
    out = np.zeros_like(saw)
    zi = None
    block = 256
    for i in range(0, len(t), block):
        cutoff = 150 + 3000 * lfo[i]
        sos = signal.butter(2, cutoff, fs=SR, output='sos')
        if zi is None:
            zi = signal.sosfilt_zi(sos) * 0
        out[i:i + block], zi = signal.sosfilt(sos, saw[i:i + block], zi=zi)
    return out * (0.3 + 0.7 * lfo)


def drums(level):
    rng = np.random.default_rng(1)
    n = int(SECONDS * SR)
    d = np.zeros(n)

    def put(sig, at):
        i = int(at * SR)
        m = min(len(sig), n - i)
        if m > 0:
            d[i:i + m] += sig[:m]
    kt = np.arange(int(0.3 * SR)) / SR
    kick = np.sin(2 * np.pi * np.cumsum(45 + 90 * np.exp(-kt * 30)) / SR) * np.exp(-kt * 9)
    sn = signal.sosfilt(signal.butter(2, [200, 5000], 'bandpass', fs=SR, output='sos'),
                        rng.standard_normal(int(0.18 * SR))) * np.exp(-np.arange(int(0.18 * SR)) / SR * 22)
    hh = signal.sosfilt(signal.butter(2, 7000, 'highpass', fs=SR, output='sos'),
                        rng.standard_normal(int(0.04 * SR))) * np.exp(-np.arange(int(0.04 * SR)) / SR * 90)
    b = 0
    while b * BEAT < SECONDS:
        put(kick * level, b * BEAT)
        if b % 2 == 1:
            put(sn * level, b * BEAT)
        for s in range(4):
            put(hh * 0.25, b * BEAT + s * BEAT / 4)
        b += 1
    return d


g = growl()
d = drums(4 * np.sqrt(np.mean(g ** 2)) / 0.22)  # four times the growl's level
mix = g + d
mix = np.stack([mix, mix], axis=1) * (0.9 / np.abs(mix).max())
skip = int(0.5 * SR)


def growl_rate(x):
    r = measure.modulation(x[skip:].mean(axis=1), SR, BPM)['growl_band']['rate']
    return r and r.get('per_beat')


plain = growl_rate(mix)
check(plain is None or abs(plain - 3) > 0.05,
      f'the drums win without isolation: the growl band reads {plain}/beat (the test is meaningful)')
alone = growl_rate(np.stack([g, g], axis=1))
check(alone is not None and abs(alone - 3) < 0.05, f'the growl alone reads {alone}/beat')
iso = growl_rate(isolate.hpss(mix, SR))
check(iso is not None and abs(iso - 3) < 0.05, f'--isolate hpss reads {iso}/beat under the drums')
bpm = isolate.auto_bpm(mix.mean(axis=1), SR)
check(abs(bpm - BPM) < 1, f'--bpm auto reads {bpm:.2f} for {BPM}')

print(f'\n{len(FAILS)} failure(s)')
sys.exit(1 if FAILS else 0)
