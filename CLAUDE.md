# GNARL — project context

A commercial VST3/AU/Standalone synthesizer for riddim and dubstep sound
design. Scope and polish target: Serum / Vital. Sound target: aggressive
triplet growls, formant wobbles, heavy FX.

**Read this file before writing any code in this repository.** The
real-time rules in §3 are not style preferences — violating one produces
crackles, dropouts or hangs in a customer's session, which is the single
fastest way to kill a plugin's reputation.

---

## 1. Architecture

```
gnarl/
├── plugin/                    # C++20 / JUCE 8 — the plugin itself
│   ├── source/
│   │   ├── dsp/               # oscillators, filters, FX, voices
│   │   ├── params/            # APVTS layout + ParameterIDs.h
│   │   ├── preset/            # .gnarl serialization + factory bank
│   │   ├── license/           # activation, offline grace period
│   │   └── PluginProcessor.{h,cpp}
│   ├── ui-bridge/             # WebBrowserComponent + parameter relays
│   └── CMakeLists.txt
├── ui/                        # React 19 + TS + Vite — the interface
│   ├── src/bridge/            # typed wrappers over JUCE's JS relay layer
│   ├── src/components/        # knobs, LFO editor, spectrum, mod matrix
│   ├── src/juce/              # GENERATED — JUCE's JS frontend, do not edit
│   └── dist/                  # built, embedded into the binary
├── backend/                   # Next.js on Vercel (Phase 7, not yet present)
├── cmake/                     # Dependencies.cmake, WebUI.cmake
├── tests/                     # Catch2 DSP + processor tests
└── docs/                      # preset format, manual, release process
```

**Audio engine** — C++20, JUCE 8 (pinned to tag `8.0.4` via FetchContent),
CMake ≥ 3.22.

**UI** — React + TypeScript rendered inside `juce::WebBrowserComponent`, built
by Vite to `ui/dist`, embedded with `juce_add_binary_data` and served from the
binary by `WebUIResourceProvider`. Nothing is written to disk at runtime.

**Backend** — separate Next.js app, Supabase (auth, presets, licenses), Stripe.
All AI inference is server-side.

---

## 2. Build

```bash
# Full build (configures JUCE, builds the UI, builds VST3 + AU + Standalone)
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel

# Tests
ctest --test-dir build --output-on-failure

# Skip the UI build (C++-only iteration; the last built bundle is reused)
cmake -B build -DGNARL_BUILD_UI=OFF

# UI on its own, in a normal browser with a mock backend
npm --prefix ui run dev
```

`COPY_PLUGIN_AFTER_BUILD` is on, so a successful build installs into the
system plugin folders and the DAW picks it up on rescan.

### Platforms

macOS and Windows are the **shipping** targets. Linux builds (VST3 +
Standalone, no AU) work and are useful for CI and headless test runs, but are
not a release target. On Debian/Ubuntu the build needs:

```bash
sudo apt-get install -y libasound2-dev libx11-dev libxext-dev libxinerama-dev \
  libxrandr-dev libxcursor-dev libxcomposite-dev libfreetype6-dev \
  libfontconfig1-dev libgl1-mesa-dev libcurl4-openssl-dev libwebkit2gtk-4.1-dev
```

JUCE attaches the webkit/GTK include paths to the plugin target privately, so
`tests/CMakeLists.txt` links `juce::pkgconfig_JUCE_BROWSER_LINUX_DEPS` and
calls `juce_link_with_embedded_linux_subprocess` itself. Without that, the test
target fails to find `gtk/gtk.h`.

### One-time UI setup note

JUCE's JavaScript frontend library is **not on npm** (there is no
`juce-framework-frontend` package). It lives inside the JUCE checkout at
`modules/juce_gui_extra/native/javascript/`, and
`ui/scripts/sync-juce-frontend.mjs` copies it into `ui/src/juce/` on every
`npm run build` / `npm run dev`.

Until CMake has fetched JUCE at least once, that script writes a **dev-mode
fallback** instead: the UI runs in a browser with in-memory parameter state and
no audio engine. That is fine for building components; it is never what ships.
After the first CMake configure, run `npm --prefix ui run sync-juce` to pick up
the real implementation.

