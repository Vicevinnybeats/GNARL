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

## Tried and rejected: Syntheon

[Syntheon](https://github.com/gudgud96/syntheon) (Apache-2.0) is the open
model that writes Vital presets from audio, so the obvious candidate. I ran
it from source, with three fixes for current librosa, numpy and torchcrepe.

Its Vital model infers ONE single-cycle wavetable and an ADSR: no LFO, no
filter, no movement. It also needs exactly 4 s of input, so each wob was
looped to fill it.

On the matcher's measure:

| Target | Syntheon | Ref Wob |
|---|---|---|
| drac07 #1 | 20.69 dB | 4.10 dB |
| meta 800 #1 | 24.49 dB | 3.14 dB |

The other two targets crashed it with a length mismatch inside the model.
A wob is its movement, which this model cannot express.

## OpenL3 as a judge (2026-10-02)

OpenL3 told the reference tracks' wobs apart best of four measures
(phase4-03-pick.md), so the producer asked for the matcher to use it.

The same instrument test as above: a random known patch is the target, the
search starts from scratch, and the result is measured on both measures.
The first row's random patch scores 1.48 when OpenL3 runs with a 0.1 s hop.

| Judge | Log-mel distance to the truth | OpenL3 distance |
|---|---|---|
| another random patch | 5.55 dB | 1.51 |
| log-mel alone (1,320 tries) | 3.38 dB | — |
| OpenL3 alone (360 tries, 0.1 s hop) | **7.88 dB** | 1.03 |
| log-mel, then the best 48 judged by both (+288 tries) | 4.09 dB | 1.00 |

**OpenL3 alone is fooled.** It found a sound it rated near the truth, but
that sound sat further from the truth on the spectrum than an unrelated
random patch. OpenL3 knows what *kind* of sound it is, not the detail.

**`--openl3` therefore adds a second stage.** The best 48 of the log-mel
search are judged again, and refined, by log-mel + 3.8 × OpenL3: each
distance scaled by its median over random patches (8.8 dB and 2.3). The
result is near on both measures.

OpenL3 runs with a 0.25 s hop: at 0.1 s it took 1.9 s per candidate on
four cores.

## Sig Wob 1–5: the signature wobs, matched with OpenL3

The five signature wobs of phase4-03-pick.md were matched with
`--openl3`. Each target at F1 (pyin: 43.9–44.3 Hz). Distances on both
measures, beside the closest earlier preset, judged by both together:

| Target | Sig Wob | Log-mel / OpenL3 | Closest earlier |
|---|---|---|---|
| 6:25:300 | 1 | **2.37 dB / 2.11** | 4.97 / 2.37 (Metal Grind) |
| cemeteryf0g | 2 | **4.08 / 1.82** | 5.42 / 2.15 (Swamp Gurgle) |
| drac07 | 3 | **2.93 / 2.04** | 3.47 / 2.62 (Ref Wob 4) |
| lily | 4 | **3.34 / 2.11** | 5.79 / 2.16 (Screech) |
| meta 800 | 5 | **2.78 / 1.59** | 4.42 / 2.55 (Yoi Talk) |

Each is closer on both measures; lily only just on OpenL3. All five are
one-shots. They lead the preset list after Vinny Bass 2 and are the AI
pick mode's first bases. Their volumes put the loudest of D1, F1, D#2 and
F2 at −3.0 dBFS. As bases, a pick chain reached −1.4 dBFS with 4.5 dB of
room, so variations now get 5 dB: chains stay at −11.5 to −1.9 dBFS.
