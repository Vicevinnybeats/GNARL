# GNARL — project context

A VST3 synthesizer for riddim and dubstep sound design, for FL Studio and
Ableton on Windows and macOS. Sound target: aggressive triplet growls,
formant wobbles, heavy FX.

**GNARL is a fork of [Vital](https://github.com/mtytel/vital)** (GPLv3) with
riddim-specific features added. It stopped being a from-scratch engine on
2026-09-30; that engine is preserved, unmodified, on the branch
`custom-engine-archive` at `222948c`.

**Read this file before writing any code in this repository.** The
real-time rules in §3 are not style preferences — violating one produces
crackles, dropouts or hangs in a customer's session.

---

## 1. Architecture

```
src/synthesis/     the audio engine. Almost entirely JUCE-free (2 of 147
                   files touch JUCE). namespace vital, Processor graph,
                   SIMD over voices via poly_float.
src/interface/     the OpenGL UI, skinned through look_and_feel/skin.{h,cpp}
src/common/        SynthBase, preset load/save (load_save.cpp), tuning,
                   synth_constants.h (file extensions live here)
src/plugin/        the VST3 entry point (synth_plugin.cpp)
src/standalone/    the standalone app
src/headless/      the offline renderer: preset in, WAV out

plugin/ standalone/ headless/ tests/
                   Projucer .jucer files AND their generated projects:
                   builds/vs17 (Windows), builds/osx (Xcode), builds/linux*
third_party/       JUCE 6.0.5 (modules ONLY), VST3 SDK, kissfft, json,
                   concurrentqueue
icons/  fonts/     UI glyphs, GNARL's marks, fonts

ui/                GNARL's own panel (TypeScript, one HTML file). Browser build;
                   the plugin still shows Vital's editor. See docs/design/phase2-02-ui.md
tools/             check_fork.py (CI guard), vst3_probe.cpp (host-style
                   loader), make_logo.py + embed_logo.py, site tooling
site/              the marketing site - separate program, NOT GPL
backend/           Cloudflare Worker for licence activation - NOT GPL
```

**What we forked is older than upstream's commit dates suggest.** Upstream's
newest commit is 2022, but 16 of its 17 commits edit only `README.md`: the
source is a single dump from **2021-02-10**, Vital 1.0.x. Every fix upstream
made after that is absent. When a bug looks like one Vital's binary doesn't
have, that is probably why.

## 2. Build

Verified on Linux (Ubuntu 24.04, GCC 13). Windows and macOS are built **only
in CI** — nobody develops on them here, so a green Linux build says nothing
about their project files.

```bash
# Linux dependencies. debian/control's list is INCOMPLETE: it lacks
# libcurl4-openssl-dev and libsecret-1-dev, and the second one fails at LINK
# time, after a four-minute compile.
sudo apt-get install -y mesa-common-dev libasound2-dev libfreetype6-dev \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev \
  freeglut3-dev libjack-jackd2-dev libgl1-mesa-dev libcurl4-openssl-dev \
  libsecret-1-dev

make vst3 CONFIG=Release              # -> plugin/builds/linux_vst/build/GNARL.vst3
make headless_server CONFIG=Release   # -> headless/builds/linux/build/gnarl-render

python3 tools/check_fork.py           # the CI guard; run before pushing

# Load the VST3 the way a DAW's scanner does, and ask it who it is
g++ -std=c++17 -I third_party/VST_SDK/VST3_SDK tools/vst3_probe.cpp -o /tmp/probe -ldl
/tmp/probe "$PWD/plugin/builds/linux_vst/build/GNARL.vst3/Contents/x86_64-linux/GNARL.so"

# Render a preset. GNARL flags: --bits 32 (float), --block N, --start S
# (transport position at note-on), --save F (write the patch). Cap: 60 s.
headless/builds/linux/build/gnarl-render --headless -o out.wav -l 4 -m C1 -b 140 patch.vital

python3 tests/test_wobble.py          # renders + measures the wobble macro

# The CMake + JUCE 8 renderer (stage 1 of docs/design/phase2-03-juce8.md),
# and the gate it must pass: bit-identical to the JUCE 6 build
cmake -B build-cmake -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --target gnarl_render
python3 tools/compare_renders.py headless/builds/linux/build/gnarl-render \
  build-cmake/gnarl_render_artefacts/Release/gnarl-render
```

**The renderer exits with an error if a named patch cannot be loaded.**
Upstream's ignored the failure and rendered the init patch with exit code 0.

**The renderer is deterministic**: rebuilt from clean and run again, the WAV
is byte-identical. That is what makes a comparison between two renders a
measurement of the *patch* rather than of the run — Phase 3 rests on it.

CI (`.github/workflows/build.yml`): `guard` gates everything; then Linux
(VST3 + renderer + host-style load + a render), Windows (`msbuild`,
toolset overridden v141 → v143), macOS (`xcodebuild`, ARCHS overridden to
`arm64 x86_64` — the project also asks for `arm64e`, which hosts do not load
for third-party code). Both desktop builds are unsigned.

**Building on a Mac with Xcode 15 needs `DEPLOYMENT_LOCATION=NO`**, as CI
passes. Upstream's project installs straight into `~/Library/Audio/Plug-Ins`
with `DSTROOT = /`, and Xcode 15 then treats the filesystem root as a build
directory containing its own output: "Cycle inside a single target". Copy
the bundle from `plugin/builds/osx/build/Release/` by hand.

**On Windows the project installs itself**: a post-build step copies
`GNARL.vst3` into `C:\Program Files\Common Files\VST3\`, which is where FL
Studio and Ableton scan. CI skips it (`/p:PostBuildEventUseInBuild=false`)
because that folder does not exist on a runner.

### Project files: two copies of everything

There is **no Projucer here** — the vendored JUCE is `modules/` only. So a
build setting lives in two places, the `.jucer` and the generated project,
and both must change together or a regeneration silently reverts one:

- `JUCE_VST3_CAN_REPLACE_VST2=0` and `NO_AUTH=1` are in the `.jucer`
  `defines` **and** every `AppConfig.h`.
- Adding a source file means editing the `.jucer`, the `.vcxproj` +
  `.filters`, the Xcode project and the Linux Makefile. This is why a CMake
  migration is planned; until then, prefer adding code to existing files
  where it genuinely belongs.
- `BinaryData.{h,cpp}` is Projucer output. `tools/embed_logo.py` rewrites
  the four logo entries in place; do not hand-edit the rest.

**Edit project files by LIST ITEM, never by line or by regex over XML.**
During the fork, deleting lines containing a path removed whole 900-char
flag lines; a quoted-token regex ate XML attributes; and a `;REQUIRE_AUTH`
pattern ate the `;` of the `&#10;` entity before it, leaving `&#10"`. Each
produced a file that looked edited and was broken. `check_fork.py` parses
every project file as XML for exactly this reason.

## 3. Real-time audio thread rules — hard constraints

`SynthPlugin::processBlock` and everything it calls, transitively, runs on
the audio thread. That thread has a hard deadline (at 48 kHz / 256 samples:
5.3 ms) and missing it produces an audible click. There is no "usually fine".

In Vital's terms, that means every `Processor::process(int num_samples)` and
`processWithInput`, everything `SoundEngine` drives, and
`SynthBase::processAudio`. Constructors, `setSampleRate`, and anything the
UI calls are not audio-thread code — but check which thread before assuming.

**Never, on the audio thread:**

| Forbidden | Why | Do instead |
|---|---|---|
| Allocate or free memory | `malloc` takes a global lock with unbounded latency | Size every buffer up front (constructor / `setSampleRate` / `kMaxBufferSize`) |
| `new`, `delete`, `std::vector::push_back`, `resize`, `juce::String`, `std::function` construction, `json` | All allocate | Pre-allocate; fixed-capacity containers; raw pointers |
| Lock a mutex, or `std::atomic` with a non-lock-free type | Priority inversion | Lock-free queue (`moodycamel::ConcurrentQueue`, as `SynthBase` does) or atomics on trivially-copyable types |
| Log, print, assert-with-message | IO plus allocation | Push a code into a lock-free queue; log from the message thread |
| File or network IO | Unbounded latency | Load elsewhere, hand over via a queue |
| Throw, or call anything that might | Unwinding is unbounded | Return error codes |
| Touch a `Component` or anything needing the message lock | Needs the message lock | Communicate via atomics / queues |
| Sleep, spin-wait, wait on a condition variable | Blows the deadline by construction | Never wait on the audio thread |
| A virtual call per sample in a hot loop, if avoidable | Prevents inlining and vectorisation | Vital already processes blocks of `poly_float`; keep per-sample work non-virtual |
| `NaN`/`Inf` or denormals | Denormals cost 100x; NaN poisons the whole path | Clamp feedback paths; see the gap below |

**How values reach the engine in Vital.** The UI enqueues
`vital::control_change` / `modulation_change` into
`moodycamel::ConcurrentQueue`s on `SynthBase`; the audio thread drains them
(`processModulationChanges`) at the top of the block. Enqueuing may allocate
— fine, it happens on the UI thread. Dequeuing on the audio thread does not.
New UI→engine paths use the same mechanism.

**Denormals are flushed** by `ScopedNoDenormals` at the top of
`SynthPlugin::processBlock`, `SynthEditor::getNextAudioBlock` and
`SynthBase::renderAudioToFile`. Upstream had none, and the compiler does not
add it: GCC 13 no longer links `crtfastmath.o` into a `-shared` library (the
built `GNARL.so` has no `set_fast_math`), and MSVC and Clang never did. Any
new entry point that runs the engine needs the same line.

A decaying tail does NOT decay to zero, so it cannot be detected by waiting
for silence: `tests/patches/fast_tail.vital` falls to about −695 dBFS and
then sits at **−693.5 dBFS indefinitely** — a rounding fixed point, and a
*normal* float, not a denormal.

**Always:**

- Clear the output before writing it. A synth must never pass through what
  the host left in the buffer.
- Handle `numSamples == 0` and `== 1`; hosts send both. Handle blocks larger
  than declared — `processBlock` already chunks by `vital::kMaxBufferSize`.
- Handle sample-rate changes at any time, mid-session.
- A parameter that jumps discontinuously clicks: smooth it.

### Lessons from the previous engine that still apply

The retired engine's `CLAUDE.md` (on `custom-engine-archive`) records each of
these with the bug that taught it. They are properties of DSP, not of that
codebase:

- **Test block-size invariance directly.** Render the same note in 32- and
  512-sample blocks; the samples must match. That one assertion found three
  separate bugs where state leaked across a boundary.
- **A stereo path is two paths.** One filter instance run over left then
  right interleaves the channels into one state.
- **A filter instance advances once per sample, no more.** An OTT crossover
  that reused one came out 24 dB down.
- **Shared modulation (one LFO for both channels) must advance once per
  sample, not once per channel.**
- **A drive control must not double as a volume control** — and the
  compensation that is right for a narrow drive range is wrong for a wide
  one. Match RMS on a reference, measured about the mean.
- **An insertion loss is a number to measure, not to assert in a comment.**
- **`prepare()`-style resets must not run on a live rate change**, and
  `juce::SmoothedValue::reset()` snaps to the target.
- **Measure level with broadband noise, not a single tone**, for anything
  multi-tap; a tone measures a comb null.
- **Use a Blackman-Harris window for spectral assertions.** Hann leakage at
  −48 dBc once passed for aliasing.
- **A test that passes with the bug put back is not a test.** Reintroduce the
  bug and watch it fail; `check_fork.py` was verified that way.

## 4. Fork rules

- **Plugin identity is frozen:** manufacturer `Gnrl`, plugin `Gnr2`, company
  "Gnarl Audio", bundle `audio.gnarl.synth`. A DAW identifies the plugin by
  these; changing them orphans every project. `Gnr2`, not the retired
  engine's `Gnr1`, so projects made with that engine show a missing plugin
  rather than loading into an unrelated parameter set. JUCE stores the codes
  as **hex** in `JucePluginDefines.h`, which a text search for the name will
  not find — that is how the first rename missed them.
- **The account system stays compiled out** (`NO_AUTH=1`). Never connect to
  upstream's services.
- **Never use "Vital", "Vital Audio", "Tytel" or "Matt Tytel"** in names or
  marketing. Keep every copyright header and `debian/` attribution intact.
  See [`LICENSING.md`](LICENSING.md).
- **Record every change** in [`CHANGES.md`](CHANGES.md) — GPLv3 §5(a).
- **The logo is the website's**: `tools/make_logo.py` reads the stroke-font
  glyphs out of `site/src/loader.ts`, colour `#64e6ff`, painted pinned (not
  from the skin) in `synth_section.h`. Change the glyphs there, regenerate,
  re-embed, in one commit.
- File extensions (`.vital`, `.vitaltable`, `.vitalskin`, `.vitallfo`,
  `.vitalbank`) are unchanged in `synth_constants.h`, deliberately: opening a
  producer's existing patches is a feature. Changing what GNARL *saves* is a
  preset-pack decision.

## 5. Parameters and presets

- Parameters are declared in `src/common/synth_parameters.cpp`.
- **Presets store every parameter keyed by its NAME** (`load_save.cpp`:
  `settings_data[control.first]`), and every modulation by source and
  destination name. So **a parameter name is frozen once any preset uses
  it**: renaming one silently drops that value from every saved patch.
  Append new parameters; never rename, reorder or remove.
- Old presets are migrated in `LoadSave::updateFromOldVersion`, keyed on the
  `synth_version` string. A change in a parameter's *meaning* needs a
  migration there.
- **Every build carries the plugin's version** (`plugin/gnarl.jucer`,
  1.0.6). `jsonToState` REFUSES a patch whose `synth_version` is newer than
  the program loading it. The renderer used to say 99999.9.9, so every patch
  it saved would have opened in the plugin as the init patch.
  `check_fork.py` enforces one version across every project, `CMakeLists.txt`
  and every committed test patch. Bump them together.
