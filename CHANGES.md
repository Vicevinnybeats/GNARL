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
