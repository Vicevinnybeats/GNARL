#pragma once

#include "DelayLine.h"
#include "SyncRates.h"
#include "../params/ParameterChoices.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>

namespace gnarl::dsp
{

/**
    The FX rack's delay: tempo-syncable, ping-pong capable, filtered and
    modulated in its feedback path.

    THE FEEDBACK PATH IS FILTERED, WHICH IS WHAT MAKES REPEATS DECAY IN TONE
    rather than just in level. An unfiltered delay repeats a bright signal
    brightly forever, which no physical echo does and which turns a long
    feedback setting into a wash of high frequencies. The tone controls
    therefore sit INSIDE the loop, and they are one-pole on purpose: a steeper
    filter's phase response accumulates over the repeats.

    THE FEEDBACK IS CLAMPED BELOW UNITY, and that is not a tuning choice. At
    exactly 1.0 the loop has no loss and the delay is an oscillator; with the
    filter in the loop it can exceed unity at some frequencies even below 1.0,
    so the clamp leaves real headroom. A delay that can run away is a delay
    that will, on somebody's preset, while they are recording.

    PING-PONG IS A CROSS-COUPLED FEEDBACK PATH, not two delays panned apart:
    the left output feeds the right line and vice versa, so a repeat physically
    alternates sides. Two independent lines panned hard would give repeats on
    both sides at once, which is a wide delay rather than a ping-pong.

    Real-time safe. The lines are sized in prepare for the longest delay at the
    highest sample rate the rack will run at.
*/
class FxDelay
{
public:
    using Division = sync::Division;

    struct Settings
    {
        bool enabled = false;
        float mix = 0.3f;

        bool syncEnabled = true;
        Division division = Division::eighth;
        /** Used when sync is off. */
        float timeMs = 250.0f;

        /** 0..1, clamped below unity internally. */
        float feedback = 0.4f;
        bool pingPong = false;
        /** 0..1 stereo offset between the two lines. */
        float width = 0.0f;

        /** Tone inside the feedback loop. */
        float lowCutHz = 20.0f;
        float highCutHz = 20000.0f;

        /** Slow modulation of the delay LENGTH, which is what stops a long
            feedback setting sounding like a static ringing tone. */
        float modRateHz = 0.3f;
        float modDepth = 0.0f;
    };

    /** MESSAGE THREAD. */
    void prepare (double sampleRate)
    {
        sampleRateHz = sampleRate;

        // Sized for the longest delay the parameter allows, plus the deepest
        // modulation excursion, plus a margin. Undersizing this would clamp a
        // long delay silently short.
        const auto maximumSamples = static_cast<int> (
            std::ceil (sampleRate * (kMaximumDelaySeconds + kMaximumModSeconds))) + 8;

        for (auto& channel : channels)
        {
            channel.line.prepare (maximumSamples);
            channel.lowCut.prepare (sampleRate);
            channel.highCut.prepare (sampleRate);
        }

        setSettings (settings);
        reset();
    }

    /** AUDIO THREAD SAFE, non-resetting. */
    void setSampleRate (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate;

        for (auto& channel : channels)
        {
            channel.lowCut.setSampleRate (sampleRate);
            channel.highCut.setSampleRate (sampleRate);
        }

        setSettings (settings);
    }

    void reset() noexcept
    {
        for (auto& channel : channels)
        {
            channel.line.reset();
            channel.lowCut.reset();
            channel.highCut.reset();
            channel.feedbackState = 0.0f;
        }

        modPhase = 0.0f;
    }

    /** BLOCK-RATE. `bpm` is only read when sync is on. */
    void setSettings (const Settings& newSettings, double bpm = 120.0) noexcept
    {
        settings = newSettings;

        const auto seconds = settings.syncEnabled
                           ? sync::getBeatsPerCycle (settings.division) * 60.0
                                 / juce::jlimit (20.0, 999.0, bpm)
                           : static_cast<double> (
                                 juce::jlimit (1.0f, 4000.0f, settings.timeMs)) * 0.001;

        // Clamped to what the buffer can actually serve: a slow tempo makes a
        // whole-bar division longer than the 4 s the time knob tops out at,
        // and reading past the buffer would give the wrong delay silently.
        const auto clampedSeconds = juce::jlimit (0.001, kMaximumDelaySeconds, seconds);

        baseDelaySamples = static_cast<float> (clampedSeconds * sampleRateHz);

        // The stereo offset is a FRACTION of the delay rather than a fixed
        // number of milliseconds, so widening a short delay and a long one
        // both read as the same amount of width.
        const auto widthAmount = juce::jlimit (0.0f, 1.0f, settings.width);
        offsetSamples = baseDelaySamples * widthAmount * kMaximumWidthFraction;

        // Below unity with real headroom - see the class comment.
        feedbackAmount = juce::jlimit (0.0f, 1.0f, settings.feedback) * kMaximumFeedback;

        modDepthSamples = static_cast<float> (
            juce::jlimit (0.0f, 1.0f, settings.modDepth) * kMaximumModSeconds * sampleRateHz);

        modIncrement = static_cast<float> (
            juce::jlimit (0.01f, 20.0f, settings.modRateHz) / sampleRateHz);

        for (auto& channel : channels)
        {
            channel.lowCut.setCutoff (settings.lowCutHz);
            channel.highCut.setCutoff (settings.highCutHz);
        }
    }

