# Phase 4 — the AI button: pick the best

The producer, after the matched sounds: "Not even one sound is good … it's
started to being a loop". Every round so far had the same shape:

1. I built sounds against a measure;
2. the producer listened once at the end;
3. the sounds missed.

Nobody who can hear phompy's sound was steering. This mode puts the
producer's ears inside the loop.

## How it works

The AI button opens four sounds (`main.ts` `evolveSheet`):

- **Tap a card** to hear it. The patch loads whole, as a starting sound
  does (`gnarlPresetFactory`), and plays MIDI 39 (the producer's D#3 in FL
  Studio) for 1.8 s, a two-beat wob at 140 BPM and its tail.
- **PICK** makes the next round from the picked patch: three variations
  near it and one wild one (`generate.ts` `evolvePatch`). The near steps
  shrink as the picks go on, by `1 / (1 + 0.35 × picks)`, never below
  0.25. That way the search settles where the picks lead.
- **BACK** undoes a pick. **NEW** starts again from the bases (Ref Wob
  1–6).
- **KEEP** closes the sheet with the last sound loaded, so SAVE keeps it.

Nothing goes over the network, and it works the same in the plugin and on
the phone.

## Level along a chain

Every variation's volume is worked out from the base's level, which is
carried in the patch as `gnarl_level` (the engine ignores unknown keys):
4.5 dB of room, less any drive added since the base. Two earlier versions
failed `tests/test_generate.py`:

- Compensating step by step, and giving back drive taken away, crept to
  **+3.9 dBFS**: less drive does not make a saturated sound proportionally
  quieter.
- With 2.5 dB of room, the other steps (the wob's place in the table, the
  filter) still moved a chain by up to 6 dB, to **0.0 dBFS**.

The test now follows five chains of twelve picks: they stay between −13.1
and −2.0 dBFS at D#2. With the room removed, the check fails at +2.5 dBFS.

## Recognisers tried

None of the open sound-recognition models I could reach was better at
telling these sounds apart.

**The test.** Fifteen reference wobs, three per track. For each wob, the
other fourteen are ranked by distance; a good "ear" puts its two track-mates
first.

| Measure | Precision@2 | Mean rank of track-mates |
|---|---|---|
| the matcher's log-mel (`tools/match.py`) | 0.50 | 4.33 |
| OpenL3, music, 512-d | 0.47 | 4.50 |
| chance | 0.14 | 7.5 |

- **OpenL3**: torchopenl3, its weights fetched by git, since direct
  downloads are refused here.
- **Not testable here**: CLAP, MERT, PANNs and VGGish are hosted on
  Hugging Face, Zenodo or GitHub release assets, which this environment's
  network policy refuses.
- **Syntheon** (phase4-02-matcher.md) writes only a static wavetable and an
  envelope, so it cannot express a wob.

## Three recognisers on the whole tracks (2026-10-02)

The producer: "test with the 3 models, recognise the tracks to get a single
wob". CLAP is reachable only through Hugging Face and Zenodo, which this
environment refuses. The three models run instead:

- **YAMNet** and **VGGish** (Google, AudioSet), from Google Storage. VGGish
  runs in PyTorch from its own checkpoint's weights. Checked against its
  smoke test: embedding mean/std **0.000657 / 0.343**, as published.
- **OpenL3**, as before.

**Where the drums are.** YAMNet tags the tracks "Dubstep" and "Electronic
music". Its drum classes stay near zero even in the drops (90th percentile
0.01–0.03), so it cannot find drum-free moments here. The percussive share
from HPSS is used for that instead.

**The wobs.** Each whole track was cut into single wobs: the bass (HPSS),
at onsets, 0.8–2.3 beats, the louder 60%. That gave 406 wobs in all.

**Which recogniser tells the tracks apart?** For each wob, the share of its
5 nearest wobs that come from the same track:

| Measure | Same-track share |
|---|---|
| OpenL3 | 0.85 |
| log-mel (the matcher's) | 0.78 |
| VGGish | 0.75 |
| YAMNet | 0.71 |
| chance | 0.26 |

**Signature wob per track.** Among the less drummy half of a track's wobs,
the one closest to its five nearest neighbours in the same track: the
centre of the densest cluster. Where two measures agree, that one;
otherwise OpenL3's.

| Track | Time | Length | Drum share | Agreed by |
|---|---|---|---|---|
| 6:25:300 | 3:33.7 | 1.03 beats | 0.11 | VGGish, OpenL3 |
| cemeteryf0g | 4:22.4 | 0.86 beats | 0.04 | OpenL3 only |
| drac07 | 4:14.7 | 1.41 beats | 0.09 | VGGish, OpenL3 |
| lily | 2:47.6 | 0.81 beats | 0.09 | YAMNet, OpenL3 |
| meta 800 | 4:22.4 | 0.84 beats | 0.06 | YAMNet, log-mel |

Timestamps only: the audio stays in the session's scratch space.
