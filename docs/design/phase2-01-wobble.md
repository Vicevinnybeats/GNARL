# Phase 2, feature 1 — the wobble macro (design proposal)

**Status: engine built and tested (2026-09-30); the WOBBLE panel in the UI is
not built yet.** The client said to continue without answering the four
questions below, so the proposed defaults were used: WT depth on osc 1 AND
osc 2, cutoff on filter 1, the third knob labelled FM/WARP, the four rates.
All cheap to change. See "What was built" at the end.

## What the producer gets

One **WOBBLE** panel:

- **Rate** — four buttons: `1/4` `1/8` `1/8T` `1/16`. One automatable host
  parameter, so a drop can switch from 1/8 to 1/8T with an automation lane.
- **Shape** — a drawable curve, the same editor as Vital's LFOs.
- **Three depth knobs** — *WT position*, *Cutoff*, *FM*. Each is its own
  automatable parameter, and turning one up is all it takes: no matrix
  routing.
- **Unipolar / bipolar** switch, **unipolar by default** (see below).
- **Locked to the DAW's transport**, so every note's wobble lands on the grid
  instead of restarting wherever the note happens to start.

At 140 BPM those rates are 2.33, 4.67, **7.00** and 9.33 Hz. The reference
tracks measured earlier carried energy at 4.67, 7.00 and 9.33 Hz at the same
time — which is the case for a *drawn* shape rather than a sine: one uneven
curve per cycle can put events on 1/8 and 1/8T positions together, and no
single-rate sine can.

## Why it can't just be a macro on an existing LFO

Checked in the source, not assumed:

- Vital's tempo sync is **two parameters**: a division index
  (`lfo_N_tempo`, the `kSyncedFrequencyRatios` table) and a sync *mode*
  (`lfo_N_sync`: time / tempo / dotted / **triplet** / keytrack).
  `TempoChooser::process` multiplies triplet by 3/2.
- `1/8T` is "1/8" **plus** triplet mode. A macro turns one parameter
  continuously, so a macro on the tempo index steps 1/4 → 1/8 → 1/16 and
  **skips the triplet** — the rate riddim uses most.

So the rate has to be one parameter that the engine maps to (division, mode)
itself.

## How it is built

**Engine** (`src/synthesis/`):

1. A `WobbleModule` owning a `SynthLfo` and its `LineGenerator` (the drawable
   shape), registered as modulation source **`wobble`** exactly the way
   `SynthVoiceHandler::init` registers `lfo_1`…`lfo_8`. It appears in the
   ordinary mod matrix too, so it can be routed anywhere else as well.
2. Sync type fixed to `SynthLfo::kSync`, which phases the LFO from the
   transport (`sync_seconds_`) rather than from note-on.
3. A small processor maps `wobble_rate` (0–3) to frequency:
   `ratio × beats_per_second × (triplet ? 3/2 : 1)` with ratios
   {1, 2, 2, 4} and triplet only on index 2. Block-rate.
4. **The three depths are fixed routes, not matrix connections.** Every
   modulatable parameter already sums its modulation through a
   `ModulationSum`; a matrix connection is just a processor plugged into it
   (`SoundEngine::connectModulation` → `destination->plugNext`). Each depth
   is one such processor, **plugged once at construction**, so:
   - it costs none of the 64 user matrix slots,
   - it is automatable, because its amount is a parameter,
   - nothing is plugged on the audio thread. That matters because upstream
     does: `processModulationChanges` runs at the top of `processBlock` and
     calls `connectModulation`, whose `plugNext` does `std::make_shared`
     and `push_back` whenever the destination has no free input slot —
     an **allocation on the audio thread** the first time a destination
     gains a connection it never had. Our routes are plugged once, at
     construction, so they never take that path. (Upstream's own path is
     worth fixing separately; see the end.)

   Targets: `osc_1_wave_frame` and `osc_2_wave_frame` (one knob drives
   both), `filter_1_cutoff`, `osc_1_distortion_amount`.

**New parameters, appended to `synth_parameters.cpp`** — appended, never
inserted, because presets store parameters by name and hosts by order:

| Name | Range | Default |
|---|---|---|
| `wobble_rate` | 0–3 (`1/4`, `1/8`, `1/8T`, `1/16`) | `1/8` |
| `wobble_amount_wave_frame` | −1…1 | 0 |
| `wobble_amount_cutoff` | −1…1 | 0 |
| `wobble_amount_fm` | −1…1 | 0 |
| `wobble_bipolar` | off/on | off |

Plus the drawn shape, saved in the preset as `settings.wobble_shape`. Older
presets without it load with a default shape; `wobble_rate` is an index into
a list that is **append-only** — adding `1/2` or `1/16T` later goes on the
end.

All of it defaults to zero depth, so **every existing preset sounds exactly
as it did** — which will be tested, not asserted (below).

**UI**: a `WOBBLE` tab beside the eight LFO tabs, reusing the LFO editor for
the shape. This is the most expensive part: Vital's interface is OpenGL and
hand-laid-out.

## Why unipolar by default

Measured on the retired engine, and a property of modulation rather than of
that code: a bipolar source reads 0 at its resting point and swings a full
depth *below* the knob. For a wobble on cutoff, that closes the filter for
half of every cycle and on every note-on. For a gate-shaped growl — which is
what the references measured as — the shape should open *from* the knob
position, not either side of it.