    /** SAMPLE-RATE, and takes BOTH channels at once.

        Unlike the filters and the EQ, this effect cannot be a per-channel
        function: ping-pong couples the two lines, so producing the left output
        requires the right line's state at the same sample. Advancing the two
        channels independently is exactly the mistake that made the voice
        filter's output depend on its block layout - here the coupling is real,
        so the interface admits it. */
    void processSample (float& left, float& right) noexcept
    {
        if (! settings.enabled)
            return;

        const auto dryLeft = left;
        const auto dryRight = right;

        // One shared LFO. Two would make the delay a chorus as well, which is
        // what the chorus is for.
        const auto modulation = std::sin (juce::MathConstants<float>::twoPi * modPhase)
                              * modDepthSamples;

        modPhase += modIncrement;

        if (modPhase >= 1.0f)
            modPhase -= 1.0f;

        const auto wetLeft = channels[0].line.read (
            juce::jmax (1.0f, baseDelaySamples + modulation));
        const auto wetRight = channels[1].line.read (
            juce::jmax (1.0f, baseDelaySamples + offsetSamples + modulation));

        // The tone controls are in the loop, so they filter what is written
        // back rather than what is heard once.
        const auto filteredLeft = filterFeedback (channels[0], wetLeft);
        const auto filteredRight = filterFeedback (channels[1], wetRight);

        if (settings.pingPong)
        {
            // Cross-coupled: each line is fed the OTHER side's repeat.
            channels[0].line.write (dryLeft + filteredRight * feedbackAmount);
            channels[1].line.write (dryRight + filteredLeft * feedbackAmount);
        }
        else
        {
            channels[0].line.write (dryLeft + filteredLeft * feedbackAmount);
            channels[1].line.write (dryRight + filteredRight * feedbackAmount);
        }

        for (auto& channel : channels)
        {
            if (channel.lowCut.hasBlownUp() || channel.highCut.hasBlownUp())
            {
                reset();
                return;
            }
        }

        const auto amount = juce::jlimit (0.0f, 1.0f, settings.mix);

        left = dryLeft + (wetLeft - dryLeft) * amount;
        right = dryRight + (wetRight - dryRight) * amount;
    }

    const Settings& getSettings() const noexcept { return settings; }

    /** The longest delay the time knob offers, in seconds. Matches
        ranges::delayTimeMs so the buffer is sized for the whole range. */
    static constexpr double kMaximumDelaySeconds = 4.0;

private:
    struct Channel
    {
        DelayLine line;
        OnePole lowCut;
        OnePole highCut;
        float feedbackState = 0.0f;
    };

    float filterFeedback (Channel& channel, float input) noexcept
    {
        // High-cut then low-cut: a one-pole pair, 6 dB per octave each.
        auto value = channel.highCut.processLowPass (input);
        value = channel.lowCut.processHighPass (value);

        channel.feedbackState = value;
        return value;
    }

    /** How far the modulation can push the delay length. 20 ms is enough to
        keep a long feedback from ringing on one pitch and small enough not to
        read as a pitch bend on the repeats. */
    static constexpr double kMaximumModSeconds = 0.02;

    /** Feedback ceiling. 0.98 rather than 1.0: with the tone filters in the
        loop the gain can exceed unity at some frequencies before the nominal
        setting does, so the clamp has to leave real headroom. */
    static constexpr float kMaximumFeedback = 0.98f;

    /** The widest stereo offset, as a fraction of the delay length. A third
        is audibly wide without the two sides reading as different delays. */
    static constexpr float kMaximumWidthFraction = 0.33f;

    double sampleRateHz = 44100.0;
    Settings settings {};

    float baseDelaySamples = 1.0f;
    float offsetSamples = 0.0f;
    float feedbackAmount = 0.0f;
    float modDepthSamples = 0.0f;
    float modIncrement = 0.0f;
    float modPhase = 0.0f;

    std::array<Channel, 2> channels {};
};

} // namespace gnarl::dsp
