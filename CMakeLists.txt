# GNARL's CMake build, on JUCE 8 (docs/design/phase2-03-juce8.md).
#
# The offline renderer and the VST3 (plus JUCE's Standalone wrapper, for
# tests). Both are compared with the Projucer / JUCE 6 builds on every push:
# tools/compare_renders.py (renderer) and tools/compare_plugins.py (VST3:
# identity, parameters, audio through the wrapper). The Projucer plugin
# still ships until this one has been tested in FL Studio and Ableton.
#
#   cmake -B build-cmake -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-cmake --target gnarl_render gnarl_plugin_VST3
#
# JUCE is fetched at a pinned tag and PATCHED: third_party/juce-patches/
# juce-8.0.9-gnarl.patch carries forward the changes Vital made to its JUCE
# 6.0.5 (vital-juce-6.0.5.diff, beside it) that the editor depends on - most
# importantly the per-component pixel scaling without which mouse positions
# are wrong on every scaled display. third_party/JUCE is the 6.0.5 the
# Projucer projects still use. Set GNARL_JUCE_GIT to a local clone for
# offline or cached builds; the patch is applied to the fresh checkout.

cmake_minimum_required(VERSION 3.22)

project(GNARL VERSION 1.0.6 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The optimisation flags come from gnarl_engine_options() only, exactly as
# the Projucer Makefiles pass them. CMake's default "-O3" here, and JUCE's
# juce_recommended_config_flags ("-O3" after ours), override -Ofast - and at
# link time, with -flto, the link flags choose the code generated. Measured:
# with them, the plugin's reverb differed from the Projucer build at -132 dB.
set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG")

set(GNARL_JUCE_TAG 8.0.9)
set(GNARL_JUCE_GIT "https://github.com/juce-framework/JUCE.git" CACHE STRING
    "JUCE repository to fetch ${GNARL_JUCE_TAG} from")
set(GNARL_JUCE_PATCH ${CMAKE_CURRENT_SOURCE_DIR}/third_party/juce-patches/juce-${GNARL_JUCE_TAG}-gnarl.patch
    CACHE FILEPATH "Patch applied to the fetched JUCE (a test may build against a variant)")

find_package(Git REQUIRED)
include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY ${GNARL_JUCE_GIT}
  GIT_TAG        ${GNARL_JUCE_TAG}
  GIT_SHALLOW    TRUE
  PATCH_COMMAND  ${GIT_EXECUTABLE} apply --whitespace=nowarn ${GNARL_JUCE_PATCH}
  UPDATE_DISCONNECTED TRUE)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------- engine flags
#
# The Projucer Linux Makefiles' Release flags, kept identical so that the
# comparison measures the JUCE upgrade and nothing else. -Ofast implies
# -ffast-math; the engine was written and tuned against it.