The synced version **must** match the JUCE version the plugin links against —
the relay wire protocol is versioned with the framework.

---

## 3. Real-time audio thread rules — hard constraints

`processBlock` and everything it calls, transitively, runs on the audio thread.
That thread has a hard deadline (at 48 kHz / 256 samples: 5.3 ms) and missing
it produces an audible click. There is no "usually fine" here.

**Never, in `processBlock` or anything it calls:**

| Forbidden | Why | Do instead |
|---|---|---|
| Allocate or free memory | `malloc` takes a global lock with unbounded latency | Size every buffer in `prepareToPlay` |
| `new`, `delete`, `std::vector::push_back`, `resize`, `juce::String`, `std::function` construction | All allocate | Pre-allocate; pass `std::span`/raw pointers; use fixed-capacity containers |
| Lock a mutex, or `std::atomic` with a non-lock-free type | Priority inversion: the audio thread waits on a lower-priority thread | Lock-free FIFO (`juce::AbstractFifo`) or atomics on trivially-copyable types |
| Log, print, assert-with-message | IO plus allocation | Push a code into a lock-free queue and log from the message thread |
| File or network IO | Unbounded latency | Load in `prepareToPlay` or on a background thread, hand over via FIFO |
| Throw, or call anything that might | Unwinding is unbounded | Return error codes |
| Call anything on the JUCE message thread, or touch a `Component` | Needs the message lock | Communicate via atomics / FIFO |
| `std::this_thread::sleep`, spin-wait, or wait on a condition variable | Blows the deadline by construction | Never wait on the audio thread |
| Call a virtual function in a per-sample inner loop, if avoidable | Prevents inlining and vectorisation | Templates / CRTP for the hot path |
| Read a parameter by string ID | Hash lookup per block | Cache `std::atomic<float>*` in the constructor |
| `NaN`/`Inf` or denormals | Denormals cost 100x on some CPUs; NaN poisons the whole signal path | `juce::ScopedNoDenormals` at the top of `processBlock`; clamp feedback paths |

**Always:**

- `juce::ScopedNoDenormals` as the first line of `processBlock`.
- All allocation, `reset()` and `prepare()` in `prepareToPlay`.
- Clear the output buffer before writing to it. A synth must never pass
  through what the host left in the buffer.
- Handle `numSamples == 0` and `numSamples == 1`. Hosts send both.
- Handle sample-rate and block-size changes at any time — `prepareToPlay` can
  be called repeatedly, mid-session.
- Assume `processBlock` can be called before `prepareToPlay` (it shouldn't be,
  but broken hosts exist). Never dereference a buffer sized there without a
  null/size check.

**Parameter reads.** Cache the raw `std::atomic<float>*` from
`APVTS::getRawParameterValue` in the constructor. Wrap every user-facing value
in a `SmoothedValue` — a parameter that jumps discontinuously clicks.
`dsp::SmoothedParameter` does both; `dsp::ramp` holds the ramp-time policy and
says why each value is what it is.

**`juce::SmoothedValue::reset()` snaps the current value to the target.** So
the obvious `reset (sr, newRamp); setTargetValue (x);` teleports the value
before ramping — exactly the discontinuity the smoother exists to prevent. To
change a ramp length mid-flight, save `getCurrentValue()`, reset, restore it
with `setCurrentAndTargetValue`, then set the new target. See
`setRampPreservingValue` in `dsp/Voice.cpp`.

**Voices render into their own float buffer**, whatever precision the host
asked for, and the processor sums that into the output. The signal path is
voices → mix → filters → FX → output. `renderVoices` chunks internally, so a
host handing over more samples than it declared in `prepareToPlay` cannot
overrun the mix buffer or force an allocation on the audio thread.

**Message-thread → audio-thread handover.** Anything larger than an atomic
(a wavetable, an LFO curve, a preset) is built on the message thread, published
through a lock-free swap, and the old object is freed on the message thread.
The audio thread never frees anything.

---

