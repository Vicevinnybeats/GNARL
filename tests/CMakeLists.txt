# The test runner links the plugin's SHARED CODE target (`GNARL`, created by
# juce_add_plugin). That is what gives the tests the real AudioProcessor plus
# all the JucePlugin_* defines, instead of a second, subtly different build of
# the same sources.

add_executable(GnarlTests
    TestMain.cpp
    ParameterLayoutTests.cpp
    ParameterMirrorTests.cpp
    PluginSmokeTests.cpp
    VoiceManagerTests.cpp
    WarpProcessorTests.cpp
    WavetableTests.cpp
    WebUIBridgeTests.cpp)

target_sources(GnarlTests PRIVATE
    ${CMAKE_SOURCE_DIR}/plugin/source/PluginProcessor.h)

target_link_libraries(GnarlTests PRIVATE
    GNARL
    Catch2::Catch2WithMain)

# On Linux, juce_gui_extra's web-browser code needs the webkit/GTK include
# paths. juce_add_plugin attaches them to the GNARL target PRIVATE-ly via
# NEEDS_WEB_BROWSER, so a target that merely links GNARL does not inherit them.
# Linux is only a development/CI platform for GNARL; the shipping targets are
# macOS and Windows.
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME MATCHES ".*BSD")
    target_link_libraries(GnarlTests PRIVATE juce::pkgconfig_JUCE_BROWSER_LINUX_DEPS)
    # juce_gui_extra also wants the embedded subprocess helper blob that JUCE
    # generates only for plugin/app targets.
    juce_link_with_embedded_linux_subprocess(GnarlTests)
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
    GNARL_UI_PARAMETER_IDS_PATH="${CMAKE_SOURCE_DIR}/ui/src/bridge/parameterIds.ts")

catch_discover_tests(GnarlTests)