- **Host order is sorted by `(version_added, name)`** (`compareValueDetails`),
  not by position in the list. A new parameter must carry a `version_added`
  newer than every existing one (upstream's highest is `0x000803`; GNARL's
  first are `0x010007`), or it sorts into the middle of the host's list and
  moves every automation lane after it. `tests/host_parameters.txt` is the
  snapshot and CI fails unless it remains an exact prefix of the live list;
  regenerate it with `check_param_order.py --update` in the same commit that
  appends.
- Ranges: skew anything frequency- or time-like so the control feels right.
- **To route something to a parameter from inside the engine, use a
  `ModulationConnectionProcessor` and connect it the way
  `SoundEngine::connectModulation` does — every step.** The step that looks
  like a detail is the audio-rate switch: an `Output` reports control rate
  only if its buffer is one sample, so for an audio-rate destination the
  source and connection become audio rate. A re-implementation that skipped
  it matched the matrix to only −15 dB and stepped once per block. See
  `SynthVoiceHandler::connectWobbleRoutes`.
- **`utils::toInt` rounds to nearest-even**, it does not truncate. Never
  write `toInt(x + 0.5f)` to round; clamp the index after converting.

## 6. Modulation rate vocabulary

Every mod source and destination is one of three rates. Say which, in a
comment, whenever you add one.

