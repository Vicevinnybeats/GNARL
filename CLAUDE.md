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

macOS and Windows are the **shipping** targets.

**Windows needs the Microsoft.Web.WebView2 NuGet package**, and this is not
optional — without it JUCE compiles the native-integration API out of
`WebBrowserComponent::Options` (so `plugin/ui-bridge` does not build) and falls
back to the Internet Explorer engine, which cannot run the React bundle. See
[`docs/windows-setup.md`](docs/windows-setup.md). `NEEDS_WEBVIEW2 TRUE` and
`JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1` are both required and are set in
`plugin/CMakeLists.txt`; do not remove either.
 Linux builds (VST3 +
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

**Every filter here is zero-delay-feedback (topology-preserving), and that is
not a stylistic choice.** A bilinear biquad recalculated per sample is not
stable, so a filter built from one cannot be modulated at audio rate — and
audio-rate filter modulation is most of what makes a growl. `FilterTests` has a
case that sweeps cutoff every single sample across the full range, for every
filter type, to keep that property honest.

**A memoryless waveshaper always aliases**, because it generates harmonics
above Nyquist by construction. Nothing in `Saturation.h` is safe to run at the
base sample rate at high drive; the caller oversamples it.

**An oversampled oscillator must still band-limit to the BASE rate's
Nyquist.** Otherwise oversampling makes the oscillator brighter as well as
giving the nonlinear stages headroom — at 4x it would emit content up to
96 kHz, none of it audible after downsampling but all of it intermodulating in
the drive stage. See `WavetableOscillator::setOversamplingRatio`.

**A filter instance can be advanced once per sample and no more.** Calling one
twice in a sample with different inputs corrupts its state. The OTT crossover
did exactly that and its three bands cancelled rather than summed — measured
24 dB down. Each cascade path needs its own filters.

**And a STEREO path is two paths.** The same rule broke a second time, more
subtly: `Voice::renderFilters` ran one `FilterSlot` over the whole left
channel and then over the whole right, so the two channels interleaved into a
single filter's state. The output was then a function of the BLOCK LAYOUT
rather than of the signal — the same note rendered in 32-sample chunks and in
256-sample chunks differed by 5× through the formant filter — and a centred
patch came out with different left and right channels. Every component test
still passed, because every component was individually correct. The voice now
holds one `FilterSlot` per channel per slot, and
`tests/EngineTests.cpp` asserts block-size invariance and channel identity,
which is the only shape of test that can see this.

**Block-size invariance is a property worth testing directly.** If rendering a
note in 64-sample blocks and in 512-sample blocks does not give the same
samples, something is carrying state across a boundary it should not. That one
assertion caught a bug three layers below where it was looking.

**A drive control must not double as a volume control.** `DriveStage`
compensates by measuring the slope of the whole stage (pre-gain included) with
respect to its input. Measuring only the curve's own slope leaves the pre-gain
in place and gives drive +15 dB of level — which was a real bug here, and one
a loose test threshold hid. Note that an *even* curve (rectify) cannot have
unity RMS gain at all: it moves most of the signal's energy to DC, which the DC
blocker then removes.

**And the same fix does not transfer to a wider drive range.** `FxDistortion`
compensated the way `DriveStage` does — the slope at the origin — and
reproduced the bug with the sign flipped: every curve came out **12.5 dB
down**. The slope at zero is the *small-signal* gain, so normalising it makes
the linear region unity and leaves the loud part of the signal wherever the
curve's compression put it. Over the FX stage's 48 dB that region is one the
signal has already left. So `FxDistortion` matches **RMS** on a reference sine
instead (`kCompensationSine`, -12 dBFS), measured **about the mean rather than
about zero** — which is exactly what the DC blocker downstream leaves, and is
why the even curve needs no exclusion here even though it does in the filter.
A nonlinear curve has no single gain, so this is a level match at one
amplitude by construction; the residual at other levels is the compression the
user asked for.

**A shared LFO makes "process each channel in turn" wrong — a third time.**
The FX rack ran the whole left channel and then the whole right, which is
correct for per-channel *state* (each channel has its own filter, its own EQ
bands) and wrong for the one thing the modulated effects *share*: the chorus,
flanger and phaser advance a single LFO phase on channel 0 only, so that its
rate does not depend on the channel count. The left pass therefore advanced
the phase `numSamples` times and handed channel 1 the leftover value — the
right channel's modulation froze at whatever the block size determined.

Every component test passed. The chorus's own tests measure channel 0, or
measure that the two channels *differ*, which they emphatically did. It took
`FxDimension` in the same chain to make it visible at all, because only a
mid/side effect folds the broken right channel back into the left; the rack
then failed block-size invariance by 0.002 with **neither effect failing
alone**. `FxRack::runPerChannel` interleaves the channels now, and
`FxRackTests` asserts the invariance **bit-exactly** — a tolerance there would
have let this through, exactly as a loose threshold hid the drive stage's
+15 dB.

**A single tone is the wrong signal for measuring the level of a multi-tap
effect.** The hyper's voices are taps spread over 3 ms; at 220 Hz one cycle is
4.5 ms, so the taps land two thirds of a cycle apart and partly CANCEL.
Measured that way the level moved 7 dB with the voice count, and chasing it
produced three different normalisation models, none of which could be right —
the quantity being measured was a comb null, not a level. Broadband noise is
what the question means, because averaged across frequency the nulls and peaks
are both present, which is what a listener hears. With the taps' base delays
always fanned out (not scaled by detune, which made the coherence itself
setting-dependent) root *n* then holds to within 4 dB across two to eight
voices.

**And the number of notches, not their depth, is what a phaser's stage count
controls.** Probing eight frequencies said twelve stages notch *less* deeply
than two (11.6 dB against 17.4) — true, and not a bug: more stages means more
notches, each narrower, and sparse probes miss narrow ones. The claim that
holds is arithmetic: an N-stage all-pass chain sweeps its phase from 0 to
−Nπ and cancels against the dry path at every odd multiple of π, so there are
**N/2** notches. Counting them needs the whole spectrum, so `FxModulationTests`
takes the impulse response and counts local minima — measured exactly N/2 at
2, 4, 8 and 12 stages.

**A mid/side round trip is not bit-exact.** `(L+R)/2 + (L−R)/2` loses a unit in
the last place, so `FxDimension` turned fully down still altered every sample,
forever. Anything built in mid/side needs an early-out at zero rather than a
multiply by zero — with fourteen effects in a chain most patches do not use,
"off" has to mean untouched. Relatedly, the obvious widener (give each channel
an inverted delayed copy of the other) does *nothing* to a mono input: both
channels get the same copy subtracted and the outputs come out identical. The
effect has to be built where width lives — mid passed through, a delayed copy
of it added to the side with opposite signs — which also makes the mono sum
exactly the dry mid.

**A TPT filter's idle state converges on a rounding fixed point, not on
zero.** The FX EQ's state settles at exactly `-2e-37` after silence and stays
there, unchanged over ten million further samples. That value is a NORMAL
float, so there is no denormal to flush and `ScopedNoDenormals` never fires on
it — which is the good news, since it means no per-sample state-snapping is
needed in the hot path. The consequence worth remembering is the other one: an
idle effect **cannot** be detected by its output reaching exactly zero,
because it never does. `FxEqTests` measures the class of the idle tail rather
than asserting exact silence, which is the assertion that was actually true.
Note also that the suite has no denormal check for any other DSP unit, despite
§8 below claiming every unit gets one.

**`juce::dsp::FFT`'s inverse transform divides by the transform size.** So
building several differently-sized frames from one harmonic series gives each a
different amplitude. Undo it explicitly (`Wavetable::buildMipLevel`) or the mip
levels end up on different scales and a glide crossing a level boundary jumps
in volume.

**`prepare()` resets; a live rate change must not.** The oversampling factor
is a live parameter, so `Voice::setSampleRate` runs while notes are sounding.
Routing envelopes and LFOs through their `prepare()` there reset every one of
them to idle at level zero, silencing every held voice the moment the user
touched the oversampling control. `Envelope` and `Lfo` therefore have a
`setSampleRate` that changes the rate and nothing else. Same family as the
`SmoothedValue::reset` trap below.

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

**Two message-side threads is still a data race.** `WavetableLibrary::getTable`
was a plain check-then-act, which was fine while only the message thread
generated tables. Phase 5 added a background loader so a preset change does
not freeze the window, and then *both* threads could see a null, build the
table, and assign over each other's `unique_ptr`. Worse,
`getTableIfLoaded` is documented real-time safe and read that same owning
pointer — reading a `std::unique_ptr` another thread is assigning is undefined
behaviour however harmless it looks. Generation now takes a lock (message and
background only, **never** audio) and the audio thread reads a separate array
of atomics published with release ordering after the table is complete. It
segfaulted rendering the factory bank, intermittently, and **never once under
a debugger** — which is what a data race looks like.

That also forced a non-generating publish path: a patch change publishes what
is already built and asks the loader for the rest, because calling the
generating version would block on the very lock the loader holds while doing
that work — reintroducing the stall the loader exists to remove.

**Message-thread → audio-thread handover.** Anything larger than an atomic
(a wavetable, an LFO curve, a preset) is built on the message thread, published
through a lock-free swap, and the old object is freed on the message thread.
The audio thread never frees anything.

---

## 4. Parameter conventions

- The TypeScript choice lists in `ui/src/bridge/choices.ts` are guarded by
  `tests/ParameterMirrorTests.cpp` as well as the IDs. A drifted choice list
  fails exactly as silently as a drifted ID: the dropdown works, it just shows
  the wrong label for every saved patch.
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
- Visual direction: **a lit instrument.** Near-black surfaces with a gradient
  lift, exactly **one** neon accent per theme (Acid `#b4ff2e`, Ember
  `#ff5c1a`), and glow marking what is ACTIVE. Radii 3–5 px. This is a
  deliberate move from the flat matte look the project started with, on the
  client's direction and against concrete references.
- **"Dream" is the exception to the one-accent rule, and the only one.** The
  reference the client chose is a two-tone instrument — violet body, cyan
  controls — which a single accent cannot reproduce. So Dream declares a
  second accent (`--gn-accent-2`), and the rule becomes: one accent per theme,
  except Dream, which has exactly two. **Cyan is the control colour and violet
  the body colour, and they never swap roles** — every knob ring, every live
  value and every meter is cyan, so the eye still has one colour that means
  "this is live". Mixing the two per control is what turns a mood into a mess.
  `--gn-accent-2` defaults to `--gn-accent`, so a component can use it
  unconditionally and stays single-toned in the themes that only have one.
- **A canvas does not repaint when a CSS custom property changes.** Every
  control that draws itself reads its colours with `getComputedStyle`, which
  is right — one palette, one file — but it means a theme switch left the
  canvas controls on the old palette: the knobs stayed acid green on a violet
  instrument. Canvas controls that redraw on demand (`Knob`, `EnvelopeDisplay`)
  therefore take `useTheme()` as a render dependency; the ones that redraw every
  frame anyway (the wavetable display, the LFO editor) re-read the properties
  each frame and do not need it. Adding a canvas control means deciding which
  of the two it is.
- **Glow is not free.** Every `box-shadow` and every canvas `shadowBlur` is
  compositing work, in a webview inside a DAW that is already busy with audio.
  So: glow marks state, never decoration; the wavetable display glows one line
  rather than every line in its stack; and nothing that animates per frame
  glows except that one line.
- **Hover glow and state glow are different things, and only one is
  optional.** `--gn-glow-accent*` marks what is ON — which of fourteen effects
  are enabled, which chain row is selected — and that is information.
  `--gn-glow-hover*` says "the pointer is here", which the cursor has already
  said. The settings panel switches the second off and never the first: a UI
  that stops telling you what is enabled is broken, not calmer.
- **View settings live in `ui/src/settings.ts`, not in the ValueTree.** Theme,
  animations, hover glow, help text and knob travel belong to the person at the
  machine, not to the patch. In the ValueTree they would travel with a preset,
  so loading someone else's patch would turn your animations back on and
  sharing yours would push your accessibility preference onto them. They are in
  the webview's `localStorage`, wrapped in try/catch because it throws rather
  than returning null when an embedded webview blocks it, and `motion`/`glow`
  default from `prefers-reduced-motion`.
- **Both gates work by redefining TOKENS, never by overriding rules.**
  `:root[data-glow='off']` sets the hover-glow tokens to `none` in one place,
  and every rule that draws one keeps reading the same token and stops drawing
  it. An override beside each glow rule would have to be maintained in step
  with every new one, and would silently miss the next one added. Motion goes
  to `0ms` rather than `transition: none`, so a transition that is in flight
  when the setting changes lands on its target instead of snapping back.
- **`overflow: hidden` clips an absolutely positioned descendant when it is
  also the containing block.** The preset browser hangs off the header's
  preset group, which sets `overflow: hidden` to clip its buttons' rounded
  corners — so making that element `position: relative` to anchor the popover
  would have clipped the popover instead. The positioning context is a
  separate `.gn-preset-group` wrapper. Same family as the stacking-context
  trap below: a popover in a header has two different ancestors that can eat
  it, and neither is visible in code review.
- **A stacking context nobody declared put the settings popover behind the
  tab.** Dream gives `.gn-header` a `backdrop-filter`, and `backdrop-filter`
  **creates a stacking context** — so the popover's `z-index: 40` was trapped
  inside a header that then lost as a whole to `.gn-body`'s `z-index: 1`. The
  panel showed only its top edge and some ghost text through the panels, in the
  default theme only, and looked simply broken. `.gn-header` is now
  `position: relative; z-index: 2`.
- **How much a hover effect actually draws is MEASURED, like the artwork
  contrast.** The first hover glow — 38% and 22% alpha at −2px spread —
  produced a maximum channel delta of **7 out of 255** against the same frame
  with glow off. It was applying correctly, confined to exactly the hovered
  row, and invisible; two screenshots side by side did not show it either.
  Diffing them did. The shipped pair measures about **28**, which reads as a
  soft halo. `tools/screenshot_ui.mjs` shoots `hover-glow-on`/`-off` with a row
  actually hovered — the first version photographed the window with the pointer
  parked in a corner, where a hover effect draws nothing either way and the two
  pictures came out all but identical.
- **Row heights in a tab are explicit, not flex proportions.** The vertical
  budget at the design size is exact — 720 minus the 58 px header, 22 px
  status bar and 16 px padding leaves 624 px. Content-sized rows overflow
  that, and an overflowing child renders *on top of* its siblings. This bit
  three times: the oscillator's send row across the Sub and Noise panel
  headers twice, then the MOD tab's envelope panels straight across the mod
  matrix.
- **The structural half of that fix is `min-height: 0` on `.gn-panel`.** A
  grid or flex item's automatic minimum size is its *content* size, so a panel
  taller than its track grows past it — and the body's own `overflow: hidden`
  cannot help, because the body is sized by the panel rather than the other
  way round. The first two fixes only adjusted row heights; this is the one
  that makes the clipping actually happen. Size the rows to what the panels
  need anyway, and screenshot after any layout change.
- Density is a feature. The four tabs are the only nesting allowed.
- **Knob values are always visible**, dim at rest and bright while
  interacting. Hiding them until hover keeps a panel tidy and makes a dense
  synth unreadable: you cannot compare two knobs you have to hover one at a
  time.
- **A panel must clip its own content.** An overflowing flex child renders ON
  TOP of its siblings, which is how the oscillator's send row ended up drawn
  across the Sub and Noise panels. `.gn-panel__body` has `overflow: hidden`
  and the tab scrolls.
- The browser preview is seeded with the plugin's **real** parameter ranges and
  defaults, dumped by `tools/dump_parameter_defaults.cpp` from the actual
  parameter tree. Without it every knob sits at zero and every readout says
  "0%", which is useless for judging layout and misleading in a screenshot.
  **`parameterDefaults.json` goes stale silently**, and did: the FX section
  added 114 parameters and the file still held 316, so every FX knob in the
  preview sat at its minimum reading "0%" — exactly the symptom above, and it
  looked like a formatting bug in the new panels rather than a stale dump.
  Regenerate it in the same commit as any parameter change:
  `cmake -B build -DGNARL_BUILD_DEMO_RENDERER=ON && cmake --build build
  --target GnarlDumpDefaults && ./build/tests/GnarlDumpDefaults
  ui/src/bridge/parameterDefaults.json`.
- **Duplicated TypeScript is checked against C++ by `npm run check-reference`,
  and it runs on every build.** Two modules mirror engine logic on purpose —
  `bridge/formatters.ts` (the formatters in `ParameterRanges.h`) and
  `bridge/warp.ts` (a port of `WarpProcessor.h`) — because the UI must show
  what the engine does and the engine's copy is on the audio thread. Asking
  the plugin to format each value would be a bridge round trip per frame per
  knob.

  `tools/dump_reference_vectors.cpp` writes `ui/tests/referenceVectors.json`:
  every parameter at 21 points, every warp mode at 33 phases × 5 amounts.
  Regenerate it in the same commit as any change to either C++ side.

  **warp.ts claimed in its own header to be checked this way and was not** —
  no dumper, no reference file, no test, the same shape of false claim as §8's
  denormal check. Building the check immediately found real drift: the port
  had `remap` sharing a case with `phaseDistortion` at a bandwidth expansion
  of 4 where C++ gives **3**, and that number picks the mip level the
  wavetable display draws from — so the display was band-limiting remap
  differently from the engine and drawing a shape brighter than the one that
  plays.

  Two things the check taught about comparing across the boundary: the mirror
  must round to **float** at every step (`Math.fround`), because JUCE does
  this arithmetic in float and several formatters branch on an exact
  threshold — the envelope attack's midpoint is exactly 10 ms, where float and
  double fall on opposite sides of `value < 0.01f`. And a **phase must be
  compared circularly**: 0.9999999 and 0 are a ten-millionth apart, not almost
  a whole cycle, and comparing them linearly reports a difference of 1.0 for
  two values naming the same point.

  The formatter is **inferred** from the text C++ produced, not named — there
  is no way to ask a `juce::AudioProcessorParameter` which function it was
  given. That is safe only because the check verifies all 430 parameters at
  every sampled point. It caught the first inference immediately: `grain_size`
  runs to 500 and is formatted in *milliseconds*, so a rule of "max ≥ 1 means
  seconds" read 500 ms as 500 seconds.

### The background artwork

The Dream theme draws a full-bleed image behind the whole interface, embedded
in the binary like any other asset — no file IO at runtime, nothing to go
missing on a customer's machine. It is **one composited layer that never
animates**, which is why it is affordable where glow is not.

The slot is `ui/public/assets/backdrop.webp` — **WebP, not PNG**: the artwork
is a smooth colour field, which PNG stores losslessly at ~2 MB and WebP stores
at 78 KB with no visible difference through the scrim and blur it is drawn
under. It ships in every install. With no artwork present the slot resolves to
a placeholder (a 1×1 image, or an empty file from the CMake stub) and
`ui/src/bridge/artwork.ts` detects either and leaves the CSS variable unset, so
the app draws its procedural gradient instead — a finished look on its own,
which matters because the art is commissioned separately from the code.

The art always sits under a scrim. **Readability on a dense synth is not
negotiable**: it sets a mood, it does not compete with a 10px label.

**How bright the artwork may be is MEASURED, not judged by eye.** Screenshot
the UI, take the modal colour of a patch where labels sit (the glyphs are a
minority of pixels, so the mode is the background) and compute the WCAG
contrast ratio. `--gn-text-faint` is the binding constraint — it carries the
value readouts, and knob values are always visible here — and **4.5:1 against
the BRIGHTEST corner** is the bar, because that is where it fails first and it
is not where the eye goes. That measurement is what set the shipped numbers:
faint text measured 3.27:1 with the artwork in, moved to `#a591c2` for 4.75:1,
and `--gn-artwork-opacity` then went to 0.62 — as far as it goes before the
readouts drop back under. Brightening the art is not free, and the smallest
text on screen pays for it.

See [`docs/artwork-brief.md`](docs/artwork-brief.md) for the size and
composition constraints, the generation prompts, and the legal position on
generated art in a product we sell.

### The wavetable display

Draws the frames receding in Z with the active frame bright and forward, so
moving the position control reads as travelling *through* the table rather than
as one shape being swapped for another.

Fed **harmonics, not samples** (`tools/dump_wavetable_spectra.cpp` →
`ui/src/bridge/wavetableSpectra.ts`), so the display can synthesise the
waveform at exactly the width its canvas happens to be and re-synthesise it
when the warp changes. The spectra are analysed back *out of* the generated
tables, so what is drawn is what the oscillator plays, band-limiting included.

The drawn position is smoothed towards the parameter rather than tracking it
exactly, so a jumped value animates instead of teleporting — the movement is
most of the point.

`ui/src/bridge/warp.ts` is a **port** of `WarpProcessor.h`, because the display
has to show what the warp does and the engine's copy is in C++ on the audio
thread. Duplicated logic is a liability; it is checked against reference values
dumped from the C++ implementation so a one-sided change fails rather than
quietly drawing the wrong shape.

### Screenshotting the UI

```bash
cd ui && npm run build
(cd dist && python3 -m http.server 4173 --bind 127.0.0.1 &)
node ../tools/screenshot_ui.mjs <output-directory>
```

Worth doing after any layout change: the overflow bug above was invisible in
code review and obvious in a screenshot at the design size.

For motion, `tools/record_ui_motion.mjs` records a video by dragging the real
controls — so the waveform moves because the parameter moves, not because
something is animating for the camera.

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

## 7. Performance notes

Measured figures, so later work argues with data rather than intuition.

| What | Release | Debug |
|---|---|---|
| Generating one wavetable (256 frames, 11 mip levels) | **21.5 ms** | 316 ms |
| Generating all 20 factory tables | **0.43 s** | 6.3 s |

(Measured at the earlier 2048-sample geometry. The shipped table is 11264
samples per frame, so expect roughly 2.5x those figures; re-measure before
quoting them.)

| Memory | |
|---|---|
| One wavetable, all mip levels | ~11.5 MB |
| Resident at once (two oscillators) | ~23 MB |

| Aliasing (full-bandwidth saw, worst case over MIDI 12-120) | |
|---|---|
| Oscillator output | **-65.6 dBc** |

| Hard-clipped sine into the drive stage, worst inharmonic partial | |
|---|---|
| Oversampling off | -28.6 dBc |
| 2x | **-43.1 dBc** |
| 4x | -42.0 dBc |

Note that 4x is not better than 2x here. Past 2x the folded partials are
already below the oscillator's own interpolation floor, so the difference is
not the clipper's any more. The tests assert that 2x beats off and that 4x is
not WORSE, rather than asserting an ordering that the measurement does not
support.

**Always benchmark in Release.** Debug is ~15x slower here, because the cost is
almost entirely `juce::dsp::FFT`. A Debug measurement of DSP code is not a
slow version of the truth, it is a different shape of it, and acting on one
leads to optimising the wrong thing.

Consequences of the figure above:

- Factory tables are generated **lazily**, so instantiating the plugin does not
  pay 0.43 s (nor hold ~80 MB of tables no patch is using).
- A table switch costs ~21 ms on the message thread. Acceptable for a click,
  but Phase 5's preset loading should generate on a background thread and
  publish the result through a lock-free swap, per the handover rule in §3 -
  loading a preset that changes both oscillators' tables would otherwise stall
  the UI for ~40 ms.

---

## 8. Testing

- `tests/` links the plugin's **shared-code target** (`GNARL`), so tests
  exercise the same build of the processor the plugin ships, with the real
  `JucePlugin_*` defines.
- Every DSP unit gets: a finite-output test across all sample rates and block
  sizes in `kSampleRates`/`kBlockSizes`, a denormal check, and a parameter
  sweep asserting no NaN.
- Nonlinear stages additionally get an aliasing measurement (assert below
  −60 dBFS at 4× oversampling) and a THD+N measurement.
- **Use a Blackman-Harris window for any spectral assertion.** A Hann
  window's first sidelobe is only -31 dB down, so with a few hundred harmonics
  present its leakage fills the gaps between them at about -48 dBc. That is
  indistinguishable from aliasing, and it does not improve when the DSP
  improves — an earlier version of the oscillator test produced a confident,
  constant, entirely fictional -48 dB "aliasing" figure this way, and nearly
  bought a doubling of the table memory to fix a measurement artefact.
  Exclude at least two mainlobe widths around each real harmonic.
- The wavetable tests **measure** band-limiting with an FFT rather than
  asserting the code was called: `WavetableTests` checks that each mip level
  holds no more than −60 dB of energy above the harmonic count it claims, and
  that the level chosen for every MIDI note at 44.1 and 48 kHz is both safe
  (no harmonic above Nyquist) and not needlessly coarse.
- Filters get a stability test at extreme resonance/feedback.
- CI runs `pluginval --strictness-level 10` on the VST3 (macOS + Windows) and
  the AU (macOS). Strictness 10 is the bar; do not lower it to get green.
- `tests/CMakeLists.txt` turns `-Wfloat-equal` off for the test target only:
  tests compare exact floats on purpose (silence is exactly `0.0`, a state
  round trip must recall bit-identically). That is why the plugin target links
  `juce::juce_recommended_warning_flags` **privately** — propagating it would
  impose the warning on the tests.

---

## 9. Legal and licensing constraints

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

## 10. Phase status

| Phase | Scope | State |
|---|---|---|
| 0 | Repo skeleton, CMake, UI scaffold, CI, this file | **done** |
| 1 | Full parameter layout, voice architecture, smoothing | **done** |
| 2 | Oscillators (wavetable + graintable), filters, formant filter | **done** |
| 2b | OTT compressor (pulled forward from Phase 4 by request) | **done** |
| 6a | UI: primitives, OSC tab, FX tab, theme switching | **partial** |
| 6b | Animated wavetable display | **done** |
| 6c | UI: MOD tab — LFO editor, envelopes, matrix, macros | **done** |
| 3 | LFO engine, envelopes, mod matrix, macros | **done** |
| 4 | FX chain (14 instances, reorderable) + FX tab | **done** |
| 5 | Preset system (`.gnarl`), browser, morph, randomize | **done** |
| 6 | Full UI | not started |
| 7 | Backend, licensing, subscription | not started |
| 8 | AI features | not started |
| 9 | Release prep, installers, manual | not started |
| 10 | Marketing site (Three.js + GSAP sticky scroll) — see [`docs/website-brief.md`](docs/website-brief.md) | not started |

**The synth now makes sound.** Oscillators, sub, noise, send routing and both
filter slots are wired end to end, and `tests/EngineTests.cpp` drives the whole
plugin through `processBlock` with real MIDI to prove the pieces are actually
connected — which is a different failure from any one piece being wrong, and
the one that produces a synth that passes every unit test and is silent.

Oversampling is in: the whole voice section runs at 2x or 4x, because a
nonlinear stage aliases the moment it runs and no filter applied afterwards
can remove those partials.

To hear it without a DAW:

```bash
cmake -B build -DGNARL_BUILD_DEMO_RENDERER=ON
cmake --build build --target GnarlRenderDemo
./build/tests/GnarlRenderDemo <output-directory>
```

The modulation in those clips is applied per block from `tools/render_demo.cpp`,
because that tool predates the LFO engine. The engine itself now modulates in
32-sample chunks, which is the number that matters: a 256-sample block is
5.3 ms, and a 1/16 wobble at 140 BPM completes in 107 ms, so once per block is
about 20 steps per cycle and is audibly stepped. 32 samples is 0.67 ms, or
about 160 steps per cycle of that same wobble.

**The modulation state that is not a parameter** — the drawable LFO curves and
the mod slots' destination strings — lives in the ValueTree and reaches the
audio thread through `params::ModStateBridge`, which publishes into a rotating
set of three snapshots with an atomic index. The UI reads and writes it through
three native functions on the editor (`gnarlGetModState`, `gnarlSetLfoCurve`,
`gnarlSetModDestination`); everything else the UI touches is a parameter and
goes through a relay, so it keeps automation, undo and gesture handling.

**Live modulation reaches the UI by push, not poll.** `WebUIEditor` emits a
`gnarlModulation` event at 60 Hz with each LFO's value and phase and the
post-modulation table positions and cutoffs, and drops a frame identical to the
last one — so an idle editor costs no bridge traffic at all. The wavetable
display and the LFO editor's playhead read it from a ref inside their animation
loops rather than through React state, because a `setState` per frame
re-renders the whole tab sixty times a second. `ui/src/bridge/previewEngine.ts`
simulates the same frames in the browser preview, where there is no engine to
push them.

---

## 11. Conventions

- C++: JUCE style — 4-space indent, `PascalCase` types, `camelCase` members,
  a space before `(` in calls (matching JUCE's own sources so the codebase
  reads as one thing).
- `#pragma once`, not include guards.
- Everything in namespace `gnarl`.
- Prefer `constexpr` constants with a comment explaining the number over a
  bare magic value. Every DSP constant should say *why* it is that value.
- No `using namespace juce;` in headers.
- TypeScript: strict mode, no `any`, named exports.