## 4. Parameter conventions

- **IDs live only in `plugin/source/params/ParameterIDs.h`.** No string
  literal parameter ID appears anywhere else in the C++ codebase.
- Both `ParameterIDs.h` and its TypeScript mirror `ui/src/bridge/parameterIds.ts`
  are **generated** — regenerate them rather than hand-editing, and commit both
  in the same commit as the C++ change. A mismatch fails silently, because the
  relay simply never connects; `tests/ParameterMirrorTests.cpp` fails the build
  if they disagree.
- A mod slot's **destination is not a parameter**. A host parameter is a
  number, so a destination would have to be an index into an ordered list of
  targets — and that index shifts the moment the list changes, silently
  repointing every saved preset's modulation at the wrong parameter.
  Destinations are parameter-ID **strings** in the plugin's ValueTree. A mod
  slot's depth, curve and enable *are* parameters, because those are worth
  automating.
- `pid::kParameterVersionHint` is **not** `kStateVersion` and must never
  change: JUCE folds the hint into the AU parameter ID, so bumping it
  invalidates every AU automation lane a customer has drawn. They are separate
  constants so that a preset migration cannot take AU automation down with it.
- Choice-list **order is frozen** too (`ParameterChoices.h`). A preset stores
  the chosen index, not the name, so inserting an entry in the middle changes
  the meaning of every existing preset that used a later entry. Append only.
- Parameter **creation order** in `ParameterLayout.cpp` is frozen: it sets the
  index a host shows in its automation list.
- ID format: `snake_case`, scoped by section.
  `osc1_table_pos`, `filter2_cutoff`, `lfo3_rate`, `mod_slot7_depth`,
  `fx_slot2_wet`, `env1_attack`, `macro1`.
- **IDs are frozen once shipped.** Renaming one breaks every saved preset and
  every host automation lane pointing at it. To retire a parameter, leave the
  ID declared and stop reading it.
- Display names are Title Case and may change freely; IDs may not.
- Declare the parameter now even if the feature lands three phases later. A
  parameter added after release cannot be inserted without breaking preset
  compatibility, so the full layout is declared up front in Phase 1.
- `pid::kStateVersion` is bumped only when the *meaning* of an existing
  parameter changes. Migration goes in `setStateInformation`.
- Ranges: use `NormalisableRange` with a skew for anything frequency- or
  time-like, so the knob feels right rather than merely covering the range.

---

## 5. Modulation rate vocabulary

Every mod source and destination is one of three rates. Say which, in a comment,
whenever you add one.

- **Sample-rate** — recomputed per sample. Audio-rate FM, oscillator phase,
  filter coefficients when audio-rate modulated. Expensive; use deliberately.
- **Note-rate** — computed once when a voice starts. Velocity, key tracking,
  per-voice random seeds, unison detune offsets.
- **Block-rate** — computed once per `processBlock` and smoothed across it.
  Almost all UI parameters, control-rate LFOs, envelopes feeding non-audio
  destinations. This is the default; only leave it when you have a reason.

---

## 6. UI conventions

- The web UI never blocks on C++. Every control updates its own state on the
  pointer event and writes to the relay fire-and-forget.
- Visualiser data (spectrum frames, wavetable frames, meters) travels as
  **binary `ArrayBuffer`s at ≤ 30 fps**, never as JSON strings. JSON at 60 fps
  stutters the UI.
- Heavy rendering runs on `requestAnimationFrame`, paused when the window is
  hidden.
- Drag gestures wrap in `sliderDragStarted()` / `sliderDragEnded()` so host
  automation records one gesture rather than hundreds of writes.
- Vite output filenames are **stable and unhashed** (`index.html`,
  `assets/index.js`, `assets/index.css`). The C++ resource provider resolves
  them by name at compile time and cannot follow a content hash. The list is
  duplicated in `cmake/WebUI.cmake` (`GNARL_UI_FILES`) and
  `ui/vite.config.ts` — keep them in sync.