- **Sample-rate** — recomputed per sample. Audio-rate FM, oscillator phase,
  filter coefficients under audio-rate modulation. Expensive; deliberate.
- **Note-rate** — once when a voice starts. Velocity, key tracking, per-voice
  random seeds, unison offsets.
- **Block-rate** — once per block, smoothed across it. Almost all UI
  parameters. The default; leave it only with a reason.

## 7. Sound verification (Phase 3)

Nobody working in this repository with an AI can hear. So:

- **Only call a sound "close" to a reference if the numbers say so**, and say
  which numbers. The producer listens and decides; the measurements are what
  make that conversation specific.
- **Never commit copyrighted audio.** Reference tracks are measured; only the
  measurements are committed. No audio, spectrum or wavetable is derived from
  them.
- Measure the instrument before trusting it. Past failures here: summing
  magnitudes instead of power (a sub balance reported as 12–21% was 79%); an
  envelope sampled at 8192-sample hops whose Nyquist made every track
  "wobble" at 1.2 Hz; autocorrelation that found the kick drum. Print the
  whole modulation spectrum, not an argmax.

## 8. Legal and licensing

Full detail in [`LICENSING.md`](LICENSING.md). The short version: GNARL is
GPLv3; customers receive the source and may redistribute it; the site and
backend are separate programs and are not GPL; presets shipped as separate
files can carry their own licence.

