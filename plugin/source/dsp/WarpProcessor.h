#pragma once

#include "../params/ParameterChoices.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>

namespace gnarl::dsp
{

/**
    Oscillator warp modes.

    A warp reshapes the waveform BEFORE the table is read, by transforming the
    read phase. That is what makes one table sound like a dozen, and it is
    where most of a riddim patch's character comes from.

    PHASE WARPS vs SAMPLE WARPS. Most modes are pure phase transforms and live
    here: given a phase in [0, 1) they return a new phase in [0, 1). Two modes
    (FM and ring modulation) need the other oscillator's output and so cannot
    be phase transforms; they are applied by the oscillator itself, and
    `isPhaseWarp` says which is which.

    ALIASING. A phase warp introduces harmonics the mip level was not chosen
    for, so warping reintroduces exactly the aliasing the mip map removes.
    The oscillator compensates two ways: it biases the mip level down by the
    warp's bandwidth expansion (`getBandwidthExpansion`), and the nonlinear
    warps run inside the oversampled path. Neither is free, which is why the
    expansion figures below are estimates tuned by ear and measurement rather
    than a formula - see WarpProcessorTests.

    Every function here is real-time safe, branch-light and allocation-free.
*/
namespace warp
{
    using Mode = choices::WarpMode;

    /** True when the mode is a phase transform this file can apply alone. */
    inline constexpr bool isPhaseWarp (Mode mode) noexcept
    {
        return mode != Mode::fmFromOther && mode != Mode::ringMod;
    }

    /** Rough factor by which a mode multiplies the waveform's bandwidth at
        full amount. Used to bias mip level selection so a warped oscillator
        does not alias. Conservative on purpose: too high only costs a little
        brightness, too low costs audible aliasing. */
    inline float getBandwidthExpansion (Mode mode, float amount) noexcept
    {
        const auto a = std::abs (amount);

        switch (mode)
        {
            case Mode::off:             return 1.0f;

            // Sync multiplies the read rate outright, so the expansion is the
            // sync ratio.
            case Mode::sync:            return 1.0f + a * 7.0f;

            // Bending compresses part of the cycle, raising the local slope.
            case Mode::bendPlus:
            case Mode::bendMinus:       return 1.0f + a * 3.0f;

            // A discontinuity at the pulse edge is broadband by definition.
            case Mode::pwm:             return 1.0f + a * 4.0f;
            case Mode::asymmetry:       return 1.0f + a * 2.5f;

            // Mirroring halves the period, doubling every harmonic number.
            case Mode::mirror:          return 1.0f + a;

            // Stepping the waveform is the harshest thing here.
            case Mode::quantize:        return 1.0f + a * 12.0f;

            case Mode::phaseDistortion: return 1.0f + a * 4.0f;
            case Mode::remap:           return 1.0f + a * 3.0f;

            // Handled by the oscillator, not by a phase transform.
            case Mode::fmFromOther:     return 1.0f + a * 8.0f;
            case Mode::ringMod:         return 2.0f;

            case Mode::count:
            default:                    return 1.0f;
        }
    }

    /** Wraps into [0, 1). Uses a loop rather than fmod because the input is
        never more than a few cycles out of range, and a couple of compares
        beat a division. */
    inline float wrapPhase (float phase) noexcept
    {
        while (phase >= 1.0f)
            phase -= 1.0f;

        while (phase < 0.0f)
            phase += 1.0f;

        return phase;
    }

    /**
        Applies a phase warp.

        @param phase   read phase in [0, 1)
        @param mode    warp mode; FM and ring mod are returned unchanged
        @param amount  -1..1, where 0 is always identity
    */
    inline float applyPhase (float phase, Mode mode, float amount) noexcept
    {
        // Amount 0 must be bit-exact identity, or every patch with warp off
        // would still pay for it.
        if (juce::exactlyEqual (amount, 0.0f))
            return phase;

        const auto a = juce::jlimit (-1.0f, 1.0f, amount);
        const auto magnitude = std::abs (a);

        switch (mode)
        {
            case Mode::sync:
            {
                // Reads the cycle faster and restarts it, the classic hard
                // sync edge. Up to 8x.
                const auto ratio = 1.0f + magnitude * 7.0f;
                return wrapPhase (phase * ratio);
            }

            case Mode::bendPlus:
            {
                // Compresses the first half of the cycle into less phase, so
                // the waveform's leading edge steepens.
                const auto bend = 1.0f - magnitude * 0.9f;
                return std::pow (phase, bend);
            }

            case Mode::bendMinus:
            {
                const auto bend = 1.0f + magnitude * 9.0f;
                return std::pow (phase, bend);
            }

            case Mode::pwm:
            {
                // Squeezes the whole cycle into a fraction of the period and
                // holds the end value for the rest - a pulse-width control
                // that works on any waveform, not just a square.
                const auto width = juce::jmax (0.02f, 1.0f - magnitude * 0.96f);
                return juce::jmin (1.0f, phase / width);
            }

            case Mode::asymmetry:
            {
                // Moves the cycle's midpoint, stretching one half and
                // squashing the other. Asymmetry is what turns a symmetric
                // table into something with even harmonics.
                const auto pivot = juce::jlimit (0.05f, 0.95f, 0.5f + a * 0.45f);

                return phase < pivot
                     ? 0.5f * phase / pivot
                     : 0.5f + 0.5f * (phase - pivot) / (1.0f - pivot);
            }

            case Mode::mirror:
            {
                // Plays the cycle forwards then backwards, halving the period
                // and forcing the waveform even.
                const auto folded = phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
                return wrapPhase (juce::jmap (magnitude, phase, folded));
            }

            case Mode::quantize:
            {
                // Steps the phase, which steps the waveform: a bitcrush
                // applied to shape rather than to amplitude.
                const auto steps = juce::jmax (2.0f, (1.0f - magnitude) * 126.0f + 2.0f);
                return std::floor (phase * steps) / steps;
            }

            case Mode::phaseDistortion:
            {
                // Casio-style: bends phase around the midpoint, which shifts
                // the spectrum without changing the table.
                const auto skewed = phase + a * 0.5f
                                  * std::sin (juce::MathConstants<float>::twoPi * phase);
                return wrapPhase (skewed);
            }

            case Mode::remap:
            {
                // An S-curve through the cycle. Gentler than bend, and
                // symmetric, so it adds odd harmonics rather than even.
                const auto centred = phase * 2.0f - 1.0f;
                const auto shaped = centred * (1.0f - magnitude)
                                  + centred * centred * centred * magnitude;
                return wrapPhase ((shaped + 1.0f) * 0.5f);
            }

            case Mode::off:
            case Mode::fmFromOther:
            case Mode::ringMod:
            case Mode::count:
            default:
                return phase;
        }
    }
}

} // namespace gnarl::dsp
