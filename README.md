# GNARL

A VST3 / AU / Standalone synthesizer for riddim and dubstep sound design.
Hybrid wavetable + graintable oscillators, a morphable formant filter, a
drawable tempo-synced LFO with first-class triplet rates, and a ten-slot FX
chain.

> **Status: Phase 0.** The plugin builds, loads in a host, and outputs silence.
> The voice architecture is Phase 1 and the sound engine is Phase 2. See the
> phase table in [`CLAUDE.md`](CLAUDE.md).

## Stack

| Layer | Technology |
|---|---|
| Audio engine | C++20, JUCE 8.0.4, CMake |
| Interface | React 19 + TypeScript + Vite, in `juce::WebBrowserComponent` |
| Backend | Next.js on Vercel, Supabase, Stripe *(Phase 7)* |
| Tests | Catch2 + `pluginval` strictness 10 |

## Requirements

Building an audio plugin needs a desktop toolchain — this cannot be built or
tested from a phone or a cloud shell.

- **macOS**: Xcode 15+ (for AU, universal binaries, and notarization)
- **Windows**: Visual Studio 2022 with the C++ desktop workload, **plus the
  Microsoft.Web.WebView2 NuGet package** — see
  [`docs/windows-setup.md`](docs/windows-setup.md). This is mandatory: without
  it there is no Windows UI.
- CMake 3.22+
- Node 20+
- A DAW to test in (FL Studio, Ableton, Bitwig, Reaper)

## Build

```bash
git clone https://github.com/Vicevinnybeats/GNARL.git
cd GNARL

# Configure (fetches JUCE 8.0.4 and Catch2 — first run takes a few minutes)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build VST3 + AU + Standalone, and install them to the system plugin folders
cmake --build build --parallel

# Run the test suite
ctest --test-dir build --output-on-failure
```

### Working on the UI alone

```bash
npm --prefix ui install
npm --prefix ui run dev     # http://localhost:5173, mock backend, no audio
```

JUCE's JavaScript frontend library is not published on npm; it is copied out of
the JUCE checkout by `ui/scripts/sync-juce-frontend.mjs`. Before the first
CMake configure, a dev-mode fallback with in-memory parameter state is used
instead. After configuring once, run `npm --prefix ui run sync-juce`.

## Layout

See [`CLAUDE.md`](CLAUDE.md) for the full architecture, the real-time audio
thread rules, and the parameter conventions. Read it before contributing.

## Licence

Proprietary. All rights reserved.
