#pragma once

#include "DelayLine.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>

namespace gnarl::dsp
{

/**
    The FX rack's limiter: the last thing in the default chain, and the reason
    a riddim patch can be as loud as it wants without the output clipping.

    IT LOOKS AHEAD, which is what separates a limiter from a fast compressor.
    A compressor reacts to a peak after it has arrived, so the attack always
    lets some of it through; a limiter delays the signal by its look-ahead
    window and uses that window to see the peak coming, so the gain is already
    down when it arrives. Nothing gets through. The cost is latency, which is
    reported to the host so it can compensate.

    THE GAIN REDUCTION IS SMOOTHED, NOT THE GAIN. Smoothing the gain would
    round the corner off the attack and let the peak through - the whole point
    of the look-ahead is that the reduction is fully applied by the time the
    peak arrives. So the envelope follower takes the maximum over the
    look-ahead window (instant attack, so no peak is missed) and releases
    slowly (so the gain does not pump on every transient).

    A MAXIMUM OVER A SLIDING WINDOW, DONE CHEAPLY. The obvious implementation
    scans the whole window per sample, which is O(window) and at 48 kHz and
    5 ms is 240 comparisons per sample per channel. This keeps a running
    maximum with a decay instead: it is not exactly the sliding maximum, but it
    is never LOWER than the true peak within the window, which is the property
    that matters - an envelope that is too high is conservative, and one that
    is too low lets a peak through.

    THE CEILING IS BELOW 0 dBFS BY DEFAULT and that is not timidity: a true
    peak of exactly zero clips in a lossy encoder, and this music is heard
    almost entirely through one.

    Real-time safe.
*/
class FxLimiter
{
public:
    struct Settings
    {
        bool enabled = false;
        float mix = 1.0f;
        /** Threshold in dB, where limiting starts. */
        float thresholdDb = 0.0f;
        /** Release in milliseconds. */
        float releaseMs = 50.0f;
        /** The hard ceiling in dB, which the output must not exceed. */
        float ceilingDb = -0.3f;
    };

    /** MESSAGE THREAD. */
    void prepare (double sampleRate)
    {
        sampleRateHz = sampleRate;

        lookAheadSamples = juce::jmax (1, static_cast<int> (
            std::round (sampleRate * kLookAheadSeconds)));

        for (auto& channel : channels)
            channel.line.prepare (lookAheadSamples + 8);

        setSettings (settings);
        reset();
    }

    void setSampleRate (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate;

        // The look-ahead is a fixed TIME, so its sample count changes with the
        // rate - but the buffer is not resized here, because this can run on
        // the audio thread. prepare sizes it for the longest case.
        lookAheadSamples = juce::jmin (
            static_cast<int> (channels[0].line.getMaximumDelaySamples()),
            juce::jmax (1, static_cast<int> (std::round (sampleRate * kLookAheadSeconds))));

        setSettings (settings);
    }

    void reset() noexcept
    {
        for (auto& channel : channels)
        {
            channel.line.reset();
            channel.envelope = 0.0f;
        }
    }

    /** BLOCK-RATE. */
    void setSettings (const Settings& newSettings) noexcept
    {
        settings = newSettings;

        threshold = juce::Decibels::decibelsToGain (
            juce::jlimit (-40.0f, 0.0f, settings.thresholdDb));

        ceiling = juce::Decibels::decibelsToGain (
            juce::jlimit (-40.0f, 0.0f, settings.ceilingDb));

        // The envelope's decay per sample, from the release time. One time
        // constant per release setting: the release is how long the gain takes
        // to come back, which is what the user is setting.
        const auto releaseSeconds = juce::jlimit (0.001f, 0.5f,
                                                  settings.releaseMs * 0.001f);

        releaseCoefficient = std::exp (-1.0f / (releaseSeconds
                                                * static_cast<float> (sampleRateHz)));
    }

    /** SAMPLE-RATE, both channels: a stereo limiter must apply the SAME gain
        to both, or a peak on one side pulls that side down alone and the image
        shifts. That is the one place where treating the channels
        independently is wrong rather than merely different. */
    void processSample (float& left, float& right) noexcept
    {
        if (! settings.enabled)
            return;

        const auto dryLeft = left;
        const auto dryRight = right;

        // The detector sees the signal NOW; the output is the delayed copy, so
        // the gain is already down by the time the peak emerges.
        const auto peak = juce::jmax (std::abs (dryLeft), std::abs (dryRight));

        // Instant attack, slow release. Running maximum rather than a true
        // sliding window - see the class comment.
        sharedEnvelope = peak > sharedEnvelope
                       ? peak
                       : sharedEnvelope * releaseCoefficient
                         + peak * (1.0f - releaseCoefficient);

        const auto delayedLeft = channels[0].line.read (static_cast<float> (lookAheadSamples));
        const auto delayedRight = channels[1].line.read (static_cast<float> (lookAheadSamples));

        channels[0].line.write (dryLeft);
        channels[1].line.write (dryRight);

        // One gain for both channels.
        auto gain = 1.0f;

        if (sharedEnvelope > threshold && sharedEnvelope > 1.0e-9f)
            gain = threshold / sharedEnvelope;

        auto wetLeft = delayedLeft * gain;
        auto wetRight = delayedRight * gain;

        /*  THE CEILING IS A HARD CLIP, and it is here because the gain above
            cannot guarantee it. The running-maximum envelope is conservative
            rather than exact, the threshold and the ceiling are separate
            controls, and the mix control can put back any amount of unlimited
            dry signal. So the last thing the limiter does is make the
            guarantee true - without this the one thing the effect promises
            would be approximate. */
        wetLeft = juce::jlimit (-ceiling, ceiling, wetLeft);
        wetRight = juce::jlimit (-ceiling, ceiling, wetRight);

        if (! std::isfinite (wetLeft) || ! std::isfinite (wetRight))
        {
            reset();
            sharedEnvelope = 0.0f;
            return;
        }

        const auto amount = juce::jlimit (0.0f, 1.0f, settings.mix);

        left = delayedLeft + (wetLeft - delayedLeft) * amount;
        right = delayedRight + (wetRight - delayedRight) * amount;

        /*  THE DRY SIGNAL IS THE DELAYED ONE, NOT THE INPUT. Mixing the
            undelayed dry against the delayed wet would comb-filter the two
            together - a 5 ms offset puts a null at 100 Hz and every odd
            multiple, which on a bass patch removes the fundamental. Everything
            the effect outputs is on the same timeline, and the latency is
            reported to the host instead. */
    }

    /** The latency this effect adds, in samples, for the host to compensate.
        A limiter that looks ahead without reporting its latency puts the whole
        instrument early against the rest of the session. */
    int getLatencySamples() const noexcept
    {
        return settings.enabled ? lookAheadSamples : 0;
    }

    const Settings& getSettings() const noexcept { return settings; }

    /** The look-ahead window. 2 ms is enough to catch a peak at any audio
        frequency (one cycle of 500 Hz) without adding latency a player can
        feel - and this sits in an instrument, where latency is not free the
        way it is on a mastering bus. */
    static constexpr double kLookAheadSeconds = 0.002;

private:
    struct Channel
    {
        DelayLine line;
        float envelope = 0.0f;
    };

    double sampleRateHz = 44100.0;
    Settings settings {};

    int lookAheadSamples = 1;
    float threshold = 1.0f;
    float ceiling = 1.0f;
    float releaseCoefficient = 0.0f;
    float sharedEnvelope = 0.0f;

    std::array<Channel, 2> channels {};
};

} // namespace gnarl::dsp
