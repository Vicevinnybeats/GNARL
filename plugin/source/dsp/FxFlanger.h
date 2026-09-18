#pragma once

#include "DelayLine.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>

namespace gnarl::dsp
{

/**
    The FX rack's flanger: one very short modulated tap per channel, with
    bipolar feedback.

    A FLANGER IS NOT A SHORT CHORUS, and the difference is what the effect is
    made of. A chorus is heard as several voices, so its taps sit at 15 ms and
    are summed at partial strength. A flanger is heard as a comb filter: one
    tap under 10 ms, mixed at full strength with the dry signal, so the two
    interfere and produce a series of nulls that sweep. That is why this has a
    `manual` control and the chorus does not - the position of the comb is the
    sound.

    THE FEEDBACK IS BIPOLAR, and that is the single most important control
    here. A negative feedback flanger has its nulls where a positive one has
    its peaks; the classic jet-plane sweep is the negative one, and the two
    sound nothing alike. A unipolar feedback knob would hide half the effect.

    Real-time safe.
*/
class FxFlanger
{
public:
    struct Settings
    {
        bool enabled = false;
        float mix = 0.4f;
        float rateHz = 0.25f;
        /** 0..1 modulation depth. */
        float depth = 0.5f;
        /** -1..1. Negative inverts the comb - see the class comment. */
        float feedback = 0.3f;
        /** 0..1 base delay, which sets where the comb sits. */
        float manual = 0.2f;
        /** 0..1 LFO phase offset between the channels. */
        float stereo = 0.5f;
    };

    void prepare (double sampleRate)
    {
        sampleRateHz = sampleRate;

        const auto maximumSamples = static_cast<int> (
            std::ceil (sampleRate * kMaximumDelaySeconds)) + 8;

        for (auto& channel : channels)
            channel.line.prepare (maximumSamples);

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
            channel.line.reset();
            channel.feedbackState = 0.0f;
        }

        phase = 0.0f;
    }

    /** BLOCK-RATE. */
    void setSettings (const Settings& newSettings) noexcept
    {
        settings = newSettings;

        // The manual control sets the base delay, and the modulation swings
        // around it. Both are kept inside the buffer: a base at the top of its
        // range plus full depth would otherwise read past the end.
        const auto manual = juce::jlimit (0.0f, 1.0f, settings.manual);
        baseSamples = static_cast<float> (
            juce::jmap (static_cast<double> (manual),
                        kMinimumDelaySeconds, kMaximumDelaySeconds * 0.5) * sampleRateHz);

        depthSamples = static_cast<float> (
            juce::jlimit (0.0f, 1.0f, settings.depth)
            * kMaximumDelaySeconds * 0.45 * sampleRateHz);

        increment = static_cast<float> (
            juce::jlimit (0.01f, 20.0f, settings.rateHz) / sampleRateHz);

        // Signed, and clamped short of unity on both sides: a comb with a loop
        // gain of one rings forever whichever way round it is.
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

        const auto wet = channel.line.read (
            juce::jmax (1.0f, baseSamples + lfo * depthSamples));

        channel.line.write (input + wet * feedbackAmount);

        if (! std::isfinite (wet))
        {
            reset();
            return input;
        }

        // Mixed at full strength against the dry signal, which is what makes
        // the comb rather than a second voice.
        const auto amount = juce::jlimit (0.0f, 1.0f, settings.mix);
        return input + (wet - input) * amount;
    }

    const Settings& getSettings() const noexcept { return settings; }

private:
    struct Channel
    {
        DelayLine line;
        float feedbackState = 0.0f;
    };

    /** A flanger's comb lives between about half a millisecond and ten. Past
        that the nulls are close enough together to read as a delay. */
    static constexpr double kMinimumDelaySeconds = 0.0005;
    static constexpr double kMaximumDelaySeconds = 0.010;

    /** 0.95: a comb with unity loop gain rings forever in either polarity. */
    static constexpr float kMaximumFeedback = 0.95f;

    double sampleRateHz = 44100.0;
    Settings settings {};

    float baseSamples = 1.0f;
    float depthSamples = 0.0f;
    float increment = 0.0f;
    float phase = 0.0f;
    float feedbackAmount = 0.0f;

    std::array<Channel, 2> channels {};
};

} // namespace gnarl::dsp
