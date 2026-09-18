#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>

namespace gnarl::dsp
{

/**
    Four-pole ladder filter with a nonlinear feedback path. The growl filter.

    Four TPT one-pole sections in series with a resonance feedback loop, and a
    tanh saturator INSIDE that loop. The saturator is the whole point: a linear
    ladder is just a 24 dB/octave low-pass, whereas saturating the feedback is
    what produces the compressed, squelching resonance a Moog-style filter is
    wanted for. It also makes self-oscillation bounded instead of explosive,
    so high resonance is a sound rather than a hazard.

    Resonance compensation: feedback drops the passband level, so the input is
    scaled back up as resonance rises. Without it, opening the resonance
    quietly turns the patch down.
*/
class LadderFilter
{
public:
    enum class Mode
    {
        lowPass = 0,
        highPass
    };

    void prepare (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;
        reset();
        setCutoff (1000.0f);
        setResonance (0.0f);
    }

    /** AUDIO THREAD SAFE. Coefficients are recomputed by the next setCutoff,
        which the filter slot calls every block. */
    void setSampleRate (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;
    }

    void reset() noexcept
    {
        stages.fill (0.0f);
        lastOutput = 0.0f;
    }

    void setMode (Mode newMode) noexcept { mode = newMode; }

    /** SAMPLE-RATE safe. */
    void setCutoff (float cutoffHz) noexcept
    {
        const auto nyquist = static_cast<float> (sampleRateHz) * 0.5f;
        const auto clamped = juce::jlimit (10.0f, nyquist * 0.98f, cutoffHz);

        const auto wd = juce::MathConstants<float>::twoPi * clamped;
        const auto t = 1.0f / static_cast<float> (sampleRateHz);
        const auto wa = (2.0f / t) * std::tan (wd * t * 0.5f);

        g = wa * t * 0.5f;
        gInverse = 1.0f / (1.0f + g);
    }

    /** Resonance 0..1. Maps to a feedback of 0..4, where 4 is the classic
        self-oscillation point. */
    void setResonance (float resonance) noexcept
    {
        const auto clamped = juce::jlimit (0.0f, 1.0f, resonance);

        // Stops just short of 4: the saturator bounds it, but sitting exactly
        // on the oscillation point makes the filter ring on any DC.
        feedback = clamped * 3.95f;

        // Passband loss from feedback, compensated at the input.
        driveCompensation = 1.0f + feedback * 0.4f;
    }

    /** Extra saturation in the feedback loop, 0..1, on top of the inherent
        tanh. Pushes the filter into its compressed, squashed region. */
    void setDrive (float amount) noexcept
    {
        feedbackDrive = 1.0f + juce::jlimit (0.0f, 1.0f, amount) * 8.0f;
    }

    /** SAMPLE-RATE. */
    float processSample (float input) noexcept
    {
        // The feedback is taken from the PREVIOUS output, which is what makes
        // this a one-sample-delayed loop rather than a zero-delay solve. The
        // saturator keeps that approximation stable at any resonance.
        const auto saturatedFeedback = std::tanh (lastOutput * feedbackDrive);
        auto x = input * driveCompensation - feedback * saturatedFeedback;

        // Four TPT one-pole low-pass sections.
        for (auto& state : stages)
        {
            const auto v = (x - state) * g * gInverse;
            const auto output = v + state;
            state = output + v;
            x = output;
        }

        lastOutput = x;

        if (mode == Mode::highPass)
        {
            // The complement of the low-pass, which is what a ladder's
            // high-pass tap actually is.
            return input - x;
        }

        return x;
    }

    bool hasBlownUp() const noexcept
    {
        for (const auto state : stages)
            if (! std::isfinite (state))
                return true;

        return ! std::isfinite (lastOutput);
    }

private:
    double sampleRateHz = 44100.0;

    Mode mode = Mode::lowPass;

    float g = 0.0f;
    float gInverse = 1.0f;
    float feedback = 0.0f;
    float feedbackDrive = 1.0f;
    float driveCompensation = 1.0f;

    std::array<float, 4> stages {};
    float lastOutput = 0.0f;
};

} // namespace gnarl::dsp
