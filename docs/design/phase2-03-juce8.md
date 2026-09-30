# Moving the build to CMake + JUCE 8

**State:** stages 1 and 2 done.
- The offline renderer and the VST3 build with CMake on JUCE 8.0.9 and are
  identical to the Projucer / JUCE 6.0.5 builds: sound, host identity and
  parameters.
- The editor has been checked at scales 1 and 2.
- The JUCE 6 plugin still ships until the JUCE 8 one has been loaded in FL
  Studio and Ableton.

## Why

- **The new panel (`ui/`) is a web page.** Running it inside the plugin
  needs JUCE 8's web view and its parameter bridge. JUCE 6.0.5 has neither.
- **One build description instead of five.** Today a new source file means
  editing the `.jucer`, the `.vcxproj` + `.filters`, the Xcode project and
  the Linux Makefile. Every broken build in the fork came from those copies.

## The rule for every stage

The engine's code does not change, so its output may not either.
`tools/compare_renders.py OLD NEW` renders five patches with both builds at
two block sizes in 32-bit float and compares every sample:

- the init patch
- every effect on
- a growl patch: warp modes, unison, noise, both filters and all three
  wobble routes
- the two tail patches in `tests/patches`

It exits non-zero unless all ten renders are bit-identical. CI runs it on
every push, then runs `tests/test_wobble.py` on the new build.

The comparison was checked against a renderer shifted by 10 ms. It reported
8 of 10 renders different and failed. The two it did not flag are the init
patch, which has nothing tied to the transport.

## Stage 1 result (2026-09-30)

- JUCE 8.0.9, GCC 13, the Projucer Makefile's Release flags (`-Ofast -flto
  -ffast-math -msse2 …`): **all 10 renders bit-identical**.
- `tests/test_wobble.py`: 0 failures on both builds.
- No engine source changed to compile against JUCE 8.

### Found on the way: the renderer's version refused its own patches

The first comparison differed by up to +24 dB. The cause was not JUCE: the
JUCE 6 renderer reported version **99999.9.9** (and project name "Vital"),
while the plugin is **1.0.6**. `LoadSave::jsonToState` refuses a patch newer
than the program loading it, and upstream's renderer ignored that failure.
It rendered the init patch instead and exited 0.

So every patch the renderer saved, including both committed test patches,
would have opened in the plugin as the init patch. Fixed:

- The renderer is GNARL 1.0.6 (headless `.jucer`, `JuceHeader.h`,
  Makefile), like the plugin.
- `tests/patches/*.vital` are stamped 1.0.6. Checked: the old renderer on
  the old patches and the new renderer on the new patches are
  bit-identical.
- The renderer exits with an error when a named patch is missing or
  refused.
- `check_fork.py` fails unless every project, the CMake project and every
  committed test patch carry the plugin's version. Tested by putting the
  bug back: it reported both halves.

## Stage 2 result (2026-09-30): the VST3 on JUCE 8

**Vital's JUCE was modified.** Diffed against the official 6.0.5 tag,
`third_party/JUCE` differs in 45 files. The editor depends on several of
those changes:

- the pixel scaling that makes mouse positions right on scaled displays;
- the OpenGL functions it calls.

They are carried forward as `third_party/juce-patches/juce-8.0.9-gnarl.patch`,
which CMake applies to the fetched JUCE. The README there lists every hunk
and why it exists.

**Source changes for JUCE 8**, each compiling on JUCE 6 as well:

- `open_gl_compat.h`: 286 OpenGL extension calls through one macro
  (`juce::gl` on JUCE 6.1+).
- A glyph warm-up that compiles on both versions.
- `JUCE_MODAL_LOOPS_PERMITTED=1` for the editor's blocking dialogs, which
  was JUCE 6's default.

**Checks**, all on Linux:

- `tools/compare_plugins.py` loads both VST3s as a host would (the probe's
  new `--render` mode):
  - identity and class IDs: identical;
  - 777 host parameters, in order, with IDs: identical;
  - a note through the VST3 wrapper on init: bit-identical;
  - the same after the host changes 12 parameters (every effect, a
    filter, the wobble routes): bit-identical.
- Renderer: all 10 comparisons still bit-identical; wobble tests pass.
- Editor at scale 1: the same picture as the JUCE 6 build.
- Editor at scale 2, simulated through JUCE's global scale factor, because
  Linux plugins never get a display scale:
  - the same picture as JUCE 6;
  - clicking EFFECTS and MATRIX opens them;
  - with the pixel-scaling patch removed, both clicks miss.

**Three problems found and fixed, each invisible in a successful build:**

1. **Compiler flags.** CMake's default `-O3` and JUCE's recommended flags
   overrode `-Ofast`, and with `-flto` the link flags choose the code. The
   plugin's reverb differed from the JUCE 6 build at −132 dB. The flags now
   match the Makefiles exactly (compile and link); the output is
   bit-identical.
2. **Texture sizes.** JUCE 8 stopped padding OpenGL textures to powers of
   two, and every label came out enlarged and clipped. The cause was
   narrowed step by step: the fonts measured 1% *smaller*, the rasterised
   text was identical, and all 234 labels requested identical sizes. Fixed
   in the JUCE patch.
3. **Viewport at scale 2.** JUCE 8 sizes the OpenGL viewport from
   `getScreenBounds()`, which the pixel-scaling patch halves for Vital's
   interface, so the frame was drawn into one corner. Fixed in the JUCE
   patch.

**Not verified here**, because no Mac or Windows machine is available:

- real Retina and Windows HiDPI;
- how the JUCE 8 plugin behaves inside FL Studio and Ableton.

CI builds the JUCE 8 VST3 for both platforms (`windows-juce8`,
`macos-juce8`) and uploads them for exactly that test.

## Next stages

1. ~~The VST3 on CMake + JUCE 8~~ - done, above. Remaining: the producer
   loads the `GNARL-windows-vst3-juce8` build in FL Studio and Ableton,
   including on a scaled display.
2. **The web panel in the plugin**: a `WebBrowserComponent` serving
   `ui/dist`, bound to host parameters by the names in `ui/src/params.ts`.
   The producer tests it in FL Studio and Ableton before anything else
   depends on it.
3. Remove the Projucer projects and `third_party/JUCE` 6.0.5 once CI builds
   all three platforms from CMake.

## Licence

JUCE 8 is dual-licensed AGPLv3 / commercial; JUCE 6 was GPLv3 / commercial.
GPLv3 and AGPLv3 may be combined (§13 of each). GNARL stays GPLv3 and the
JUCE parts carry AGPLv3's terms. Its network clause concerns software users
interact with over a network, which a desktop plugin is not. LICENSING.md is
to be updated in the same change that ships a JUCE 8 plugin.
