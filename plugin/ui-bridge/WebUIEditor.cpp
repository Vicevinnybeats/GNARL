#include "WebUIEditor.h"

#include "PluginProcessor.h"
#include "WebUIResourceProvider.h"
#include "params/ParameterIDs.h"

namespace gnarl
{

namespace
{
    constexpr int kDefaultWidth  = 1180;
    constexpr int kDefaultHeight = 720;

    // The UI is laid out for a 1180x720 design frame and scales 70%-200%.
    constexpr int kMinWidth  = 826;   // 0.70x
    constexpr int kMinHeight = 504;
    constexpr int kMaxWidth  = 2360;  // 2.00x
    constexpr int kMaxHeight = 1440;
}

WebUIEditor::WebUIEditor (GnarlProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    // Relays are created before the web view, because the view's Options hold
    // references to them.
    attachSliderParameter (pid::masterGain);

    webView = std::make_unique<juce::WebBrowserComponent> (makeWebOptions());
    addAndMakeVisible (*webView);

    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    setResizable (true, true);
    setResizeLimits (kMinWidth, kMinHeight, kMaxWidth, kMaxHeight);
    getConstrainer()->setFixedAspectRatio ((double) kDefaultWidth / (double) kDefaultHeight);
    setSize (kDefaultWidth, kDefaultHeight);
}

WebUIEditor::~WebUIEditor() = default;

void WebUIEditor::attachSliderParameter (const juce::String& parameterID)
{
    auto& apvts = processor.getValueTreeState();

    if (auto* param = apvts.getParameter (parameterID))
    {
        auto relay = std::make_unique<juce::WebSliderRelay> (parameterID);

        sliderAttachments.push_back (
            std::make_unique<juce::WebSliderParameterAttachment> (
                *param, *relay, apvts.undoManager));

        sliderRelays.push_back (std::move (relay));
    }
    else
    {
        // A missing parameter here means ParameterIDs.h and the layout in
        // PluginProcessor.cpp have drifted apart.
        jassertfalse;
    }
}

juce::WebBrowserComponent::Options WebUIEditor::makeWebOptions()
{
    auto options = juce::WebBrowserComponent::Options {}
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options (
            juce::WebBrowserComponent::Options::WinWebView2 {}
                .withBackgroundColour (juce::Colour (0xff0a0a0c))
                // Per-user data folder: a plugin must not write into the
                // host's install directory.
                .withUserDataFolder (juce::File::getSpecialLocation (
                    juce::File::SpecialLocationType::tempDirectory)))
        .withNativeIntegrationEnabled()
        .withResourceProvider ([] (const auto& url) { return WebUIResourceProvider::get (url); },
                               juce::URL (WebUIResourceProvider::getOrigin()).getOrigin())
        .withInitialisationData ("pluginVersion", juce::String (JucePlugin_VersionString))
        .withInitialisationData ("stateVersion",  pid::kStateVersion);

    for (auto& relay : sliderRelays)
        options = options.withOptionsFrom (*relay);

    return options;
}

void WebUIEditor::paint (juce::Graphics& g)
{
    // Only visible for the instant before the web view paints, and behind any
    // transparent region of the page.
    g.fillAll (juce::Colour (0xff0a0a0c));
}

void WebUIEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

} // namespace gnarl
