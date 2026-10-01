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
