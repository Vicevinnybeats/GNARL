#pragma once

#include "Saturation.h"
#include "StateVariableFilter.h"
#include "../params/ParameterChoices.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>

namespace gnarl::dsp
{

/** Reference signal for the FX distortion's drive compensation.

    -12 dBFS, which is roughly where a synth voice arrives at the rack. The
    compensation is a level match on ONE amplitude, because a nonlinear curve
    does not have a single gain - a quieter signal through the same setting
    comes out slightly louder relative to its input, and that residual is the
    compression the user asked for.

    Built at static-initialisation time: setSettings runs on the audio thread,
    where a function-local static would mean a guard variable and std::sin
    would mean 32 transcendentals per block.
*/
inline constexpr int kCompensationPoints = 32;
inline constexpr float kCompensationAmplitude = 0.25f;

inline const std::array<float, kCompensationPoints> kCompensationSine = []
{
    std::array<float, kCompensationPoints> table {};

    for (int i = 0; i < kCompensationPoints; ++i)
        table[static_cast<std::size_t> (i)] = std::sin (
            juce::MathConstants<float>::twoPi * static_cast<float> (i)
            / static_cast<float> (kCompensationPoints));

    return table;
}();

/**
    The FX rack's distortion.

    Two instances of this exist, because stacking drive is most of a riddim
    patch - see docs/fx-architecture.md.

    Its curve list is a SUPERSET of the voice filter's drive curves, plus two
    that only make sense here:

      - bitcrush quantises the amplitude. That is not saturation, and running
        it through the filter's oversampled drive stage would be pointless -
        the whole character is the quantisation error.
      - downsample aliases ON PURPOSE. The filter's drive stage is oversampled
        specifically to remove aliasing, so a curve whose point is aliasing
        cannot live there without fighting it.

    THE TONE CONTROL IS A PRE-TILT, NOT A POST EQ, and that ordering is the
    whole reason it exists. Driving a bass-heavy signal into a saturator with
    no tilt just makes the low end louder, because the low end is where all
    the energy already is; tilting up first is what makes the drive land on the
    harmonics. Correcting afterwards cannot reproduce that - the harmonics it
    would be correcting were never generated.

    Real-time safe. Aliasing is the CALLER's problem: like everything in
    Saturation.h, this generates harmonics above Nyquist by construction and
    has to be run oversampled. See CLAUDE.md section 3.
*/
class FxDistortion
{
public:
    using Type = choices::FxDistortionType;

    struct Settings
    {
        bool enabled = false;
        float mix = 1.0f;
        Type type = Type::tanh;
        /** Input gain in dB. A level decision, so it reads as one. */
        float driveDb = 6.0f;
        /** -1..1 pre-tilt: negative darkens the drive, positive brightens it. */
        float tone = 0.0f;
        /** -1..1 asymmetry. A symmetric curve makes only odd harmonics; this
            is what puts even ones in, and it is the difference between
            "fuzzy" and "growling". */
        float bias = 0.0f;
        /** Output trim in dB. */
        float outputDb = 0.0f;
    };

    void prepare (double sampleRate) noexcept
    {
        setSampleRate (sampleRate);
        reset();
    }

    /** AUDIO THREAD SAFE, and does not reset - the oversampling factor is a
        live parameter and this runs while audio is flowing. */
    void setSampleRate (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;

        for (auto& channel : channels)
        {
            channel.tilt.setSampleRate (sampleRateHz);
            channel.tilt.setCutoff (kTiltCentreHz);
            channel.tilt.setQ (StateVariableFilter::getFlatQ());
        }

        // A DC blocker at 20 Hz. `bias` and every even-harmonic curve shift
        // the signal off zero, and DC that reaches the master is headroom
        // thrown away for something nobody can hear.
        dcBlockerCoefficient = 1.0f - std::exp (-juce::MathConstants<float>::twoPi
                                                * 20.0f / static_cast<float> (sampleRateHz));
    }

    void reset() noexcept
    {
        for (auto& channel : channels)
        {
            channel.tilt.reset();
            channel.dcState = 0.0f;
            channel.holdCounter = 0;
            channel.heldSample = 0.0f;
        }
    }

    /** BLOCK-RATE. */
    void setSettings (const Settings& newSettings) noexcept
    {
        settings = newSettings;

        preGain = juce::Decibels::decibelsToGain (
            juce::jlimit (0.0f, 48.0f, settings.driveDb));
        outputGain = juce::Decibels::decibelsToGain (
            juce::jlimit (-24.0f, 24.0f, settings.outputDb));

        // Drive compensation. DriveStage measures the slope at the origin, and
        // that is the WRONG measurement over this much range: the slope at
        // zero is the SMALL-SIGNAL gain, so normalising it to unity makes the
        // linear region right and leaves the loud part of the signal wherever
        // the curve's compression put it. With 36 dB of pre-gain into a
        // saturator almost none of the signal is in the linear region any
        // more, and the first version of this measured -12.5 dB across every
        // curve - a drive control that is still a volume control, just
        // pointing the other way.
        //
        // So match RMS instead: push one period of a reference sine through
        // the curve and divide by how much its RMS grew.
        auto sum = 0.0f;
        auto sumSquares = 0.0f;

        for (const auto sine : kCompensationSine)
        {
            const auto out = shape (kCompensationAmplitude * sine);
            sum += out;
            sumSquares += out * out;
        }

        constexpr auto points = static_cast<float> (kCompensationPoints);

        // About the MEAN, not about zero, because the DC blocker downstream
        // removes exactly this offset. Measuring RMS about zero would let an
        // even curve's DC term pay for the compensation and then throw that
        // term away, leaving rectify quiet by the size of its own offset.
        const auto mean = sum / points;
        const auto outputRms = std::sqrt (juce::jmax (0.0f, sumSquares / points - mean * mean));

        // A sine's RMS is its amplitude over root two.
        constexpr auto inputRms = kCompensationAmplitude
                                / juce::MathConstants<float>::sqrt2;

        compensation = outputRms > 1.0e-5f ? inputRms / outputRms : 1.0f;

        // Downsample hold length, from the tone control rather than a
        // parameter of its own: the two do the same job for this curve.
        const auto factor = juce::jmap (juce::jlimit (0.0f, 1.0f, settings.driveDb / 48.0f),
                                        1.0f, 64.0f);
        holdLength = juce::jmax (1, juce::roundToInt (factor));

        // Bitcrush step count, likewise driven by `drive`: 16 bits down to
        // about 2. Below two bits it is a square wave and stops being a
        // control.
        const auto bits = juce::jmap (juce::jlimit (0.0f, 1.0f, settings.driveDb / 48.0f),
                                      16.0f, 2.0f);
        quantiseStep = 1.0f / std::exp2 (juce::jmax (1.0f, bits) - 1.0f);
    }

