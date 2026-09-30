# GNARL's CMake build, on JUCE 8 (docs/design/phase2-03-juce8.md).
#
# STAGE 1: the offline renderer only. It runs the unchanged engine, so its
# output can be compared sample for sample with the Projucer/JUCE 6 build
# (tools/compare_renders.py) before anything user-facing moves. The plugin
# and standalone still build from the Projucer projects until they have
# passed the same comparison.
#
#   cmake -B build-cmake -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-cmake --target gnarl_render
#
# JUCE is fetched at a pinned tag, not vendored: third_party/JUCE is the
# 6.0.5 the Projucer projects still use. Set GNARL_JUCE_DIR to use a local
# checkout (offline builds, CI caches).

cmake_minimum_required(VERSION 3.22)

project(GNARL VERSION 1.0.6 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GNARL_JUCE_TAG 8.0.9)
set(GNARL_JUCE_DIR "" CACHE PATH "Local JUCE ${GNARL_JUCE_TAG} checkout (empty = fetch)")

if(GNARL_JUCE_DIR)
  add_subdirectory(${GNARL_JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${GNARL_JUCE_TAG}
    GIT_SHALLOW    TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------- engine flags
#
# The Projucer Linux Makefiles' Release flags, kept identical so that the
# comparison measures the JUCE upgrade and nothing else. -Ofast implies
# -ffast-math; the engine was written and tuned against it.

set(GNARL_INCLUDE_DIRS
  src
  src/common
  src/common/wavetable
  # The engine includes a few interface headers (synth_strings.h lives in
  # look_and_feel), exactly as the Projucer projects allow.
  src/interface/editor_components
  src/interface/editor_sections
  src/interface/look_and_feel
  src/interface/wavetable
  src/interface/wavetable/editors
  src/interface/wavetable/overlays
  src/standalone
  src/synthesis/synth_engine
  src/synthesis/effects
  src/synthesis/filters
  src/synthesis/framework
  src/synthesis/lookups
  src/synthesis/modulators
  src/synthesis/modules
  src/synthesis/producers
  src/synthesis/utilities
  third_party)

function(gnarl_engine_options target)
  target_include_directories(${target} PRIVATE ${GNARL_INCLUDE_DIRS})
  target_compile_definitions(${target} PRIVATE
    NO_AUTH=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_DARK_SPLASH_SCREEN=1)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE
      $<$<CONFIG:Release>:-Ofast -flto -ffast-math -ftree-vectorize -ftree-slp-vectorize -funroll-loops>
      $<$<STREQUAL:${CMAKE_SYSTEM_PROCESSOR},x86_64>:-msse2>)
    target_link_options(${target} PRIVATE $<$<CONFIG:Release>:-flto -ffast-math>)
  endif()
endfunction()

# ------------------------------------------------------------------ renderer

juce_add_console_app(gnarl_render
  PRODUCT_NAME "GNARL"
  VERSION 1.0.6
  COMPANY_NAME "Gnarl Audio")

set_target_properties(gnarl_render PROPERTIES OUTPUT_NAME gnarl-render)

juce_generate_juce_header(gnarl_render)

target_sources(gnarl_render PRIVATE
  src/headless/main.cpp
  src/unity_build/common.cpp
  src/unity_build/synthesis.cpp)

gnarl_engine_options(gnarl_render)
target_compile_definitions(gnarl_render PRIVATE
  HEADLESS=1
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0
  JUCE_STANDALONE_APPLICATION=1)

target_link_libraries(gnarl_render PRIVATE
  juce::juce_audio_basics
  juce::juce_audio_formats
  juce::juce_core
  juce::juce_data_structures
  juce::juce_dsp
  juce::juce_events
  juce::juce_recommended_config_flags)