set(GNARL_INCLUDE_DIRS
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
    # The Makefiles' link line: no -O level (with -flto GCC then keeps the
    # level each function was compiled at), the same vectorisation flags.
    # PUBLIC: juce_add_plugin links the shared code into separate format
    # targets (gnarl_plugin_VST3), and those do the final LTO link.
    target_link_options(${target} PUBLIC
      $<$<CONFIG:Release>:-flto -ffast-math -ftree-vectorize -ftree-slp-vectorize>
      $<$<STREQUAL:${CMAKE_SYSTEM_PROCESSOR},x86_64>:-msse2>)
  elseif(MSVC)
    # plugin/builds/vs17's PreprocessorDefinitions. MSVC never defines
    # __SSE2__, and poly_values.h selects its SIMD path on it (C1017
    # without it, windows-juce8, run 137).
    target_compile_definitions(${target} PRIVATE
      __SSE2__=1
      _ENABLE_EXTENDED_ALIGNED_STORAGE
      _CRT_SECURE_NO_WARNINGS
      WINVER=0x0601
      _WIN32_WINNT=0x0601)
    # plugin/builds/vs17: Optimization MaxSpeed, FloatingPointModel Fast,
    # WholeProgramOptimization true.
    target_compile_options(${target} PRIVATE $<$<CONFIG:Release>:/O2 /fp:fast /GL>)
    target_link_options(${target} PUBLIC $<$<CONFIG:Release>:/LTCG>)
  endif()
endfunction()

# ------------------------------------------------------------------ renderer

juce_add_console_app(gnarl_render
  PRODUCT_NAME "GNARL"
  BUNDLE_ID audio.gnarl.render
  VERSION 1.0.6
  COMPANY_NAME "Gnarl Audio")

set_target_properties(gnarl_render PROPERTIES OUTPUT_NAME gnarl-render)

juce_generate_juce_header(gnarl_render)

target_sources(gnarl_render PRIVATE
  src/headless/main.cpp
  src/unity_build/common.cpp
  src/unity_build/synthesis.cpp)

gnarl_engine_options(gnarl_render)
# As the headless .jucer: src/standalone, not src/plugin. Both hold a
# synth_editor.h, so each target sees exactly one of them.
target_include_directories(gnarl_render PRIVATE src src/standalone)
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
  juce::juce_events)

# -------------------------------------------------------------------- plugin
#
# STAGE 2: the VST3 with the existing (Vital-derived) editor. Every identity
# value below is frozen (CLAUDE.md §4) and mirrors plugin/gnarl.jucer; the
# host sees the same codes, the same class IDs and the same parameter list,
# which tools/vst3_probe.cpp checks against the Projucer build.

# Standalone is JUCE's own wrapper, for running the plugin's editor without a
# DAW (tests, screenshots). The shipped standalone app is src/standalone.
set(GNARL_PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND GNARL_PLUGIN_FORMATS AU)
endif()

juce_add_plugin(gnarl_plugin
  PRODUCT_NAME "GNARL"
  VERSION 1.0.6
  COMPANY_NAME "Gnarl Audio"
  COMPANY_WEBSITE "gnarl.vercel.app"
  COMPANY_EMAIL ""
  BUNDLE_ID audio.gnarl.synth
  PLUGIN_MANUFACTURER_CODE Gnrl
  PLUGIN_CODE Gnr2
  DESCRIPTION ""
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  VST3_CATEGORIES Instrument Synth
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  AU_EXPORT_PREFIX gnarl
  AAX_IDENTIFIER audio.gnarl.synth
  COPY_PLUGIN_AFTER_BUILD FALSE
  # The web panel's view: WebKitGTK on Linux, WebView2 on Windows (linked
  # statically, so no loader DLL ships beside the plugin), WebKit on macOS.
  NEEDS_WEB_BROWSER TRUE
  NEEDS_WEBVIEW2 ${WIN32}
  FORMATS ${GNARL_PLUGIN_FORMATS})

juce_generate_juce_header(gnarl_plugin)

# The same 27 files the .jucer embeds. juceaide names the symbols exactly as
# the Projucer did (gnarl_ring_svg, LatoRegular_ttf, ...), which the editor
# looks up by name.
juce_add_binary_data(gnarl_binary_data
  NAMESPACE BinaryData
  HEADER_NAME BinaryData.h
  SOURCES
    tunings/5\ Limit.scl
    tunings/7\ Limit.scl
    tunings/Pythagorean.scl
    default.vitalskin
    fonts/DroidSansMono.ttf
    fonts/Lato-Light.ttf
    fonts/Lato-Regular.ttf
    fonts/Montserrat-Light.otf
    fonts/Montserrat-Regular.ttf
    icons/chorus.svg
    icons/cog.svg
    icons/compressor.svg
    icons/delay.svg
    icons/distortion.svg
    icons/drag_drop_arrows.svg
    icons/effects_filter.svg
    icons/equalizer.svg
    icons/flanger.svg
    icons/folder.svg
    icons/link.svg
    icons/phaser.svg
    icons/reverb.svg
    icons/shuffle.svg
    icons/gnarl_ring.svg
    icons/gnarl_mark.svg
    icons/gnarl_word.svg
    icons/gnarl_word_ring.svg)

