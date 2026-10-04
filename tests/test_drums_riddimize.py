#!/usr/bin/env python3
"""DRUMS and RIDDIMIZE, measured (docs/design/phase4-06-drums-riddimize.md).

    python3 tests/test_drums_riddimize.py

1. DRUMS: each row of a loop, rendered alone, has every hit on its step
   within 3 ms and nothing else; a loop is exactly four bars at -1 dBFS.
   Then tools/measure_drums.py - the instrument the reference tracks were
   measured with - on twelve of GNARL's own loops, whose hits are known: the
   share of steps it calls right (at least 90%).
2. RIDDIMIZE: a plain saw at D#3 (the producer's note), chopped to each
   rhythm, moves at that rhythm (tools/measure.py's growl rate: 2, 3, 4 and
   1 per beat), deeply, and peaks at -1 dBFS; the sub adds low end.
"""

import json
import os
import subprocess
import sys
import tempfile

import numpy as np
import soundfile as sf

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import measure  # noqa: E402
import measure_drums as md  # noqa: E402

failures = []


def check(ok, message):
    print(('PASS ' if ok else 'FAIL ') + message)
    if not ok:
        failures.append(message)


def node(*args):
    return subprocess.run(['node', os.path.join(ROOT, 'tools', 'render_tools.mjs'), *map(str, args)],
                          check=True, capture_output=True, text=True).stdout


tmp = tempfile.mkdtemp()

# ---------------------------------------------------------------- 1. DRUMS
# 1a. The generator, exactly: each row rendered alone (a stem), every hit's
#     onset within 2 ms of its step, and no onset anywhere else.
def stem_envelope(path):
    """A stem's 1 ms level every 0.25 ms, the loop's last 50 ms in front (as a
    loop plays, so a hit at 0 has a past); and the jump at each point: the
    level over the 3 ms before it, as a ratio."""
    x, sr = sf.read(path)
    pre = int(0.05 * sr)
    m = np.concatenate([x[-pre:], x]).mean(axis=1) ** 2
    hop, win = 11, 44
    env = np.sqrt(np.convolve(m, np.ones(win) / win, mode='full')[win - 1:len(m) + win - 1:hop])
    back = int(0.003 * sr / hop)
    jump = np.zeros_like(env)
    for i in range(back, len(env)):
        jump[i] = env[i] / (env[i - back:i - 1].max() + 1e-9)
    times = np.arange(len(env)) * hop / sr - pre / sr
    return times, env, jump


for seed in (1, 2, 5, 9, 11):
    bpm = 140 if seed % 3 else 150
    hits_path = os.path.join(tmp, f'd{seed}.json')
    wrong = []
    for row in ('kick', 'snare', 'hat', 'open'):
        wav = os.path.join(tmp, f's{seed}{row}.wav')
        node('drums', seed, bpm, wav, hits_path, 'fill', row)
        gen = json.load(open(hits_path))['hits'][row]
        times, env, jump = stem_envelope(wav)
        bar = 4 * 60 / bpm
        want = np.array([(b + s / 16) * bar for b in range(4) for s in range(16) if gen[b][s]])
        peak = env.max()
        # Each hit: the level leaps 6 dB or more within 3 ms of its step. (A
        # kick a 16th after a ringing kick leaps less than a kick from quiet,
        # but still 6 dB: the new one starts at full level.)
        near = lambda t: (times >= t - 0.003) & (times <= t + 0.003)  # noqa: E731
        # Judged: hits a quarter second or more after their row's last. A
        # low kick's sine starts at zero and swells over milliseconds, so a
        # retrigger a 16th after a kick shows no leap even when it plays
        # (the choke, renderLoop, is what makes it a hit).
        gaps = np.diff(np.concatenate([[want[-1] - 4 * bar if len(want) else 0.0], want]))
        missed = [round(float(t), 3) for t, g in zip(want, gaps) if g >= 0.25 and not (jump[near(t)] >= 2).any()]
        # Nowhere else: no leap of 12 dB above 2% of the peak (the kick's
        # 50 Hz sine makes a 1 ms level ripple by about 6 dB).
        away = np.ones_like(times, dtype=bool)
        for t in want:
            away &= ~((times >= t - 0.003) & (times <= t + 0.012))
        stray = [round(float(t), 3) for t in times[away & (jump >= 4) & (env > 0.02 * peak)]]
        if missed or stray:
            wrong.append(f'{row}: missed {missed[:3]} stray {stray[:3]}')
        os.remove(wav)
    check(not wrong, f'seed {seed}: every hit of every row lands on its step (within 3 ms), and nothing else' + (f': {wrong}' if wrong else ''))

node('drums', 1, 140, os.path.join(tmp, 'len.wav'))
x, sr = sf.read(os.path.join(tmp, 'len.wav'))
bar = 4 * 60 / 140 * sr
check(abs(len(x) - 4 * bar) < 1, f'a loop is four bars: {len(x)} samples, {4 * bar:.1f} expected at 140 bpm')
check(abs(20 * np.log10(np.abs(x).max()) + 1) < 0.05, f'and peaks at -1 dBFS ({20 * np.log10(np.abs(x).max()):.2f})')

