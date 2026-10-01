# Phase 2 — GNARL's own panel (design + first build)

**State:** browser build done (`ui/`), not yet inside the plugin.

## What the producer asked for

- A UI "totally different and easy to use" instead of Vital's.
- The layout and buttons of the mock-up made in another chat (one page:
  OSC 1 / OSC 2 / SUB / VOWEL FILTER, WOBBLE LFO / ENVELOPE / MOD MATRIX,
  four FX tiles), restyled like the retired engine's UI.
- The corner logo smaller, turning, and transparent like the website's.

## What was built

`ui/` — TypeScript, no framework, built by Vite into one 30 kB HTML file
(`npm run build` → `ui/dist/gnarl-ui.html`).

- **Layout and buttons: the mock-up's.** Navy chips, solid blue when on,
  violet for the wobble and modulation.
- **Surfaces: the retired UI's "Dream" theme** — violet body lit from two
  corners, cyan knob arcs, glow only on what is active, value readouts
  always visible under every knob (dim at rest, lit while in use).
- **Corner mark:** the W as additive line art in `#64e6ff`, drawn like the
  site's figures, turning once every 9 s. Angle is a function of time, not
  frames. Still under `prefers-reduced-motion`.
- The panel is laid out at 1280×720 and **scaled, never reflowed**.
- The displays redraw from the controls every frame. The wobble moves the
  osc 1 picture and the vowel curve at the preview tempo (140 BPM), so the
  routing is visible before any audio.

## Scope and phone layout (second pass)

- **Scope beside MASTER**: a moving waveform of the output. Hold it (or use
  the keys A–K, or the touch keyboard) to play a note; it shows the note's
  name, and pitch shows as how many cycles fit a fixed 30 ms window. In the
  browser it draws a MODEL of the patch (oscillators, sub, the wobble on
  the cutoff, the FX that are on) shaped by the amp envelope
  (`ui/src/voice.ts`). It is not the engine's sound; inside the plugin it
  will read the real output.
- **Phone layout** (`phone()` in `ui/src/main.ts`), chosen when the window
  is under 760 px wide, or under 500 px tall and under 1000 px wide:
  - Upright: logo, master, preset and AI button, scope, five section tabs
    (OSC, FILTER, WOBBLE, MOD, FX), the section scrolling, and a two-octave
    keyboard (C1–C3) at the bottom under the thumbs.
  - Sideways: scope and keys on the left, sections on the right, two
    panels side by side where they fit.
  - Knobs are 46 px and buttons 32–40 px: finger size.
  - The same panels and the same state as the desktop, so a change on one
    layout shows on the other.

## Binding to the engine

`ui/src/params.ts` lists every control with the engine parameter it will
drive (`vital:` field). Every name there has been checked against
`src/common/synth_parameters.cpp`. Controls with `vital: null` have nothing
behind them yet:

| Control | Needs |
|---|---|
| OSC FM knob | Vital's FM is a warp MODE, not an amount; needs a real FM amount |
| OSC mode FOLD | not a Vital warp mode |
| SUB (all) | Phase 2 "clean mono sub" |
| VOWEL A–U buttons, wobble → VOWEL | Phase 2 vowel filter (Vital has `formant_x/y`, no vowel buttons) |
| WOBBLE DEPTH / SMOOTH / PHASE / SHAPE | the engine has per-route depths and a drawn shape; these are a front end over them |
| FOLD, CRUSH tiles | Phase 2 distortion chain (Vital has one distortion stage) |

Note: the wobble's existing "FM" route (`wobble_amount_fm`) targets
`osc_1_distortion_amount`, which this panel labels WARP.

## Running it inside the plugin — the open decision

The panel is a web page. Inside the plugin that means a web view, and the
vendored JUCE 6.0.5 has no parameter bridge for one; JUCE 8 does (the retired
engine used it). The path:

1. Move the fork to JUCE 8 + CMake with the engine unchanged, and prove it
   with byte-identical renders.
2. Host this page in a `WebBrowserComponent`; bind `params.ts` to host
   parameters by name.
3. Before step 2 is trusted: load a test build in FL Studio and Ableton
   (the producer, at home). A web view inside a DAW is the risky part.

JUCE 8's open licence is AGPLv3, which may be combined with GPLv3 (§13 of
both); it would be recorded in LICENSING.md.

The same page is the base for the mobile version: docs/design/phase2-09-mobile.md
(built: the page with the real engine inside, in WebAssembly).
