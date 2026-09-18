#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"
#include "params/ModStateBridge.h"
#include "params/ParameterIDs.h"

using namespace gnarl;
using Catch::Approx;

namespace
{
    /** A shape a producer might actually draw: a step down a third of the way
        in, then a curved climb back. Chosen because it exercises both point
        shapes and a non-zero tension in one curve. */
    dsp::LfoCurve drawnCurve()
    {
        dsp::LfoCurve curve;
        curve.clear();
        curve.addPoint ({ 0.0f, 1.0f, 0.0f, dsp::LfoCurve::Shape::step });
        curve.addPoint ({ 0.333f, 0.0f, 0.5f, dsp::LfoCurve::Shape::curved });
        curve.addPoint ({ 1.0f, 0.9f, -0.25f, dsp::LfoCurve::Shape::curved });

        return curve;
    }

    void setParameter (juce::AudioProcessorValueTreeState& apvts,
                       const char* id,
                       float value)
    {
        auto* parameter = apvts.getParameter (id);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }
}

TEST_CASE ("A drawn curve survives a state round trip", "[modstate]")
{
    // The curves are the feature the plugin is bought for, so losing one on a
    // save is not a cosmetic bug.
    juce::MemoryBlock state;

    {
        GnarlProcessor processor;
        processor.getModState().setCurve (1, drawnCurve());
        processor.getStateInformation (state);
    }

    GnarlProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    const auto curve = restored.getModState().getCurve (1);
    const auto expected = drawnCurve();

    REQUIRE (curve.getNumPoints() == expected.getNumPoints());

    for (int i = 0; i < expected.getNumPoints(); ++i)
    {
        CHECK (curve.getPoint (i).time == Approx (expected.getPoint (i).time));
        CHECK (curve.getPoint (i).value == Approx (expected.getPoint (i).value));
        CHECK (curve.getPoint (i).tension == Approx (expected.getPoint (i).tension));
        CHECK (curve.getPoint (i).shape == expected.getPoint (i).shape);
    }

    // The other LFOs are untouched, not wiped by the one that was saved.
    CHECK (restored.getModState().getCurve (0).isDefaultRamp());
}

TEST_CASE ("A mod slot's destination survives a state round trip", "[modstate]")
{
    juce::MemoryBlock state;

    {
        GnarlProcessor processor;
        processor.getModState().setDestination (0, pid::filter[0].cutoff);
        processor.getModState().setDestination (7, pid::osc[1].tablePos);
        processor.getStateInformation (state);
    }

    GnarlProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    CHECK (restored.getModState().getDestinationParameterID (0)
        == juce::String (pid::filter[0].cutoff));
    CHECK (restored.getModState().getDestinationParameterID (7)
        == juce::String (pid::osc[1].tablePos));
    CHECK (restored.getModState().getDestinationParameterID (1).isEmpty());
}

TEST_CASE ("A destination this build does not know loads as none", "[modstate]")
{
    // Forward compatibility: a preset written by a later version naming a
    // destination that does not exist here must load with that slot inert
    // rather than failing or pointing somewhere arbitrary.
    GnarlProcessor processor;
    processor.getModState().setDestination (0, "some_future_parameter");

    CHECK (processor.getModState().getDestinationParameterID (0).isEmpty());
    CHECK (processor.getModState().getSnapshot().destinations[0]
        == dsp::ModDestination::none);
}

TEST_CASE ("An empty curve element in the tree falls back to the ramp", "[modstate]")
{
    // A curve with nothing usable in it would evaluate to zero at every
    // phase, which looks like a broken LFO rather than a broken preset.
    GnarlProcessor processor;

    auto& apvts = processor.getValueTreeState();
    auto modState = apvts.state.getOrCreateChildWithName (
        params::ModStateBridge::getModStateType(), nullptr);

    juce::ValueTree curve { params::ModStateBridge::getCurveType() };
    curve.setProperty ("index", 2, nullptr);
    modState.appendChild (curve, nullptr);

    CHECK (processor.getModState().getCurve (2).isDefaultRamp());
    CHECK (processor.getModState().getSnapshot().curves[2].evaluate (0.0f) == Approx (1.0f));
}

TEST_CASE ("Publishing a curve makes it visible to the audio thread", "[modstate]")
{
    GnarlProcessor processor;

    REQUIRE (processor.getModState().getSnapshot().curves[0].isDefaultRamp());

    processor.getModState().setCurve (0, drawnCurve());

    const auto& published = processor.getModState().getSnapshot().curves[0];

    CHECK_FALSE (published.isDefaultRamp());
    CHECK (published.evaluate (0.1f) == Approx (1.0f));
    CHECK (published.evaluate (0.4f) < 0.5f);
}

TEST_CASE ("A modulated patch reaches the engine and changes the sound", "[modstate][engine]")
{
    // The end-to-end check the whole of Phase 3 exists for: set up an LFO on
    // the filter cutoff through the PARAMETERS, play a note, and the output
    // must differ from the same note with the slot switched off. Every link in
    // the chain - parameter read, curve publication, matrix, per-chunk
    // evaluation - has to work for this to pass.
    const auto renderNote = [] (bool modulationEnabled)
    {
        GnarlProcessor processor;
        auto& apvts = processor.getValueTreeState();

        // A resonant low-pass that a cutoff sweep will obviously change.
        setParameter (apvts, pid::filter[0].enabled, 1.0f);
        setParameter (apvts, pid::filter[0].cutoff, 400.0f);
        setParameter (apvts, pid::filter[0].resonance, 0.6f);

        // A fast free-running saw, so one block covers a good part of a cycle.
        setParameter (apvts, pid::lfo[0].shape,
                      static_cast<float> (choices::LfoShape::sawUp));
        setParameter (apvts, pid::lfo[0].mode,
                      static_cast<float> (choices::LfoMode::trigger));
        setParameter (apvts, pid::lfo[0].syncEnabled, 0.0f);
        setParameter (apvts, pid::lfo[0].rateHz, 8.0f);

        setParameter (apvts, pid::modSlot[0].enabled, modulationEnabled ? 1.0f : 0.0f);
        setParameter (apvts, pid::modSlot[0].source,
                      static_cast<float> (choices::ModSource::lfo1));
        setParameter (apvts, pid::modSlot[0].depth, 0.5f);
        setParameter (apvts, pid::modSlot[0].bipolar, 0.0f);

        processor.getModState().setDestination (0, pid::filter[0].cutoff);

        processor.prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 36, 1.0f), 0);

        processor.processBlock (buffer, midi);

        juce::AudioBuffer<float> tail (2, 512);
        juce::MidiBuffer empty;

        std::vector<float> samples;

        for (int block = 0; block < 8; ++block)
        {
            processor.processBlock (tail, empty);

            for (int i = 0; i < tail.getNumSamples(); ++i)
                samples.push_back (tail.getSample (0, i));
        }

        return samples;
    };

    const auto withModulation = renderNote (true);
    const auto withoutModulation = renderNote (false);

    REQUIRE (withModulation.size() == withoutModulation.size());

    auto difference = 0.0;
    auto energy = 0.0;

    for (std::size_t i = 0; i < withModulation.size(); ++i)
    {
        difference += std::abs (withModulation[i] - withoutModulation[i]);
        energy += std::abs (withoutModulation[i]);
    }

    // The unmodulated render has to be audible in the first place, or the
    // comparison proves nothing.
    REQUIRE (energy > 1.0);

    // A cutoff sweep of half the modulation range is not subtle: anything
    // less than a few percent of the total energy would mean the modulation
    // never reached the filter.
    CHECK (difference > energy * 0.02);
}
