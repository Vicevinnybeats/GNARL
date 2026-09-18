#include "WebUIEditor.h"

#include "PluginProcessor.h"
#include "WebUIResourceProvider.h"
#include "dsp/FxChain.h"
#include "dsp/LfoCurve.h"
#include "dsp/Modulation.h"
#include "params/ParameterChoices.h"
#include "params/ParameterIDs.h"

#include <cmath>

namespace gnarl
{

namespace
{
    /** Where WebView2 keeps its cache and local storage on Windows. */
    juce::File getWebViewDataFolder()
    {
        auto folder = juce::File::getSpecialLocation (
                          juce::File::SpecialLocationType::userApplicationDataDirectory)
                          .getChildFile ("GNARL")
                          .getChildFile ("WebView");

        // Created here, on the message thread, so the web view never has to.
        folder.createDirectory();

        return folder;
    }

    constexpr int kDefaultWidth  = 1180;
    constexpr int kDefaultHeight = 720;

    // The UI is laid out for a 1180x720 design frame and scales 70%-200%.
    constexpr int kMinWidth  = 826;   // 0.70x
    constexpr int kMinHeight = 504;
    constexpr int kMaxWidth  = 2360;  // 2.00x
    constexpr int kMaxHeight = 1440;

    /** How often the live modulation values are pushed to the page.

        60 Hz, not 30: the display draws a wobble, and at 30 Hz a 1/16 pattern
        at 140 BPM is sampled about three times per cycle, which reads as a
        stutter rather than as motion. The payload is a few dozen numbers, and
        a frame identical to the last one is not sent at all. */
    constexpr int kModulationFrameHz = 60;

    /** Rounded before comparing, so a value jittering in the seventh decimal
        does not defeat the identical-frame check. Three decimals is finer than
        one pixel at any size this UI runs at. */
    juce::var rounded (float value)
    {
        return juce::var (std::round (value * 1000.0f) / 1000.0f);
    }
}

WebUIEditor::WebUIEditor (GnarlProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    // EVERY parameter gets a relay, not just the ones a particular panel
    // happens to use today. A relay the page never reads costs one registered
    // event listener; a parameter with no relay is a control that silently
    // does nothing in the plugin while working perfectly in the browser
    // preview - which is the worse failure by a long way, and is exactly what
    // happened while only the master gain was attached here.
    for (auto* parameter : processor.getParameters())
        if (const auto* withID = dynamic_cast<const juce::AudioProcessorParameterWithID*> (parameter))
            attachSliderParameter (withID->paramID);

    webView = std::make_unique<juce::WebBrowserComponent> (makeWebOptions());
    addAndMakeVisible (*webView);

    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    startTimerHz (kModulationFrameHz);

    setResizable (true, true);
    setResizeLimits (kMinWidth, kMinHeight, kMaxWidth, kMaxHeight);
    getConstrainer()->setFixedAspectRatio ((double) kDefaultWidth / (double) kDefaultHeight);
    setSize (kDefaultWidth, kDefaultHeight);
}

WebUIEditor::~WebUIEditor()
{
    // Before the web view goes away: a timer callback that reached a
    // half-destroyed view would be a crash on editor close.
    stopTimer();
}

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
    // The type is spelled out rather than deduced with `auto`: MSVC rejects
    // reassigning an `auto`-declared variable from a call on itself with
    // C3536 ("cannot be used before it is initialized"), which the relay loop
    // below does. Clang and GCC accept it, so this only breaks the Windows
    // build. Do not "simplify" this back to `auto`.
    using Options = juce::WebBrowserComponent::Options;

    Options options = Options {}
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options (
            juce::WebBrowserComponent::Options::WinWebView2 {}
                .withBackgroundColour (juce::Colour (0xff0a0a0c))
                // A per-user app-data folder, NOT the temp directory: WebView2
                // keeps this folder open for the life of the view, and a temp
                // cleaner removing it mid-session breaks the UI in a running
                // project. A plugin must also never write into the host's own
                // install directory.
                .withUserDataFolder (getWebViewDataFolder()))
        .withNativeIntegrationEnabled()
        .withResourceProvider ([] (const auto& url) { return WebUIResourceProvider::get (url); },
                               juce::URL (WebUIResourceProvider::getOrigin()).getOrigin())
        .withInitialisationData ("pluginVersion", juce::String (JucePlugin_VersionString))
        .withInitialisationData ("stateVersion",  pid::kStateVersion)
        .withNativeFunction ("gnarlGetModState",
                             [this] (const juce::Array<juce::var>& args, auto complete)
                             { complete (handleGetModState (args)); })
        .withNativeFunction ("gnarlSetLfoCurve",
                             [this] (const juce::Array<juce::var>& args, auto complete)
                             { complete (handleSetLfoCurve (args)); })
        .withNativeFunction ("gnarlSetModDestination",
                             [this] (const juce::Array<juce::var>& args, auto complete)
                             { complete (handleSetModDestination (args)); })
        .withNativeFunction ("gnarlGetFxOrder",
                             [this] (const juce::Array<juce::var>& args, auto complete)
                             { complete (handleGetFxOrder (args)); })
        .withNativeFunction ("gnarlSetFxOrder",
                             [this] (const juce::Array<juce::var>& args, auto complete)
                             { complete (handleSetFxOrder (args)); })
        .withNativeFunction ("gnarlMoveFxSlot",
                             [this] (const juce::Array<juce::var>& args, auto complete)
                             { complete (handleMoveFxSlot (args)); });

