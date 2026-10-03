# Phase 2, feature 5 — the panel's last dimmed controls

**Status:** built and tested (2026-10-01). Every control on the panel now has
an engine path; `ui/tests/bridge.test.mjs` fails if one is dimmed again.
Nobody has heard any of it yet.

| Control | Engine | Test |
|---|---|---|
| osc FM knob | `osc_N_fm_amount` (new) | `tests/test_fm.py` 1–6 |
| osc FOLD warp mode | `osc_N_fold` (new switch) | `tests/test_fm.py` 7–10 |
| DIST TUBE | `distortion_tube` (new switch) | `tests/test_drive_chain.py` |
| CRUSH HARD / SOFT | `distortion_crush_mode` (new) | `tests/test_drive_chain.py` |
| OTT 3-BAND / 2-BAND | `compressor_enabled_bands` 0 / 1 (Vital's) | — |
| wobble SMOOTH | `wobble_smooth_time` (new) | `tests/test_wobble.py` 9, 11 |
| wobble PHASE | `wobble_phase` (new) | `tests/test_wobble.py` 9, 10 |
| wobble → FM destination | `wobble_amount_osc_fm` (new route) | `tests/test_wobble.py` 12 |
| MOD MATRIX | Vital's modulation connections | `ui/tests/bridge.test.mjs` |

Eleven host parameters are appended at `version_added` 0x01000B. The other 789
are unchanged; `tests/host_parameters.txt` is regenerated in the same commit.

## A rule this feature kept: switches, not new enum values

TUBE and FOLD look like more entries in the lists they sit in. Those lists are
Vital's `distortion_type` (6 modes) and `osc_N_distortion_type` (13 warp
modes). But a host automates a choice parameter as a normalised 0–1 value, so
widening the range moves every existing value. Automation recorded on "Sine
Fold" would then play back as something else.

So each is a separate on/off parameter that overrides the mode while on:
- the panel shows TUBE as a third DIST option;
- it shows FOLD as a fourth warp option;
- `ui/src/params.ts` maps such an option to a `flag` (name + option index)
  instead of a value.

## FM knob

**Vital's FM, not a new FM.** Vital already has FM as warp modes ("FM ← Osc
2" and so on), but a warp mode takes the oscillator's only warp slot. The knob
applies the same law in a second place, through the oscillator's existing
phase-buffer hook, so FM and a warp mode such as FORMANT run together.

- The law is Vital's: `toInt(mod · amount² · kFmPhaseMult) · kMaxFmModulation`
  added to the phase.
- The source is the oscillator's first modulation partner, as in Vital: osc 1
  ← osc 2, osc 2 ← osc 1, osc 3 ← osc 1.
- The amount is block-rate: ramped across each block and snapped on a voice
  reset.
- `ProducersModule` already orders oscillators so that an FM source is
  processed before the oscillator it modulates. It now counts the knob as well
  as the warp mode.
- **Mutual FM** (osc 1 and osc 2 each FM-ing the other) cannot be ordered.
  The lower index goes first and hears the other's *previous* block, rather
  than both waiting for each other forever.

Measured (`tests/test_fm.py`):
- FM at 0.3 and 0.7 is **bit-identical** to Vital's "FM ← Osc 2" warp at the
  same amount.
- Zero, or a source that is off, is bit-identical to no FM.
- FORMANT + FM differs from either alone by more than −20 dB.
- Mutual FM plays at −18.1 dBFS RMS.
- Blocks of 32 vs 512 samples differ by −116.2 dB peak; Vital's FM warp
  differs by about the same.

## FOLD warp mode

- **Replaces** the warp mode while on.
- **Folds the oscillator's own output** with Vital's sine fold
  (`futils::sin1`): drive `1 + 7·WARP`, mix `min(4·WARP, 1)`, with WARP ramped
  per block.
- Runs in `processBlend`, after the stereo blend and before the level.

It is a waveshaper on the oscillator, not on the bus, so each voice and each
unison voice folds separately.

`setFourierWaveBuffers` reads the warp type too. The first build missed it,
so FOLD over FORMANT still formant-shifted the spectrum.