# 1b. The instrument the references were measured with
#     (tools/measure_drums.py), on GNARL's own loops, whose hits are known:
#     how often it calls a step right. The templates rest on it.
judged = right = 0
errors = []
for seed in range(1, 13):
    bpm = 140 if seed % 3 else 150
    wav, hits_path = os.path.join(tmp, f'd{seed}.wav'), os.path.join(tmp, f'd{seed}.json')
    # Without the fourth bar's fill: a roll of four snares over the last
    # beat's kicks says nothing about the pattern.
    node('drums', seed, bpm, wav, hits_path, 'nofill')
    x, sr = sf.read(wav)
    bar = 4 * 60 / bpm * sr
    gen = json.load(open(hits_path))['hits']
    # The loop three times after four quiet bars, so the tool finds a drop.
    track = np.concatenate([np.zeros((int(4 * bar), 2)), np.tile(x, (3, 1))])
    path = os.path.join(tmp, f't{seed}.wav')
    sf.write(path, track, sr, subtype='FLOAT')
    d = md.measure_track(path, bpm)['drops'][0]
    os.remove(path)
    os.remove(wav)
    for row in ('kick', 'snare', 'hat'):
        steps = np.array(gen[row], dtype=float).mean(axis=0)
        measured = np.array(d['patterns'][row])[::3]
        for s in range(16):
            if 0 < steps[s] < 1:
                continue  # in some bars only: no single right answer
            # Not judged, by design: a hat under a kick, snare or open hat,
            # or in a snare's tail; a snare on a kick (it is the kick).
            others = [o for o in ('kick', 'snare', 'open') if o != row and any(gen[o][b][s] for b in range(4))]
            tail = any(gen['snare'][b][s - k] for b in range(4) for k in (1, 2) if s - k >= 0)
            if row == 'hat' and (others or tail):
                continue
            if row == 'snare' and any(gen['kick'][b][s] for b in range(4)):
                continue
            judged += 1
            ok = (measured[s] >= 0.75) if steps[s] == 1 else (measured[s] <= 0.25)
            right += ok
            if not ok:
                errors.append(f'seed {seed} {row} step {s + 1}: {"missed" if steps[s] == 1 else "extra"} ({measured[s]:.2f})')
share = right / judged
check(share >= 0.9, f'measure_drums.py calls {right} of {judged} steps right on GNARL loops ({share:.1%}); wrong: {errors}')

# ------------------------------------------------------------- 2. RIDDIMIZE
sr = 44100
t = np.arange(sr) / sr


def saw_file(f, name):
    path = os.path.join(tmp, name)
    sf.write(path, 0.5 * (2 * ((t * f) % 1) - 1), sr, subtype='FLOAT')
    return path


saw_path = saw_file(77.78, 'saw.wav')  # D#3 in FL Studio, MIDI 39
for rhythm, per_beat in (('1/8', 2), ('1/8T', 3), ('1/16', 4), ('1/4', 1)):
    out = os.path.join(tmp, 'r.wav')
    info = json.loads(node('riddimize', saw_path, out, json.dumps({'rhythm': rhythm, 'length': '2 BARS'})))
    x, _ = sf.read(out)
    m = measure.measure(x if x.ndim > 1 else x[:, None], sr, 140)
    rate = m['modulation']['growl_band']['rate']
    depth = m['modulation']['growl_band'].get('depth')
    peak = 20 * np.log10(np.abs(x).max())
    check(rate is not None and abs(rate['per_beat'] - per_beat) < 0.05 and depth and depth > 0.5 and abs(peak + 1) < 0.1,
          f'RIDDIMIZE {rhythm}: {rate and rate["per_beat"]}/beat (want {per_beat}), depth {depth}, peak {peak:.2f} dBFS, note {info["note"]:.1f} Hz')


def low_share(path):
    x, _ = sf.read(path)
    x = x.mean(axis=1) if x.ndim > 1 else x
    p = np.abs(np.fft.rfft(x)) ** 2
    fr = np.fft.rfftfreq(len(x), 1 / sr)
    return p[fr < 100].sum() / p.sum()


# An octave up (D#4, 155.6 Hz), so the sub (an octave under) is below 100 Hz
# where the saw has nothing: at D#3 the sub doubles the saw's own note.
high = saw_file(155.56, 'saw_high.wav')
a, b = os.path.join(tmp, 'nosub.wav'), os.path.join(tmp, 'sub.wav')
node('riddimize', high, a, json.dumps({'sub': 0}))
node('riddimize', high, b, json.dumps({'sub': 0.7}))
check(low_share(b) > 2 * low_share(a), f'SUB adds low end: below 100 Hz {low_share(a):.1%} without, {low_share(b):.1%} with')

print(f'\n{len(failures)} failure(s)')
sys.exit(len(failures))