GNARL's own licence check (Phase 7, `backend/`):

- **It never silences the plugin.** If verification fails, audio keeps
  playing; saving and paid features disable and a banner appears.
- Offline grace period 30 days, not negotiable.
- Only an explicit valid/rejected answer is a decision; every error,
  timeout, non-200 or captive-portal page opens the grace period.
- The licence key lives in machine settings, never in a preset.
- What crosses the wire as a machine id is a SHA-256 of the device id.
- Given GPLv3, a customer can remove the check. It exists to make paying the
  easy path, not to prevent copying.

## 9. Phase status

| Phase | Scope | State |
|---|---|---|
| 0 | Plan: Vital structure, build, GPLv3, CI | **done** |
| 1 | Fork, rebrand, CI, this file | **done** — VST3 built on Windows, macOS (universal) and Linux in CI run 36698376877 |
| 2 | Riddim features, one at a time, design first | wobble macro: **engine done and tested**, UI tab done; new panel `ui/` (desktop + phone layouts) built in the browser, not yet in the plugin (`docs/design/phase2-02-ui.md`); JUCE 8 move: stage 1 (renderer) done and bit-identical (`docs/design/phase2-03-juce8.md`) |
| 3 | Render + compare tooling, reference measurement | not started |
| 4 | AI preset generation | **not to be started** |

