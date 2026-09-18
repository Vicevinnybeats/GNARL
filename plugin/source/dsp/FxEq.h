#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>

namespace gnarl::dsp
{

/**
    One band of the FX EQ: a topology-preserving (TPT) state variable filter
    whose three internal signals are mixed to give a shelf, a bell or a cut.

    WHY THIS DUPLICATES StateVariableFilter'S CORE. The shared SVF exposes
    `setCutoff` and `setQ` as independent controls, and a shelf cannot be
    expressed that way: its `g` is the prewarped frequency scaled by the square
    root of the gain, and a bell's `k` is divided by it. The coefficients are
    JOINT functions of frequency, Q and gain, so the band has to own them. The
    duplicated arithmetic is a liability, so `FxEqTests` measures each mode's
    magnitude response against the analytic target rather than asserting the
    code ran - a one-sided change to either copy fails.

    The mixing coefficients are Simper's: output = m0*input + m1*v1 + m2*v2,
    where v1 is the raw band-pass and v2 the low-pass. The useful property is
    that a shelf or bell at 0 dB gives m1 = m2 = 0 and m0 = 1, so it is
    BIT-TRANSPARENT rather than merely close - a flat EQ costs nothing and
    changes nothing.
*/
class FxEqBand
{
public:
    enum class Mode
    {
        bell,
        lowShelf,
        highShelf,
        highPass,
        lowPass
    };

    void prepare (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate;
        reset();
        setBand (Mode::bell, 1000.0f, 0.0f, 0.707f);
    }

    /** AUDIO THREAD SAFE, and does NOT reset: an EQ whose band state cleared
        on a sample-rate change would click mid-note. Coefficients are stale
        until the next setBand, which runs every block. */
    void setSampleRate (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate;
    }

    void reset() noexcept
    {
        state1 = 0.0f;
        state2 = 0.0f;
    }

