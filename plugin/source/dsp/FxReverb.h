#pragma once

#include "DelayLine.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>

namespace gnarl::dsp
{

/**
    The FX rack's reverb: a feedback delay network.

    WHY AN FDN AND NOT A SCHROEDER OR FREEVERB. A bank of parallel combs plus
    series all-passes is cheaper, and it is what juce::Reverb is. It also has a
    characteristic metallic ring, because each comb's resonances are a
    harmonic series and nothing mixes energy between them - a comb at 50 ms
    resonates at 20 Hz and every multiple. A feedback delay network replaces
    the independent combs with delays that feed EACH OTHER through an
    orthogonal mixing matrix, so energy moves between the delays on every pass
    and no single delay's harmonic series survives. That is what makes the tail
    smooth rather than ringing, and it is what a product at this price has to
    sound like.

    THE MATRIX MUST BE ORTHOGONAL - or, strictly, its largest singular value
    must not exceed one. That is the entire stability argument: the decay is
    then set by the gain applied to the feedback and by nothing else, so a
    decay control cannot make the reverb explode. A hand-written "mix things
    together" matrix does not have this property and will blow up on some
    setting nobody tested. This uses a HOUSEHOLDER reflection, which is
    orthogonal by construction:

        y = x - (2/N) * sum(x)

    for N delays, which costs one sum and one multiply-subtract per delay
    rather than the N*N multiplies a general matrix would.

    THE DELAY LENGTHS ARE MUTUALLY PRIME. If two delays share a factor their
    resonances coincide, which puts back exactly the ringing the network is
    there to avoid. They are prime numbers of samples at the reference rate
    and scaled by `size`.

    MODULATION IS NOT DECORATION HERE. A static FDN still has modes, and on a
    sustained note they are audible as a pitch. Slowly moving the delay lengths
    smears the modes so the tail reads as space rather than as a chord.

    Real-time safe.
*/
class FxReverb
{
public:
    /** Eight delays. Four is audibly sparse on a long tail and sixteen costs
        twice as much for a difference that does not survive the mix. */
    static constexpr int kNumDelays = 8;

    struct Settings
    {
        bool enabled = false;
        float mix = 0.25f;
        /** 0..1 room size, which scales the delay lengths. */
        float size = 0.5f;
        /** 0..1 decay time. */
        float decay = 0.5f;
        /** 0..1 high-frequency damping in the tail. */
        float damping = 0.4f;
        /** Pre-delay in milliseconds. */
        float preDelayMs = 10.0f;
        /** 0..1 stereo width of the tail. */
        float width = 1.0f;
        float lowCutHz = 200.0f;
        float highCutHz = 7000.0f;
        /** 0..1 modulation of the delay lengths. */
        float modDepth = 0.2f;
    };

    /** MESSAGE THREAD. */
    void prepare (double sampleRate)
    {
        sampleRateHz = sampleRate;

        const auto rateScale = sampleRate / kReferenceSampleRate;

        for (int i = 0; i < kNumDelays; ++i)
        {
            const auto longest = static_cast<double> (kPrimeLengths[static_cast<std::size_t> (i)])
                               * rateScale * kMaximumSizeScale;

            delays[static_cast<std::size_t> (i)].line.prepare (
                static_cast<int> (std::ceil (longest + kMaximumModSamples * rateScale)) + 8);

            delays[static_cast<std::size_t> (i)].damper.prepare (sampleRate);
        }

        const auto maximumPreDelay = static_cast<int> (
            std::ceil (sampleRate * kMaximumPreDelaySeconds)) + 8;

        for (auto& line : preDelayLines)
            line.prepare (maximumPreDelay);

        for (auto& channel : toneChannels)
        {
            channel.lowCut.prepare (sampleRate);
            channel.highCut.prepare (sampleRate);
        }

        setSettings (settings);
        reset();
    }

    void setSampleRate (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate;

        for (auto& delay : delays)
            delay.damper.setSampleRate (sampleRate);

        for (auto& channel : toneChannels)
        {
            channel.lowCut.setSampleRate (sampleRate);
            channel.highCut.setSampleRate (sampleRate);
        }

        setSettings (settings);
    }

    void reset() noexcept
    {
        for (auto& delay : delays)
        {
            delay.line.reset();
            delay.damper.reset();
            delay.modPhase = 0.0f;
        }

        for (auto& line : preDelayLines)
            line.reset();

        for (auto& channel : toneChannels)
        {
            channel.lowCut.reset();
            channel.highCut.reset();
        }
    }

