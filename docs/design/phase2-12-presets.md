# Phase 2 #12: built-in presets, in the plugin and on the phone

## What changed

- **Full patches as starting sounds.** `presets/*.vital` are complete GNARL
  patches (wavetables, LFO shapes, matrix, effects), built by
  `tools/build_presets.mjs` from readable specs. `ui/src/web/factory.json`
  names them (`"patch"`) beside the older settings-only starting sounds.
- **The plugin's panel has the preset sheet.** Click the patch name: the
  starting sounds and INIT; the arrows step through the starting sounds.
  Saving and opening files stay in Vital's browser (ADVANCED). The page
  sends `gnarlPresetFactory {name, patch}`; `WebPanel::loadFactory` loads it
  through `SynthPlugin::setStateInformation` - the path a DAW restoring a
  project takes, the same `.vital` JSON and loader. Settings-only sounds and
  `gnarlPresetInit` go through `loadInitPreset` and `valueChangedInternal`.
- **The phone opens on Alien Riddim.** Riddim Wobble, designed from
  whole-bar measurements, is gone: the producer called it wrong, and the
  bar was the wrong unit (phase3-02-isolate.md).
- **The downloads carry `Presets/`**, the same files, for Vital's browser.

## The two patches

Rebuilt from the producer's screenshots of two Vital patches - by eye, not
by file: knob positions are estimates on a 7:30-4:30 sweep, and each value
is in the spec with the screenshot's reading. The names are GNARL's own.

**Riddim Sub** - osc 1 a sine (built in, 2048 samples), -24 semitones, phase
180, no random phase, one voice; instant attack, hold, full sustain, short
release; glide; LFO 1 shaped (fast rise, long fall) at 1/2, retriggered.
**Not routed**: the screenshots do not show the MATRIX tab.

**Alien Riddim** - osc 1 a square, osc 2 a sine through the Random
Amplitudes spectral morph (about 2/3), both -12 into filters 1+2; filter 1
Comb Low High Flange+, filter 2 Comb Low High Comb; LFO 1 at 1/4, retriggered,
two humps per cycle as drawn; chorus (16 voices, frozen), soft clip,
flanger (4/1), phaser (frozen). **LFO 1's routes are a guess** - filter 1
and 2 cutoffs, flanger and phaser centres - from the modulation rings in the
screenshots; the MATRIX tab would settle it.

## One wob per key (revised)

