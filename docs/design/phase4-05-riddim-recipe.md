# Phase 4-05: the AI makes wobs — every sound inside Vinny Bass 2's recipe

## What the producer heard

"Every sound is sounding bad, like all screech sounds instead of a wob …
Vinny Bass 2 is a good example, this is the only one, and the sub bass
too." (2026-10-02)

## What the numbers say

Every preset rendered at the producer's note (D#3 in FL, MIDI 39) for 16
beats at 140 BPM, and measured with `tools/measure.py` — the same
instrument the reference drops were measured with:

| Patch | Growl movement (depth) | Rate | Energy above 5 kHz |
|---|---|---|---|
| **Vinny Bass 2** | **0.72** | 2 per beat | 11% |
| Triplet Riddim | 0.40 | 3 per beat | 9% |
| Ref Wob 5 | 0.07 | 0.9 per beat | 20% |
| Sig Wob 1, Sig Wob 3, Ref Wob 3 | none found | — | 1–29% |

The level swing over a held note tells the same story: Vinny Bass 2's
level moves 24.9 dB; Ref Wob 3 moves 0.7 dB, Sig Wob 1 1.7, Ref Wob 5 1.9.

**The matcher's patches do not wob.** Each was built as *one wob* (an LFO
that runs once), matched on a 1.25-second picture of a single wob's tone.
Held or repeated in a track, that is one sweep and then a steady tone: a
screech. A good match score said nothing about rhythm, because the measure
never asked whether the sound moves on the beat. Every sound the AI made
was a variation of those patches, so every one inherited it.

## What changed

**The AI works inside Vinny Bass 2's recipe** (`ui/src/generate.ts`,
`riddimVary`). Vinny Bass 2 is built as a riddim engine:

- LFO 1 gates the oscillator's level (amount 1.0) and opens the comb
  filter — the rhythm, at 1/8;
- LFO 2 sweeps a spectral low pass (morph type 7) — the wah;
- LFO 4 bends LFO 1's speed within the bar;
- LFO 3 pushes the down-sample drive; OTT on.

A variation keeps all four routes and moves only:

| What | Range |
|---|---|
| Rhythm (LFO 1) | 1/8, 1/8T, 1/4, 1/16, 1/4T (weighted to 1/8 and 1/8T); every first round of four has one 1/4 wob, the producer's ask |
| LFO 4's bend | its amount × 0.4–1.6 |
| Sweep | LFO 2's depth × 0.5–1.7; its speed 1/2, 1/4, 1/8, sometimes triplet |
| Tone | one of the table's full-level slices (frames 60, 110, 160, 200), or 20%: a warm GNARL table (Growl, Vowel, Wub, Yoi, Hollow, Harmonic — never Screech, Metal, Tear, Sync, PD, FM) |
| Filter | comb pitch ±10, ring 0.25–0.65; 15%: the formant filter (a talking wob) |
| Drive | ±3, never above 9; LFO 3's push × 0.5–1.5 |

No reverb, phaser or flanger is added any more.

**Vinny Bass 2's table is an audio file cut into slices, not a morph.**
Frames 80–108 all play one slice 9 dB quieter than the rest (measured
frame by frame: 110–130 −1.3 dBFS, 80–108 −9.5). A first version nudged
the frame and pinned half of forty variations in that slice, 12–17 dB
down. The tone now picks a slice.

**The starting-sound list is the two good ones**: Vinny Bass 2 and Riddim
Sub. Sig Wob 1–5, Ref Wob 1–6 and the earlier starting sounds stay in
`presets/` as a record.

## Measured

Forty variations (seeds 1–40), each rendered and measured as above:

| | Vinny Bass 2 | Forty variations |
|---|---|---|
| Growl movement | 0.72 | 0.52–0.81, median 0.72 |
| Rate (per beat) | 2 | 0.5, 1, 2, 3.2 (1/8T, bent by LFO 4), 4 |
| Energy above 5 kHz | 11% | 4–13%, median 7% |
| Peak at D#3 FL | −0.4 dBFS (its own volume) | −11.6 to −2.0 dBFS, median −6.8 (5 dB of room) |

Every one wobs; none is harsher than the patch it came from by more than
two points above 5 kHz. `tests/test_generate.py` now requires it of every
seed: movement at least 0.4, a riddim rate, highs no more than Vinny Bass
2's plus 4 points. With the old bases put back (the Sig and Ref Wobs), it
fails.

Nobody here has heard them. These numbers say each one moves like Vinny
Bass 2 and is no brighter; whether each is *good* is the producer's call.

## The 1/4 wob

The producer asked for wobs in 1/4 too. LFO 1 at 1/4 with Vinny Bass 2's
bend (LFO 4 on LFO 1's speed) measured 1.25 a beat, off the grid - the bend
that keeps 1/8 on the grid pulls 1/4 toward 1/8. A 1/4 wob turns the bend
off: eight measured 1.0 a beat (one 0.5, its slower sweep the series'
fundamental), movement 0.69-0.90. `tests/test_generate.py` checks four.

## How the producer can help

- **More patches like Vinny Bass 2.** Each patch the producer made and
  likes becomes another recipe the AI can work inside. Vital patches open
  as they are (`.vital`).
- **PICK in the AI sheet.** Four sounds, all wobs now; the pick steers.
- **Say what is wrong in sound words** ("too thin", "needs more sub",
  "the wob is too slow", "too much distortion") — each maps to one of the
  ranges above.

## Not changed

MATCH A SOUND still searches from its own base and matches one wob's tone;
it has the same blind spot. The next step for it is the same as here:
search inside a recipe that already wobs.
