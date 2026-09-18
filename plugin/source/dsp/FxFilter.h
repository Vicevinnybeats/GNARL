#pragma once

#include "Saturation.h"
#include "StateVariableFilter.h"
#include "../params/ParameterChoices.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>

namespace gnarl::dsp
{

/**
    The FX rack's filter.

    DISTINCT FROM THE TWO VOICE FILTERS, and the difference is where it sits
    rather than what it does. A voice filter runs per voice, before the mix, so
    sixteen held notes get sixteen independent sweeps - which is what makes a
    growl. This one runs once on the summed signal, so a sweep here moves the
    whole chord together. Both are worth having and neither substitutes for the
    other, which is why the FX filter's types are a deliberate SUBSET: no
    formant and no comb, because those are per-voice effects whose character
    comes from tracking the note.

    THE DRIVE RUNS BEFORE THE FILTER, the same ordering FilterSlot uses and for
    the same reason: saturate, then filter the harmonics the saturation added.
    Driving afterwards distorts the filtered result, which sounds like a
    distortion pedal rather than like a filter with attitude.

    Its aliasing is the CALLER's problem, exactly as in FxDistortion: a
    memoryless waveshaper generates harmonics above Nyquist by construction and
    no filter after it can take them out again (CLAUDE.md section 3), so the
    rack runs this oversampled.

    A STEREO PATH IS TWO PATHS. Each channel holds its own pair of SVFs; one
    shared filter advanced twice per sample makes the output a function of the
    block layout, which is the bug the voice filter shipped.

    Real-time safe.
*/
class FxFilter
{
public:
    using Type = choices::FxFilterType;

    struct Settings
    {
        bool enabled = false;
        float mix = 1.0f;
        Type type = Type::lowPass12;
        float cutoffHz = 20000.0f;
        /** 0..1, mapped to Q by StateVariableFilter. */
        float resonance = 0.1f;
        /** 0..1 pre-filter saturation. */
        float drive = 0.0f;
    };

    void prepare (double sampleRate) noexcept
    {
        for (auto& channel : channels)
        {
            channel.first.prepare (sampleRate);
            channel.second.prepare (sampleRate);
        }

        setSettings (settings);
        reset();
    }

    /** AUDIO THREAD SAFE, non-resetting: the oversampling factor is a live
        parameter, so this runs while notes are sounding. Resetting here would
        clear the filter state mid-note - the same family of bug as routing a
        live rate change through prepare(). */
    void setSampleRate (double sampleRate) noexcept
    {
        for (auto& channel : channels)
        {
            channel.first.setSampleRate (sampleRate);
            channel.second.setSampleRate (sampleRate);
        }

        setSettings (settings);
    }

    void reset() noexcept
    {
        for (auto& channel : channels)
        {
            channel.first.reset();
            channel.second.reset();
        }
    }

    /** BLOCK-RATE. */
    void setSettings (const Settings& newSettings) noexcept
    {
        settings = newSettings;

        cascaded = isCascaded (settings.type);

        for (auto& channel : channels)
        {
            channel.first.setCutoff (settings.cutoffHz);

            if (cascaded)
            {
                channel.second.setCutoff (settings.cutoffHz);

                // ONLY ONE SECTION CARRIES THE RESONANCE - the lesson
                // FilterSlot records. Giving both the user's Q multiplies
                // their peaks into Q squared, so the 24 dB modes measure six
                // times louder than the 12 dB ones at the same setting and
                // closing the filter makes the patch LOUDER. The first section
                // stays maximally flat, so the slope is 24 dB/octave with a
                // single peak of the height the user asked for.
                channel.first.setQ (StateVariableFilter::getFlatQ());
                channel.second.setResonance (settings.resonance);
            }
            else
            {
                channel.first.setResonance (settings.resonance);
            }
        }

        // Pre-gain for the drive. Up to +24 dB, which is where a filter's
        // drive stops adding attitude and starts being a fuzz.
        preGain = juce::Decibels::decibelsToGain (
            juce::jlimit (0.0f, 1.0f, settings.drive) * 24.0f);

        // Level-matched the way FxDistortion is, and NOT the way DriveStage
        // is: an RMS match on a reference sine rather than the slope at the
        // origin, because the slope at zero is the small-signal gain and over
        // 24 dB of pre-gain the signal has left that region. See CLAUDE.md.
        auto sumSquares = 0.0f;

        for (const auto sine : kCompensationSine)
        {
            const auto out = saturation::tanhCurve (kCompensationAmplitude * sine * preGain);
            sumSquares += out * out;
        }

        constexpr auto points = static_cast<float> (kCompensationPoints);
        const auto outputRms = std::sqrt (sumSquares / points);

        // tanh is odd, so its output has no DC term and there is no mean to
        // subtract - unlike FxDistortion, whose curve list includes an even
        // one.
        constexpr auto inputRms = kCompensationAmplitude
                                / juce::MathConstants<float>::sqrt2;

        driveCompensation = outputRms > 1.0e-5f ? inputRms / outputRms : 1.0f;
    }