The producer, after playing it: Alien Riddim "has to be one single wob and
not continuously ... like a 1 second wob". LFO 1 now plays its shape ONCE
(Envelope mode) over 1 s, and the amp envelope holds for 0.9 s and falls in
about 40 ms. A 3 s held F3, every 100 ms: level -14.9 to -18.3 dB through
0.8 s, -26 dB at 0.9 s, -72 dB at 1.0 s; brightness swinging 437-1222 Hz,
darkest at 0.2 and 0.6 s (the two humps drawn in the patch's LFO). Peak
-3.2 dBFS. Whether one wob should hold one hump or two is the producer's
call; an exported example of one wob from their patch would settle it in
numbers.

## Measured

Rendered at F3 (the patches are transposed down, as in Vital: the sub
sounds F1, 44 Hz, the note all five references' drops sit on):

| | Riddim Sub | Alien Riddim |
|---|---|---|
| peak | -1.8 dBFS | -3.0 dBFS |
| loudness | -7.3 LUFS | -12.6 LUFS |
| bands | 100% sub, mono (side/mid -278 dB) | 1.7% sub, 47% low, 25% mid; wide (side/mid -3 to -5 dB) |
| movement | none (LFO not routed) | level and brightness 2.0 per beat: two humps per 1/4 |

Alien's 2 per beat is the line the two 1/8-sounding references (drac07,
6:25:300) have strongest. Its low end is wide, which a riddim mix keeps the
sub for: play Riddim Sub under it.

In the desktop plugin (JUCE 8 VST3, through `tools/vst3_probe.cpp`'s new
`GNARL_PROBE_STATE`, which hands the plugin a patch as a DAW restores one):
the plugin accepts Alien Riddim, the init saw's mono output (peak 0.36)
becomes stereo (side/mid -5.6 dB, peak 0.80), and the movement measures 2.0
per beat in level and brightness - as in the renderer.

Tests: `ui/tests/bridge.test.mjs` (the plugin's sheet: seven sounds, no
saving, a sound sent whole, Riddim Sub's patch arriving as text, the arrows,
INIT); `ui/tests/web.test.mjs` (the phone opens on Alien Riddim, seven sounds
in order).

**Nobody here has heard them.** The producer's ear decides; the screenshot
of the MATRIX tab would remove the two guesses.

## Vital 1.5 patches open; Vinny Bass 2 replaces Alien Riddim

The producer sent two patches made in Vital 1.5.5 (their own Vinny Bass 2,
and a third-party Alien patch the screenshots were of) and two exported
wobs. GNARL refused both patches: its engine is Vital 1.0's, and
`jsonToState` refuses a newer feature version.

**What 1.5.5 adds, in these files:** five settings - `custom_warps` (all the
default triangle), `osc_N_spectral_morph_phase` (0.5 on every oscillator,
the unused third too), `random_values` (seeds). Nothing else is unknown.
So `LoadSave::readableNewerPatch` (and its copy in `wasm/shim/load_save.h`)
opens a patch up to 1.5 that keeps every spectral morph phase at 0.5 and
every warp type within this engine's range; anything else is still refused.
`tests/test_web.py`: a 1.5.5 patch using none of it loads in both builds
(browser vs desktop -41.4 dB, that patch's known wavetable gain); with a
phase of 0.3, both refuse it. (A first version compared against "1.5",
which the feature comparison cut to "1", and refused everything.)

**Does GNARL play them as Vital does?** Against the producer's own exports,
every 20 ms, aligned at the note:

| Export | GNARL render | Lowest line | Brightness trace | Brightness |
|---|---|---|---|---|
| WoB_1 (0.22 s) | the Alien patch at D2 | 36.4 Hz = 36.4 Hz | correlation 0.73 | 2.4 st darker |
| Wob_2 (0.8 s) | Vinny Bass 2 at D1 | 37.7 Hz = 37.7 Hz | correlation 0.69 | 1.7 st darker |

(Wob_2's level trace correlates only 0.25: the export's note is released by
about 0.5 s, the render holds 0.8 s.)

**Vinny Bass 2 is now built in and the phone opens on it** - the producer's
own patch, kept exactly (presets/source/), with only the master volume
4.5 dB down: it peaked at +0.6 to +1.2 dBFS, which a phone clips; now -0.9
to -1.4 dBFS on D1-D2. GNARL's copy and the original render bit-identically
with the volumes matched. **Alien Riddim is withdrawn**: the producer's
patch now opens in GNARL as it is, and a third-party preset is not GNARL's
to ship.

## One-shot wobs from the references' stems; six test sounds

The producer: separate the reference tracks into stems and see what one
wob does. Each drop's first 8 bars, the bass stem by HPSS (harmonic part,
margin 1; Demucs cannot run here), cut at every onset in 150-6000 Hz,
each wob measured (length while within 20 dB of its peak, brightness
turns, where it is brightest):

| Track | Wobs | Length, median | Brightness turns | Brightest at |
|---|---|---|---|---|
| 6:25:300 | 17 | 1.85 beats | 1 in 10 | the end (1.00) |
| cemeteryf0g | 14 | 2.06 beats | 1-3 | 0.80 |
| drac07 | 18 | 1.81 beats | 1 in 9 | 0.21 |
| lily | 33 | 0.82 beats | 0 in 17 | the hit (0.06) |
| meta 800 | 31 | 0.96 beats | 0-1 | 0.34 |

A wob is about two beats - 0.8-0.9 s at 140, the producer's "1 second
wob" - and opens and closes once, centred about 2 kHz.

The test sounds (replacing the five settings-only ones), each one wob per
key, LFO 1 once (Envelope mode) over a tempo-synced length:

| Sound | Length | Turns | Brightest at | Centroid (10-90%) |
|---|---|---|---|---|
| Wob Open | 1.99 beats | 1 | 0.80 | 768 Hz (357-1972) |
| Wob Peak | 1.94 beats | 1 | 0.21 | 649 Hz (299-1607) |
| Wob Close | 0.96 beats | 0 | 0.00 | 738 Hz (316-1754) |
| Frog Croak | 1.99 beats | 2 | 0.89 | 1639 Hz |
| Ribbit | 1.02 beats | 0 | 0.99 | 1514 Hz |
| Swamp Gurgle | 1.99 beats | 3 | 0.95 | 1659 Hz |

The three Wobs match their references' length, turns and brightest point;
they are DARKER (650-770 Hz centre against about 2 kHz). A plain saw at F1
centres at 170 Hz even unfiltered, so the growl is built first (FORMANT
warp, soft clip, FOLD) and the filter starts at about 370 Hz. The froggy
three run Vital's formant filter's vowel from LFO 1 (Croak also bends the
pitch). Peaks -4.7 to -7.5 dBFS. No sub in any: play Riddim Sub under them.

## Fixed: the wobble's RATE on a Vital patch, the arrows, the delay LED

- On a patch that moves with LFO 1 and leaves the wobble at zero (Vinny
  Bass 2), the WOBBLE panel's RATE sets LFO 1's rate, and its corner says
  RATE MOVES LFO 1. Vinny Bass 2 at 1/8 moves 2.0 per beat; at 1/4, about
  1.25 (its LFO 4 also modulates LFO 1's rate).
- The phone's arrows step through the starting sounds, then the saved ones;
  they stepped only through saved patches, so with none saved did nothing.
- The delay LED in MS moves 10 ms a tap (50 held); 1 ms was inaudible -
  Vinny Bass 2's delay is MS 215 ms at a 13% mix.
