#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"
#include "dsp/Modulation.h"
#include "params/ModStateBridge.h"
#include "params/ParameterIDs.h"
#include "preset/FactoryBank.h"

#include <cmath>

using namespace gnarl;

/*
    THE BUG THIS FILE EXISTS FOR.

    Every preset in the bank enabled a mod slot, gave it a depth, and left
    both its SOURCE and its DESTINATION at `none`. "Triplet Growl" - the
    patch the product exists for, whose own comment calls it that - did not
    growl. It was a static formant filter with an LFO running next to it,
    connected to nothing.

    Nothing caught it because a destination is not a parameter (it is a
    ValueTree string, for the reasons in CLAUDE.md section 4) and the factory
    bank was built only from parameter overrides. Every parameter it set was
    set correctly. The bank round-tripped, recalled, and rendered without a
    NaN. It just did not modulate.

    Same shape as the synth that passed every unit test and made no sound:
    the pieces were right and the connection was missing. So these assertions
    are about the CONNECTION, and the last one measures the sound rather than
    inspecting the tree - because a destination string that is spelled right
    and maps to nothing would satisfy every check above it.
*/

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;

    juce::ValueTree modStateOf (const juce::ValueTree& preset)
    {
        return preset.getChildWithName ("GNARL")
                     .getChildWithName (params::ModStateBridge::getModStateType());
    }
}

TEST_CASE ("Every enabled mod slot has a source and a destination", "[factory]")
{
    GnarlProcessor processor;

    auto& apvts = processor.getValueTreeState();
    const auto definitions = preset::FactoryBank::getDefinitions();
    const auto bank = preset::FactoryBank::build (apvts, apvts.copyState());

    REQUIRE (bank.size() == definitions.size());

    for (std::size_t i = 0; i < definitions.size(); ++i)
    {
        const auto& definition = definitions[i];

        INFO ("preset: " << definition.name);

        // Which slots the patch switches on, and with what depth.
        for (std::size_t slot = 0; slot < pid::kNumModSlots; ++slot)
        {
            auto enabled = false;
            auto depth = 0.0f;
            auto source = static_cast<float> (choices::ModSource::none);

            for (const auto& setting : definition.settings)
            {
                if (juce::String (setting.id) == pid::modSlot[slot].enabled)
                    enabled = setting.value > 0.5f;
                else if (juce::String (setting.id) == pid::modSlot[slot].depth)
                    depth = setting.value;
                else if (juce::String (setting.id) == pid::modSlot[slot].source)
                    source = setting.value;
            }

            if (! enabled)
                continue;

            INFO ("mod slot " << slot);

            /*  A slot that is ON with a depth and no source, or no
                destination, is the bug. Each of the three is individually
                harmless and the combination does nothing at all. */
            CHECK (std::abs (depth) > 0.0f);
            CHECK (source != static_cast<float> (choices::ModSource::none));

            const char* destination = nullptr;

            for (const auto& routing : definition.routings)
                if (routing.slot == static_cast<int> (slot))
                    destination = routing.destination;

            REQUIRE (destination != nullptr);

            // And it must be an ID the engine actually resolves. A typo here
            // maps to `none`, which is exactly as silent as no routing at all.
            CHECK (dsp::getDestinationForParameterID (destination)
                     != dsp::ModDestination::none);
        }
    }
}

TEST_CASE ("A routed preset carries its modulation into the tree", "[factory]")
{
    GnarlProcessor processor;

    auto& apvts = processor.getValueTreeState();
    const auto definitions = preset::FactoryBank::getDefinitions();
    const auto bank = preset::FactoryBank::build (apvts, apvts.copyState());

    for (std::size_t i = 0; i < definitions.size(); ++i)
    {
        const auto& definition = definitions[i];

        if (definition.routings.empty() && definition.curves.empty())
            continue;

        INFO ("preset: " << definition.name);

        const auto modState = modStateOf (bank[i]);

        // This is the assertion that was false for the whole bank: there was
        // no MODSTATE child at all.
        REQUIRE (modState.isValid());

        for (const auto& routing : definition.routings)
        {
            auto found = false;

            for (auto child : modState)
                if (child.hasType (params::ModStateBridge::getSlotType())
                    && static_cast<int> (child["index"]) == routing.slot)
                {
                    found = true;
                    CHECK (child["destination"].toString()
                             == juce::String (routing.destination));
                }

            INFO ("routing for slot " << routing.slot);
            CHECK (found);
        }

        for (const auto& curve : definition.curves)
        {
            auto points = 0;

            for (auto child : modState)
                if (child.hasType (params::ModStateBridge::getCurveType())
                    && static_cast<int> (child["index"]) == curve.lfo)
                    points = child.getNumChildren();

            INFO ("curve for LFO " << curve.lfo);

            // An empty LFOCURVE element means "the default" to the bridge,
            // not "this shape", so a curve that lost its points would load
            // as a plain ramp and look like a taste problem.
            CHECK (points == static_cast<int> (curve.points.size()));
        }
    }
}

