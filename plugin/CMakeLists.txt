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
    NEEDS_WEB_BROWSER           TRUE)

gnarl_add_web_ui(GNARL_WEB_UI_TARGET)

target_sources(GNARL PRIVATE
    source/PluginProcessor.cpp
    source/PluginProcessor.h
    source/params/ParameterIDs.h
    ui-bridge/WebUIEditor.cpp
    ui-bridge/WebUIEditor.h
    ui-bridge/WebUIResourceProvider.cpp
    ui-bridge/WebUIResourceProvider.h)

# PUBLIC so the test target, which links this shared-code target, sees them.
target_include_directories(GNARL PUBLIC source ui-bridge)

target_compile_definitions(GNARL PUBLIC
    JUCE_WEB_BROWSER=1              # the entire UI is a WebBrowserComponent
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
