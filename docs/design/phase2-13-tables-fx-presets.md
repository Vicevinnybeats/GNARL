# Phase 2 — OSC wavetables and FX presets

The producer asked for "different sort of wavetables to add on osc 1 and 2,
also presets for all the FX".

## Wavetables

`ui/src/wavetables.ts` computes nine tables from formulas: GNARL's own, so
there is no file to license and nothing to fetch. Each morphs across
WT POS (frames 0 to 255):

| Table | Frame 0 to frame 255 |
|---|---|
| Basic | saw into square (the even harmonics fade) |
| Growl | a saw with a resonant peak sweeping from the 2nd harmonic to the 28th |
| Vowel | A, E, I, O, U formants on the harmonics of a nominal 65 Hz (about C2) |
| Croak | a damped ring restarted each cycle, its pitch from the 2nd to the 11th harmonic |
| Fold | a sine folded at 1x to 8x |
| Sync | a hard-synced saw, ratio 1 to 8 |
| FM | a sine modulated at twice its rate, index 0 to 6 |
| Pulse | a pulse narrowing from 50% to 4% |
| Steps | a saw quantised from 32 steps down to 3 |

A table goes to the engine as Vital's wavetable JSON: 16 keyframes of 2048
samples, spectral interpolation between them (WaveSource `kFrequency`), DC
removed and normalised. The engine band-limits it per note, as any table.

**Path.** The page sends `gnarlWavetable {osc, table}` (the JSON as text).
The plugin (`WebPanel::loadWavetable`) hands it to the oscillator's
`WavetableCreator::jsonToState` on the message thread. That is what Vital's
own editor does when it loads a wavetable file; the creator hands the audio
thread the new frames. The browser engine does the same in
`gnarl_load_wavetable`, between blocks. Both report OSC 1 and 2's table
names in `gnarlFrame.tables` when they change. A preset's table therefore
shows as itself: INIT, or a producer's own import, outside the list. The
table is saved inside the patch, as every Vital table is.

**Measured** (desktop renderer, Wob Open with OSC 1's table replaced, WT POS
at frame 128, C2, one bar):

- every table loads and is saved under its name;
- every render is within 0.8 dB of the others (−12.3 to −13.1 dBFS);
- the spectral centroids run from 417 Hz (Basic) to 1145 Hz (FM).

`ui/tests/web.test.mjs` picks a table in the phone page and checks that:

- the engine names it back;
- the output's shape changes (correlation with the init table −0.63);
- the arrows step;
- the table is saved in the patch;
- INIT restores the init table.

With the worklet's handler disabled, the test fails three checks.

## FX presets

`ui/src/fxpresets.ts`: three or four named settings for each of the ten
effects. Each effect's PRESET button applies the next one:

- it sets only that effect's controls, and switches the effect on;
- each control gets a gesture, so a host records the change as an edit;
- the button shows the preset's name until one of the effect's controls is
  moved by hand.

The settings are starting points chosen from each control's range, not
measured against anything.
