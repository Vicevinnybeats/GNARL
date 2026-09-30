# Moving the build to CMake + JUCE 8

**State:** stage 1 done — the offline renderer builds with CMake on JUCE
8.0.9 and is bit-identical to the Projucer / JUCE 6.0.5 build. The plugin
and standalone still build from the Projucer projects.

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

## Next stages

1. **The VST3 on CMake + JUCE 8**, with Vital's editor still in place:
   - the same render comparison through the plugin, on all three
     platforms;
   - `tools/vst3_probe.cpp` must report the same identity and the same
     777-parameter host order (`tests/host_parameters.txt`).
   Vital's OpenGL UI is the risky part of the port. JUCE 7 and 8 changed
   OpenGL context and font APIs.
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