Measured:
- off is bit-identical to no `osc_N_fold` keys;
- on, over FORMANT at WARP 0, is bit-identical to no warp;
- on a pure sine at WARP 0.8, the 3rd harmonic is **−5.6 dBc** (sine: none
  measurable);
- blocks of 32 vs 512 differ by −116.1 dB.

## DIST TUBE

TUBE replaces the drive stage's curve with an asymmetric `tanh`:

```
y = (tanh(x·g + b) − tanh(b)) / (1 − tanh²(b))      b = 0.3
```

- `g` is the drive stage's own dB scaling.
- Subtracting `tanh(b)` keeps silence at zero.
- Dividing by the slope at zero gives unity gain for small signals, so TUBE
  is no louder than SOFT at low drive.
- The bias is what makes even harmonics. A symmetric curve (Vital's soft clip)
  makes only odd ones.

A biased curve also produces DC under signal. A one-pole DC blocker at 10 Hz
follows it, its state is reset with the effect, and the bias's `tanh` is a
namespace constant so nothing initialises on the audio thread.

Measured on a pure sine:
- 2nd harmonic **−19.0 dBc** (soft clip: −121.1, so none);
- DC 0.27 % of peak after the blocker;
- blocks of 32 vs 512 differ by −116.7 dB (soft clip: −115.8).

## CRUSH SOFT

- HARD rounds each held sample down to its step, as before.
- SOFT eases between steps with a smoothstep:
  ```
  level = floor(x / step)
  f     = x / step − level
  y     = (level + f²(3 − 2f)) · step
  ```
  The ease is continuous, so the stairs keep their shape (and the bit-depth
  sound) without the full edge at each step.

Measured on a dark source (filter 1 low-passed at cutoff 40), energy above
8 kHz:

| Setting | Level |
|---|---|
| clean | −115.1 dB |
| SOFT | −93.9 dB |
| HARD | −26.8 dB |

The first version of this test used the init saw. It is already bright above
8 kHz, so all three measured about −24 dB, and the check said nothing.

## OTT 2-BAND

This is Vital's own `compressor_enabled_bands` 1, "Low Band": the multiband
compressor's low and high bands, split at the low crossover. 3-BAND is 0,
"Multiband". The ratios, DEPTH and TIME are shared.

## Wobble SMOOTH and PHASE

These were constants in `WobbleModule`. They are now poly mod controls:

- `wobble_phase`, 0–1, shown in degrees;
- `wobble_smooth_time`, exponential, −10 to 4, default −7.5 (the old
  constant).

Both are block-rate. With the defaults, a render is NOT byte-identical to
before: it differs at **−111.6 dB**. Vital's `kExponential` controls use
`cr::ExponentialScale`, a polynomial approximation of `2^x`, and the constant
used `std::exp2`. The difference is that approximation and nothing else:
`tests/test_wobble.py` 9 checks that the defaults equal an explicit −7.5 / 0
bit for bit.

Measured:
- PHASE 0.5 delays the wobble by 0.537 periods (the 0.037 is the smoothing's
  lag);
- SMOOTH −3 cuts a square wobble's swing from 1.2 dB to 0.4 dB (criterion:
  less than half).

## Wobble → FM

The panel's FM destination is the new FM knob of osc 1, through a sixth fixed
route, `wobble_amount_osc_fm` → `osc_1_fm_amount`.

The existing `wobble_amount_fm` still drives `osc_1_distortion_amount` (WARP).
It is saved in presets under that name, so changing what it drives would
change old patches.

Measured: the route is bit-identical to the same connection made in Vital's
matrix, and it changes the render by −5.4 dB.

## The mod matrix

The panel's matrix is a view of **Vital's own modulation connections**. They
are the connections the classic editor's MATRIX tab shows. Nothing is
duplicated, so a preset, the classic editor and the panel always agree.

Protocol (`src/plugin/web_panel.cpp`):

