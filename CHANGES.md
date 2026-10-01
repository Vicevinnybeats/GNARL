# Changes from upstream

GNARL forked [Vital](https://github.com/mtytel/vital) at upstream commit
`636ca0e` - whose source is a single dump dated **2021-02-10** (every later
upstream commit edits only `README.md`). This file summarises what GNARL has
changed since, as GPLv3 §5(a) asks. The full record is the git history from
the merge commit forward: `git log 7651809..`.

## 2026-09-30 - Phase 1: fork and rebrand

**Build**
- `JUCE_VST3_CAN_REPLACE_VST2=0`. The VST2 SDK is not distributable and not
  present; JUCE's default fails the build on a missing VST2 header.

**Removed**
- The account system: `NO_AUTH=1` in every `AppConfig.h` and `.jucer`, which
  compiles out login, token refresh and preset download.
- `third_party/firebase_cpp_sdk` (137 MB) and every include path, library
  path and link flag that referred to it. `REQUIRE_AUTH=1`, which nothing
  read.
- Vital's logo artwork (`icons/vital_*.svg`) and its embedded copies in
  `BinaryData`.
- **Intel IPP** (`INTEL_IPP=1`, `UseIntelIPP`) from every Windows project.
  IPP is proprietary: upstream links it as the copyright holder, but a GPLv3
  binary distributed by anyone else may not include it. The FFT in
  `src/common/fourier_transform.h` falls through to JUCE's `juce_dsp` FFT,
  the same backend the Linux build already used. Render output is
  deterministic per platform; bit-identity *across* platforms is not
  claimed.

**Renamed**
- Product `Vial` → `GNARL` in every build project, bundle identifier and
  plugin name. Company `Vial Audio` / `Matt Tytel` → `Gnarl Audio` in the
  product metadata (copyright attribution in `debian/` is unchanged).
- Plugin identity: manufacturer code `Open` → `Gnrl`, plugin code `Vial` →
  `Gnr2`. **Frozen from here** - a DAW identifies the plugin by these, and
  changing them orphans every project that uses it.
- Contact metadata: the manufacturer email is now empty (it was upstream's
  personal address), and the website is `gnarl.vercel.app`.
- The wavetable export metadata string `wavetable (vital.audio)` →
  `wavetable (gnarl)`.

**Added**
- GNARL's marks, generated from the website's own stroke font
  (`tools/make_logo.py`) and embedded (`tools/embed_logo.py`), painted in the
  site's accent `#64e6ff` regardless of skin.

**Fixed**
- `Paths::vitalV()` (now `gnarlMark()`) read its SVG with the *ring's* byte
  count. Harmless upstream, where both files were 2658 bytes; with files of
  different sizes it would have truncated the mark.

**Deliberately not changed yet**
- Preset and file extensions (`.vital`, `.vitaltable`, `.vitalskin`,
  `.vitallfo`, `.vitalbank`) in `src/common/synth_constants.h`. Opening a
  producer's existing patches is a feature; whether GNARL *saves* under its
  own extension is decided with the preset pack.
- Internal identifiers containing "vital" (namespaces, class names). Not
  user-visible, and renaming them would make every future upstream
  comparison harder for no gain.

## 2026-09-30 - Phase 2 groundwork

**Renderer** (`src/headless/main.cpp`, `SynthBase::renderAudioToFile`)
- `--bits 24|32` (32 = IEEE float), `--block N`, `--save FILE`; length cap
  15 s → 60 s. Defaults unchanged.
- The render no longer depends on its own block size: exact pre-roll and
  final-block lengths, note-off split to the exact sample instead of the
  next block boundary, and a per-sample rather than per-block end fade.

**Real-time safety**
- `ScopedNoDenormals` at the top of `SynthPlugin::processBlock`,
  `SynthEditor::getNextAudioBlock` and the offline render. Upstream had no
  denormal protection on any platform.

## 2026-09-30 - Phase 2: the wobble macro (engine)

- New modulation source `wobble` (`WobbleModule`, `WobbleRate` in
  `lfo_module.{h,cpp}`): a transport-locked LFO with a drawable shape.
- New parameters, appended with `version_added` 0x010007 so they sort after
  every upstream parameter in the host: `wobble_rate`,
  `wobble_amount_wave_frame`, `wobble_amount_cutoff`, `wobble_amount_fm`,
  `wobble_bipolar`.
- Four fixed routes in `SynthVoiceHandler` (osc 1 and 2 wave frame, filter 1
  cutoff, osc 1 distortion amount), each a `ModulationConnectionProcessor`.
- `ModulationConnectionProcessor` gained a constructor taking its
  bipolar/stereo/bypass controls from outside. The existing constructor and
  the matrix path are unchanged.
- Presets save and load `settings.wobble_shape`.
- Renderer: `--start S` sets the transport position at note-on.
- `tools/vst3_probe.cpp --params` lists host parameters;
  `tests/host_parameters.txt` + `tools/check_param_order.py` fail CI if the
  order changes anywhere but the end.


## 2026-09-30 - Logo fixes (standalone, header)

- The standalone and tests projects had their own `BinaryData` still holding
  upstream's logo and icon references to deleted files; they did not compile.
  `tools/embed_logo.py` now rewrites every `*/JuceLibraryCode/BinaryData`,
  and `check_fork.py` checks all of them plus every `.jucer` icon reference.
- `gnarl_ring.svg` and `gnarl_mark.svg` are drawn in the shared 1701-unit
  logo frame, so the header's G is no longer a speck.
- The header logo button paints the pinned brand blue `#64e6ff` instead of
  skin colours.

## 2026-09-30 - Phase 2: the wobble macro (UI)

- `WobbleSection` (`lfo_section.{h,cpp}`): shape editor, RATE, POLARITY and
  the WT / CUTOFF / FM depth knobs.
- It is the first tab (WOBBLE) of the LFO area in `ModulationInterface`;
  LFO 1-8 follow it.

## 2026-09-30 - GNARL's own panel (browser build)

- `ui/`: a new one-page interface (the producer's layout, the retired UI's
  styling, a turning line-art logo). Runs in a browser; not yet in the
  plugin. See `docs/design/phase2-02-ui.md`.
- `ui/`: a scope beside MASTER that draws a moving waveform when a note is
  held, a touch keyboard, and a phone layout for upright and sideways use.

## 2026-09-30 - Renderer version fix; CMake + JUCE 8 renderer

- The headless renderer is named GNARL and versioned 1.0.6 like the plugin.
  It was "Vital" 99999.9.9, and the loader refuses patches newer than
  itself, so its saved patches would have opened in the plugin as init.
  `tests/patches/*.vital` re-stamped 1.0.6 (renders unchanged, checked).
- The renderer exits with an error when a named patch is missing or cannot
  be loaded (`src/headless/main.cpp`).
- `check_fork.py`: one version across every project, CMakeLists.txt and the
  committed test patches.
- `CMakeLists.txt`: the renderer builds with CMake on JUCE 8.0.9, fetched
  at a pinned tag. `tools/compare_renders.py` proves it bit-identical to the
  JUCE 6 build; CI runs the comparison. See docs/design/phase2-03-juce8.md.

## 2026-09-30 - The VST3 on CMake + JUCE 8

- `CMakeLists.txt`: the VST3 (and JUCE's Standalone wrapper, for tests)
  builds on JUCE 8.0.9, patched with `third_party/juce-patches/
  juce-8.0.9-gnarl.patch`, which carries forward Vital's changes to JUCE and
  two JUCE 8 fixes (see the README there). Compiler flags match the Projucer
  Makefiles exactly; MSVC gets the vcxproj's /O2 /fp:fast /GL.
- `third_party/juce-patches/vital-juce-6.0.5.diff`: Vital's changes to JUCE
  6.0.5, recorded.
- `src/interface/look_and_feel/open_gl_compat.h` (new): OpenGL calls
  through one macro that compiles on JUCE 6 and 8; 286 call sites in 19
  interface files use it.
- `fonts.cpp`: glyph warm-up through GlyphArrangement (JUCE 6 and 8).
- `tools/vst3_probe.cpp --render`: plays a note through a VST3 as a host
  would, optionally after host parameter changes; `tools/compare_plugins.py`
  compares two builds with it. CI builds the JUCE 8 VST3 on all three
  platforms and requires it identical to the JUCE 6 one on Linux.

## 2026-09-30 - GNARL's panel inside the plugin (JUCE 8 build)

- `src/plugin/web_panel.{h,cpp}` (new): the JUCE 8 plugin opens on GNARL's
  own panel (`ui/`), a web page in JUCE's `WebBrowserComponent`, bound to
  the host parameters. `ui/src/bridge.ts` and `ui/src/engine.ts` (new) are
  the page's side. ADVANCED shows Vital's editor; its logo comes back. See
  docs/design/phase2-04-web-panel.md.
- `synth_editor.{h,cpp}`, `synth_gui_interface.h`, `full_interface.cpp`,
  `synth_plugin.h`: the switch between the two views, and a parameter
  lookup for the panel. Compiled only where `GNARL_WEB_UI` is defined (the
  CMake build); the Projucer builds are unchanged.
- **Fix, both builds:** `SynthVoiceHandler::disableModSource` never disables
  the wobble. Vital's editor disabled any source without a matrix
  connection whenever the source's button was inactive or destroyed, which
  silenced the wobble macro when the editor was opened or closed.
- JUCE patch: two fixes to JUCE 8's Linux web view pipe (a page over 64 KB
  killed its WebKit process).
- `ui/`: fonts bundled rather than fetched from Google (`fonts.css`, SIL OFL
  1.1); ASCII-only output, checked by `inline.mjs`.
- CI: Node for the JUCE 8 jobs, WebKitGTK on Linux, a pinned WebView2 SDK on
  Windows.

## 2026-09-30 - Phase 2: the clean mono sub

- `SubOscillator` (`producers_module.{h,cpp}`, new class): a sine at the
  played note, 0/-1/-2 octaves, added to the voice's direct output, so it
  rides the amp envelope and bypasses every filter and effect. Identical
  in both channels; the phase resets on the note's own sample; optional
  tanh drive with RMS make-up. Wired in `SynthVoiceHandler::init`.
- Four parameters appended at `version_added` 0x010008: `mono_sub_on`
  (default off), `mono_sub_level`, `mono_sub_octave`, `mono_sub_drive`.
  Not Vital's `sub_*` names, which belong to its retired sub oscillator.
  `tests/host_parameters.txt`: 4 appended, 777 unchanged.
- `tests/test_sub.py` (new), in CI for both builds. `tools/vst3_probe.cpp`:
  `offset=N` starts the note mid-block. `tools/compare_plugins.py` includes
  the sub.
- The panel's SUB section is bound to the new parameters.

## 2026-09-30 - Phase 2: the vowel filter and the vowel wobble

- The wobble's fifth route: `wobble_amount_formant` (appended, 0x010009,
  default 0) modulates filter 1's formant X, built as the other four in
  `SynthVoiceHandler::connectWobbleRoutes`.
