#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>

namespace gnarl::dsp
{

/**
    A first-order all-pass in topology-preserving form, the building block of
    the phaser.

    WRITTEN AS A TPT LOW-PASS AND THEN REFLECTED: a first-order all-pass is
    `lowPass - highPass`, and for a one-pole `highPass = input - lowPass`, so
    the all-pass is `2 * lowPass - input`. Doing it that way rather than with
    the direct-form recursion is what makes the stage safe to modulate at audio
    rate, which the phaser does: a direct-form all-pass recalculated per sample
    is not stable (CLAUDE.md section 3), and the sweep IS the effect.
*/
class AllPassStage
{
public:
    void prepare (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate;
        reset();
        setFrequency (1000.0f);
    }

    void setSampleRate (double sampleRate) noexcept { sampleRateHz = sampleRate; }

    void reset() noexcept { state = 0.0f; }

    /** SAMPLE-RATE safe: the phaser sweeps this continuously. */
    void setFrequency (float frequencyHz) noexcept
    {
        const auto nyquist = static_cast<float> (sampleRateHz) * 0.5f;
        const auto clamped = juce::jlimit (10.0f, nyquist * 0.98f, frequencyHz);

        const auto prewarped = std::tan (juce::MathConstants<float>::pi * clamped
                                         / static_cast<float> (sampleRateHz));

        gain = prewarped / (1.0f + prewarped);
    }

    /** SAMPLE-RATE. */
    float processSample (float input) noexcept
    {
        const auto v = (input - state) * gain;
        const auto lowPass = v + state;

        state = lowPass + v;

        return 2.0f * lowPass - input;
    }

    bool hasBlownUp() const noexcept { return ! std::isfinite (state); }

private:
    double sampleRateHz = 44100.0;
    float gain = 0.5f;
    float state = 0.0f;
};

/**
    The FX rack's phaser: two to twelve swept all-pass stages per channel with
    bipolar feedback.

    A PHASER IS NOT A FLANGER, even though both are comb filters. A flanger's
    nulls are HARMONICALLY spaced, because a delay's comb repeats every 1/T Hz;
    a phaser's are spaced by where its all-pass stages sit, which is not
    harmonic and is why a phaser sounds like movement rather than like metal.
    That also means the stage count is the character: each PAIR of stages adds
    one null, so 4 stages give 2 notches and 12 give 6.

    THE STAGE COUNT IS ALLOWED TO BE ODD, which is deliberate. An odd count
    inverts the relationship between the dry and wet paths, so summing them
    cancels where an even count reinforces. That is a usable sound rather than
    a bug, which is why the parameter is 2..12 and not "even numbers only".

    Real-time safe. No delay line: the all-pass stages are the whole effect,
    which is also why this is the cheapest of the modulated effects.
*/
class FxPhaser
{
public:
    static constexpr int kMaxStages = 12;

    struct Settings
    {
        bool enabled = false;
        float mix = 0.5f;
        float rateHz = 0.3f;
        /** 0..1 sweep depth, in octaves around the centre. */
        float depth = 0.6f;
        /** 2..12. */
        int stages = 4;
        /** The centre of the sweep, in Hz. */
        float centreHz = 800.0f;
        /** -1..1. */
        float feedback = 0.2f;
        /** 0..1 LFO phase offset between the channels. */
        float stereo = 0.5f;
    };

    void prepare (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate;

        for (auto& channel : channels)
            for (auto& stage : channel.stages)
                stage.prepare (sampleRate);

        setSettings (settings);
        reset();
    }

    void setSampleRate (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate;

        for (auto& channel : channels)
            for (auto& stage : channel.stages)
                stage.setSampleRate (sampleRate);

        setSettings (settings);
    }

    void reset() noexcept
    {
        for (auto& channel : channels)
        {
            for (auto& stage : channel.stages)
                stage.reset();

            channel.feedbackState = 0.0f;
        }

        phase = 0.0f;
    }