Phase 2 candidates, with what already exists in upstream:

| Feature | Upstream |
|---|---|
| Tempo-synced wobble 1/4, 1/8, 1/8T, 1/16 on WT position, cutoff, FM | **engine done** (`tests/test_wobble.py`); UI panel next |
| Vowel/formant filter with morph | `formant_filter`, `formant_manager`, `vocal_tract` exist |
| Waveshaper / fold / bitcrush chain | `distortion.h` has all six modes; one stage, not a chain |
| OTT-style multiband | `MultibandCompressor` with upper+lower ratios exists |
| Clean mono sub under the growl | no dedicated sub path |
| Riddim preset pack | nothing; upstream ships no presets |
| Hardening: denormals | **done** — see §3 |

## 10. The marketing site

The site is unchanged by the fork and is still deployed by Vercel from
`site/` (`vercel.json` → `tools/assemble_deploy.mjs`). `/app` is a tombstone
page: the React web app that lived there was the retired engine's UI and is
on `custom-engine-archive`.

### The marketing site

[`site/`](site/) — **five pages**, one universe. Three.js + GSAP, built by
Vite to static files. See [`site/README.md`](site/README.md) for the page
table, the model drop-in slot and where to get models.

Four of the five are the **same shader walking a different slice** of one
continuous transformation — orbits, the oscillator's harmonic surface, a
double helix, a vortex, the interface's own rectangle — so arriving on
ENGINE picks up where HOME left off instead of resetting to something
unrelated. A page differs only by the `{ from, to }` it walks.

The fifth, FX, is a **dissolve**: a solid object burning away into its own
dust. The rack takes a signal apart, so the page's subject and its animation
are the same idea rather than an animation applied to a subject.

**Damping is a function of TIME, not of frames**, and this is the third time
this project has hit that mistake. `x += (target - x) * 0.075` advances 7.5%
*per frame*: half a second to settle at 60 fps, two seconds at 15 — and each
frame jumps 7.5% of whatever distance a fast flick opened. You see four or
five discrete positions and the page appears to cut between them. That is
what "it teleports when I scroll fast" was. Everything eases through
`1 - exp(-rate * dt)`, which is the same exponential sampled correctly and
composes exactly — the identical argument the meter ballistics in §3 rest
on. The other two in the family: the meter that read the block size, and the
text decode that counted frames.

**Additive light SUMS.** The per-particle alpha that looks right for one
particle is flat white for ninety thousand of them. Both the figure and the
dissolve's dust were rebuilt after a screenshot showed a white disc with the
copy floating on it, and the second time the cause was arithmetic rather than
taste: the mote fade window was 0.55 while the burn threshold stopped at
1.15, so the last motes released reached an age of only 0.27 and sat there at
near-full brightness. The bloom **threshold** is the number that matters, not
its strength.

**A figure made of soft sprites is a cloud, never a drawing.** Line art —
thin bright curves on black — is legible because the strokes are thin and the
gaps are empty; a dense field of sprites is fog. The figure is `LineSegments`.

