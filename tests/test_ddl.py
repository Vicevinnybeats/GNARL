#!/usr/bin/env python3
"""The delay's STEPS (delay_steps), run against the real engine through gnarl-render.

    python3 tests/test_ddl.py [path/to/gnarl-render]

docs/design/phase2-11-ddl.md. A short click of a note goes through the delay
with the mix at 100% (only the echo) and no feedback, so the first sound in
the render IS the echo, and its onset is the delay time. Checked:

1. STEPS x STEP LENGTH: 1, 3 and 16 steps of 1/16, and 3 of 1/8T, at 140
   bpm, land within 1 ms of steps x note value.
2. MS (free time) ignores STEPS: 250 ms with 5 steps is still 250 ms.
3. A patch saved before delay_steps existed (no key) renders bit-identically
   to delay_steps = 1: every existing preset keeps its delay.
4. The echo lands at the same time rendered in blocks of 32 and 512.
"""
import json, os, subprocess, sys, tempfile
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'tools'))
import wavio  # noqa: E402

RENDER = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'headless/builds/linux/build/gnarl-render')
TMP = tempfile.mkdtemp(prefix='gnarl-ddl-')
FAILS = []
BPM = 140
BEAT = 60 / BPM
# Vital's tempo indices (kSyncedFrequencyNames) and sync modes (TempoChooser).
TEMPO_1_8, TEMPO_1_16 = 9, 10
SYNC_FREE, SYNC_TEMPO, SYNC_TRIPLET = 0, 1, 3


def check(ok, msg):
    print(('PASS ' if ok else 'FAIL ') + msg)
    if not ok:
        FAILS.append(msg)


def init_preset():
    path = os.path.join(TMP, 'init.vital')
    subprocess.run([RENDER, '--headless', '--save', path, '-o', os.path.join(TMP, 'x.wav'), '-l', '0.1'],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return json.load(open(path))


def patch(base, name, drop=(), **overrides):
    d = json.loads(json.dumps(base))
    s = d['settings']
    for k in drop:
        s.pop(k)
    for k, v in overrides.items():
        if k not in s:
            raise KeyError(f'unknown parameter {k}')
        s[k] = float(v)
    path = os.path.join(TMP, name + '.vital')
    json.dump(d, open(path, 'w'))
    return path


def render(path, block=64):
    out = path + f'.{block}.wav'
    subprocess.run([RENDER, '--headless', '-o', out, '-l', '2', '-m', 'C3', '-b', str(BPM),
                    '--bits', '32', '--block', str(block), path],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    x, sr = wavio.read(out)
    return x, sr


def onset(x, sr):
    """Seconds to the first sample within 40 dB of the render's peak."""
    m = np.abs(x).max(axis=1) if x.ndim > 1 else np.abs(x)
    return int(np.argmax(m > m.max() * 0.01)) / sr


base = init_preset()
# A click: instant attack, 20 ms to silence. Only the echo comes out.
CLICK = dict(env_1_attack=0, env_1_decay=0.3, env_1_sustain=0, env_1_release=0.3,
             delay_on=1, delay_dry_wet=1, delay_feedback=0)

dry, sr = render(patch(base, 'dry', **{**CLICK, 'delay_on': 0}))
t0 = onset(dry, sr)
check(t0 < 0.002, f'the dry click starts at {t0 * 1000:.2f} ms')

cases = [
    ('1 x 1/16', dict(delay_sync=SYNC_TEMPO, delay_tempo=TEMPO_1_16, delay_steps=1), BEAT / 4),
    ('3 x 1/16', dict(delay_sync=SYNC_TEMPO, delay_tempo=TEMPO_1_16, delay_steps=3), 3 * BEAT / 4),
    ('16 x 1/16 (one bar)', dict(delay_sync=SYNC_TEMPO, delay_tempo=TEMPO_1_16, delay_steps=16), 4 * BEAT),
    ('3 x 1/8T', dict(delay_sync=SYNC_TRIPLET, delay_tempo=TEMPO_1_8, delay_steps=3), 3 * BEAT / 3),
    # Free time: delay_frequency 2 is 2^2 Hz, 250 ms. STEPS does not apply.
    ('MS 250 with 5 steps', dict(delay_sync=SYNC_FREE, delay_frequency=2, delay_steps=5), 0.25),
]
for name, settings, expected in cases:
    x, sr = render(patch(base, name.replace(' ', '_').replace('/', '-'), **CLICK, **settings))
    got = onset(x, sr) - t0
    check(abs(got - expected) < 0.001,
          f'{name}: echo at {got * 1000:.1f} ms, expected {expected * 1000:.1f} ms')

# 3. An old patch (no delay_steps key) = delay_steps 1, sample for sample.
settings = dict(delay_sync=SYNC_TEMPO, delay_tempo=TEMPO_1_16, delay_feedback=0.5)
old, _ = render(patch(base, 'old', drop=('delay_steps',), **{**CLICK, **settings}))
one, _ = render(patch(base, 'one', **{**CLICK, **settings, 'delay_steps': 1}))
check(np.array_equal(old, one), f'a patch without delay_steps renders bit-identically to 1 step '
      f'(max difference {np.abs(old - one).max():.3g})')

# 4. Block size.
path = patch(base, 'blocks', **CLICK, delay_sync=SYNC_TEMPO, delay_tempo=TEMPO_1_16, delay_steps=5)
a, _ = render(path, 32)
b, _ = render(path, 512)
check(abs(onset(a, sr) - onset(b, sr)) < 0.001,
      f'5 x 1/16 lands at {onset(a, sr) * 1000:.1f} ms in blocks of 32 and {onset(b, sr) * 1000:.1f} ms in 512')

print(f'\n{len(FAILS)} failure(s)')
sys.exit(1 if FAILS else 0)