- `filter_section.cpp`: Vital's two formant style labels were swapped
  (measured: style 0 sounds A-I-U-O, style 1 A-O-I-E); the display now
  names what is heard. Stored values unchanged.
- Panel: the VOWEL FILTER follows filter 1's model (formant knobs in the
  formant model, cutoff/resonance/blend/drive otherwise); A E I O U set
  filter 1 to the measured formant position (`ui/src/vowels.json`) and
  switch it on; the VOWEL wobble destination is bound.
- **Fix (panel):** `store.refresh()` notified listeners as a user change, so
  the page sent every engine echo back - about 12,000 values a second while
  idle, each a host parameter change. In the panel since 7e53d2a.
- Tests: `tests/test_vowel.py` (new), `tests/test_wobble.py` checks 7-8,
  `ui/tests/bridge.test.mjs` (new; Chromium, fake plugin; in CI).
  `tests/host_parameters.txt`: 781 unchanged, 1 appended.
- See docs/design/phase2-06-vowel-filter.md.

## 2026-09-30 - Phase 2: the drive chain (DIST -> FOLD -> CRUSH)

- `DistortionModule`: two stages after Vital's drive stage in the
  distortion slot - FOLD (a second instance of Vital's Distortion
  processor, sine or linear fold, own drive and mix) and CRUSH (new: 1-16
  bits, sample hold 1x-64x). `ReorderableEffectChain` runs the slot while
  any stage is on; `distortion_on` switches the drive stage only.