    /** SAMPLE-RATE. */
    float processSample (int channelIndex, float input) noexcept
    {
        if (! settings.enabled)
            return input;

        auto& channel = channels[static_cast<std::size_t> (
            juce::jlimit (0, static_cast<int> (channels.size()) - 1, channelIndex))];

        const auto dry = input;

        // The pre-tilt. A low-shelf either way about 700 Hz, built from the
        // SVF's low and high outputs so it costs one filter rather than two.
        auto x = input;

        if (! juce::exactlyEqual (settings.tone, 0.0f))
        {
            const auto outputs = channel.tilt.processSample (input);
            const auto tilt = juce::jlimit (-1.0f, 1.0f, settings.tone);

            // tone > 0 lifts the highs and drops the lows, and the reverse
            // below zero. Gains are symmetric so the tilt is level-neutral at
            // the centre frequency.
            const auto lowGain = 1.0f - tilt * 0.8f;
            const auto highGain = 1.0f + tilt * 0.8f;

            x = outputs.lowPass * lowGain + outputs.highPass * highGain;
        }

        auto wet = shapeWithState (channel, x);

        // DC blocker. Bias and the even-harmonic curves both shift the signal
        // off zero.
        channel.dcState += dcBlockerCoefficient * (wet - channel.dcState);
        wet = (wet - channel.dcState) * compensation * outputGain;

        const auto mix = juce::jlimit (0.0f, 1.0f, settings.mix);
        return dry + (wet - dry) * mix;
    }

private:
    /** Where the tone control pivots. 700 Hz sits between the fundamental of a
        riddim bass note and the formant region the growl lives in, so the tilt
        trades one for the other rather than just changing the level. */
    static constexpr float kTiltCentreHz = 700.0f;

    struct Channel
    {
        StateVariableFilter tilt;
        float dcState = 0.0f;
        int holdCounter = 0;
        float heldSample = 0.0f;
    };

    /** The stateless part of the curve, used both to process and to measure
        the slope for drive compensation. */
    float shape (float input) const noexcept
    {
        const auto driven = input * preGain
                          + juce::jlimit (-1.0f, 1.0f, settings.bias) * 0.5f;

        switch (settings.type)
        {
            case Type::tanh:     return saturation::tanhCurve (driven);
            case Type::tube:     return saturation::tubeCurve (driven);
            case Type::hardClip: return saturation::hardClipCurve (driven);
            case Type::fold:     return saturation::foldCurve (driven);
            case Type::rectify:  return saturation::rectifyCurve (driven);

            case Type::bitcrush:
                // Quantise after a soft clip, so the input is bounded and the
                // step count means something. Quantising an unbounded signal
                // gives a step size that depends on how loud the patch is.
                return quantise (saturation::tanhCurve (driven));

            case Type::downsample:
                // The hold is stateful, so the stateless path just clips. This
                // makes the slope measurement slightly optimistic for this one
                // curve, which costs a little accuracy in the drive
                // compensation and nothing else.
                return saturation::tanhCurve (driven);

            case Type::count:
            default:             return driven;
        }
    }

    /** The full curve, including the one type that needs per-channel state. */
    float shapeWithState (Channel& channel, float input) noexcept
    {
        if (settings.type != Type::downsample)
            return shape (input);

        // Sample-and-hold at a fraction of the sample rate. The aliasing IS
        // the effect, so this deliberately does not band-limit.
        if (channel.holdCounter <= 0)
        {
            channel.heldSample = saturation::tanhCurve (
                input * preGain + juce::jlimit (-1.0f, 1.0f, settings.bias) * 0.5f);
            channel.holdCounter = holdLength;
        }

        --channel.holdCounter;
        return channel.heldSample;
    }

    float quantise (float x) const noexcept
    {
        return quantiseStep > 0.0f
             ? std::round (x / quantiseStep) * quantiseStep
             : x;
    }

    double sampleRateHz = 44100.0;

    Settings settings {};

    float preGain = 2.0f;
    float outputGain = 1.0f;
    float compensation = 1.0f;
    float dcBlockerCoefficient = 0.0f;

    int holdLength = 1;
    float quantiseStep = 0.0f;

    std::array<Channel, 2> channels {};
};

} // namespace gnarl::dsp
