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

The JavaScript side is in `ui/src/web/`: `engine-core.js` wraps the exports,
`worklet.js` runs them in an AudioWorklet, and `host.ts` connects the page.

## What is different from the desktop builds

- **No JUCE.** The engine does not need it, and JUCE does not target
  WebAssembly. What the engine did take from it is in `shim/`.
- **kissfft**, not JUCE's FFT. `fourier_transform.h` falls through to it
  when neither Apple's vDSP nor JUCE is there. It is used for wavetable
  creation and spectral warps.
- **libc++**, not libstdc++. `std::uniform_real_distribution` is
  implementation-defined, so random oscillator phases differ from the
  desktop's for the same seed. Everything deterministic agrees to float
  rounding: `tests/test_web.py`.
- **No denormal flush.** WebAssembly has no flush-to-zero mode; its floats
  are IEEE 754 with denormals, fixed. `FloatVectorOperations::
  disableDenormalisedNumberSupport` is a no-op in the shim. The desktop
  builds flush (CLAUDE.md §3). A tail decaying into the denormal range costs
  more CPU here than on the desktop.
- **No preset loader, no tuning files, no MIDI manager.** They are
  `SynthBase` and `LoadSave`, which need JUCE's files and strings. The page
  sets every control by name, as the plugin's panel does. Loading `.vital`
  files in the browser would mean porting `LoadSave::jsonToState`; it is
  the next step if the mobile version should open presets.
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