    for (auto& relay : sliderRelays)
        options = options.withOptionsFrom (*relay);

    return options;
}

juce::var WebUIEditor::handleGetModState (const juce::Array<juce::var>&)
{
    auto& modState = processor.getModState();

    auto* root = new juce::DynamicObject();

    auto curves = juce::Array<juce::var>();

    for (std::size_t lfo = 0; lfo < pid::kNumLfos; ++lfo)
    {
        const auto curve = modState.getCurve (lfo);
        auto points = juce::Array<juce::var>();

        for (int i = 0; i < curve.getNumPoints(); ++i)
        {
            const auto& point = curve.getPoint (i);

            auto* entry = new juce::DynamicObject();
            entry->setProperty ("time", rounded (point.time));
            entry->setProperty ("value", rounded (point.value));
            entry->setProperty ("tension", rounded (point.tension));
            entry->setProperty ("step", point.shape == dsp::LfoCurve::Shape::step);

            points.add (juce::var (entry));
        }

        curves.add (juce::var (points));
    }

    auto destinations = juce::Array<juce::var>();

    for (std::size_t slot = 0; slot < pid::kNumModSlots; ++slot)
        destinations.add (modState.getDestinationParameterID (slot));

    // The destination list comes from the engine's own table rather than being
    // duplicated in TypeScript, so a destination cannot exist in the picker
    // and not in the matrix.
    auto available = juce::Array<juce::var>();

    for (int i = 1; i < static_cast<int> (dsp::ModDestination::count); ++i)
    {
        const auto destination = static_cast<dsp::ModDestination> (i);

        auto* entry = new juce::DynamicObject();
        entry->setProperty ("id", dsp::getParameterIDForDestination (destination));
        entry->setProperty ("name", dsp::getDestinationDisplayName (destination));

        available.add (juce::var (entry));
    }

    root->setProperty ("curves", curves);
    root->setProperty ("destinations", destinations);
    root->setProperty ("available", available);

    return juce::var (root);
}

juce::var WebUIEditor::handleSetLfoCurve (const juce::Array<juce::var>& args)
{
    if (args.size() < 2)
        return juce::var (false);

    const auto index = static_cast<std::size_t> (juce::jmax (0, static_cast<int> (args[0])));

    if (index >= pid::kNumLfos)
        return juce::var (false);

    const auto* points = args[1].getArray();

    if (points == nullptr)
        return juce::var (false);

    dsp::LfoCurve curve;
    curve.clear();

    for (const auto& entry : *points)
    {
        if (auto* object = entry.getDynamicObject())
        {
            dsp::LfoCurve::Point point {};
            point.time = juce::jlimit (0.0f, 1.0f,
                static_cast<float> (static_cast<double> (object->getProperty ("time"))));
            point.value = juce::jlimit (0.0f, 1.0f,
                static_cast<float> (static_cast<double> (object->getProperty ("value"))));
            point.tension = juce::jlimit (-1.0f, 1.0f,
                static_cast<float> (static_cast<double> (object->getProperty ("tension"))));
            point.shape = static_cast<bool> (object->getProperty ("step"))
                ? dsp::LfoCurve::Shape::step
                : dsp::LfoCurve::Shape::curved;

            curve.addPoint (point);
        }
    }

    // An empty curve would evaluate to zero at every phase, which reads as a
    // dead LFO. The editor cannot produce one, but the bridge is an interface
    // and an interface gets whatever it gets.
    if (curve.getNumPoints() == 0)
        return juce::var (false);

    processor.getModState().setCurve (index, curve);

    return juce::var (true);
}