    /** BLOCK-RATE. */
    void setSettings (const Settings& newSettings) noexcept
    {
        settings = newSettings;

        activeStages = juce::jlimit (2, kMaxStages, settings.stages);

        increment = static_cast<float> (
            juce::jlimit (0.01f, 20.0f, settings.rateHz) / sampleRateHz);

        // The sweep is measured in OCTAVES rather than in Hz, so it covers the
        // same musical distance wherever the centre is put. A sweep in Hz is
        // huge at 8 kHz and inaudible at 100.
        depthOctaves = juce::jlimit (0.0f, 1.0f, settings.depth) * kMaximumDepthOctaves;

        centreHz = juce::jlimit (20.0f, 18000.0f, settings.centreHz);

        feedbackAmount = juce::jlimit (-1.0f, 1.0f, settings.feedback) * kMaximumFeedback;
    }

    /** SAMPLE-RATE. */
    float processSample (int channelIndex, float input) noexcept
    {
        if (! settings.enabled)
            return input;

        const auto index = static_cast<std::size_t> (
            juce::jlimit (0, static_cast<int> (channels.size()) - 1, channelIndex));
        auto& channel = channels[index];

        /*  ONE SHARED LFO, advanced on channel 0 only: advancing it per
            channel would make its rate depend on the channel count.

            THIS REQUIRES THE CALLER TO INTERLEAVE THE CHANNELS. If it runs the
            whole left channel and then the whole right, the phase is already
            at the end of the block by the time channel 1 is processed, and the
            right channel's modulation freezes at a value set by the block
            size. FxRack::runPerChannel interleaves for exactly this reason,
            and the comment there records what it cost to find. */
        if (index == 0)
        {
            phase += increment;

            if (phase >= 1.0f)
                phase -= 1.0f;
        }

        const auto channelOffset = index == 0
                                 ? 0.0f
                                 : juce::jlimit (0.0f, 1.0f, settings.stereo) * 0.5f;

        const auto lfo = std::sin (juce::MathConstants<float>::twoPi
                                   * (phase + channelOffset));

        // Exponential in frequency, which is what "octaves" means.
        const auto frequency = centreHz * std::exp2 (lfo * depthOctaves);

        auto value = input + channel.feedbackState * feedbackAmount;

        for (int stage = 0; stage < activeStages; ++stage)
        {
            auto& allPass = channel.stages[static_cast<std::size_t> (stage)];

            // Every stage sits at the same frequency: staggering them spreads
            // the notches out, which is a different and much wider effect. One
            // frequency keeps the notches together, which is the classic sound.
            allPass.setFrequency (frequency);
            value = allPass.processSample (value);
        }

        channel.feedbackState = value;

        for (auto& stage : channel.stages)
        {
            if (stage.hasBlownUp())
            {
                reset();
                return input;
            }
        }

        const auto amount = juce::jlimit (0.0f, 1.0f, settings.mix);
        return input + (value - input) * amount;
    }

    const Settings& getSettings() const noexcept { return settings; }

private:
    struct Channel
    {
        std::array<AllPassStage, kMaxStages> stages {};
        float feedbackState = 0.0f;
    };

    /** Three octaves either side at full depth: enough to hear the notches
        travel across the whole of a growl's harmonic range. */
    static constexpr float kMaximumDepthOctaves = 3.0f;

    /** 0.9: the feedback path goes through every stage, so the loop gain at
        the resonant frequency is higher than the setting suggests. */
    static constexpr float kMaximumFeedback = 0.9f;

    double sampleRateHz = 44100.0;
    Settings settings {};

    int activeStages = 4;
    float increment = 0.0f;
    float phase = 0.0f;
    float depthOctaves = 0.0f;
    float centreHz = 800.0f;
    float feedbackAmount = 0.0f;

    std::array<Channel, 2> channels {};
};

} // namespace gnarl::dsp