TEST_CASE ("Removing a preset's routing changes what it sounds like", "[factory]")
{
    /*  THE ONE THAT MEASURES THE SOUND, second attempt - and the first
        attempt is the point.

        That version held a note, measured the RMS of successive windows, and
        asserted the level moved by more than a decibel over the LFO's cycle.
        It passed. It also passed with the modulation deliberately removed,
        because Yoi Growl runs two unison voices detuned by eight cents and
        two detuned voices BEAT - the level rises and falls on its own, with
        no LFO involved at all. It was a test of the unison, not of the
        modulation. Same family as the meter test that measured a sustained
        note instead of the decay it claimed to measure.

        What actually holds is a difference: render the patch as defined,
        render it again with its MODSTATE stripped - which is precisely the
        bug, and leaves every parameter, every envelope and the unison
        identical - and the two must not come out the same. Nothing else in
        the patch changes, so nothing else can explain a difference. */
    const auto renderPreset = [] (const juce::ValueTree& state)
    {
        GnarlProcessor processor;
        processor.applyPresetState (state);
        processor.prepareToPlay (kSampleRate, kBlockSize);

        juce::AudioBuffer<float> buffer (2, kBlockSize);
        std::vector<float> samples;

        for (int block = 0; block < 200; ++block)
        {
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 36, 1.0f), 0);

            buffer.clear();
            processor.processBlock (buffer, midi);

            // Past the attack, so the comparison is of the sustained patch.
            if (block < 40)
                continue;

            for (int i = 0; i < kBlockSize; ++i)
                samples.push_back (buffer.getSample (0, i));
        }

        return samples;
    };

    GnarlProcessor processor;

    auto& apvts = processor.getValueTreeState();
    const auto definitions = preset::FactoryBank::getDefinitions();
    const auto bank = preset::FactoryBank::build (apvts, apvts.copyState());

    std::size_t index = definitions.size();

    for (std::size_t i = 0; i < definitions.size(); ++i)
        if (juce::String (definitions[i].name) == "Yoi Growl")
            index = i;

    REQUIRE (index < definitions.size());

    auto routed = bank[index].getChildWithName ("GNARL").createCopy();

    auto unrouted = routed.createCopy();
    unrouted.removeChild (
        unrouted.getChildWithName (params::ModStateBridge::getModStateType()), nullptr);

    const auto withModulation = renderPreset (routed);
    const auto control = renderPreset (routed);
    const auto without = renderPreset (unrouted);

    REQUIRE (withModulation.size() == without.size());
    REQUIRE (! withModulation.empty());

    auto signal = 0.0;
    auto difference = 0.0;

    for (std::size_t i = 0; i < withModulation.size(); ++i)
    {
        const auto a = static_cast<double> (withModulation[i]);
        const auto b = static_cast<double> (without[i]);

        signal += a * a;
        difference += (a - b) * (a - b);
    }

    REQUIRE (signal > 0.0);

    const auto relative = std::sqrt (difference / signal);

    /*  THE CONTROL, and it is not decoration. Rendering the same state twice
        must give exactly zero, or the difference measured above could be
        coming from anywhere - a random per-voice seed, the analog drift, an
        uninitialised tail. It measures 0.0 exactly, so the only thing that
        can explain a non-zero figure is the modulation. */
    auto controlDiff = 0.0;
    for (std::size_t i = 0; i < withModulation.size(); ++i)
    {
        const auto d = static_cast<double> (withModulation[i]) - control[i];
        controlDiff += d * d;
    }
    const auto controlRelative = std::sqrt (controlDiff / signal);

    INFO ("relative difference: " << relative);
    INFO ("control (same state twice): " << controlRelative);

    CHECK (controlRelative == 0.0);

    /*  The threshold is set by the SEPARATION, not by the measurement.

        Routed, this measures about 0.06. With the bug it is exactly 0.0 and
        the two renders are bit-identical, because stripping a MODSTATE that
        was never written changes nothing at all. So any positive threshold
        separates the two cases.

        0.02 sits three times below the measurement and infinitely above the
        bug, and is deliberately NOT set just under 0.06: this asserts "the
        routing changes the sound", and tightening it to today's figure
        would turn every future rebalance of this patch into a test failure
        about something the test is not asking. */
    CHECK (relative > 0.02);
}