    /** BLOCK-RATE. */
    void setSettings (const Settings& newSettings) noexcept
    {
        settings = newSettings;

        const auto rateScale = sampleRateHz / kReferenceSampleRate;

        // Size scales every delay together, so the room gets bigger rather
        // than the pattern of reflections changing.
        const auto sizeScale = juce::jmap (juce::jlimit (0.0f, 1.0f, settings.size),
                                           kMinimumSizeScale, kMaximumSizeScale);

        for (int i = 0; i < kNumDelays; ++i)
        {
            auto& delay = delays[static_cast<std::size_t> (i)];

            delay.lengthSamples = static_cast<float> (
                static_cast<double> (kPrimeLengths[static_cast<std::size_t> (i)])
                * rateScale * static_cast<double> (sizeScale));

            // Each delay modulates at its own rate, so they do not all move
            // together - which would be a pitch bend on the whole tail.
            delay.modIncrement = static_cast<float> (
                kModRatesHz[static_cast<std::size_t> (i)] / sampleRateHz);

            delay.damper.setCutoff (juce::jmap (
                juce::jlimit (0.0f, 1.0f, settings.damping), 18000.0f, 800.0f));
        }

        modDepthSamples = static_cast<float> (
            juce::jlimit (0.0f, 1.0f, settings.modDepth) * kMaximumModSamples * rateScale);

        /*  THE DECAY GAIN. The tail loses `feedbackGain` per pass through the
            network, and a pass takes roughly the mean delay length, so the
            time to decay by 60 dB is that length times how many passes it
            takes to lose 60 dB. Solving for the gain rather than exposing it
            directly is what makes the decay control mean the same thing at
            every room size - otherwise making the room bigger would also make
            the tail longer, twice over. */
        const auto meanLengthSeconds = kMeanPrimeLength * rateScale
                                     * static_cast<double> (sizeScale) / sampleRateHz;

        const auto decaySeconds = juce::jmap (
            static_cast<double> (juce::jlimit (0.0f, 1.0f, settings.decay)),
            kMinimumDecaySeconds, kMaximumDecaySeconds);

        const auto passes = decaySeconds / juce::jmax (1.0e-6, meanLengthSeconds);

        // -60 dB over `passes` passes.
        feedbackGain = static_cast<float> (
            std::pow (10.0, -3.0 / juce::jmax (1.0, passes)));

        // Clamped short of unity whatever the arithmetic says: a decay of
        // exactly one is an oscillator, and the damping in the loop means the
        // effective gain is not uniform across frequency.
        feedbackGain = juce::jlimit (0.0f, kMaximumFeedbackGain, feedbackGain);

        preDelaySamples = static_cast<float> (
            juce::jlimit (0.0, kMaximumPreDelaySeconds,
                          static_cast<double> (settings.preDelayMs) * 0.001)
            * sampleRateHz);

        widthAmount = juce::jlimit (0.0f, 1.0f, settings.width);

        for (auto& channel : toneChannels)
        {
            channel.lowCut.setCutoff (settings.lowCutHz);
            channel.highCut.setCutoff (settings.highCutHz);
        }
    }