## FM needs an FM mode

In Vital, FM lives in an oscillator's *distortion* slot. `wobble_amount_fm`
moves `osc_1_distortion_amount`, which only means "FM amount" when osc 1's
distortion type is one of the three FM types. With any other type the knob
still moves that amount — sync, formant, bend — which is arguably useful,
but the label would be wrong. Proposal: label it **FM / WARP** and say so in
the tooltip.

## How it will be tested

Using `gnarl-render`, deterministic, in `tests/`:

1. **Rate is right.** Render a held note with the wobble on cutoff at
   140 BPM for each rate; take the modulation spectrum of the 200–2000 Hz
   band envelope (hop ≤ 256 samples, so its Nyquist is far above 9.33 Hz)
   and assert the peak is within ±2% of 2.33 / 4.67 / 7.00 / 9.33 Hz. The
   whole spectrum is printed, not only the argmax — the earlier
   autocorrelation found the kick drum.
2. **Transport lock.** Two notes started half a beat apart must have their
   wobble phase-aligned after the start.
3. **Zero depth changes nothing.** A preset rendered before and after
   this feature, depths at 0, must be bit-identical.
4. **Block-size invariance.** Same render in 32- and 512-sample blocks.
5. **Automation.** Switching `wobble_rate` mid-note does not click (no
   discontinuity above a threshold in the output derivative).

## Questions for you

1. **WT position on osc 1 only, or osc 1 and osc 2 together?** Proposed:
   both, one knob.
2. **Filter 1 only**, or both filters? Proposed: filter 1.
3. OK to label the third knob **FM / WARP**?
4. Any rate beyond the four you listed worth reserving now (`1/2`, `1/16T`,
   `1/32`)? Adding later is fine — the list is append-only — but it is
   cheaper to design the buttons once.

## Before this: one hardening change

Upstream's `processBlock` has **no denormal protection** — no
`ScopedNoDenormals`, nothing in `src/` sets flush-to-zero, and GCC 13 no
longer adds it via `-ffast-math` for a shared library (the built plugin has
no `set_fast_math`). A decaying filter or reverb tail can hit the denormal
range and cost ~100× CPU on some machines — an audible dropout in a busy
session. Proposal: one line, `juce::ScopedNoDenormals` at the top of
`SynthPlugin::processBlock` (and the headless render loop), verified by
rendering a long release tail and asserting no subnormal samples and no
change above −120 dB versus today.

A second upstream real-time issue, found while designing this, for later:
routing a new modulation **allocates on the audio thread** (see "How it is
built", point 4). The fix is to pre-size every `ModulationSum`'s inputs so
`plugNext` always finds a free slot. Not part of this feature; listed so it
is not forgotten.

## What was built — and where it differs from the proposal

Measured by `tests/test_wobble.py`, which renders through the engine:

| Check | Result |
|---|---|
| 1/4, 1/8, 1/8T, 1/16 at 140 BPM | 2.334 / 4.668 / 6.997 / 9.34 Hz (all within 0.2%) |
| 1/8 at 100 BPM | 3.333 Hz |
| Note started half a beat later | wobble shifted 0.499 periods; a note-triggered LFO (negative control) shifted 0.000 |
| Depth 0.6 vs a 0.6 matrix connection from the wobble | **bit-identical**, at 32- and 128-sample blocks |
| Zero depth | bit-identical to the same patch with no wobble keys, and to a render from the build before the wobble existed |
| Block 32 vs 128 | −64.2 dB — the same as Vital's own LFO on the same destination |
| CPU, one voice, 60 s | 3.28–3.52 s, against 3.39–3.71 s before the wobble existed |
| Host parameter order | the five parameters sit at indices 772–776, after every upstream parameter |

**The routes are `ModulationConnectionProcessor`s, not a new class.** The
proposal said a route would "match a matrix connection". A re-implementation
with identical arithmetic was built first and matched to only −15 dB. The
routes now use the very class a matrix connection uses (given a second
constructor that takes its bipolar/stereo/bypass controls from outside), so
the claim is exact rather than approximate.

**They run at audio rate.** The missing step was one line of
`SoundEngine::connectModulation`: for an audio-rate destination it switches
the source and the connection to audio rate. That is why a matrix connection
from an LFO is smooth per sample — and without it the wobble reached the
cutoff once per block. Cost, measured: none visible on one voice.

**The drawn shape is saved as `settings.wobble_shape`,** a separate key, so a
preset still has exactly eight `lfos`.

**Three bugs found by the tests, each on the first run that could see it:**

1. `utils::toInt` rounds to nearest-even (it is the SSE conversion) rather
   than truncating, so `toInt(rate + 0.5f)` turned 1 into 2 and 3 into 4 —
   and index 4 read past the end of the rate table. Measured: 1/8 at 7.0 Hz,
   1/16 at 17.5 Hz, a number from outside the table.
2. Vital's LFOs default to smoothing ON (5.5 ms); the wobble had it off.
3. `WobbleModule` did not forward `setControlRate` to its inner `SynthLfo`,
   as `LfoModule` does.

The first block-size threshold, −100 dB, was set before measuring what the
engine can do. Vital interpolates control values across each block, so no
modulated patch is block-size exact; its own LFO measures −64 dB. The test
now holds the wobble to the engine's own figure.

