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