    /** BLOCK-RATE. Frequency in Hz, gain in dB (ignored by the two cuts), Q
        as a ratio. */
    void setBand (Mode newMode, float frequencyHz, float gainDb, float q) noexcept
    {
        mode = newMode;

        // A is the SQUARE ROOT of the linear gain, because every place it
        // appears below it appears squared. Halving the exponent here rather
        // than taking a square root three times is the whole reason the
        // literature writes it this way.
        const auto a = std::pow (10.0f, juce::jlimit (-24.0f, 24.0f, gainDb) / 40.0f);

        const auto nyquist = static_cast<float> (sampleRateHz) * 0.5f;

        // 0.98 of Nyquist: tan() is infinite at Nyquist and a modulated
        // frequency will be driven there. Same clamp as StateVariableFilter.
        const auto clamped = juce::jlimit (10.0f, nyquist * 0.98f, frequencyHz);
        const auto prewarped = std::tan (juce::MathConstants<float>::pi * clamped
                                         / static_cast<float> (sampleRateHz));

        const auto safeQ = juce::jmax (0.05f, q);

        switch (mode)
        {
            case Mode::bell:
                // k carries the gain so that a boost and the matching cut are
                // mirror images. Without it a -12 dB cut is narrower than a
                // +12 dB boost and the two do not undo each other.
                g = prewarped;
                k = 1.0f / (safeQ * a);
                m0 = 1.0f;
                m1 = k * (a * a - 1.0f);
                m2 = 0.0f;
                break;

            case Mode::lowShelf:
                g = prewarped / std::sqrt (a);
                k = 1.0f / safeQ;
                m0 = 1.0f;
                m1 = k * (a - 1.0f);
                m2 = a * a - 1.0f;
                break;

            case Mode::highShelf:
                g = prewarped * std::sqrt (a);
                k = 1.0f / safeQ;
                m0 = a * a;
                m1 = k * (1.0f - a) * a;
                m2 = 1.0f - a * a;
                break;

            case Mode::highPass:
                g = prewarped;
                k = 1.0f / safeQ;
                m0 = 1.0f;
                m1 = -k;
                m2 = -1.0f;
                break;

            case Mode::lowPass:
            default:
                g = prewarped;
                k = 1.0f / safeQ;
                m0 = 0.0f;
                m1 = 0.0f;
                m2 = 1.0f;
                break;
        }

        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    /** SAMPLE-RATE. */
    float processSample (float input) noexcept
    {
        const auto v3 = input - state2;
        const auto v1 = a1 * state1 + a2 * v3;
        const auto v2 = state2 + a2 * state1 + a3 * v3;

        state1 = 2.0f * v1 - state1;
        state2 = 2.0f * v2 - state2;

        return m0 * input + m1 * v1 + m2 * v2;
    }

    bool hasBlownUp() const noexcept
    {
        return ! std::isfinite (state1) || ! std::isfinite (state2);
    }

private:
    double sampleRateHz = 44100.0;
    Mode mode = Mode::bell;

    float g = 0.0f;
    float k = 1.0f;
    float a1 = 1.0f;
    float a2 = 0.0f;
    float a3 = 0.0f;

    float m0 = 1.0f;
    float m1 = 0.0f;
    float m2 = 0.0f;

    float state1 = 0.0f;
    float state2 = 0.0f;
};

/**
    The FX rack's EQ: high-pass, low shelf, two bells, high shelf, low-pass.

    Two instances exist, and the reason is ORDERING rather than count: one
    carves before a distortion and one fixes what the distortion did. Those are
    different jobs and a single instance cannot do both, because the whole
    point of the first is that it changes what the drive stage is given. See
    docs/fx-architecture.md.

    A STEREO PATH IS TWO PATHS (CLAUDE.md section 3): each channel holds its
    own set of six bands. Sharing one band across channels makes the output a
    function of the block layout, which is the bug the voice filter shipped.

    Real-time safe. No allocation, no locks, no branching on denormals.
*/
class FxEq
{
public:
    struct Settings
    {
        bool enabled = false;
        float mix = 1.0f;

        /** 20 Hz is "off" for the high-pass and 20 kHz for the low-pass: both
            sit at the edge of the audible band rather than having a bypass
            switch of their own, which is one fewer control for the same
            result. */
        float highPassFreq = 20.0f;
        float lowShelfFreq = 100.0f;
        float lowShelfGain = 0.0f;
        float band1Freq = 500.0f;
        float band1Gain = 0.0f;
        float band1Q = 1.0f;
        float band2Freq = 3000.0f;
        float band2Gain = 0.0f;
        float band2Q = 1.0f;
        float highShelfFreq = 8000.0f;
        float highShelfGain = 0.0f;
        float lowPassFreq = 20000.0f;
    };

    void prepare (double sampleRate) noexcept
    {
        for (auto& channel : channels)
            for (auto& band : channel)
                band.prepare (sampleRate);

        setSettings (settings);
    }

    /** AUDIO THREAD SAFE, non-resetting - see FxEqBand::setSampleRate. */
    void setSampleRate (double sampleRate) noexcept
    {
        for (auto& channel : channels)
            for (auto& band : channel)
                band.setSampleRate (sampleRate);

        setSettings (settings);
    }

    void reset() noexcept
    {
        for (auto& channel : channels)
            for (auto& band : channel)
                band.reset();
    }

