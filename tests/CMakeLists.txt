# The test runner links the plugin's SHARED CODE target (`GNARL`, created by
# juce_add_plugin). That is what gives the tests the real AudioProcessor plus
# all the JucePlugin_* defines, instead of a second, subtly different build of
# the same sources.

add_executable(GnarlTests
    TestMain.cpp
    EngineTests.cpp
    FilterTests.cpp
    FxChainTests.cpp
    FxDistortionTests.cpp
    FxEqTests.cpp
    FxModulationTests.cpp
    FxRackTests.cpp
    FxReverbLimiterTests.cpp
    FxDelayTests.cpp
    FxFilterTests.cpp
    ModStateBridgeTests.cpp
    ModulationTests.cpp
    ParameterLayoutTests.cpp
    PresetTests.cpp
    ParameterMirrorTests.cpp
    PluginSmokeTests.cpp
    VoiceManagerTests.cpp
    WarpProcessorTests.cpp
    WavetableOscillatorTests.cpp
    WavetableTests.cpp
    WebUIBridgeTests.cpp)

target_sources(GnarlTests PRIVATE
    ${CMAKE_SOURCE_DIR}/plugin/source/PluginProcessor.h)

target_link_libraries(GnarlTests PRIVATE
    GNARL
    Catch2::Catch2WithMain)

# --- Web-browser dependencies for the test target ---------------------------
#
# juce_gui_extra's sources are compiled into EVERY target that links them, and
# the web-browser part needs platform SDK headers. juce_add_plugin attaches
# those to the GNARL target PRIVATE-ly (see _juce_link_optional_libraries in
# JUCE's CMake), so a target that merely links GNARL inherits the
# JUCE_USE_WIN_WEBVIEW2 / JUCE_WEB_BROWSER defines but not the include paths -
# and then fails to compile on a missing header.
#
# This bit them on both platforms in turn: gtk/gtk.h on Linux, WebView2.h on
# Windows. Same cause, same shape of fix.

if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME MATCHES ".*BSD")
    target_link_libraries(GnarlTests PRIVATE juce::pkgconfig_JUCE_BROWSER_LINUX_DEPS)
    # juce_gui_extra also wants the embedded subprocess helper blob that JUCE
    # generates only for plugin/app targets.
    juce_link_with_embedded_linux_subprocess(GnarlTests)
endif()

if (WIN32)
    # Created by find_package(WebView2) inside juce_add_plugin, which the
    # plugin/ subdirectory has already run by the time this is processed.
    if (TARGET juce::juce_webview2)
        target_link_libraries(GnarlTests PRIVATE juce::juce_webview2)
    else()
        message(FATAL_ERROR
            "GNARL: the WebView2 SDK was not found, so the test target cannot "
            "compile juce_gui_extra. See docs/windows-setup.md.")
    endif()
endif()

# Tests compare exact float values on purpose (silence is exactly 0.0, a state
# round trip must recall bit-identically). -Wfloat-equal from JUCE's
# recommended warnings fires on every such assertion, including inside Catch2's
# own expression decomposer, so it is off here and only here.
if (NOT MSVC)
    target_compile_options(GnarlTests PRIVATE -Wno-float-equal)
endif()

# LTO makes an already-slow test link glacial and buys the tests nothing.
set_target_properties(GnarlTests PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION FALSE
    FOLDER "GNARL")

# The mirror test reads the TypeScript ID list from source, so it needs the
# path. Passed as a define rather than resolved at runtime, because the test
# binary's own location says nothing about where the repo is.
target_compile_definitions(GnarlTests PRIVATE
    GNARL_UI_PARAMETER_IDS_PATH="${CMAKE_SOURCE_DIR}/ui/src/bridge/parameterIds.ts"
    GNARL_UI_CHOICES_PATH="${CMAKE_SOURCE_DIR}/ui/src/bridge/choices.ts"
    GNARL_UI_MOD_DESTINATIONS_PATH="${CMAKE_SOURCE_DIR}/ui/src/bridge/modDestinations.ts")

catch_discover_tests(GnarlTests)

# --- Headless demo renderer -------------------------------------------------
# Off by default: it is a listening tool, not part of the build or the tests.
# Exists because the one thing a unit test cannot tell you about a synth is
# whether it sounds any good.
if (GNARL_BUILD_DEMO_RENDERER)
    add_executable(GnarlRenderDemo ${CMAKE_SOURCE_DIR}/tools/render_demo.cpp)

    target_link_libraries(GnarlRenderDemo PRIVATE GNARL juce::juce_audio_formats)

    if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME MATCHES ".*BSD")
        target_link_libraries(GnarlRenderDemo PRIVATE juce::pkgconfig_JUCE_BROWSER_LINUX_DEPS)
        juce_link_with_embedded_linux_subprocess(GnarlRenderDemo)
    endif()

    if (WIN32 AND TARGET juce::juce_webview2)
        target_link_libraries(GnarlRenderDemo PRIVATE juce::juce_webview2)
    endif()

    set_target_properties(GnarlRenderDemo PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION FALSE
        FOLDER "GNARL")

    # Dumps parameter ranges and defaults to JSON, so the UI's browser preview
    # can be seeded from the same source of truth as the plugin.
    add_executable(GnarlDumpDefaults ${CMAKE_SOURCE_DIR}/tools/dump_parameter_defaults.cpp)
    target_link_libraries(GnarlDumpDefaults PRIVATE GNARL)

    if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME MATCHES ".*BSD")
        target_link_libraries(GnarlDumpDefaults PRIVATE juce::pkgconfig_JUCE_BROWSER_LINUX_DEPS)
        juce_link_with_embedded_linux_subprocess(GnarlDumpDefaults)
    endif()

    if (WIN32 AND TARGET juce::juce_webview2)
        target_link_libraries(GnarlDumpDefaults PRIVATE juce::juce_webview2)
    endif()

    set_target_properties(GnarlDumpDefaults PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION FALSE
        FOLDER "GNARL")

    # Dumps the factory wavetables as display spectra for the UI.
    add_executable(GnarlDumpSpectra ${CMAKE_SOURCE_DIR}/tools/dump_wavetable_spectra.cpp)
    target_link_libraries(GnarlDumpSpectra PRIVATE GNARL)

    if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME MATCHES ".*BSD")
        target_link_libraries(GnarlDumpSpectra PRIVATE juce::pkgconfig_JUCE_BROWSER_LINUX_DEPS)
        juce_link_with_embedded_linux_subprocess(GnarlDumpSpectra)
    endif()

    if (WIN32 AND TARGET juce::juce_webview2)
        target_link_libraries(GnarlDumpSpectra PRIVATE juce::juce_webview2)
    endif()

    set_target_properties(GnarlDumpSpectra PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION FALSE
        FOLDER "GNARL")
endif()
