#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>
#include <vector>

namespace gnarl
{

class GnarlProcessor;

/**
    GNARL's editor: a single WebBrowserComponent filling the window, running the
    React bundle embedded in the binary.

    Parameter traffic goes through JUCE's relay/attachment pair rather than
    hand-rolled messages, so the UI participates properly in host automation,
    undo, and gesture begin/end.

    Everything here runs on the message thread. The audio thread must never
    reach into this class.
*/
class WebUIEditor final : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit WebUIEditor (GnarlProcessor&);
    ~WebUIEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** Creates the relay + attachment pair for one float parameter. */
    void attachSliderParameter (const juce::String& parameterID);

    juce::WebBrowserComponent::Options makeWebOptions();

    /** Pushes the live modulation values to the page. */
    void timerCallback() override;

    // --- Native functions, called from the page ---------------------------
    //
    // These cover the modulation state that is NOT a host parameter: the
    // drawable LFO curves and the mod slots' destinations. Everything else the
    // UI touches is a parameter and goes through a relay instead, which is
    // what keeps automation, undo and gesture handling working.

    juce::var handleGetModState (const juce::Array<juce::var>& args);
    juce::var handleSetLfoCurve (const juce::Array<juce::var>& args);
    juce::var handleSetModDestination (const juce::Array<juce::var>& args);

    GnarlProcessor& processor;

    // Relays must outlive the WebBrowserComponent they were registered with,
    // so they are declared before it and destroyed after it.
    std::vector<std::unique_ptr<juce::WebSliderRelay>> sliderRelays;
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>> sliderAttachments;

    std::unique_ptr<juce::WebBrowserComponent> webView;

    /** The last frame pushed, so an unchanged frame is not sent again. With
        nothing playing that means no bridge traffic at all, rather than 60
        identical messages a second for as long as the editor is open. */
    juce::String lastModulationFrame;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebUIEditor)
};

} // namespace gnarl