- Seven parameters appended at 0x01000A (`distortion_fold_*`,
  `distortion_crush_*`), all off by default. `tests/host_parameters.txt`:
  782 unchanged, 7 appended.
- `tests/test_drive_chain.py` (new, in CI for both builds): FOLD alone is
  bit-identical to Vital's single fold stage; off is bit-identical to no
  keys; CRUSH quantises and holds as specified and is exactly block-size
  independent.
- The panel's FOLD and CRUSH tiles are bound. See
  docs/design/phase2-07-drive-chain.md.

## 2026-10-01 - Phase 3: measure and compare

- `tools/measure.py` (new): loudness (BS.1770-4), band power shares,
  stereo per band, level and brightness modulation spectra with the wobble
  rate in cycles per beat; JSON out. `tools/compare.py` (new): a reference
  against a JSON, a WAV or a rendered `.vital` patch, in numbers.
- `tests/test_measure.py` (new, in CI): known signals and GNARL renders,
  with each past analysis mistake run as a negative control.
- `references/` (new): measurements only; `.gitignore` refuses audio.
- `tools/check_fork.py` check 8: CI fails if any audio file is committed.
- See docs/design/phase3-01-measure.md.


## 2026-10-01 - Phase 2: the panel's last dimmed controls

- Osc FM knob (`osc_N_fm_amount`): Vital's FM law applied through the
  oscillator's phase-buffer hook, so it runs alongside a warp mode.
  `ProducersModule` orders oscillators for it; mutual FM hears the previous
  block.
