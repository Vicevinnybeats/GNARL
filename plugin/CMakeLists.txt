include(${CMAKE_SOURCE_DIR}/cmake/WebUI.cmake)

juce_add_plugin(GNARL
    COMPANY_NAME                "Gnarl Audio"
    COMPANY_WEBSITE             "https://gnarl.audio"
    COMPANY_EMAIL               "support@gnarl.audio"
    BUNDLE_ID                   "audio.gnarl.gnarl"
    PLUGIN_MANUFACTURER_CODE    Gnrl
    PLUGIN_CODE                 Gnr1
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "GNARL"
    DESCRIPTION                 "Riddim/dubstep synthesizer"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    VST3_CATEGORIES             Instrument Synth
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    NEEDS_WEB_BROWSER           TRUE
    # Windows only, and NOT optional. Without it JUCE compiles the whole
    # native-integration API out of WebBrowserComponent::Options (no
    # withResourceProvider, no relays) and silently falls back to the
    # Internet Explorer engine, which cannot run a modern React bundle at
    # all. Requires the Microsoft.Web.WebView2 NuGet package on the build
    # machine - see docs/windows-setup.md.
    NEEDS_WEBVIEW2              TRUE)

gnarl_add_web_ui(GNARL_WEB_UI_TARGET)

target_sources(GNARL PRIVATE
    source/PluginProcessor.cpp
    source/PluginProcessor.h
    source/dsp/CombFilter.h
    source/dsp/Envelope.h
    source/dsp/FilterSlot.h
    source/dsp/FormantFilter.h
    source/dsp/LadderFilter.h
    source/dsp/Lfo.h
    source/dsp/LfoCurve.h
    source/dsp/ModMatrix.h
    source/dsp/Modulation.cpp
    source/dsp/Modulation.h
    source/dsp/Saturation.h
    source/dsp/SmoothedParameter.h
    source/dsp/StateVariableFilter.h
    source/dsp/SyncRates.h
    source/dsp/Voice.cpp
    source/dsp/Voice.h
    source/dsp/NoiseGenerator.h
    source/dsp/OttCompressor.h
    source/dsp/VoiceManager.cpp
    source/dsp/VoiceManager.h
    source/dsp/VoiceOversampler.h
    source/dsp/VoiceSettings.cpp
    source/dsp/VoiceSettings.h
    source/dsp/WarpProcessor.h
    source/dsp/Wavetable.cpp
    source/dsp/Wavetable.h
    source/dsp/WavetableLibrary.cpp
    source/dsp/WavetableLibrary.h
    source/dsp/WavetableOscillator.cpp
    source/dsp/WavetableOscillator.h
    source/params/ModStateBridge.cpp
    source/params/ModStateBridge.h
    source/params/ParameterChoices.h
    source/params/ParameterIDs.h
    source/params/ParameterLayout.cpp
    source/params/ParameterLayout.h
    source/params/ParameterRanges.h
    source/params/SettingsReader.cpp
    source/params/SettingsReader.h
    ui-bridge/WebUIEditor.cpp
    ui-bridge/WebUIEditor.h
    ui-bridge/WebUIResourceProvider.cpp
    ui-bridge/WebUIResourceProvider.h)

# PUBLIC so the test target, which links this shared-code target, sees them.
target_include_directories(GNARL PUBLIC source ui-bridge)

target_compile_definitions(GNARL PUBLIC
    JUCE_WEB_BROWSER=1              # the entire UI is a WebBrowserComponent
    # Static linking, so the shipped VST3 has no WebView2Loader.dll sitting
    # next to it for a user to lose. Implies JUCE_USE_WIN_WEBVIEW2=1.
    # No effect on macOS or Linux, which use the system WebKit.
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0)   # modal loops on the message thread = hangs

# The modules and config flags are PUBLIC because tests/ links this shared-code
# target and needs the JUCE headers plus the JucePlugin_* defines.
#
# Warning and LTO flags stay PRIVATE: they are a policy for OUR plugin code,
# and propagating them would impose JUCE's -Wfloat-equal on the test suite
# (which compares exact float values on purpose) and slow the test link down
# with LTO for no benefit.
target_link_libraries(GNARL
    PUBLIC
        ${GNARL_WEB_UI_TARGET}
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
    PRIVATE
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