| Direction | Event | Payload |
|---|---|---|
| page → plugin | `gnarlRoute` | `{source, destination, amount}`: connect if needed, then set `modulation_N_amount` through `valueChangedInternal`. The host records it, as for a Vital slider. Amount clamped to −1..1 |
| page → plugin | `gnarlRoute` with `remove: true` | `disconnectModulation` |
| plugin → page | `connect()` result | gains `routes` |
| plugin → page | `gnarlRoutes` | `[{source, destination, amount}]`, from the 30 Hz timer whenever its JSON text changes (preset load, classic editor, automation of an amount) |

Page behaviour (`ui/src/main.ts`):
- The rows are rebuilt from `engine.routes` only. A change the page makes
  shows when the plugin sends it back, not before, so the page cannot drift
  from the engine.
- **Retargeting** a row sends a remove, then a set.
- **Dragging** an amount sends a set.
- **Right-click** removes the row.
- **ADD** picks the first free source / destination pair.
- The panel offers five sources (wobble, ENV 2, LFO 1, macro 1, velocity)
  and seven destinations. A connection outside those lists is counted in the
  aside ("+k in ADVANCED") rather than drawn.

Checked under Xvfb (JUCE 8 Standalone): a route added on the panel appears in
the classic MATRIX tab with the same source, destination and amount.

`showClassicEditor` now calls `notifyModulationsChanged()` as well as
`updateFullGui()`, so the classic editor redraws connections the panel made
while it was hidden.

## VOLUME: the gate wob in one tap (2026-10-03)

The producer, in Vital, makes a wob by turning an oscillator's LEVEL to 0
and putting an LFO on it at 1.0, so the LFO alone opens and shuts the
sound - and GNARL's panel has no LEVEL knob to turn to 0. VOLUME, the last
chip under the WOBBLE LFO's destinations, does it in one tap
(`ui/src/bridge.ts` setVolumeWob): every playing oscillator's level to 0
and the wobble on it at +1.0 through the matrix, so RATE and SHAPE are the
rhythm. A second tap removes the routes and gives the levels back. The
chip is lit whenever the engine reports wobble -> osc 1 level above zero,
so a patch that already has it shows it. With VOLUME on, the wobble is the
movement, so RATE moves the wobble and not LFO 1.

Measured in `ui/tests/web.test.mjs` on the scope, frame by frame, over a
held note of the init patch: the 10th-90th percentile swing is 0.9 dB
without it and 27.6 dB with it.

## The matrix slides (2026-10-03)

"Sliding doesn't work in the route section, I have to click all the time."
The amount bar did take a drag, but every step sends the route, the engine
answers with the whole matrix, and that answer redrew the rows - replacing
the bar under the pointer, which ended the drag after one step. While a
row is dragged the matrix is no longer redrawn; it is drawn from the
engine's answer when the pointer is let go. The source and destination
buttons slide too (right or up for the next, left or down for the one
before, a step every 18 px; a click still steps once), and each row has an
x to delete it (right-click still works). `ui/tests/bridge.test.mjs`: one
drag reaches 0.90, a 60 px slide steps three destinations, the x deletes;
with the redraw put back, the drag stops at 0.20 and the slide at one step.

## TIPS (2026-10-03)

The producer asked for "tips with a popup window with steps how to make a
sound like a wobble". TIPS sits beside the AI button (desktop header and
phone) and opens seven recipes - BASIC WOB, MOVING TONE, TALKING WOB, GROWL,
MAKE IT HEAVY, SUB + SPACE, LET THE AI DO IT - each a numbered list naming
the section and the control as the panel labels them (`main.ts` RECIPES).
Text only: `ui/tests/bridge.test.mjs` opens it, switches recipes, closes it
with Escape and checks nothing was sent to the engine. The steps were
checked against the engine: the formant model ignores CUTOFF (CLAUDE.md
section 5), so MOVING TONE says so and points to VOWEL.

## Not done

- **Nobody has listened.** Every number above is a measurement of
  correctness, not of taste. TUBE's bias, FOLD's drive range and SOFT's ease
  are first choices.
- Negative controls (put the bug back, watch the test fail) were run for the
  FORMANT/FOLD override (that is how it was found), but not yet for TUBE's DC
  blocker or for the FM ordering.
