#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"
#include "params/ParameterChoices.h"
#include "params/ParameterIDs.h"
#include "params/ParameterLayout.h"

#include <set>
#include <string>
#include <vector>

using namespace gnarl;

namespace
{
    /** Every ID the header declares, in one list, so a test can sweep them.

        THIS LIST GOES STALE, and it went stale when the FX section landed: it
        is the only place in the project that enumerates parameters by hand.
        It is worth keeping anyway, because it is what catches an ID that is
        declared in the header and never added to the layout - a dead ID that
        nothing else notices, since the layout is what the APVTS is built from.
        Adding a family means adding it here too, and the count assertion below
        is what says so. */
    std::vector<std::string> allDeclaredIDs()
    {
        std::vector<std::string> ids;

        const auto add = [&ids] (const char* id) { ids.emplace_back (id); };

        for (std::size_t i = 0; i < pid::kNumOscillators; ++i)
        {
            const auto& p = pid::osc[i];
            for (const auto* id : { p.enabled, p.mode, p.wavetable, p.tablePos,
                                    p.pitchSemi, p.pitchFine, p.phase, p.phaseRandom,
                                    p.pan, p.level, p.unisonVoices, p.unisonDetune,
                                    p.unisonBlend, p.unisonSpread, p.warpMode,
                                    p.warpAmount, p.grainSize, p.grainDensity,
                                    p.grainPosJitter, p.grainPitchJitter,
                                    p.sendFilter1, p.sendFilter2, p.sendDirect })
                add (id);
        }

        for (const auto* id : { pid::sub.enabled, pid::sub.waveform, pid::sub.octave,
                                pid::sub.pitchFine, pid::sub.phase, pid::sub.pan,
                                pid::sub.level, pid::sub.sendFilter1,
                                pid::sub.sendFilter2, pid::sub.sendDirect })
            add (id);

        for (const auto* id : { pid::noise.enabled, pid::noise.type, pid::noise.level,
                                pid::noise.pan, pid::noise.pitchSemi, pid::noise.pitchFine,
                                pid::noise.phaseRandom, pid::noise.sendFilter1,
                                pid::noise.sendFilter2, pid::noise.sendDirect })
            add (id);

        for (const auto* id : { pid::ott.enabled, pid::ott.depth, pid::ott.time,
                                pid::ott.mix, pid::ott.inputGain, pid::ott.outputGain,
                                pid::ott.crossoverLow, pid::ott.crossoverHigh,
                                pid::ott.lowGain, pid::ott.midGain, pid::ott.highGain,
                                pid::ott.lowUpward, pid::ott.midUpward,
                                pid::ott.highUpward, pid::ott.lowDownward,
                                pid::ott.midDownward, pid::ott.highDownward })
            add (id);

        for (std::size_t i = 0; i < pid::kNumFilters; ++i)
        {
            const auto& p = pid::filter[i];
            for (const auto* id : { p.enabled, p.type, p.cutoff, p.resonance, p.drive,
                                    p.driveCurve, p.mix, p.keyTrack, p.formantX,
                                    p.formantY, p.formantThroat, p.combFeedback,
                                    p.combDamping })
                add (id);
        }

        for (std::size_t i = 0; i < pid::kNumEnvelopes; ++i)
        {
            const auto& p = pid::envelope[i];
            for (const auto* id : { p.mode, p.delay, p.attack, p.hold, p.decay,
                                    p.sustain, p.release, p.attackCurve, p.decayCurve,
                                    p.releaseCurve, p.velocityAmount })
                add (id);
        }

        for (std::size_t i = 0; i < pid::kNumLfos; ++i)
        {
            const auto& p = pid::lfo[i];
            for (const auto* id : { p.shape, p.syncEnabled, p.rateHz, p.rateDivision,
                                    p.mode, p.phase, p.smooth, p.gridDivision, p.bipolar })
                add (id);
        }

        for (std::size_t i = 0; i < pid::kNumModSlots; ++i)
        {
            const auto& p = pid::modSlot[i];
            for (const auto* id : { p.enabled, p.source, p.depth, p.curve,
                                    p.auxSource, p.auxAmount, p.bipolar })
                add (id);
        }

        for (std::size_t i = 0; i < pid::kNumMacros; ++i)
            add (pid::macro[i]);

        for (const auto* id : { pid::masterGain, pid::bypass, pid::maxVoices,
                                pid::polyMode, pid::glideTime, pid::glideAlways,
                                pid::pitchBendRange, pid::oversampling,
                                pid::velocityCurve, pid::analogDrift,
                                pid::filterRouting })
            add (id);

        // --- FX ------------------------------------------------------------
        for (std::size_t i = 0; i < pid::kNumFxDistortions; ++i)
        {
            const auto& p = pid::fxDistortion[i];
            for (const auto* id : { p.enabled, p.mix, p.type, p.drive, p.tone,
                                    p.bias, p.output })
                add (id);
        }

        for (std::size_t i = 0; i < pid::kNumFxEqs; ++i)
        {
            const auto& p = pid::fxEq[i];
            for (const auto* id : { p.enabled, p.mix, p.highPassFreq,
                                    p.lowShelfFreq, p.lowShelfGain,
                                    p.band1Freq, p.band1Gain, p.band1Q,
                                    p.band2Freq, p.band2Gain, p.band2Q,
                                    p.highShelfFreq, p.highShelfGain,
                                    p.lowPassFreq })
                add (id);
        }

        for (std::size_t i = 0; i < pid::kNumFxFilters; ++i)
        {
            const auto& p = pid::fxFilter[i];
            for (const auto* id : { p.enabled, p.mix, p.type, p.cutoff,
                                    p.resonance, p.drive })
                add (id);
        }

        for (const auto* id : { pid::fxDelay.enabled, pid::fxDelay.mix,
                                pid::fxDelay.syncEnabled, pid::fxDelay.division,
                                pid::fxDelay.timeMs, pid::fxDelay.feedback,
                                pid::fxDelay.pingPong, pid::fxDelay.width,
                                pid::fxDelay.lowCut, pid::fxDelay.highCut,
                                pid::fxDelay.modRate, pid::fxDelay.modDepth })
            add (id);

        for (const auto* id : { pid::fxReverb.enabled, pid::fxReverb.mix,
                                pid::fxReverb.size, pid::fxReverb.decay,
                                pid::fxReverb.damping, pid::fxReverb.preDelay,
                                pid::fxReverb.width, pid::fxReverb.lowCut,
                                pid::fxReverb.highCut, pid::fxReverb.modDepth })
            add (id);

        for (const auto* id : { pid::fxChorus.enabled, pid::fxChorus.mix,
                                pid::fxChorus.rate, pid::fxChorus.depth,
                                pid::fxChorus.voices, pid::fxChorus.spread,
                                pid::fxChorus.feedback })
            add (id);

        for (const auto* id : { pid::fxFlanger.enabled, pid::fxFlanger.mix,
                                pid::fxFlanger.rate, pid::fxFlanger.depth,
                                pid::fxFlanger.feedback, pid::fxFlanger.manual,
                                pid::fxFlanger.stereo })
            add (id);

        for (const auto* id : { pid::fxPhaser.enabled, pid::fxPhaser.mix,
                                pid::fxPhaser.rate, pid::fxPhaser.depth,
                                pid::fxPhaser.stages, pid::fxPhaser.centre,
                                pid::fxPhaser.feedback, pid::fxPhaser.stereo })
            add (id);

        for (const auto* id : { pid::fxHyper.enabled, pid::fxHyper.mix,
                                pid::fxHyper.amount, pid::fxHyper.detune,
                                pid::fxHyper.voices, pid::fxHyper.width })
            add (id);

        for (const auto* id : { pid::fxDimension.enabled, pid::fxDimension.mix,
                                pid::fxDimension.amount, pid::fxDimension.width,
                                pid::fxDimension.timeMs })
            add (id);

        for (const auto* id : { pid::fxLimiter.enabled, pid::fxLimiter.mix,
                                pid::fxLimiter.threshold, pid::fxLimiter.release,
                                pid::fxLimiter.ceiling })
            add (id);

        return ids;
    }
}