    /** BLOCK-RATE. */
    void setSettings (const Settings& newSettings) noexcept
    {
        settings = newSettings;

        // WHICH BANDS ARE WORTH RUNNING. A shelf or bell at 0 dB is
        // bit-transparent (m1 = m2 = 0, m0 = 1), so running it is arithmetic
        // with a guaranteed result; and the two cuts parked at the edge of the
        // audible band are what "off" means for them, since neither has a
        // bypass switch of its own. A typical patch uses two of the six, so
        // this is most of the EQ's cost rather than a micro-optimisation - and
        // it is what makes a flat EQ transparent rather than merely close.
        active[kHighPass]  = settings.highPassFreq > kHighPassOffHz;
        active[kLowShelf]  = settings.lowShelfGain != 0.0f;
        active[kBand1]     = settings.band1Gain != 0.0f;
        active[kBand2]     = settings.band2Gain != 0.0f;
        active[kHighShelf] = settings.highShelfGain != 0.0f;
        active[kLowPass]   = settings.lowPassFreq < kLowPassOffHz;

        // Both channels get identical coefficients; only their STATE differs.
        for (auto& channel : channels)
        {
            channel[kHighPass].setBand (FxEqBand::Mode::highPass,
                                        settings.highPassFreq, 0.0f, kCutQ);
            channel[kLowShelf].setBand (FxEqBand::Mode::lowShelf,
                                        settings.lowShelfFreq, settings.lowShelfGain, kShelfQ);
            channel[kBand1].setBand (FxEqBand::Mode::bell,
                                     settings.band1Freq, settings.band1Gain, settings.band1Q);
            channel[kBand2].setBand (FxEqBand::Mode::bell,
                                     settings.band2Freq, settings.band2Gain, settings.band2Q);
            channel[kHighShelf].setBand (FxEqBand::Mode::highShelf,
                                         settings.highShelfFreq, settings.highShelfGain, kShelfQ);
            channel[kLowPass].setBand (FxEqBand::Mode::lowPass,
                                       settings.lowPassFreq, 0.0f, kCutQ);
        }
    }

    /** SAMPLE-RATE. */
    float processSample (int channelIndex, float input) noexcept
    {
        if (! settings.enabled)
            return input;

        auto& channel = channels[static_cast<std::size_t> (
            juce::jlimit (0, static_cast<int> (channels.size()) - 1, channelIndex))];

        auto wet = input;

        for (std::size_t i = 0; i < kNumBands; ++i)
        {
            if (! active[i])
                continue;

            wet = channel[i].processSample (wet);

            // A NaN arriving from upstream latches in the state, and every
            // later sample out of that band is NaN too. Cheaper to notice than
            // to poison the mix.
            if (channel[i].hasBlownUp())
            {
                for (auto& band : channel)
                    band.reset();

                return input;
            }
        }

        const auto amount = juce::jlimit (0.0f, 1.0f, settings.mix);
        return input + (wet - input) * amount;
    }

    const Settings& getSettings() const noexcept { return settings; }

private:
    // Band order inside a channel. The cuts sit at the ends because that is
    // what they are for: remove what should not be there before shaping, and
    // limit the top after.
    static constexpr std::size_t kHighPass  = 0;
    static constexpr std::size_t kLowShelf  = 1;
    static constexpr std::size_t kBand1     = 2;
    static constexpr std::size_t kBand2     = 3;
    static constexpr std::size_t kHighShelf = 4;
    static constexpr std::size_t kLowPass   = 5;

    static constexpr std::size_t kNumBands = 6;

    /** Butterworth: the cuts are level controls at the edge of the band, not
        a resonant effect. The FX FILTER is where a resonant cut lives. */
    static constexpr float kCutQ = 0.70710678f;

    /** A shelf's Q sets how abruptly it turns over. 0.707 is the gentle slope
        people expect from a shelf; higher puts a bump at the corner. */
    static constexpr float kShelfQ = 0.70710678f;

    /** The frequencies at which the two cuts count as off. They are the ends
        of the parameter's own range, so a knob at its limit is bypass. */
    static constexpr float kHighPassOffHz = 20.0f;
    static constexpr float kLowPassOffHz = 20000.0f;

    Settings settings {};
    std::array<bool, kNumBands> active {};
    std::array<std::array<FxEqBand, kNumBands>, 2> channels {};
};

} // namespace gnarl::dsp