juce::var WebUIEditor::handleSetModDestination (const juce::Array<juce::var>& args)
{
    if (args.size() < 2)
        return juce::var (false);

    const auto slot = static_cast<std::size_t> (juce::jmax (0, static_cast<int> (args[0])));

    if (slot >= pid::kNumModSlots)
        return juce::var (false);

    processor.getModState().setDestination (slot, args[1].toString());

    return juce::var (true);
}

juce::var WebUIEditor::handleGetFxOrder (const juce::Array<juce::var>&)
{
    const auto& order = processor.getFxOrder().getOrder();

    /*  Returns SLOT INDICES, not names. The names are already mirrored into
        ui/src/bridge/choices.ts and guarded by ParameterMirrorTests, so
        sending them over the bridge every time would be a second copy that
        nothing checks - and the UI needs the index anyway to look the
        instance's parameters up. The ValueTree stores names; that is a
        different trade, because a preset outlives a build. */
    auto slots = juce::Array<juce::var>();

    for (std::size_t i = 0; i < dsp::FxOrder::kNumSlots; ++i)
        slots.add (static_cast<int> (order.getSlot (i)));

    auto* root = new juce::DynamicObject();
    root->setProperty ("slots", slots);

    return juce::var (root);
}

juce::var WebUIEditor::handleSetFxOrder (const juce::Array<juce::var>& args)
{
    if (args.isEmpty())
        return juce::var (false);

    const auto* incoming = args[0].getArray();

    if (incoming == nullptr
        || incoming->size() != static_cast<int> (dsp::FxOrder::kNumSlots))
        return juce::var (false);

    std::array<choices::FxSlot, dsp::FxOrder::kNumSlots> order {};

    for (int i = 0; i < incoming->size(); ++i)
    {
        const auto index = static_cast<int> ((*incoming)[i]);

        if (index < 0 || index >= static_cast<int> (choices::FxSlot::count))
            return juce::var (false);

        order[static_cast<std::size_t> (i)] = static_cast<choices::FxSlot> (index);
    }

    // The bridge rejects anything that is not a permutation, so a malformed
    // message changes nothing rather than producing a chain that runs one
    // effect twice.
    return juce::var (processor.getFxOrder().setOrder (order));
}

juce::var WebUIEditor::handleMoveFxSlot (const juce::Array<juce::var>& args)
{
    if (args.size() < 2)
        return juce::var (false);

    const auto from = static_cast<int> (args[0]);
    const auto to = static_cast<int> (args[1]);

    if (from < 0 || to < 0)
        return juce::var (false);

    return juce::var (processor.getFxOrder().move (static_cast<std::size_t> (from),
                                                  static_cast<std::size_t> (to)));
}

void WebUIEditor::timerCallback()
{
    if (webView == nullptr)
        return;

    const auto snapshot = processor.getModulationSnapshot();

    auto* root = new juce::DynamicObject();

    auto lfoValues = juce::Array<juce::var>();
    auto lfoPhases = juce::Array<juce::var>();

    for (std::size_t i = 0; i < pid::kNumLfos; ++i)
    {
        lfoValues.add (rounded (snapshot.lfoValues[i]));
        lfoPhases.add (rounded (snapshot.lfoPhases[i]));
    }

    auto tablePositions = juce::Array<juce::var>();

    for (std::size_t i = 0; i < pid::kNumOscillators; ++i)
        tablePositions.add (rounded (snapshot.tablePositions[i]));

    auto cutoffs = juce::Array<juce::var>();

    for (std::size_t i = 0; i < pid::kNumFilters; ++i)
        cutoffs.add (rounded (snapshot.filterCutoffHz[i]));

    root->setProperty ("lfoValues", lfoValues);
    root->setProperty ("lfoPhases", lfoPhases);
    root->setProperty ("tablePositions", tablePositions);
    root->setProperty ("cutoffHz", cutoffs);
    root->setProperty ("voices", processor.getSoundingVoiceCount());
    root->setProperty ("playing", snapshot.hasVoice);

    const juce::var frame (root);

    // Identical frames are dropped. An idle editor - no notes, nothing
    // modulating - then costs no bridge traffic at all, instead of 60
    // messages a second for however long the plugin window stays open.
    const auto serialised = juce::JSON::toString (frame, true);

    if (serialised == lastModulationFrame)
        return;

    lastModulationFrame = serialised;

    // ...IfBrowserIsVisible: a hidden view cannot draw, so pushing to it is
    // pure waste.
    webView->emitEventIfBrowserIsVisible ("gnarlModulation", frame);
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
