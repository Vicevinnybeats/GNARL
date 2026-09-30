# Phase 2, feature 1 — the wobble macro (design proposal)

**Status: proposed, not built.** Nothing here is implemented until it is
approved.

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