TEST_CASE ("Choice lists match their enums", "[params]")
{
    // A drifted list maps a parameter index onto a mode the engine does not
    // have, which surfaces as a filter type that silently does nothing.
    CHECK (choices::choiceListsAreConsistent());
}

TEST_CASE ("Every declared ID exists in the layout", "[params]")
{
    GnarlProcessor processor;
    auto& apvts = processor.getValueTreeState();

    for (const auto& id : allDeclaredIDs())
    {
        INFO ("parameter id: " << id);
        CHECK (apvts.getParameter (id) != nullptr);
    }
}

TEST_CASE ("No duplicate parameter IDs", "[params]")
{
    const auto ids = allDeclaredIDs();
    const std::set<std::string> unique (ids.begin(), ids.end());

    // A duplicate would mean two controls share one value, which the plugin
    // would not report as an error anywhere.
    CHECK (unique.size() == ids.size());
}

TEST_CASE ("The layout declares exactly the expected parameter count", "[params]")
{
    GnarlProcessor processor;

    // A change here is either intentional (bump getDeclaredParameterCount) or
    // an accidental addition that will break preset compatibility.
    CHECK (processor.getParameters().size() == params::getDeclaredParameterCount());
    CHECK (static_cast<int> (allDeclaredIDs().size()) == params::getDeclaredParameterCount());
}

TEST_CASE ("Every parameter has a name and a default inside its range", "[params]")
{
    GnarlProcessor processor;

    for (auto* param : processor.getParameters())
    {
        auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (param);
        REQUIRE (withID != nullptr);

        INFO ("parameter id: " << withID->paramID);

        CHECK (withID->getName (128).isNotEmpty());

        // A default outside 0..1 normalised means the default value is outside
        // the declared range, which a host will clamp differently from us.
        const auto defaultValue = param->getDefaultValue();
        CHECK (defaultValue >= 0.0f);
        CHECK (defaultValue <= 1.0f);
    }
}

TEST_CASE ("Every parameter reports a text value at the range extremes", "[params]")
{
    GnarlProcessor processor;

    for (auto* param : processor.getParameters())
    {
        auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (param);
        REQUIRE (withID != nullptr);
        INFO ("parameter id: " << withID->paramID);

        // Hosts show this text in their automation lanes. An empty string
        // there looks like a broken plugin.
        CHECK (param->getText (0.0f, 64).isNotEmpty());
        CHECK (param->getText (1.0f, 64).isNotEmpty());
    }
}

TEST_CASE ("A full parameter sweep produces no non-finite output", "[params][audio]")
{
    GnarlProcessor processor;
    processor.setPlayConfigDetails (0, 2, 48000.0, 256);
    processor.prepareToPlay (48000.0, 256);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 48, 1.0f), 0);

    // Every parameter driven to both extremes, one at a time, with a note
    // sounding. This is the cheapest test that catches a range whose endpoint
    // divides by zero.
    for (auto* param : processor.getParameters())
    {
        const auto original = param->getValue();

        for (const auto value : { 0.0f, 1.0f, 0.5f })
        {
            param->setValueNotifyingHost (value);

            buffer.clear();
            processor.processBlock (buffer, midi);
            midi.clear();

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    REQUIRE (std::isfinite (buffer.getReadPointer (ch)[i]));
        }

        param->setValueNotifyingHost (original);
    }
}
