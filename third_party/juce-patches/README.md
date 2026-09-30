# JUCE patches

GNARL's editor is Vital's, and Vital shipped a **modified JUCE 6.0.5**. The
editor depends on those modifications, so moving to JUCE 8 means carrying
them forward. They are kept here as patches, applied by CMake to the fetched
pinned JUCE, so every change GNARL makes to JUCE is visible and reviewable
(and, for the GPL, identified).

| File | What it is |
|---|---|
| `vital-juce-6.0.5.diff` | Vital's changes: `third_party/JUCE` against the official 6.0.5 tag (LV2 wrapper excluded). A record, not applied. |
| `juce-8.0.9-gnarl.patch` | What CMakeLists.txt applies to JUCE 8.0.9 (`GNARL_JUCE_PATCH`). |

## What the JUCE 8 patch contains, and why

Each hunk is marked `GNARL` in the source.

**From Vital's JUCE 6.0.5, ported**

- **Per-component pixel scaling**, in `Component`, `ComponentHelpers`,
  `ScalingHelpers` and `ComponentPeer`. Vital's `FullInterface` lays its
  children out in physical pixels and returns the display scale from
  `getPixelScaling()`. Mouse positions and hit tests are converted through
  it. Without it, clicks land in the wrong place on every scaled display
  (Retina, Windows at 125–200%). Tested at scale 2: with the patch, the
  EFFECTS and MATRIX tabs open when clicked; without it, both clicks miss.
- `Component::setTopLeftPosition` virtual, plus TextEditor change
  notifications on scroll and focus (Vital draws text boxes in OpenGL).
- A modifier key pressed mid-drag reaches the dragged control at once.
- Slider drags are incremental, so the drag speed can change mid-drag.
- `Button::isMouseSourceOver` by position for every input source.
- No caret blinking.
- No re-centring while a window is resized.
- Windows: `DragLeave` delivers a drop (some hosts send it instead of
  `Drop`).
- WAV writer: the `clm ` chunk wavetable WAVs carry.

**New for JUCE 8** (it changed behaviour Vital relies on)

- `OpenGLTexture` pads to a power of two, as JUCE 6 always did.
  `OpenGlImageComponent::redrawImage` sizes its quad from `nextPowerOfTwo()`.
  With JUCE 8's exact-size textures, every label was drawn enlarged by
  that ratio and clipped.
- `OpenGLContext::updateViewportSize` measures the component relative to its
  window, as JUCE 6 did. JUCE 8's `getScreenBounds()` passes through the
  pixel scaling above and came out at 1/scale, which drew the frame into one
  corner at scale 2.

**Not ported**, because JUCE 8 already does it or GNARL doesn't need it:

- the VST3 controller null check (JUCE 8 checks where it dereferences);
- the extra OpenGL functions and constants (all in `juce::gl`);
- the OpenGL 3.2 context changes (Vital asks for 3.2 through the API);
- the splash-screen removal (JUCE 8 has none);
- the shape-button, bubble and callout shadows (Vital doesn't use them);
- the standalone-only and ARM-only changes (MIDI keyboard, device
  selector, macOS shutdown, curl, GLSL ES).

## Changing JUCE

1. Clone the tag.
2. Apply `juce-<tag>-gnarl.patch`.
3. Edit, keeping each file's line endings (JUCE mixes CRLF and LF).
4. Write `git diff` back to the patch file.
5. Build from clean and run `tools/compare_renders.py` and
   `tools/compare_plugins.py`.