**The dissolve samples its noise in OBJECT space.** In world space the
pattern is nailed to the room, so rotating the object makes the burn crawl
across it like a searchlight rather than like the thing itself decaying. The
particles read the *same* field at the *same* coordinates as the surface
shader — one shared GLSL string — so each mote lets go exactly when the
surface under it opens. A timer or a random stagger drifts out of step with
the hole it is supposed to be coming from.

**Model → particles: sample the TRIANGLES, not the vertices.** Vertex
positions give a cloud whose density maps how the modeller subdivided — dense
along detailed parts, empty across a large flat face. Points are scattered
across triangles picked with probability proportional to area, via a
cumulative table and a binary search (a loop per point over a hundred
thousand triangles freezes the tab). The uniform point inside a triangle
needs the `sqrt` on the first barycentric weight; without it the points
bunch at every centroid, which on a low-poly model is a visible dot per face.

**The default figure is generated, not downloaded** — a loudspeaker driver
as a lathe. On theme, and with no licence question, which §9 makes a
requirement rather than a preference: a downloaded mesh is an asset exactly
as a wavetable is. The model slot is opt-in through a `<meta>` tag so a
default build makes no request and logs no 404.

**The figure sits BESIDE the words, never behind them**, and the offset is
measured along the camera's own **right vector** as a fraction of the
frustum. Not world x: the camera orbits, so by the last page it is a hundred
degrees round the arc and world x has become depth — the figure obediently
stayed centred and sat on the copy, and every fix that read it as "not far
enough" made it worse somewhere else.

**On a phone there is no fixed band to aim at, because the card scrolls.**
Three attempts: aiming the camera up (which pitches the view and slides the
world *down*, the opposite of the intent), then raising the object into the
strip above the card at 40svh (which only exists at the top of the page — by
a third of the way down the object had left the viewport and the screen was
empty). What works is centred slightly high and scaled to about a quarter of
the frame, with the card passing over its lower part. A subject partly behind
the text reads as depth; a subject that leaves the screen reads as broken.

```bash
cd site && npm run build
(cd dist && python3 -m http.server 4174 --bind 127.0.0.1 &)
node ../tools/screenshot_site.mjs <output-directory>
```

That script drives **both sizes to six fractions of the one page's scroll** —
the interesting stops fall *between* sections, where no element sits, which
is why it scrolls by absolute offset rather than into view. It re-reads
`scrollY` in a separate `evaluate` to check where it landed, because
Playwright's phone emulation briefly reports a viewport four times too tall,
the browser clamps `scrollTop` against it, and an earlier version photographed
the wrong section entirely while the check passed.

**A fixed wait before a screenshot is a GUESS AT THE FRAME RATE, and it was
wrong.** The easing here is a function of time with the per-frame step
clamped at 0.1s — correct, so a backgrounded tab does not fling the camera
across the world on its first frame back — but it means that under software
GL, at a few frames a second, easing advances *at most 0.1s per frame*. The
tool's 2.6s wait therefore bought five frames of a two-second camera move,
and **every picture was of the camera still closing on its mark**. Two
compositional faults were diagnosed and "fixed" from those pictures before
the pictures themselves turned out to be the problem.

The scene now reports whether it has settled (`Journey.isSettled`) and the
tool asks. It also waits 700ms **before** asking, because `isSettled()` still
describes the previous stop until ScrollTrigger has fired — without that the
wait returns instantly on a stale `true`, which is the same mistake as
reading `scrollY` in the `evaluate` that set it.

**A composition is a fraction of the frustum, never a number of world
units** — and the journey broke this twice over. The camera's pull-back was a
constant 22 units, so how large a formation appeared was decided by whatever
radius that formation happened to be built at: the hero filled 45% of the
frame and everything after it 10–20%, so the page appeared to be running away
from the viewer. Each formation declares a radius now and the distance is
solved from it. The *panel's* declared radius is deliberately not its
bounding sphere: it is a wide flat rectangle, so framing its half-diagonal
against the frame's **height** left the width two thirds empty and the
instrument arrived as a postage stamp at the one moment the page asks you to
look at it.