    /** SAMPLE-RATE, both channels: the network is shared, so producing either
        output needs the whole state. */
    void processSample (float& left, float& right) noexcept
    {
        if (! settings.enabled)
            return;

        const auto dryLeft = left;
        const auto dryRight = right;

        // Pre-delay first: the gap before the tail starts is what tells the
        // ear how far away the walls are.
        const auto delayedLeft = preDelaySamples >= 1.0f
                               ? preDelayLines[0].read (preDelaySamples)
                               : dryLeft;
        const auto delayedRight = preDelaySamples >= 1.0f
                                ? preDelayLines[1].read (preDelaySamples)
                                : dryRight;

        preDelayLines[0].write (dryLeft);
        preDelayLines[1].write (dryRight);

        // Read every delay, then mix - the read has to happen before anything
        // is written, or the network would be reading this sample's output on
        // some lines and last sample's on others.
        std::array<float, kNumDelays> taps {};

        auto sum = 0.0f;

        for (int i = 0; i < kNumDelays; ++i)
        {
            auto& delay = delays[static_cast<std::size_t> (i)];

            delay.modPhase += delay.modIncrement;

            if (delay.modPhase >= 1.0f)
                delay.modPhase -= 1.0f;

            const auto modulation = std::sin (juce::MathConstants<float>::twoPi
                                              * delay.modPhase) * modDepthSamples;

            taps[static_cast<std::size_t> (i)] = delay.line.read (
                juce::jmax (1.0f, delay.lengthSamples + modulation));

            sum += taps[static_cast<std::size_t> (i)];
        }

        // Householder reflection: y = x - (2/N) * sum(x). Orthogonal by
        // construction, so the network cannot add energy - see the class
        // comment, because this one line is the stability argument.
        const auto reflection = sum * (2.0f / static_cast<float> (kNumDelays));

        // Input goes into every line. Alternating the two channels across the
        // lines is what gives the tail a stereo image without needing two
        // networks.
        for (int i = 0; i < kNumDelays; ++i)
        {
            auto& delay = delays[static_cast<std::size_t> (i)];

            const auto mixed = (taps[static_cast<std::size_t> (i)] - reflection)
                             * feedbackGain;

            // Damping inside the loop, so the tail loses its top end as it
            // decays rather than being filtered once on the way out.
            const auto damped = delay.damper.processLowPass (mixed);

            const auto injected = (i % 2 == 0 ? delayedLeft : delayedRight)
                                * kInputGain;

            delay.line.write (damped + injected);
        }

        // The two outputs take alternating halves of the network, so they
        // share the room but not the reflections.
        auto wetLeft = 0.0f;
        auto wetRight = 0.0f;

        for (int i = 0; i < kNumDelays; ++i)
        {
            if (i % 2 == 0)
                wetLeft += taps[static_cast<std::size_t> (i)];
            else
                wetRight += taps[static_cast<std::size_t> (i)];
        }

        constexpr auto perSide = 2.0f / static_cast<float> (kNumDelays);
        wetLeft *= perSide;
        wetRight *= perSide;

        // Width collapses the tail towards mono without touching the dry
        // signal, so a narrow setting is a centred room rather than a quieter
        // one.
        const auto tailMid = (wetLeft + wetRight) * 0.5f;
        wetLeft = tailMid + (wetLeft - tailMid) * widthAmount;
        wetRight = tailMid + (wetRight - tailMid) * widthAmount;

        // Tone shaping on the way OUT, which is where a low cut belongs: in
        // the loop it would compound over the tail and remove the body
        // entirely.
        wetLeft = toneChannels[0].highCut.processLowPass (wetLeft);
        wetLeft = toneChannels[0].lowCut.processHighPass (wetLeft);
        wetRight = toneChannels[1].highCut.processLowPass (wetRight);
        wetRight = toneChannels[1].lowCut.processHighPass (wetRight);

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
    struct Delay
    {
        DelayLine line;
        OnePole damper;
        float lengthSamples = 1.0f;
        float modIncrement = 0.0f;
        float modPhase = 0.0f;
    };

    struct ToneChannel
    {
        OnePole lowCut;
        OnePole highCut;
    };

    /** The lengths the primes are quoted at; every one is scaled by the
        actual rate so a room is the same size at 44.1 and 96 kHz. */
    static constexpr double kReferenceSampleRate = 48000.0;

    /** MUTUALLY PRIME lengths in samples, 12 ms to 60 ms at the reference
        rate. Prime because a shared factor makes two delays' resonances
        coincide, which puts back the ringing the network exists to avoid. */
    static constexpr std::array<int, kNumDelays> kPrimeLengths {
        601, 787, 941, 1123, 1361, 1597, 1861, 2111
    };

    /** The mean of the above, used to turn a decay TIME into a per-pass
        gain. */
    static constexpr double kMeanPrimeLength = 1297.75;

    /** Each delay modulates at its own rate so the tail smears rather than
        bending in pitch. Deliberately not harmonically related. */
    static constexpr std::array<double, kNumDelays> kModRatesHz {
        0.11, 0.17, 0.23, 0.29, 0.37, 0.43, 0.53, 0.61
    };

    static constexpr double kMaximumModSamples = 24.0;

    static constexpr float kMinimumSizeScale = 0.35f;
    static constexpr float kMaximumSizeScale = 2.2f;

    static constexpr double kMinimumDecaySeconds = 0.15;
    static constexpr double kMaximumDecaySeconds = 12.0;

    static constexpr double kMaximumPreDelaySeconds = 0.25;

    /** 0.995: a per-pass gain of exactly one is an oscillator, and the damping
        in the loop means the effective gain is not uniform across frequency,
        so the clamp needs real headroom. */
    static constexpr float kMaximumFeedbackGain = 0.995f;

    /** Input gain into the network. Low, because eight lines each carrying the
        input sum to eight times it on the first pass. */
    static constexpr float kInputGain = 0.25f;

    double sampleRateHz = 44100.0;
    Settings settings {};

    float feedbackGain = 0.5f;
    float modDepthSamples = 0.0f;
    float preDelaySamples = 0.0f;
    float widthAmount = 1.0f;

    std::array<Delay, kNumDelays> delays {};
    std::array<DelayLine, 2> preDelayLines {};
    std::array<ToneChannel, 2> toneChannels {};
};

} // namespace gnarl::dsp
