#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"
#include "params/ParameterIDs.h"

#include <cmath>

using namespace gnarl;

namespace
{
    /** Buffer sizes a host will actually hand us, including the nasty ones. */
    constexpr int kBlockSizes[] = { 1, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096 };
    constexpr double kSampleRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };

    bool bufferIsFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* data = buffer.getReadPointer (ch);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (! std::isfinite (data[i]))
                    return false;
        }

        return true;
    }

    /** Fills the buffer with garbage, so a processor that forgets to clear is
        caught instead of passing on an already-zeroed buffer. */
    void fillWithNoise (juce::AudioBuffer<float>& buffer, juce::Random& rng)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.getWritePointer (ch)[i] = rng.nextFloat() * 2.0f - 1.0f;
    }
}

TEST_CASE ("Processor constructs and reports a synth layout", "[processor]")
{
    GnarlProcessor processor;

    CHECK (processor.acceptsMidi());
    CHECK_FALSE (processor.producesMidi());
    CHECK_FALSE (processor.isMidiEffect());
    CHECK (processor.getTotalNumInputChannels() == 0);
    CHECK (processor.getTotalNumOutputChannels() == 2);
}

TEST_CASE ("Parameter layout exposes the declared parameters", "[params]")
{
    GnarlProcessor processor;
    auto& apvts = processor.getValueTreeState();

    REQUIRE (apvts.getParameter (pid::masterGain) != nullptr);

    // Every host-visible parameter must be reachable by its ID. A null here
    // means ParameterIDs.h drifted from createParameterLayout().
    for (auto* param : processor.getParameters())
    {
        auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (param);
        REQUIRE (withId != nullptr);
        CHECK (apvts.getParameter (withId->paramID) != nullptr);
    }
}

TEST_CASE ("Phase 0 output is exactly silence", "[processor][audio]")
{
    GnarlProcessor processor;
    processor.setPlayConfigDetails (0, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    juce::Random rng (0x9117);

    fillWithNoise (buffer, rng);
    processor.processBlock (buffer, midi);

    CHECK (bufferIsFinite (buffer));

    // Exactly zero, not "quiet": a synth with no active voices must write
    // silence, and anything non-zero here means the buffer was not cleared.
    CHECK (buffer.getMagnitude (0, buffer.getNumSamples()) == 0.0f);
}

TEST_CASE ("Output stays finite across every sample rate and block size",
           "[processor][audio]")
{
    juce::Random rng (0x4242);

    for (const auto sampleRate : kSampleRates)
    {
        for (const auto blockSize : kBlockSizes)
        {
            GnarlProcessor processor;
            processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
            processor.prepareToPlay (sampleRate, blockSize);

            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;

            for (int block = 0; block < 8; ++block)
            {
                fillWithNoise (buffer, rng);
                processor.processBlock (buffer, midi);

                REQUIRE (bufferIsFinite (buffer));
            }
        }
    }
}

TEST_CASE ("Sample rate and block size changes mid-stream are safe",
           "[processor][audio]")
{
    GnarlProcessor processor;
    juce::MidiBuffer midi;
    juce::Random rng (0x1234);

    // Hosts really do this: change the device while the plugin is loaded.
    const std::pair<double, int> configs[] = {
        { 44100.0, 512 }, { 96000.0, 64 }, { 48000.0, 2048 }, { 44100.0, 1 }
    };

    for (const auto& [rate, size] : configs)
    {
        processor.setPlayConfigDetails (0, 2, rate, size);
        processor.prepareToPlay (rate, size);

        juce::AudioBuffer<float> buffer (2, size);
        fillWithNoise (buffer, rng);
        processor.processBlock (buffer, midi);

        REQUIRE (bufferIsFinite (buffer));
    }
}

TEST_CASE ("State survives a save/load round trip", "[state]")
{
    juce::MemoryBlock saved;
    float savedValue = 0.0f;

    {
        GnarlProcessor processor;
        auto* master = processor.getValueTreeState().getParameter (pid::masterGain);
        REQUIRE (master != nullptr);

        master->setValueNotifyingHost (0.31f);
        savedValue = master->getValue();

        processor.getStateInformation (saved);
        REQUIRE (saved.getSize() > 0);
    }

    {
        GnarlProcessor restored;
        restored.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));

        auto* master = restored.getValueTreeState().getParameter (pid::masterGain);
        REQUIRE (master != nullptr);
        CHECK (master->getValue() == Catch::Approx (savedValue).margin (1.0e-5));
    }
}

TEST_CASE ("Garbage state is rejected without crashing", "[state]")
{
    GnarlProcessor processor;

    const char junk[] = "this is not a GNARL preset";
    processor.setStateInformation (junk, static_cast<int> (sizeof (junk)));

    // Nothing should have changed, and nothing should have thrown.
    CHECK (processor.getValueTreeState().getParameter (pid::masterGain) != nullptr);

    processor.setStateInformation (nullptr, 0);
    SUCCEED ("null state handled");
}

TEST_CASE ("A parameter sweep never produces a non-finite sample",
           "[processor][audio][params]")
{
    GnarlProcessor processor;
    processor.setPlayConfigDetails (0, 2, 48000.0, 256);
    processor.prepareToPlay (48000.0, 256);

    auto* master = processor.getValueTreeState().getParameter (pid::masterGain);
    REQUIRE (master != nullptr);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    juce::Random rng (0x7777);

    for (int step = 0; step <= 100; ++step)
    {
        master->setValueNotifyingHost (static_cast<float> (step) / 100.0f);

        fillWithNoise (buffer, rng);
        processor.processBlock (buffer, midi);

        REQUIRE (bufferIsFinite (buffer));
    }
}