**And the schedule that maps sections to objects is MEASURED.** It was
`scroll / 0.82`, a constant with nothing behind it; the sections actually
centre at 0, 0.244, 0.489, 0.733 and 0.977, so the interface was landing on
the FX section and the dissolve on PRESETS — every object one section early.
`boot.ts` measures the last section's centre, which also means it cannot
drift the next time a paragraph gets longer.

### The boot screen

`site/src/loader.ts` — a HUD dial that fills while the page loads, the
wordmark inside it, drawn **entirely in three.js**: no DOM text, no font
file, no image. Every glyph is a polyline from a small stroke font in that
file, and the percentage is seven-segment digits switched through a buffer
attribute rather than re-tessellated.

**The number is the real load**, from `THREE.DefaultLoadingManager` and the
stages either side of it, and it never reaches 100 before a frame has
actually been *drawn* — not merely before the objects exist, because the
first frame is where the shaders compile and on a slow machine that is the
longest pause of the load. `journey.ts` is reached through a **dynamic
import**, which is the whole reason the loader is worth having: a static one
puts the scene in the first chunk and the browser parses every byte of it
before a frame of the loader can be drawn.

**A clear colour is not a background.** `OutputPass` converts the frame to
sRGB on the way out, so `setClearColor(0x05030e)` — picked as a near-black —
came back out a visible violet-grey, several stops brighter than the site it
introduces. The backdrop is a CSS gradient on `#boot`; CSS is not in that
pipeline. And the first layout put all four elements at the origin, so the
bar ran through the dial's lower arc and the wordmark measured 0.735 across
inside a ring 0.60 wide. Both were invisible in the diff and unmissable in
the picture, which is §6's rule about this repository's other UI as well.

```bash
node ../tools/screenshot_loader.mjs <output-directory>
```

It throttles the connection through CDP, because on a local server the state
worth photographing lasts under a second. `screenshot_site.mjs` now has to
**click through the boot screen** before it can scroll, or every picture it
takes is of the loader — and it clicks *repeatedly* until the canvas is gone,
because the loader ignores a click until a frame has been drawn and the
`gn-booting` class comes off while the fade still has frames to run.

**The page is ASSEMBLED out of the boot screen, not cut to.** Clicking
through starts one arrival: the dial rushes past the camera, the camera
dollies in from 2.4× the framing distance, the figure's strokes fly in from
every side and converge — each along its own line, on its own slice of the
window, brighter while still travelling — and the header, the hero's lines
and the footer slide in from the edges they belong to.

**One clock drives all of it**, a clamped frame step in `boot.ts` — the same
step the loader's exit advances on. A CSS transition or a GSAP tween would be
a second easing on a different clock, and *this repository has now made that
mistake five times*: the meter that read the block size, the text decode that
counted frames, the scroll damping, the screenshot tool's fixed wait, and the
loader torn down on a 1400ms timer while its fade advanced per frame — which
left a half-faded boot screen frozen over a live page, permanently, because a
disposed renderer keeps whatever it last drew.

`isSettled()` waits on the figure's build as well as the dolly, since the
build finishes later. Reduced motion skips the build **in the 3D half as well
as the CSS half**.

**A phone runs out of FILL RATE here, not geometry** — the scene is a bloomed
full-screen composite. Three levers, in order: the pixel ratio caps at 1.25
(a phone reports 3, so even 1.5 rendered 2.25× the panel's pixels through a
chain touching each several times), the bloom runs at half resolution (its
output is entirely low frequency, so a half-size target is very nearly the
same picture for a quarter of the pixels), and MSAA is off (the bloom's blur
was buying most of what it was for).

---


## 11. Conventions

- **New engine and UI code matches Vital's style**, because it lives beside
  Vital's code and should read as one thing: 2-space indent, `snake_case`
  members with a trailing underscore (`last_distorted_value_`), `camelCase`
  methods, `k`-prefixed constants and enum values, `namespace vital` for
  engine code, `force_inline` for hot helpers. Not the JUCE style the
  retired engine used.
- `#pragma once`, not include guards.
- Every DSP constant says *why* it is that value.
- Keep upstream's copyright header on files derived from it; add a line
  noting GNARL's modification when a file changes materially.
- TypeScript (site): strict mode, no `any`, named exports.
