#!/usr/bin/env bash
# The GNARL engine for the browser: wasm/README.md.
#
#   source /path/to/emsdk/emsdk_env.sh
#   wasm/build.sh            # -> wasm/build/gnarl.wasm
#
# A standalone module (no Emscripten JS glue): the AudioWorklet that runs it
# (ui/src/web/worklet.js) instantiates it itself, with a handful of WASI
# imports stubbed, because a worklet has no fetch and no DOM for the glue.
set -euo pipefail
cd "$(dirname "$0")/.."

version=$(python3 - <<'PY'
import xml.etree.ElementTree as ET
print(ET.parse('plugin/gnarl.jucer').getroot().get('version'))
PY
)

includes=(-Iwasm/shim)
for d in src/common src/common/wavetable src/interface/look_and_feel \
         src/synthesis/synth_engine src/synthesis/effects_engine src/synthesis/effects \
         src/synthesis/filters src/synthesis/framework src/synthesis/lookups \
         src/synthesis/modulators src/synthesis/modules src/synthesis/producers \
         src/synthesis/utilities third_party; do
  includes+=("-I$d")
done

# -msimd128 -msse2: poly_float's SSE2 path, which Emscripten maps onto wasm
# SIMD (Safari 16.4+, Chrome 91+). -ffast-math as the desktop builds.
# -O2, as the desktop builds, so the compiler makes
# the same kinds of choices. A browser render is compared with the desktop
# one in tests/test_web.py, not assumed equal.
flags=(-std=c++17 -O2 -ffast-math -msimd128 -msse2 -DNO_AUTH=1 -DHEADLESS=1
       "-DGNARL_VERSION=\"$version\"" "${includes[@]}")

mkdir -p wasm/build
em++ "${flags[@]}" -c src/unity_build/synthesis.cpp -o wasm/build/synthesis.o &
em++ "${flags[@]}" -c wasm/common_web.cpp -o wasm/build/common.o &
em++ "${flags[@]}" -c wasm/gnarl_web.cpp -o wasm/build/gnarl_web.o &
wait

em++ -O2 -msimd128 -flto=none wasm/build/synthesis.o wasm/build/common.o wasm/build/gnarl_web.o \
  -o wasm/build/gnarl.wasm \
  -sSTANDALONE_WASM=1 --no-entry -sFILESYSTEM=0 \
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB -sSTACK_SIZE=1MB

ls -l wasm/build/gnarl.wasm
