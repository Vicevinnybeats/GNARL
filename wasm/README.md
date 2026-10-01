# wasm/ — the engine for the browser

GNARL's engine compiled to WebAssembly, for the mobile version
(docs/design/phase2-09-mobile.md). It is the same C++ as the plugin's
`src/synthesis/`, built with Emscripten instead of a desktop compiler.

```bash
git clone --depth 1 https://github.com/emscripten-core/emsdk.git /tmp/emsdk
/tmp/emsdk/emsdk install latest && /tmp/emsdk/emsdk activate latest
source /tmp/emsdk/emsdk_env.sh

wasm/build.sh                        # -> wasm/build/gnarl.wasm (about 1 MB)
(cd ui && npm run build)             # -> ui/dist/gnarl-web.html, engine inside
python3 tests/test_web.py            # the browser engine against gnarl-render
(cd ui && node tests/web.test.mjs)   # the page, in Chromium, end to end
node tools/web_render.mjs patch.vital -o out.wav -l 2 -m C2 -b 140
```

## What is in here

| File | What it is |
|---|---|
| `build.sh` | Compiles `src/unity_build/synthesis.cpp` (the whole engine, as the desktop builds it), `common_web.cpp` and `gnarl_web.cpp`; links a standalone module with no JS glue |
| `common_web.cpp` | The part of `src/common/` the engine needs: parameters, LFO shapes, modulation connections, the wavetable creator for the init wavetable |
| `gnarl_web.cpp` | The host: what `SynthBase`, `ValueBridge` and `WebPanel` do for the plugin, as `gnarl_*` C exports |
| `shim/JuceHeader.h` | Stands in for JUCE. The engine uses it for a leak-detector macro, the standard headers it pulls in, and base64 for samples in presets |
| `shim/load_save.h` | Stands in for `LoadSave`: the three helpers `wavetable_creator.cpp` uses to migrate old wavetable JSON |
| `shim/juce_dsp_fft.h` | JUCE's fallback FFT, the one Windows and Linux run, so wavetables come out the same (below) |

The JavaScript side is in `ui/src/web/`: `engine-core.js` wraps the exports,
`worklet.js` runs them in an AudioWorklet, and `host.ts` connects the page.

## What is different from the desktop builds

- **No JUCE.** The engine does not need it, and JUCE does not target
  WebAssembly. What the engine did take from it is in `shim/`.
- **JUCE's FFT, copied** (`shim/juce_dsp_fft.h`): the desktop runs JUCE's
  fallback FFT on Windows and on Linux, and the browser must too. Vital's
  spectral wavetable morph is sensitive to the FFT's rounding: a keyframe's
  near-silent bins have phases that are rounding noise, the morph
  interpolates from them, and the loudest morphed frame sets the whole
  table's normalisation. With a different FFT (kissfft, the fallback in
  `fourier_transform.h`) a custom wavetable came out 0.3 dB louder; with
  this one, every check in `tests/test_web.py` improved (init −109 → −138
  dB). That kissfft fallback is also wrong: it applies IPP's packed-format
  fix-up to interleaved output. No build uses it, and it is left as
  upstream's.
- **musl's maths library**, not glibc's: `atan2`, `sin` and `cos` round
  differently in the last bit. In a spectral morph that reaches the same
  noise phases, so a wavetable morphing spectrally between very different
  keyframes differs by a uniform gain of about 0.01 dB, and its swept
  frames by −41 dB once that gain is matched. Morphing in the time domain
  is exact (−131 dB). Vital on macOS (vDSP) and Windows already differ from
  each other the same way.
- **libc++**, not libstdc++. `std::uniform_real_distribution` is
  implementation-defined, so random oscillator phases differ from the
  desktop's for the same seed. Everything deterministic agrees to float
  rounding: `tests/test_web.py`.
- **No denormal flush.** WebAssembly has no flush-to-zero mode; its floats
  are IEEE 754 with denormals, fixed. `FloatVectorOperations::
  disableDenormalisedNumberSupport` is a no-op in the shim. The desktop
  builds flush (CLAUDE.md §3). A tail decaying into the denormal range costs
  more CPU here than on the desktop.
- **Presets in the current format only.** `gnarl_save` / `gnarl_load`
  mirror `LoadSave::stateToJson` / `jsonToState` (controls, modulations and
  their slots, sample, wavetables, LFOs, the wobble's shape). The migration
  of older patches (`LoadSave::updateFromOldVersion`, ~750 lines of JUCE
  strings) is not ported: a patch from an older version is refused with its
  own answer, and the plugin opens it. As in Vital, a preset restores the
  mod wheel and not the pitch wheel.
- **Exceptions in the host only.** The engine is built without them, as on
  the desktop. `gnarl_web.cpp` and the wavetable / JSON code are built with
  WebAssembly exceptions, so a malformed file is caught in `gnarl_load`
  (the engine returns to the init patch) instead of trapping the worklet.
- **No tuning files, no MIDI manager.** They need JUCE's files and strings;
  the page plays notes and wheels directly.
- **No tuning.** `Tuning::convertMidiNote` is defined as 12-TET in
  `gnarl_web.cpp` so the engine links; the voices only call it when a
  tuning is set, and none can be.
- `-msimd128 -msse2`: Emscripten maps `poly_float`'s SSE2 path onto
  WebAssembly SIMD. Safari 16.4+ and Chrome 91+ have it; `host.ts` checks
  and falls back to the page's preview when a browser does not.

## Threads

The AudioWorklet is the audio thread, and every export is called on it,
between blocks. The page's values arrive as messages on the worklet's port
and are applied before the next block, which is where Vital applies UI
changes anyway (`SynthBase::processModulationChanges`). Nothing is shared
between threads, so there is no lock.

Allocation happens in `gnarl_init` and in `gnarl_route` (a new matrix
connection, as Vital's own matrix allocates one). The per-block calls
(`gnarl_process`, `gnarl_set`, `gnarl_note`) do not allocate. The worklet's
30 Hz frame posts JavaScript arrays (the scope, changed values), which the
garbage collector reclaims; that is the browser's cost, measured as fine in
`tests/test_web.py` (the heavy patch at 4x real time on the CI machine).
