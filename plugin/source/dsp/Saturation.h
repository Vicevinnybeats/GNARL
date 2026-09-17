#pragma once

#include "../params/ParameterChoices.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>

namespace gnarl::dsp
{

/**
    Saturation curves for the filter drive stages and the distortion effect.

    Every curve here is a memoryless waveshaper, which means two things:
      - it is cheap, branch-light and stateless, so it vectorises and needs no
        reset; and
      - it ALIASES, unavoidably, because it generates harmonics above Nyquist.
        Nothing in this file is safe to run at the base sample rate at high
        drive. The caller is responsible for oversampling it.

    Each curve is normalised so that an input of +/-1 maps to roughly +/-1 at
    drive 0, so turning drive up is not also a volume control - a distortion
    that gets louder as it gets dirtier is impossible to A/B.
*/
namespace saturation
{
    using Curve = choices::DriveCurve;

    /** Drive amount 0..1 mapped to a pre-gain. 1 is unity; the top of the
        range is +30 dB, which is enough to fully saturate every curve. */
    inline float driveToPreGain (float drive) noexcept
    {
        return std::exp2 (juce::jlimit (0.0f, 1.0f, drive) * 5.0f);
    }

    /** Smooth, symmetric, odd harmonics. The default because it is the curve
        that stays musical at any drive. */
    inline float tanhCurve (float x) noexcept
    {
        return std::tanh (x);
    }

    /** Asymmetric soft clip: adds even harmonics, which reads as "warm" rather
        than "fuzzy". The asymmetry is what distinguishes it from tanh. */
    inline float tubeCurve (float x) noexcept
    {
        // Positive half compresses sooner than the negative half.
        return x >= 0.0f
             ? std::tanh (x * 0.7f)
             : std::tanh (x * 1.3f) * 0.85f;
    }

    /** Hard clip. Harsh, broadband, and the most aliasing-prone curve here. */
    inline float hardClipCurve (float x) noexcept
    {
        return juce::jlimit (-1.0f, 1.0f, x);
    }

    /** Wavefolding: beyond +/-1 the signal folds back instead of clipping, so
        the harmonic content keeps changing as drive rises rather than simply
        squaring off. */
    inline float foldCurve (float x) noexcept
    {
        // Triangle wave of period 4 that passes |x| <= 1 through unchanged:
        // continuous, odd, with a discontinuous derivative at the folds.
        //
        // Note the form: 1 - |t - 2|, NOT |t - 2| - 1. The latter is the same
        // shape inverted, which makes the curve a polarity flip at low level.
        auto t = std::fmod (x + 1.0f, 4.0f);

        if (t < 0.0f)
            t += 4.0f;

        return 1.0f - std::abs (t - 2.0f);
    }

    /** Full-wave rectification, mixed back with the input. Doubles the
        perceived pitch and is heavily even-harmonic. */
    inline float rectifyCurve (float x) noexcept
    {
        // Blended rather than pure, because a pure rectifier has a large DC
        // offset that would thump every time drive is automated.
        return std::abs (x) * 1.4f - 0.7f;
    }

    inline float apply (Curve curve, float x) noexcept
    {
        switch (curve)
        {
            case Curve::tanh:     return tanhCurve (x);
            case Curve::tube:     return tubeCurve (x);
            case Curve::hardClip: return hardClipCurve (x);
            case Curve::fold:     return foldCurve (x);
            case Curve::rectify:  return rectifyCurve (x);

            case Curve::count:
            default:              return x;
        }
    }

    /** True for curves that are asymmetric and therefore produce DC. The
        caller must high-pass their output, or a patch will slowly pull the
        mix bus off centre. */
    inline constexpr bool producesDC (Curve curve) noexcept
    {
        return curve == Curve::tube || curve == Curve::rectify;
    }
}

/**
    A drive stage: pre-gain, waveshape, compensating post-gain.

    The gains are computed ONCE per block by setDrive(), not per sample.
    Measuring the curve's slope needs two extra curve evaluations, and doing
    that inside the sample loop would triple the cost of the cheapest stage in
    the signal path.

    The post-gain is the reciprocal of the curve's slope at a small signal, so
    quiet material passes through at the same level whatever the drive setting
    is. Without it, drive doubles as a 30 dB volume control and the ear reads
    "louder" as "better" in every A/B.
*/
class DriveStage
{
public:
    using Curve = saturation::Curve;

    /** BLOCK-RATE. */
    void setDrive (Curve newCurve, float drive) noexcept
    {
        curve = newCurve;
        amount = juce::jlimit (0.0f, 1.0f, drive);

        if (amount <= 0.0f)
        {
            preGain = 1.0f;
            postGain = 1.0f;
            bypassed = true;
            return;
        }

        bypassed = false;
        preGain = saturation::driveToPreGain (amount);

        // The slope of the WHOLE stage with respect to its input, so the
        // post-gain cancels the pre-gain as well as the curve's own gain.
        //
        // Dividing by the scaled input instead measures only the curve, leaves
        // the pre-gain in place, and turns drive into a 30 dB volume control -
        // which is the bug this expression replaced. It measured +15 dB of
        // "compensated" gain at drive 0.5.
        constexpr float probe = 1.0e-3f;
        const auto scaled = probe * preGain;
        const auto totalSlope = (saturation::apply (curve, scaled)
                               - saturation::apply (curve, -scaled)) / (2.0f * probe);

        // An EVEN curve (rectify) has zero odd-symmetric slope, so the probe
        // above says nothing about it. Cancelling the pre-gain alone is the
        // best available meaning of "same level" for a rectifier.
        postGain = std::abs (totalSlope) > 1.0e-3f
                 ? 1.0f / totalSlope
                 : 1.0f / preGain;
    }

    /** SAMPLE-RATE. */
    float processSample (float x) const noexcept
    {
        if (bypassed)
            return x;

        return saturation::apply (curve, x * preGain) * postGain;
    }

    bool isBypassed() const noexcept { return bypassed; }
    bool producesDC() const noexcept { return ! bypassed && saturation::producesDC (curve); }

private:
    Curve curve = Curve::tanh;
    float amount = 0.0f;
    float preGain = 1.0f;
    float postGain = 1.0f;
    bool bypassed = true;
};

} // namespace gnarl::dsp
