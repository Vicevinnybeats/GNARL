#pragma once

#include "DelayLine.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>

namespace gnarl::dsp
{

/**
    The FX rack's hyper: a unison-iser that turns one signal into several
    slightly detuned ones, spread across the stereo field.

    HOW YOU DETUNE A SIGNAL YOU CANNOT RE-SYNTHESISE. The oscillator's unison
    detunes by running more oscillators; an effect has only the finished audio,
    so it has to shift pitch. A delay line whose length is CHANGING is a pitch
    shifter - reading slower than you write lowers the pitch and vice versa -
    so each voice here is a tap whose delay ramps continuously, and the rate of
    that ramp is its detune. That is the same mechanism the chorus uses; the
    difference is the amount and the intent. A chorus modulates a few
    milliseconds and is heard as movement; this one is tuned so the voices sit
    a few cents apart and is heard as thickness.

    THE VOICES SWEEP AT DIFFERENT RATES, not at one rate with phase offsets.
    Phase offsets give voices that are detuned in opposite directions at the
    same instant and then swap, which beats. Different rates keep each voice at
    a roughly constant offset from its neighbours, which is what a supersaw
    does and is why it sounds wide rather than seasick.

    WIDTH PANS THE VOICES, and it pans them DETERMINISTICALLY across the field
    rather than randomly, so the result is symmetric: an odd voice count leaves
    one in the centre, and an even one puts them in pairs. A random spread
    sounds wider on some notes than others, which is not a feature.

    Real-time safe.
*/
class FxHyper
{
public:
    static constexpr int kMaxVoices = 8;

    struct Settings
    {
        bool enabled = false;
        float mix = 1.0f;
        /** 0..1 overall intensity: how far the voices move. */
        float amount = 0.5f;
        /** 0..1 detune spread between the voices. */
        float detune = 0.4f;
        /** 2..8. */
        int voices = 4;
        /** 0..1 stereo spread of the voices. */
        float width = 0.8f;
    };

    void prepare (double sampleRate)
    {
        sampleRateHz = sampleRate;

        const auto maximumSamples = static_cast<int> (
            std::ceil (sampleRate * (kCentreSeconds + kMaximumDepthSeconds
                                     + kBaseSpreadSeconds))) + 8;

        for (auto& channel : channels)
            for (auto& line : channel)
                line.prepare (maximumSamples);

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
            for (auto& line : channel)
                line.reset();

        for (auto& value : phases)
            value = 0.0f;
    }

    /** BLOCK-RATE. */
    void setSettings (const Settings& newSettings) noexcept
    {
        settings = newSettings;

        activeVoices = juce::jlimit (2, kMaxVoices, settings.voices);

        centreSamples = static_cast<float> (kCentreSeconds * sampleRateHz);

        depthSamples = static_cast<float> (
            juce::jlimit (0.0f, 1.0f, settings.amount)
            * kMaximumDepthSeconds * sampleRateHz);

        const auto detuneAmount = juce::jlimit (0.0f, 1.0f, settings.detune);

        for (int voice = 0; voice < kMaxVoices; ++voice)
        {
            // Rates fanned out around the base: voice 0 slowest, the last
            // fastest, so each sits at a different constant-ish offset.
            const auto share = activeVoices > 1
                             ? static_cast<float> (voice)
                               / static_cast<float> (activeVoices - 1)
                             : 0.5f;

            const auto rate = kBaseRateHz
                            * (1.0f + detuneAmount * kRateSpread * (share - 0.5f) * 2.0f);

            increments[static_cast<std::size_t> (voice)] =
                static_cast<float> (juce::jmax (0.01f, rate) / sampleRateHz);

            // Spread across the window UNCONDITIONALLY, not scaled by
            // detune. Scaling it by detune was an attempt to let detune zero
            // stack the voices, and it put the level back on a knife edge:
            // the voices' coherence then depended on the detune setting, so
            // no single normalisation held and the measured level moved by
            // 6-9 dB with the voice count in one direction or the other. With
            // the bases always fanned out the taps are always distinct, the
            // sum is always partly incoherent, and one scale works.
            baseOffsets[static_cast<std::size_t> (voice)] = static_cast<float> (
                (share - 0.5f) * 2.0f * kBaseSpreadSeconds * sampleRateHz);
        }

        /*  Root n, as in the chorus. The voices are the same signal read at
            different delays: not identical, so the sum is not n, and not
            independent, so it is not 1 either. Root n is the right model only
            because the base delays above are ALWAYS fanned out - it was wrong
            when their spread depended on detune. Measured within 4 dB across
            two to eight voices, which is what the test asserts. */
        voiceScale = 1.0f / std::sqrt (static_cast<float> (activeVoices));
    }