    /** SAMPLE-RATE. */
    float processSample (int channelIndex, float input) noexcept
    {
        if (! settings.enabled)
            return input;

        auto& channel = channels[static_cast<std::size_t> (
            juce::jlimit (0, static_cast<int> (channels.size()) - 1, channelIndex))];

        // Drive first, then filter.
        auto wet = settings.drive > 0.0f
                 ? saturation::tanhCurve (input * preGain) * driveCompensation
                 : input;

        wet = pick (channel.first, channel.first.processSample (wet));

        if (cascaded)
            wet = pick (channel.second, channel.second.processSample (wet));

        if (channel.first.hasBlownUp() || channel.second.hasBlownUp())
        {
            channel.first.reset();
            channel.second.reset();
            return input;
        }

        const auto amount = juce::jlimit (0.0f, 1.0f, settings.mix);
        return input + (wet - input) * amount;
    }

    const Settings& getSettings() const noexcept { return settings; }

    /** True for the types built from two cascaded sections. */
    static constexpr bool isCascaded (Type type) noexcept
    {
        return type == Type::lowPass24 || type == Type::highPass24;
    }

private:
    /** Which of the SVF's four simultaneous outputs this type wants.

        Takes the filter as well as its outputs because the band-pass needs the
        filter's normalisation factor: the RAW band-pass output has a peak gain
        of Q, so without it turning resonance up would make the filter louder
        instead of narrower - and a band-pass at full resonance would arrive
        +26 dB hot. All four outputs come from one call because a filter
        instance may be advanced once per sample and no more. */
    float pick (const StateVariableFilter& filter,
                const StateVariableFilter::Outputs& outputs) const noexcept
    {
        switch (settings.type)
        {
            case Type::lowPass12:
            case Type::lowPass24:   return outputs.lowPass;
            case Type::highPass12:
            case Type::highPass24:  return outputs.highPass;
            case Type::notch12:     return outputs.notch;

            case Type::bandPass12:
                return outputs.bandPass * filter.getBandPassNormalisation();

            case Type::count:
            default:                return outputs.lowPass;
        }
    }

    /** Reference signal for the drive compensation, as in FxDistortion:
        -12 dBFS, one period, built at static-initialisation time so
        setSettings costs no transcendentals on the audio thread. */
    static constexpr int kCompensationPoints = 32;
    static constexpr float kCompensationAmplitude = 0.25f;

    static inline const std::array<float, kCompensationPoints> kCompensationSine = []
    {
        std::array<float, kCompensationPoints> table {};

        for (int i = 0; i < kCompensationPoints; ++i)
            table[static_cast<std::size_t> (i)] = std::sin (
                juce::MathConstants<float>::twoPi * static_cast<float> (i)
                / static_cast<float> (kCompensationPoints));

        return table;
    }();

    Settings settings {};
    bool cascaded = false;

    float preGain = 1.0f;
    float driveCompensation = 1.0f;

    // One PAIR per channel: a stereo path is two paths.
    std::array<std::pair<StateVariableFilter, StateVariableFilter>, 2> channels {};
};

} // namespace gnarl::dsp
