#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>

namespace gnarl::dsp
{

/**
    Zero-delay-feedback state variable filter, 12 dB/octave.

    Topology-preserving transform (Zavalishin), which matters for a synth
    rather than being a detail: the cutoff and resonance coefficients can be
    changed EVERY SAMPLE without the filter blowing up or zipping. A classic
    bilinear biquad recalculated per sample is not stable, which is why a
    filter built from one cannot be modulated at audio rate - and audio-rate
    filter modulation is most of what makes a growl.

    All four responses are available from the same state, so a morphing filter
    costs one filter rather than four.
*/
class StateVariableFilter
{
public:
    struct Outputs
    {
        float lowPass = 0.0f;
        float bandPass = 0.0f;
        float highPass = 0.0f;
        float notch = 0.0f;
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
        state1 = 0.0f;
        state2 = 0.0f;
    }

    /** SAMPLE-RATE safe. `cutoffHz` is clamped below Nyquist: the prewarping
        tangent goes to infinity at Nyquist, and a modulated cutoff WILL be
        driven there. */
    void setCutoff (float cutoffHz) noexcept
    {
        const auto nyquist = static_cast<float> (sampleRateHz) * 0.5f;

        // 0.49 rather than 0.5 of the sample rate: tan() at exactly Nyquist is
        // infinite, and the last fraction of a percent of range is inaudible.
        const auto clamped = juce::jlimit (10.0f, nyquist * 0.98f, cutoffHz);

        g = std::tan (juce::MathConstants<float>::pi * clamped
                      / static_cast<float> (sampleRateHz));

        updateCoefficients();
    }

    /** Resonance 0..1. Mapped so 1 is just short of self-oscillation: a
        filter that can actually self-oscillate is a filter that can be left
        screaming by a preset load. */
    void setResonance (float resonance) noexcept
    {
        const auto clamped = juce::jlimit (0.0f, 1.0f, resonance);
        setQ (juce::jmap (clamped, 0.5f, 20.0f));
    }

    /** Sets Q directly. Used to give one section of a cascaded 24 dB filter a
        flat Butterworth response while the other carries the resonance - see
        FilterSlot, and the note there about why both sections must not share
        the user's Q. */
    void setQ (float q) noexcept
    {
        k = 1.0f / juce::jmax (0.05f, q);
        updateCoefficients();
    }

    /** Butterworth Q: maximally flat, no resonant peak. */
    static constexpr float getFlatQ() noexcept { return 0.70710678f; }

    /** SAMPLE-RATE. */
    Outputs processSample (float input) noexcept
    {
        const auto v3 = input - state2;
        const auto v1 = a1 * state1 + a2 * v3;
        const auto v2 = state2 + a2 * state1 + a3 * v3;

        state1 = 2.0f * v1 - state1;
        state2 = 2.0f * v2 - state2;

        Outputs out;
        out.lowPass = v2;
        out.bandPass = v1;
        out.highPass = input - k * v1 - v2;
        out.notch = input - k * v1;

        return out;
    }

    /** Band-pass only, normalised to unity peak gain. Used by the formant
        filter, where the resonant peaks must not also boost the level. */
    float processBandPassNormalised (float input) noexcept
    {
        return processSample (input).bandPass * k;
    }

    /** The factor that normalises the RAW band-pass output to unity peak gain.

        `processBandPassNormalised` applies it internally, but a caller that
        needs all four outputs from ONE call - because a filter instance may be
        advanced once per sample and no more (CLAUDE.md section 3) - has to
        apply it itself. Without it a band-pass's peak gain IS its Q, so
        turning resonance up makes the filter louder rather than narrower. */
    float getBandPassNormalisation() const noexcept { return k; }

    /** True when the state has gone non-finite, which can only happen if a
        NaN was fed in. Recovering is cheaper than letting it poison the mix. */
    bool hasBlownUp() const noexcept
    {
        return ! std::isfinite (state1) || ! std::isfinite (state2);
    }

private:
    void updateCoefficients() noexcept
    {
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    double sampleRateHz = 44100.0;

    float g = 0.0f;
    float k = 1.0f;
    float a1 = 1.0f;
    float a2 = 0.0f;
    float a3 = 0.0f;

    float state1 = 0.0f;
    float state2 = 0.0f;
};

} // namespace gnarl::dsp
