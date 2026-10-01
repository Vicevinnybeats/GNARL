# Phase 4 — the sound matcher

The producer, after trying the generated sounds: "very bad … not even close
to those sounds that i like" — phompy's, made in Reason. The generator had
been tuned on two numbers per wob (brightness and length), and two sounds
can share both and sound nothing alike. `tools/match.py` compares the whole
picture instead.

## What it compares

A log-mel spectrogram of the growl band:

- 100 Hz to 8 kHz, in 24 bands;
- power averaged over 80 ms;
- normalised to its own loudest cell and floored at −50 dB.

The distance between two of them is the mean absolute difference, in dB per
cell. Below 100 Hz is the sub, a separate voice in a riddim patch, which
would otherwise dominate.

**Sensitivity.** I nudged one setting of a known patch by 5% of its range
and measured the change:

| Setting nudged 5% | Distance |
|---|---|
| unison detune | 2.2 dB |
| WT start | 1.7 dB |
| drive | 1.3 dB |
| LFO peak | 0.2 dB |
| another table | 5.6 dB |
| another random patch | 5.6 dB |

At 10 ms resolution and 48 bands, detune alone cost 3.0 dB, about half a
different patch. The beating between unison voices moves notches across
the spectrum; averaging in power over 80 ms takes some of that out.

## How it searches

Each candidate is a set of genes: osc 1's table and where LFO 1 takes it, the
LFO's shape, rate and one-shot or loop, a warp, filter 1 (off, analog, dirty,
formant, comb) and its movement, drive, fold, OTT, a shelf, unison and the
amp envelope - about thirty settings. Each is rendered by the desktop renderer
at the target's note (F1: the references' wobs measured 43.9-49.3 Hz by pyin,
MIDI 29-31) and scored. A random round, equal per table, then generations of
mutation from the best two of each of the six best tables, so a table whose
first tries were unlucky is not dropped.

**Instrument test** (`scratchpad`, reproducible from `tools/match.py`): a
random known patch is the target and the search starts from scratch.

- The search found **3.38 dB**. The random round's median was 8.83 dB, and
  a different random patch sits 5.55 dB away.
- It does not recover the exact patch: the truth used the Yoi table, and
  it found Screech through the dirty filter. With about thirty settings and
  1,320 tries, it finds a near neighbour in sound, not the settings.

That is the limit to keep in mind for the results below. A matched patch
looks like its target to this measure; it is not a reconstruction of how
the target was made.

## Targets

The drop's bass stem (HPSS, as phase3-02-isolate.md), cut at onsets, keeping
wobs of 0.8-2.3 beats; per track the clearest by pyin voicing, then level. Six:
6:25:300 #1 (2.16 beats), cemeteryf0g #1 (1.74), drac07 #1 (1.82), lily #2
(1.03), meta 800 #1 (1.01) and #2 (2.15). The audio stays in the session's
scratch space; only distances and the patches leave it.

## Results

1,920 tries per target (480 random, 60 generations of 24). Distance in dB
per cell on the matcher's measure; lower is closer. The second and third
columns are every built-in preset and the first generator's 24 seeds,
rendered and measured the same way:

| Target | Matched | Closest built-in before | Vinny Bass 2 | Matched patch |
|---|---|---|---|---|
| 6:25:300 #1 | **3.41** | 5.44 (Croak Table) | 7.21 | Ref Wob 1: Screech table, sync warp, one shot over a bar |
| cemeteryf0g #1 | **3.50** | 6.28 (Frog Croak) | 10.13 | Ref Wob 2: Croak table, comb filter, looping 1/2 |
| drac07 #1 | **4.10** | 7.07 (Yoi Talk) | 8.64 | Ref Wob 3: Croak table, formant filter, looping 1/2 |
| lily #2 | **3.31** | 4.41 (Metal Grind) | 8.10 | Ref Wob 4: Pulse table, formant filter, looping 1/8T |
| meta 800 #1 | **3.14** | 4.16 (Metal Grind) | 9.59 | Ref Wob 5: saw, formant filter, looping 1/2 |
| meta 800 #2 | **4.04** | 6.77 (Croak Table) | 7.92 | Ref Wob 6: Comb table, one shot at 1/8T |

The first generator's seeds measured a median of 8.9–13.9 dB from these
targets. Each matched patch is closer than anything GNARL had before.
They are built-in presets now: Ref Wob 1–6 (`presets/source/matched/`,
volumes set in `tools/build_presets.mjs` so the loudest of D1, F1, D#2 and
F2 peaks at −3 dBFS). Matched loudness varied widely, from −14 to −2 dBFS
at the same volume.

What the numbers do not say is whether they sound like the references to
the producer: 3–4 dB is near, on a measure where another patch of the same
family sits 5–6 dB away, but it is not the same sound.

## The generator on these

The AI button now makes variations of Ref Wob 1–6 (phase4-01-generator.md).
`tests/test_generate.py`, 24 seeds:

- each variation sits a median **1.79 dB** from its base (at most 4.55);
- the loudest peaks at −2.0 dBFS;
- against the six targets, the closest variation is 3.3–4.7 dB and the
  median 5.1–10.6 dB. The first generator's median was 8.9–13.9 dB.

With every variation forced to swap its table, the "stays near its base"
check fails (median 3.33 dB).
