# Phase 3 #2: hearing the wobble under the drums

The producer asked for something that can read the wobbles in reference
tracks. Nothing on GitHub does that as such; what exists are the parts:

| Tool | What it does | Used here |
|---|---|---|
| [Demucs](https://github.com/facebookresearch/demucs) (Meta, htdemucs) | splits a mix into drums / bass / vocals / other with a trained model | `--isolate demucs`: measure the bass stem. **Not run in this repository's sandbox**: the model download is blocked there. On a producer's machine: `pip install demucs` |
| [librosa](https://librosa.org) HPSS | harmonic/percussive separation by median-filtering the spectrogram; no model | `--isolate hpss`: drop the drums. Tested here |
| librosa beat tracker (also [madmom](https://github.com/CPJKU/madmom), [Essentia](https://essentia.upf.edu)) | tempo and beats | `--bpm auto` |

`tools/measure.py` already measured the wobble itself (the modulation
spectrum, phase3-01-measure.md). What was missing: the drums move a track's
level far more than a wobble does, and a whole track has to be cut by hand
into drops. The additions, in `tools/isolate.py` (librosa and Demucs are
imported only when asked for, so CI still needs only numpy and scipy):

- `--isolate hpss | demucs` - measure the bass without the drums.
- `--bpm auto` - librosa's beat tracker, folded into 100-200 bpm.
- `--drops N` - finds each drop and measures its first N bars, with the
  drums and (with `--isolate`) without. A drop: two bars within 3 dB of the
  track's loudest after eight averaging 5 dB or more below it, at least 16
  bars after the last; it starts at the sharpest 50 ms rise in level near
  that bar. `tests/test_measure.py` checks it on a built track (two drops,
  fills that are not drops, 0.3 s off the bar grid) and on one at a single
  level; without the 16-bar gap or the quiet-before rule it finds 12 and 43.
- `--scan N` - every N-bar window, hop N/2, across the whole track: level,
  growl-band rate and depth, brightness rate and depth. The drops find
  themselves.
- Any file ffmpeg reads; the decoded WAV is temporary and deleted.

## The instrument, measured first

`tests/test_isolate.py` (needs librosa; skips without it). A synthetic growl
- a saw at 87 Hz swept by a low-pass three times a beat (1/8T at 140) -
under a synthetic kit (kick every beat, snare on 2 and 4, hats on 1/16) at
four times its level:

| | Growl-band rate |
|---|---|
| the growl alone | 3.0 per beat |
| under the drums, as measured before | **0.5 per beat** - the drums win |
| under the drums, `--isolate hpss` | 3.0 per beat |
| `--bpm auto` | 140 (it read **69.84** before the fold: half-time drums) |

Also on a GNARL render (Triplet Growl) under the same kit at 2x and 4x: as
measured before, 1.5 and 0.5 per beat; with HPSS margin 1, 3.0 both times.
HPSS margin 3 failed at 4x (it took the wobble's own edges), so margin 1.
The tempo read 139.67 on all of them.

## Five references, at the drops

The producer's five tracks by phompy (free downloads), JSON only in
`references/`: `-drops` (`--bpm auto --isolate hpss --drops 8`: the first 8
bars of every drop) and `-scan` (`--scan 8`: the whole track).

All five read **139.67 bpm** (140), and all five drop at the same bars: **16
(27.4 s) and 96 (164.5 s)** - the same arrangement throughout; checked by
hand on meta 800 (bar 15 -27 dB, bar 16 -4 dB). The first 8 bars of each
drop, the three strongest lines of each movement, per beat:

| Track | Drop | Full mix | Sub share | Level (no drums) | Brightness (no drums) | Centroid |
|---|---|---|---|---|---|---|
| 6:25:300 | 1 | -6.1 LUFS | 75% | 2.01, 0.75, 0.49 | **1.00**, 0.50, 1.50 | 311 Hz |
| 6:25:300 | 2 | -5.4 LUFS | 70% | 2.00, 0.50, 0.75 | **1.00**, 0.50, 1.50 | 447 Hz |
| cemeteryf0g | 1 | -6.6 LUFS | 82% | 1.00, 2.00, 0.25 | **1.00**, 0.25, 1.25 | 170 Hz |
| cemeteryf0g | 2 | -6.6 LUFS | 77% | 1.00 | **1.00**, 0.25, 1.50 | 233 Hz |
| drac07 | 1 | -5.8 LUFS | 74% | 2.00, 1.00, 1.50 | **1.00**, 0.50, 1.50 | 559 Hz |
| drac07 | 2 | -5.7 LUFS | 75% | 2.00, 1.00, 0.50 | **1.00**, 0.50, 1.50 | 482 Hz |
| lily | 1 | -5.6 LUFS | 76% | 1.00, 0.25, 0.50 | **1.00**, 0.50, 2.51 | 222 Hz |
| lily | 2 | -5.5 LUFS | 74% | 1.00, 0.50, 0.25 | **1.00**, 0.50, 1.50 | 243 Hz |
| meta 800 | 1 | -5.6 LUFS | 72% | 0.25, 0.50, 1.00 | **1.00**, 0.25, 2.51 | 140 Hz |
| meta 800 | 2 | -5.6 LUFS | 73% | 0.25, 0.50, 1.00 | **1.00**, 0.25, 2.51 | 139 Hz |

**The brightness moves once a beat - 1/4 - in all ten drops.** The level's
strongest line is 2 per beat in two tracks and 1 in two: a 1/4 wobble whose
level peaks twice a cycle shows exactly that (CLAUDE.md §7: the loudest line
can be the 2nd harmonic). Lines at 0.25 and 0.5 are the bar and half bar the
riff repeats over. **1/8T does not appear** among any drop's three strongest
lines.

The drops: -5.4 to -6.6 LUFS; the sub 70-82% of the power, mono (side/mid
about -40 dB); the centroid of the bass without drums 140-560 Hz.

The producer hears the drops at about 0:32. By level they start at 27.4 s
(drac07 and meta 800, beat by beat, drums removed: the sub goes from -81 to
-8 dB in one beat at 27.4 s, and nothing changes at 32). Measured as 8 bars
from 32.0 s instead, the answer holds: brightness 1.00 per beat in all five,
the level lines as above, the centroid within 30 Hz (drac07 546 against
559). Something the ear marks at 0:32 is not in the level - worth asking
what it is.

An earlier pass measured a window picked by hand from the middle of each
drop, and read the 1/8 level line as the rate; measured at the drops, the
brightness says 1/4.

## The opening sound: Riddim Wobble (withdrawn)

**Withdrawn**: the producer called it wrong - the measurements above average
whole bars, kick and snare included, where the unit that matters is one wob
of the bass alone. The phone now opens on Alien Riddim, rebuilt from the
producer's own patch (phase2-12-presets.md). What follows is the record.


The phone page now opens on **Riddim Wobble** (`factory.json`'s first): a
FORMANT-warped saw, three narrow unison voices (stereo spread 20%), a
**band-passed** growl around 1 kHz wobbled at **1/4** (square) on cutoff
and warp, DIST and OTT, over the mono sub. Designed with `tools/compare.py`
against drop 1 of drac07, cemeteryf0g and 6:25:300 without drums (F1):

| | drac07 | cemeteryf0g | 6:25:300 | Riddim Wobble |
|---|---|---|---|---|
| sub share | 94.0% | 96.9% | 95.1% | 79.6% |
| sub side/mid | -40.1 dB | -40.6 dB | -40.3 dB | -35.9 dB |
| centroid | 559 Hz | 170 Hz | 311 Hz | 345 Hz |
| strongest brightness line | 1.00/beat | 1.00/beat | 1.00/beat | 1.00/beat |

Whole modulation-spectrum correlation with each reference, at 1/8 and then
at 1/4: level 0.21 -> 0.28, 0.14 -> 0.33, 0.16 -> 0.22; brightness 0.11 ->
0.24, 0.09 -> 0.22, 0.09 -> 0.26. A deeper cutoff wobble (0.8, 1.0) did not
help: the level swing passed twice the references' and the centroid fell
7-17 semitones. The references' riffs move over bars (0.25, 0.5 per beat), a
held note cannot - that part of the match is the producer's playing.

The references' sub share is inflated by HPSS, which removes the growl's
attacks with the drums: the same drops as full mixes are 70-82% sub. Early
drafts were far off - two unison voices put the sub at -8.6 dB side/mid; a
low-pass growl put 20.6% in the low mids against the references' 0.3% - and
the band-pass is what the references' gap between sub and growl asked for.

Its level is set for a phone, not a master: peak -1.8 dBFS, -14.8 LUFS. At
the references' loudness it peaked at +4.7 dBFS; they get there with a
mastering limiter.

**Nobody has heard it.** These numbers say where it sits relative to the
references, not whether it sounds like them; that is the producer's call.

INIT in the preset sheet is still Vital's plain saw, and the plugin still
opens on it.