- Osc FOLD (`osc_N_fold`): a switch that replaces the warp mode with a sine
  fold of the oscillator's output, driven by WARP.
- DIST TUBE (`distortion_tube`): an asymmetric tanh in place of the drive
  stage's curve, then a 10 Hz DC blocker.
- CRUSH SOFT (`distortion_crush_mode`): smoothstep between quantiser steps.
- Wobble SMOOTH and PHASE are parameters (`wobble_smooth_time`,
  `wobble_phase`), formerly constants. Default renders differ by -111.6 dB:
  Vital's exponential scale approximates `exp2`.
- Wobble -> FM (`wobble_amount_osc_fm`), a sixth fixed route to osc 1's FM
  knob. `wobble_amount_fm` keeps driving WARP.
- Panel: OTT 2-BAND binds Vital's "Low Band"; the mod matrix edits Vital's
  modulation connections (`gnarlRoute` / `gnarlRoutes`). No control is
  dimmed any more.
- Eleven parameters appended at 0x01000B. `tests/host_parameters.txt`: 789
  unchanged, 11 appended.
- Tests: `tests/test_fm.py` (new, in CI for both builds),
  `tests/test_wobble.py` 9-12, `tests/test_drive_chain.py` TUBE and SOFT,
  `ui/tests/bridge.test.mjs` matrix and flag choices.
- See docs/design/phase2-08-panel-controls.md.

## 2026-10-01 - Panel: pitch and mod wheels

- The panel has PITCH and MOD wheels: left of the keys on a phone, beside
  the scope on a desktop. They set Vital's `pitch_wheel` / `mod_wheel` and
  tell the engine the way Vital's own wheels do. PITCH springs back to the
  centre when let go, inside the same gesture, so a DAW records the return.
- `ui/tests/bridge.test.mjs`: the spring, its gesture order, and that MOD
  stays put (checked failing with the spring removed).

## 2026-10-01 - Phase 2: the mobile version

- The engine builds for the browser: `wasm/` (Emscripten; the same
  `src/synthesis` unity file, a small part of `src/common`, a host in
  `wasm/gnarl_web.cpp`, and shims for the few JUCE names the engine uses).
  No engine source changed.
