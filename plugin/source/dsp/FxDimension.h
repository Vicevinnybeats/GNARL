#pragma once

#include "DelayLine.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>

namespace gnarl::dsp
{

/**
    The FX rack's dimension: a stereo expander built from a short delay with
    inverted cross-feed.

    HOW IT WIDENS, and why it is built in MID/SIDE rather than as a
    cross-feed. The obvious construction - give each channel an inverted,
    delayed copy of the other - does not work, and the test caught it doing
    nothing: with a mono input the two channels are equal, so each gets the
    same copy subtracted and the two outputs come out IDENTICAL. A widener
    that cannot widen a mono signal is no use on a synth, where most patches
    are centred.

    So the effect is built where the width actually lives. The mid is passed
    through untouched and a delayed copy of it is added to the SIDE, with
    opposite sign on the two channels:

        mid'  = mid
        side' = side + crossFeed * delayed (mid)
        left  = mid' + side'      right = mid' - side'

    THAT IS WHAT MAKES IT MONO-SAFE, and the property is exact rather than
    approximate: summing the outputs to mono gives (left + right) / 2 = mid',
    which is the dry mid, whatever the input was. Everything the effect added
    lives in the side, and side content cancels in a mono sum by definition.

    A widener built by boosting the side channel of an already-stereo signal
    has the same mono behaviour but cannot widen a centred patch; one built by
    panning delayed copies apart widens a mono patch but loses level when
    summed. This construction does both, and for this music that matters -
    every club system and every phone speaker sums to mono.

    THE DELAY IS SHORT AND FIXED, 1 to 40 ms. Past about 40 ms the copy is
    heard as an echo rather than as space, and under 1 ms it is a comb filter
    on the mid. `width` scales the cross-feed and `amount` scales the wet
    path, so width at zero is exactly bit-transparent.

    Real-time safe.
*/
class FxDimension
{
public:
    struct Settings
    {
        bool enabled = false;
        float mix = 1.0f;
        /** 0..1 overall intensity. */
        float amount = 0.5f;
        /** 0..1 how much of the other channel is cross-fed. */
        float width = 0.7f;
        /** The cross-feed delay, in milliseconds. */
        float timeMs = 12.0f;
    };

    void prepare (double sampleRate)
    {
        sampleRateHz = sampleRate;

        const auto maximumSamples = static_cast<int> (
            std::ceil (sampleRate * kMaximumTimeSeconds)) + 8;

        midLine.prepare (maximumSamples);

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
        midLine.reset();
    }

    /** BLOCK-RATE. */
    void setSettings (const Settings& newSettings) noexcept
    {
        settings = newSettings;

        const auto seconds = juce::jlimit (kMinimumTimeSeconds, kMaximumTimeSeconds,
                                           static_cast<double> (settings.timeMs) * 0.001);

        delaySamples = static_cast<float> (seconds * sampleRateHz);

        crossFeed = juce::jlimit (0.0f, 1.0f, settings.width)
                  * juce::jlimit (0.0f, 1.0f, settings.amount);
    }

    /** SAMPLE-RATE, both channels at once: the effect is defined on the mid
        and side, which do not exist until both channels are in hand. */
    void processSample (float& left, float& right) noexcept
    {
        if (! settings.enabled || crossFeed <= 0.0f)
            return;

        /*  The early-out at zero cross-feed is not only an optimisation. A
            mid/side round trip is not bit-exact - (L+R)/2 + (L-R)/2 loses a
            unit in the last place - so without it an effect turned fully down
            still altered the signal, by one ULP per sample, forever. The test
            caught that. Fourteen of these sit in a chain most patches do not
            use, so "off" has to mean untouched rather than nearly untouched. */

        const auto dryLeft = left;
        const auto dryRight = right;

        const auto mid = (dryLeft + dryRight) * 0.5f;
        const auto side = (dryLeft - dryRight) * 0.5f;

        const auto delayedMid = midLine.read (delaySamples);
        midLine.write (mid);

        // Everything the effect adds goes in the SIDE. The mid is untouched,
        // which is what makes the mono sum exact.
        const auto widenedSide = side + delayedMid * crossFeed;

        const auto wetLeft = mid + widenedSide;
        const auto wetRight = mid - widenedSide;

        if (! std::isfinite (wetLeft) || ! std::isfinite (wetRight))
        {
            reset();
            return;
        }

        const auto amount = juce::jlimit (0.0f, 1.0f, settings.mix);

        left = dryLeft + (wetLeft - dryLeft) * amount;
        right = dryRight + (wetRight - dryRight) * amount;
    }

    const Settings& getSettings() const noexcept { return settings; }

private:
    /** Under a millisecond the cross-feed is a comb filter on the mono
        content; past 40 ms it is heard as an echo rather than as space. */
    static constexpr double kMinimumTimeSeconds = 0.001;
    static constexpr double kMaximumTimeSeconds = 0.040;

    double sampleRateHz = 44100.0;
    Settings settings {};

    float delaySamples = 1.0f;
    float crossFeed = 0.0f;

    // ONE line, on the mid. The side is built from it rather than delayed
    // itself: delaying the side as well would move the existing stereo
    // content in time, which smears a stereo source instead of widening it.
    DelayLine midLine;
};

} // namespace gnarl::dsp