- Visual direction: near-black base `#0a0a0c`, panels `#141418`, hairline
  borders `#242430`, exactly **one** accent per theme (Acid `#b4ff2e`, Ember
  `#ff5c1a`) used only for active/modulated state. Radii 2–4 px. No soft
  shadows, no glass, no pastel. It should read as hardware, not a dashboard.
- Density is a feature. The four tabs are the only nesting allowed.

**Content Security Policy.** `ui/index.html` sets `script-src 'self'` with no
`unsafe-eval`. JUCE's `check_native_interop.js` contains a direct `eval`, which
Vite warns about on every build — it is reached only on Android, behind a
`getAndroidUserScripts` guard, and GNARL does not target Android. Do not add
`unsafe-eval` to silence the warning.

**The binary-data name mapping is load-bearing.** `juce_add_binary_data`
mangles `assets/index.js` into the symbol `index_js`, and
`WebUIResourceProvider` reproduces that mangling by hand. If the two drift, the
plugin builds, loads, and shows a blank window with nothing in any log.
`tests/WebUIBridgeTests.cpp` exists to catch that; do not delete it.

---

## 7. Testing

- `tests/` links the plugin's **shared-code target** (`GNARL`), so tests
  exercise the same build of the processor the plugin ships, with the real
  `JucePlugin_*` defines.
- Every DSP unit gets: a finite-output test across all sample rates and block
  sizes in `kSampleRates`/`kBlockSizes`, a denormal check, and a parameter
  sweep asserting no NaN.
- Nonlinear stages additionally get an aliasing measurement (assert below
  −60 dBFS at 4× oversampling) and a THD+N measurement.
- Filters get a stability test at extreme resonance/feedback.
- CI runs `pluginval --strictness-level 10` on the VST3 (macOS + Windows) and
  the AU (macOS). Strictness 10 is the bar; do not lower it to get green.
- `tests/CMakeLists.txt` turns `-Wfloat-equal` off for the test target only:
  tests compare exact floats on purpose (silence is exactly `0.0`, a state
  round trip must recall bit-identically). That is why the plugin target links
  `juce::juce_recommended_warning_flags` **privately** — propagating it would
  impose the warning on the tests.

---

## 8. Legal and licensing constraints

- Ship **only** wavetables we generated or hold a commercial license for. Never
  import tables from Serum, Vital, Massive, Malström or any commercial product.
- Match Serum's *information architecture* (a category convention, free to
  use). Never its artwork, colours, knob art, panel textures or typography.
- The license check never silences the plugin. If verification fails, audio
  keeps playing; preset saving and AI features disable and a banner appears.
  Offline grace period is 30 days and is not negotiable — a producer in a
  studio with no wifi must not be locked out mid-take.
- License verification runs on a background thread with a timeout. It never
  touches the audio thread and never blocks the UI.

---

## 9. Phase status

| Phase | Scope | State |
|---|---|---|
| 0 | Repo skeleton, CMake, UI scaffold, CI, this file | **done** |
| 1 | Full parameter layout, voice architecture, smoothing | **done** |
| 2 | Oscillators (wavetable + graintable), filters, formant filter | not started |
| 3 | LFO engine, envelopes, mod matrix, macros | not started |
| 4 | FX chain (10 slots) | not started |
| 5 | Preset system (`.gnarl`), browser, morph, randomize | not started |
| 6 | Full UI | not started |
| 7 | Backend, licensing, subscription | not started |
| 8 | AI features | not started |
| 9 | Release prep, installers, manual | not started |

Phases 0-1 ship a plugin that loads in a host, responds to MIDI, allocates and
steals voices correctly, and outputs **silence**. That is intentional: the
sound engine is Phase 2.

---

## 10. Conventions

- C++: JUCE style — 4-space indent, `PascalCase` types, `camelCase` members,
  a space before `(` in calls (matching JUCE's own sources so the codebase
  reads as one thing).
- `#pragma once`, not include guards.
- Everything in namespace `gnarl`.
- Prefer `constexpr` constants with a comment explaining the number over a
  bare magic value. Every DSP constant should say *why* it is that value.
- No `using namespace juce;` in headers.
- TypeScript: strict mode, no `any`, named exports.