- `ui/dist/gnarl-web.html`: the phone panel with the engine inside, in an
  AudioWorklet (`ui/src/web/`). Tap to play; every control works; the page's
  defaults are sent to the engine as the starting patch.
- `bridge.ts`: `isPlugin()` is false for the web engine (no ADVANCED); a
  connect answer may name the preset.
- The panel's page: no double-tap zoom or long-press menu on a phone;
  home-screen meta tags.
- Tests: `tests/test_web.py` (browser engine vs gnarl-render: -109 dB on init,
  every GNARL feature -81 dB or better, wheels, block size, speed),
  `ui/tests/web.test.mjs` (the page end to end in Chromium). Both in CI;
  the page is uploaded as `GNARL-mobile-web`. `tools/web_render.mjs` renders
  with the browser build.
- See docs/design/phase2-09-mobile.md and wasm/README.md.

## 2026-10-01 - Phase 7: the licence check in the fork

- `src/plugin/licence/`: the retired engine's licence policy and client,
  ported (CMake / JUCE 8 build only, `GNARL_LICENSING`). Enforcement is off
  until `-DGNARL_LICENCE_ENDPOINT` is given; every build today reports
  "Development build" and saves normally.
- The one gate: `SynthBase::presetSavingAllowed()`, asked by `saveToFile`.
  Audio and the host's project state are never gated.
- Changed from the archive: the timeout is enforced on the message thread (a
  hung verifier used to go unreported); the last successful check is saved
  with the key so the grace period survives a restart; Linux settings live
  in `~/.config/GNARL`.
- The panel: a licence chip with the banner's sentence and a key field
  (`gnarlLicence` / `gnarlLicenceKey`).
- `tests/licence_tests.cpp` (`gnarl_licence_tests`, 43 checks, in CI); the
  bridge test covers the chip.
- See docs/design/phase7-01-licence.md.

## 2026-10-01 - Mobile version: presets; the browser build's FFT

- The browser engine saves and loads patches in the plugin's format
  (`gnarl_save` / `gnarl_load` in `wasm/gnarl_web.cpp`); the phone page has a
  preset sheet (save, saved list, open a `.vital`, export, init) and its
  arrows step through saved patches (IndexedDB). Older-version patches are
  refused with the reason. The host and JSON code are built with WebAssembly
  exceptions, so a malformed file cannot trap the audio worklet.
- The browser build now carries JUCE's fallback FFT (`wasm/shim/
  juce_dsp_fft.h`), the one Windows and Linux run, instead of upstream's
  kissfft fallback, which is wrong and never ran anywhere. Wavetables now
  come out as on the desktop; every browser-vs-desktop figure improved (init
  -109 -> -138 dB).
- `tests/test_web.py`: presets both ways against the desktop renderer, and
  refusals; `ui/tests/web.test.mjs`: the preset sheet end to end.
- Panel: a new preset name with no curve no longer resets the wobble shape
  (a save under a new name).

## 2026-10-01 - Mobile version opens on the init patch

- The phone page now opens on the engine's init patch - Vital's single
  saw - as Vital and the plugin do, instead of sending the panel's riddim
  defaults at start; INIT returns to it. The web test checks that start
  sends no values.

## 2026-10-01 - Downloads on the website

- `.github/workflows/release.yml` (new): run by hand with a tag (or on a tag
  push), it builds `GNARL-Windows.zip` (VST3 + standalone), `GNARL-macOS.zip`
  (universal VST3, AU + standalone) and `gnarl-web.html` (the phone version),
  and publishes them as a GitHub Release. READMEs and notes in
  `docs/release/`; the process and the paid-download options in
  `docs/release.md`.
- `site/index.html`: WINDOWS and MACOS buttons link to the latest release's
  files; the phone button opens `/app`; the beta note and two FAQ answers
  that described the retired engine are corrected; a source-code link.
- `tools/assemble_deploy.mjs`: each site deploy puts the latest release's
  phone version at `/app` (the tombstone stays until there is one).
- The phone page's start screen names its licence and links the source.