# ----------------------------------------------------------------- web panel
#
# ui/ is built with npm into ONE html file (scripts/inline.mjs) and embedded;
# src/plugin/web_panel.cpp serves it to the web view. npm install runs once
# (and when package-lock.json changes), the build whenever ui/ changes.

find_program(GNARL_NPM npm REQUIRED)
set(GNARL_UI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ui)
set(GNARL_UI_HTML ${CMAKE_CURRENT_BINARY_DIR}/ui/gnarl-ui.html)
file(GLOB GNARL_UI_SOURCES CONFIGURE_DEPENDS ${GNARL_UI_DIR}/src/*.ts ${GNARL_UI_DIR}/src/*.css)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ui/npm-installed.stamp
  COMMAND ${GNARL_NPM} ci --no-audit --no-fund
  COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/ui/npm-installed.stamp
  DEPENDS ${GNARL_UI_DIR}/package-lock.json
  WORKING_DIRECTORY ${GNARL_UI_DIR}
  COMMENT "Installing the web panel's build tools (npm ci)")

add_custom_command(
  OUTPUT ${GNARL_UI_HTML}
  COMMAND ${GNARL_NPM} run build
  COMMAND ${CMAKE_COMMAND} -E copy ${GNARL_UI_DIR}/dist/gnarl-ui.html ${GNARL_UI_HTML}
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/ui/npm-installed.stamp ${GNARL_UI_SOURCES}
          ${GNARL_UI_DIR}/index.html ${GNARL_UI_DIR}/vite.config.ts ${GNARL_UI_DIR}/scripts/inline.mjs
  WORKING_DIRECTORY ${GNARL_UI_DIR}
  COMMENT "Building the web panel (ui/)")

juce_add_binary_data(gnarl_ui_data
  NAMESPACE GnarlUi
  HEADER_NAME GnarlUiData.h
  SOURCES ${GNARL_UI_HTML})

target_sources(gnarl_plugin PRIVATE
  src/plugin/web_panel.cpp
  src/unity_build/common.cpp
  src/unity_build/interface_editor_components.cpp
  src/unity_build/interface_editor_sections.cpp
  src/unity_build/interface_editor_sections2.cpp
  src/unity_build/interface_look_and_feel.cpp
  src/unity_build/interface_wavetable.cpp
  src/unity_build/plugin.cpp
  src/unity_build/synthesis.cpp)

gnarl_engine_options(gnarl_plugin)
target_include_directories(gnarl_plugin PRIVATE src/plugin)
target_compile_definitions(gnarl_plugin PUBLIC
  JUCE_VST3_CAN_REPLACE_VST2=0
  JucePlugin_PreferredChannelConfigurations=0,2
  JUCE_OPENGL3=1
  JUCE_USE_XRANDR=0
  JUCE_USE_CURL=0
  # The web panel (src/plugin/web_panel.cpp). GNARL_WEB_UI keeps it out of the
  # Projucer / JUCE 6 builds, which have no web view bridge.
  GNARL_WEB_UI=1
  JUCE_WEB_BROWSER=1
  $<$<BOOL:${WIN32}>:JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1>
  # The editor opens file choosers and alert boxes synchronously. JUCE 6
  # allowed that by default; JUCE 7+ only with this. Same behaviour as today;
  # converting them to async is for the editor that replaces this one.
  JUCE_MODAL_LOOPS_PERMITTED=1)

target_link_libraries(gnarl_plugin
  PRIVATE
    gnarl_binary_data
    gnarl_ui_data
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_opengl
  PUBLIC
    juce::juce_recommended_warning_flags)