    /** SAMPLE-RATE, and takes both channels: a voice's PAN needs both outputs
        at once, so splitting this per channel would mean either running the
        voices twice or sharing their state - and sharing it is the bug that
        makes output depend on block layout. */
    void processSample (float& left, float& right) noexcept
    {
        if (! settings.enabled)
            return;

        const auto dryLeft = left;
        const auto dryRight = right;

        // One mono sum feeds the voices. Feeding the two channels separately
        // would detune them independently, which decorrelates the bass as well
        // as the top and makes the low end vanish on a mono system.
        const auto monoInput = (dryLeft + dryRight) * 0.5f;

        const auto widthAmount = juce::jlimit (0.0f, 1.0f, settings.width);

        auto wetLeft = 0.0f;
        auto wetRight = 0.0f;

        for (int voice = 0; voice < activeVoices; ++voice)
        {
            const auto i = static_cast<std::size_t> (voice);

            phases[i] += increments[i];

            if (phases[i] >= 1.0f)
                phases[i] -= 1.0f;

            const auto lfo = std::sin (juce::MathConstants<float>::twoPi * phases[i]);

            // EACH VOICE SITS AT ITS OWN BASE DELAY, not just at its own sweep
            // rate. With one shared base the voices are the same signal at
            // almost the same delay, so at any instant they are nearly
            // identical and sum COHERENTLY - measured +6 dB from two voices to
            // eight, which is a voice-count control that doubles as a volume
            // control. Fanning the base delays across the window is what makes
            // them genuinely different taps.
            const auto delay = juce::jmax (
                1.0f, centreSamples + baseOffsets[i] + lfo * depthSamples);

            const auto value = channels[0][i].read (delay) * voiceScale;

            // Deterministic, symmetric pan: -1 for the first voice, +1 for the
            // last, scaled by width.
            const auto position = activeVoices > 1
                                ? (static_cast<float> (voice)
                                   / static_cast<float> (activeVoices - 1)) * 2.0f - 1.0f
                                : 0.0f;

            const auto pan = position * widthAmount;

            // Constant-power pan, so moving a voice out does not change the
            // total level.
            const auto angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;

            wetLeft += value * std::cos (angle) * juce::MathConstants<float>::sqrt2;
            wetRight += value * std::sin (angle) * juce::MathConstants<float>::sqrt2;
        }

        for (int voice = 0; voice < activeVoices; ++voice)
            channels[0][static_cast<std::size_t> (voice)].write (monoInput);

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
    /** The window the voices sit in. Shorter than the chorus's, because these
        are meant to fuse into one thick sound rather than to be heard apart. */
    static constexpr double kCentreSeconds = 0.008;
    static constexpr double kMaximumDepthSeconds = 0.004;

    /** The base sweep rate. Slow: a fast sweep is a vibrato, and this is meant
        to read as detune. */
    static constexpr float kBaseRateHz = 0.17f;

    /** How far the voices' rates fan out at full detune. */
    static constexpr float kRateSpread = 0.9f;

    /** How far the voices' BASE delays fan out at full detune. A few
        milliseconds is enough to decorrelate them without the window growing
        long enough to be heard as separate taps. */
    static constexpr double kBaseSpreadSeconds = 0.003;

    double sampleRateHz = 44100.0;
    Settings settings {};

    int activeVoices = 4;
    float centreSamples = 1.0f;
    float depthSamples = 0.0f;
    float voiceScale = 1.0f;

    std::array<float, kMaxVoices> increments {};
    std::array<float, kMaxVoices> baseOffsets {};
    std::array<float, kMaxVoices> phases {};

    // One set of lines, fed the mono sum - see processSample. The outer array
    // is kept so the shape matches the other effects and a future true-stereo
    // mode has somewhere to go.
    std::array<std::array<DelayLine, kMaxVoices>, 1> channels {};
};

} // namespace gnarl::dsp
