#pragma once

#include "DelayLine.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>

namespace gnarl::dsp
{

/**
    The FX rack's chorus: up to four modulated taps per channel.

    WHY THE VOICES ARE SPREAD IN LFO PHASE AND NOT JUST DETUNED. Four taps all
    modulated in step are one tap four times as loud - they sum coherently and
    the effect is a vibrato, not a chorus. What makes a chorus is the taps
    disagreeing about where they are, so each voice's LFO gets an equal share
    of a full cycle of phase offset.

    THE VOICE SUM IS SCALED BY THE ROOT OF THE COUNT, not by the count. The
    taps are neither identical nor independent: they are the same signal at
    different delays, so they sum somewhere between coherently (divide by n)
    and incoherently (divide by root n). Root n is the right choice for the
    perceived level because the taps decorrelate at exactly the frequencies
    where the ear is sensitive, and dividing by n makes adding voices quieter -
    which reads as the control doing the wrong thing.

    A STEREO PATH IS TWO PATHS: each channel has its own lines. The stereo
    IMAGE comes from offsetting the two channels' LFO phase by `spread`, which
    is what makes the movement arrive at different times on the two sides
    rather than making the same movement louder.

    Real-time safe.
*/
class FxChorus
{
public:
    static constexpr int kMaxVoices = 4;

    struct Settings
    {
        bool enabled = false;
        float mix = 0.4f;
        /** LFO rate in Hz. */
        float rateHz = 0.6f;
        /** 0..1 modulation depth. */
        float depth = 0.4f;
        /** 1..4. */
        int voices = 2;
        /** 0..1 stereo spread: the LFO phase offset between the channels. */
        float spread = 0.6f;
        /** 0..1. */
        float feedback = 0.0f;
    };

    /** MESSAGE THREAD. */
    void prepare (double sampleRate)
    {
        sampleRateHz = sampleRate;

        const auto maximumSamples = static_cast<int> (
            std::ceil (sampleRate * (kCentreSeconds + kMaximumDepthSeconds))) + 8;

        for (auto& channel : channels)
            for (auto& voice : channel.voices)
                voice.prepare (maximumSamples);

        setSettings (settings);
        reset();
    }

    void setSampleRate (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate;
        setSettings (settings);
    }

    void reset() noexcept
    {
        for (auto& channel : channels)
        {
            for (auto& voice : channel.voices)
                voice.reset();

            channel.feedbackState = 0.0f;
        }

        phase = 0.0f;
    }

    /** BLOCK-RATE. */
    void setSettings (const Settings& newSettings) noexcept
    {
        settings = newSettings;

        activeVoices = juce::jlimit (1, kMaxVoices, settings.voices);

        centreSamples = static_cast<float> (kCentreSeconds * sampleRateHz);
        depthSamples = static_cast<float> (
            juce::jlimit (0.0f, 1.0f, settings.depth) * kMaximumDepthSeconds * sampleRateHz);

        increment = static_cast<float> (
            juce::jlimit (0.01f, 20.0f, settings.rateHz) / sampleRateHz);

        feedbackAmount = juce::jlimit (0.0f, 1.0f, settings.feedback) * kMaximumFeedback;

        // Root n, not n - see the class comment.
        voiceScale = 1.0f / std::sqrt (static_cast<float> (activeVoices));
    }

    /** SAMPLE-RATE. */
    float processSample (int channelIndex, float input) noexcept
    {
        if (! settings.enabled)
            return input;

        const auto index = static_cast<std::size_t> (
            juce::jlimit (0, static_cast<int> (channels.size()) - 1, channelIndex));
        auto& channel = channels[index];

        // The two channels read the same LFO at different phases, so the phase
        // itself is advanced once per SAMPLE rather than once per channel -
        // advancing it per channel would make its rate depend on the channel
        // count.
        if (index == 0)
        {
            phase += increment;

            if (phase >= 1.0f)
                phase -= 1.0f;
        }

        const auto channelOffset = index == 0
                                 ? 0.0f
                                 : juce::jlimit (0.0f, 1.0f, settings.spread) * 0.5f;

        auto wet = 0.0f;

        for (int voice = 0; voice < activeVoices; ++voice)
        {
            // An equal share of a full cycle, so the taps disagree.
            const auto voiceOffset = static_cast<float> (voice)
                                   / static_cast<float> (activeVoices);

            const auto lfo = std::sin (juce::MathConstants<float>::twoPi
                                       * (phase + voiceOffset + channelOffset));

            wet += channel.voices[static_cast<std::size_t> (voice)].read (
                juce::jmax (1.0f, centreSamples + lfo * depthSamples));
        }

        wet *= voiceScale;

        for (int voice = 0; voice < activeVoices; ++voice)
            channel.voices[static_cast<std::size_t> (voice)].write (
                input + wet * feedbackAmount);

        if (! std::isfinite (wet))
        {
            reset();
            return input;
        }

        const auto amount = juce::jlimit (0.0f, 1.0f, settings.mix);
        return input + (wet - input) * amount;
    }

    const Settings& getSettings() const noexcept { return settings; }

private:
    struct Channel
    {
        std::array<DelayLine, kMaxVoices> voices {};
        float feedbackState = 0.0f;
    };

    /** The delay the taps sit around. 15 ms is the classic chorus window: long
        enough that the taps are heard as separate voices, short enough that
        they are not heard as echoes. */
    static constexpr double kCentreSeconds = 0.015;

    /** How far the modulation moves a tap. 10 ms at full depth, which is a
        detune of a few cents at these rates. */
    static constexpr double kMaximumDepthSeconds = 0.010;

    /** Feedback ceiling. Lower than the delay's: the taps are short, so the
        loop gain compounds far faster. */
    static constexpr float kMaximumFeedback = 0.7f;

    double sampleRateHz = 44100.0;
    Settings settings {};

    int activeVoices = 2;
    float centreSamples = 1.0f;
    float depthSamples = 0.0f;
    float increment = 0.0f;
    float phase = 0.0f;
    float feedbackAmount = 0.0f;
    float voiceScale = 1.0f;

    std::array<Channel, 2> channels {};
};

} // namespace gnarl::dsp
