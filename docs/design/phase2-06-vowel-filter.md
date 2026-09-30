# Phase 2, feature 3 — the vowel filter and the vowel wobble

**Status:** built and tested (2026-09-30). The producer has not heard it:
the measurements below say what it does, not whether it sounds good.

## What was wrong

The panel's VOWEL FILTER was bound to filter 1, and filter 1 defaults to
Vital's **Analog** model. In Vital's **Formant** model the filter reads its
own controls (`formant_x`, `formant_y`, `formant_transpose`,
`formant_resonance`) and ignores cutoff, resonance and drive
(`filter_module.cpp`, `formant_module.cpp`). So:

- **In Analog,** MORPH was bound to `formant_x`, which Analog never reads.
- **In Formant,** CUTOFF, RES and DRIVE were bound to controls Formant never
  reads.
- **The vowel buttons** had no binding at all.
- **The wobble** had routes to WT position, cutoff and FM. A cutoff wobble
  does nothing in the formant model, so the riddim "yoy" — a vowel moving
  on the grid — could not be made with the macro. The panel's VOWEL
  destination had no engine route.

## What it does now

**The panel follows filter 1's model.** Its corner names the model, and for
Formant the vowel set too ("FORMANT AOIE").

| Knob | Formant model | Any other model |
|---|---|---|
| CUTOFF | `filter_1_formant_transpose` (moves the vowel up or down) | `filter_1_cutoff` |
| RES | `filter_1_formant_resonance` | `filter_1_resonance` |
| MORPH | `filter_1_formant_x` (slides between vowels) | `filter_1_blend` (low → band → high pass) |
| DRIVE | none: dimmed, "Not in the formant filter" | `filter_1_drive` |

Pressing **A E I O U**:

- switches filter 1 on;
- sets it to Formant;
- sets the style and X/Y where that vowel is (`ui/src/vowels.json`).

A vowel lights when the engine is exactly there. After a MORPH it is
between vowels and none lights.

**The wobble's fifth route:** `wobble_amount_formant` modulates
`filter_1_formant_x`. It is built exactly as the other four
(`connectWobbleRoutes`). Appended at `version_added` 0x010009, default 0.
The panel's VOWEL destination drives it.

## Vital's formant labels are swapped

Vital's two formant styles put four vowels on the corners of the X/Y square.
Its editor calls style 0 "AOIE" and style 1 "AIUO". Rendering white noise
through each corner and reading the response (`tests/test_vowel.py`):

| Corner (x, y) | Style 0 | Style 1 |
|---|---|---|
| (0, 0) | A | A |
| (1, 0) | I | O |
| (0, 1) | U | I |
| (1, 1) | O | E |

So style 0 is **AIUO** and style 1 is **AOIE**. `formant_filter.cpp`
agrees: it maps style 0 to its A-I-U-O table. The editor's labels are fixed
(`filter_section.cpp`). The stored index is unchanged, so no preset sounds
different.

The buttons use style 1 for A, E, I and O, so MORPH moves within one square.
U exists only in style 0.

## Found on the way: the panel sent every echo back, forever

`store.refresh()` redraws a knob whose engine readout changed, and it told
listeners the change came from the user. So the bridge:

1. sent the value back;
2. `web_panel.cpp` echoed it;
3. the page refreshed and sent it again.

Idle, the page sent about **12,000 values a second**. In a DAW each is a
host parameter change. It also wrote a stale formant X over a vowel
halfway through the button's four messages, which is how it showed up.
This was in the JUCE 8 panel from its first commit (7e53d2a).

Under Xvfb the web view process fell from 130 % CPU to about 5 %.
`ui/tests/bridge.test.mjs` asserts that an idle page sends nothing. With
the bug put back it fails with 6,300 values in 0.5 s.

## Tests

- **`tests/test_vowel.py`:** each button's setting, rendered, produces its
  vowel, winning by 12–27 dB. The negative control: a map with A and E
  swapped is judged wrong for both.
- **`tests/test_wobble.py` check 7:** the vowel depth is bit-identical to a
  matrix connection `wobble → filter_1_formant_x`, at blocks 32 and 128.
  Pointed at `formant_y` instead, it fails at both.
- **`tests/test_wobble.py` check 8:** the vowel route changes the render
  (−0.1 dB difference against depth 0), so check 7 cannot pass on a route
  that does nothing.
- **`ui/tests/bridge.test.mjs`** (Chromium, fake plugin):
  - every vowel button's engine values, and the button it lights;
  - that a vowel switches filter 1 on;
  - rebinding when the engine changes the model;
  - no echo loop.
