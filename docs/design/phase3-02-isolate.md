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

## Five references

The producer's five tracks by phompy (free downloads), measured with
`--bpm auto --isolate hpss --scan 8`; JSON only in `references/` (the
`-scan` files), with one full-mix drop window per track (`-drop`) and, for
the two 1/8 tracks, the same window without drums (`-drop-bass`).

All five read **139.67 bpm** (140). In every drop window (level above
-9.5 dBFS, about 30 per track), the strongest line of the growl band:

| Track | Strongest line, per beat, in the drops |
|---|---|
| 6:25:300 | 2 (1/8) in 23 of 30, 0.5 in 6 |
| drac07 | 2 (1/8) in 30 of 30 |
| cemeteryf0g | 1 (1/4) in 30 of 30 |
| lily | 1 (1/4) in 17, 0.5 in 11 |
| meta 800 | 0.5 in 17, 0.25 in 12 |

**1/8T is rare here**: 3 per beat is among the four strongest lines in only
12 of about 150 drop windows. The "rate" the scan prints is often 0.25 or
0.5 per beat - the bar or half-bar the pattern repeats over, the
fundamental of the series - with the faster movement as its harmonics, so
read the peaks, not only the rate.

The drops' sound, full mix (one window per track): -5.6 to -6.7 LUFS;
**sub 71-77% of the power**; the sub mono (side/mid -37 to -41 dB), the
mids moderately wide (-10 to -14 dB); the brightness swinging hard
(centroid of the drums-removed bass 150-470 Hz, moving by 180-730 Hz).

## The opening sound: Riddim Wobble

The phone page now opens on **Riddim Wobble** (`factory.json`'s first): a
FORMANT-warped saw, three narrow unison voices (stereo spread 20%), a
**band-passed** growl around 1 kHz wobbled at **1/8** (square), DIST and
OTT, over the mono sub. Designed with `tools/compare.py` against the two
1/8 drops without drums. Against drac07's:

| | drac07 drop, bass only | Riddim Wobble, F1 |
|---|---|---|
| sub share | 93.7% | 79.4% |
| low-mid share | 0.3% | 4.6% |
| sub side/mid | -39.6 dB | -35.6 dB |
| centroid | 473 Hz | 339 Hz (-5.8 st) |
| growl movement | strongest line 2/beat | 2.0/beat |

The reference's sub share is inflated by HPSS, which removes the growl's
attacks with the drums: the same window as a full mix is 74% sub. The
first drafts were far off - two unison voices put the sub at -8.6 dB
side/mid; a low-pass growl put 20.6% in the low mids against the
references' 0.3% - and the band-pass is what the references' gap between
sub and growl asked for.

Its level is set for a phone, not a master: peak -1.4 dBFS, -14.8 LUFS. At
the references' loudness (-8.7 LUFS) it peaked at +4.7 dBFS; they get there
with a mastering limiter (crest 7.7 dB against the patch's 10.3).

**Nobody has heard it.** These numbers say where it sits relative to the
references, not whether it sounds like them; that is the producer's call.

INIT in the preset sheet is still Vital's plain saw, and the plugin still
opens on it.
