#pragma once

#include "StateVariableFilter.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>

namespace gnarl::dsp
{

/**
    Morphable vowel filter: five parallel band-passes on an X/Y pad.

    This is the filter GNARL exists for. A table-position wobble through a
    vowel morph is a talking growl, and it is the sound the genre is built on,
    so this gets more care than anything else in the filter section.

    WHY FIVE BAND-PASSES IN PARALLEL, not a cascade. A vowel is defined by the
    positions of its formants - resonant peaks in the spectrum - and peaks add.
    Cascading band-passes would multiply their responses and produce a single
    narrow peak where the passbands happen to overlap, which sounds like a
    resonant filter rather than like a voice.

    FORMANT DATA. The frequencies and relative amplitudes below are the
    standard published values for sung vowels, the same figures used in
    phonetics texts and in every formant filter since the 1970s. They are
    measurements of human speech, not anyone's creative work.

    MORPHING. The five vowels sit at fixed anchors on the X/Y pad and the puck
    is interpolated between them by inverse-distance weighting. Interpolating
    the FREQUENCIES (logarithmically) rather than crossfading five filter
    outputs is what makes the morph sound like a mouth moving: crossfading
    would sound like two voices fading over each other.

    THROAT shifts every formant together, which is perceived as the size of the
    speaker rather than as a change of vowel - a child at one end, something
    enormous at the other. It is the control that turns this from a vowel
    filter into a monster generator.
*/
class FormantFilter
{
public:
    static constexpr int kNumFormants = 5;
    static constexpr int kNumVowels = 5;

    enum class Vowel { a = 0, e, i, o, u };

    /** A point on the X/Y pad, 0..1 in each axis.

        A local struct rather than juce::Point, deliberately: that type lives
        in juce_graphics, and a DSP header has no business dragging the
        graphics module into the audio path's include graph. */
    struct PadPosition
    {
        float x = 0.5f;
        float y = 0.5f;
    };

    void prepare (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;

        for (auto& filter : bandPasses)
            filter.prepare (sampleRate);

        reset();
        setPosition (0.5f, 0.5f);
        setThroat (0.0f);
        setResonance (0.5f);
    }

    void reset() noexcept
    {
        for (auto& filter : bandPasses)
            filter.reset();
    }

    /**
        Sets the puck position on the pad, 0..1 in each axis.

        Anchor layout, matching the labels the UI draws:

              A
           E     O
             I U

        A at top centre because it is the most open vowel and the natural
        home position; I and U at the bottom because they are the closed ones.
    */
    void setPosition (float x, float y) noexcept
    {
        padX = juce::jlimit (0.0f, 1.0f, x);
        padY = juce::jlimit (0.0f, 1.0f, y);
        coefficientsDirty = true;
    }

    /** -1..1. Negative shifts every formant up (smaller throat), positive
        shifts them down. Up to an octave each way. */
    void setThroat (float amount) noexcept
    {
        throat = juce::jlimit (-1.0f, 1.0f, amount);
        coefficientsDirty = true;
    }

    /** 0..1. Controls formant bandwidth: low is broad and breathy, high is
        narrow and vocal. */
    void setResonance (float amount) noexcept
    {
        resonance = juce::jlimit (0.0f, 1.0f, amount);
        coefficientsDirty = true;
    }

    /** BLOCK-RATE. Recomputes the filter coefficients if anything changed.
        Kept out of processSample because five tangents per sample is not a
        cost worth paying for a control that moves at control rate. */
    void updateCoefficients() noexcept
    {
        if (! coefficientsDirty)
            return;

        coefficientsDirty = false;

        const auto weights = getVowelWeights();

        // Octave shift from the throat control.
        const auto throatRatio = std::exp2 (-throat);

        for (int formant = 0; formant < kNumFormants; ++formant)
        {
            // Interpolated in LOG frequency, because pitch perception is
            // logarithmic: a linear blend from 350 Hz to 800 Hz spends most of
            // its travel in the top half of the interval.
            auto logFrequency = 0.0f;
            auto amplitude = 0.0f;

            for (int vowel = 0; vowel < kNumVowels; ++vowel)
            {
                const auto weight = weights[static_cast<std::size_t> (vowel)];
                logFrequency += weight * std::log (kFormantFrequencies[vowel][formant]);
                amplitude += weight * kFormantAmplitudes[vowel][formant];
            }

            const auto frequency = std::exp (logFrequency) * throatRatio;

            auto& filter = bandPasses[static_cast<std::size_t> (formant)];
            filter.setCutoff (frequency);

            // Higher formants are broader in real voices, so Q rises with
            // formant index rather than being uniform.
            const auto formantQ = juce::jmap (resonance, 0.3f, 0.9f)
                                * (1.0f - static_cast<float> (formant) * 0.08f);
            filter.setResonance (juce::jlimit (0.1f, 0.95f, formantQ));

            formantGains[static_cast<std::size_t> (formant)] = amplitude;
        }
    }

    /** SAMPLE-RATE. */
    float processSample (float input) noexcept
    {
        auto sum = 0.0f;

        for (int formant = 0; formant < kNumFormants; ++formant)
        {
            sum += bandPasses[static_cast<std::size_t> (formant)]
                       .processBandPassNormalised (input)
                 * formantGains[static_cast<std::size_t> (formant)];
        }

        // Normalised so a morph does not also change level. The vowels have
        // very different total formant energy, and without this the pad would
        // double as a volume control.
        return sum * kOutputScale;
    }

    bool hasBlownUp() const noexcept
    {
        for (const auto& filter : bandPasses)
            if (filter.hasBlownUp())
                return true;

        return false;
    }

    /** Where each vowel sits on the pad. Public because the UI draws these
        as labelled anchors. */
    static constexpr PadPosition kVowelAnchors[kNumVowels] = {
        { 0.5f, 1.0f },   // A - top centre, the open vowel
        { 0.0f, 0.5f },   // E - left
        { 0.15f, 0.0f },  // I - bottom left
        { 1.0f, 0.5f },   // O - right
        { 0.85f, 0.0f },  // U - bottom right
    };

    /** Exposed for the UI, which draws the anchors, and for tests. */
    static PadPosition getVowelAnchor (Vowel vowel) noexcept
    {
        return kVowelAnchors[static_cast<int> (vowel)];
    }

    static float getFormantFrequency (Vowel vowel, int formant) noexcept
    {
        return kFormantFrequencies[static_cast<int> (vowel)]
                                  [juce::jlimit (0, kNumFormants - 1, formant)];
    }

private:
    /** Inverse-distance weights over the five anchors, summing to 1. */
    std::array<float, kNumVowels> getVowelWeights() const noexcept
    {
        std::array<float, kNumVowels> weights {};
        auto total = 0.0f;

        for (int vowel = 0; vowel < kNumVowels; ++vowel)
        {
            const auto anchor = kVowelAnchors[vowel];
            const auto dx = padX - anchor.x;
            const auto dy = padY - anchor.y;
            const auto distanceSquared = dx * dx + dy * dy;

            // Sitting exactly on an anchor must give that vowel alone, not a
            // division by zero.
            if (distanceSquared < 1.0e-6f)
            {
                weights.fill (0.0f);
                weights[static_cast<std::size_t> (vowel)] = 1.0f;
                return weights;
            }

            // Squared inverse distance: sharper than plain inverse distance,
            // so the pad's corners read clearly as their own vowel instead of
            // everything sounding like a blur of all five.
            const auto weight = 1.0f / (distanceSquared * distanceSquared);
            weights[static_cast<std::size_t> (vowel)] = weight;
            total += weight;
        }

        if (total > 0.0f)
            for (auto& weight : weights)
                weight /= total;

        return weights;
    }

    /** Published formant frequencies for sung vowels, in Hz. */
    static constexpr float kFormantFrequencies[kNumVowels][kNumFormants] = {
        { 800.0f, 1150.0f, 2900.0f, 3900.0f, 4950.0f },   // A  (father)
        { 400.0f, 1600.0f, 2700.0f, 3300.0f, 4950.0f },   // E  (bed)
        { 350.0f, 1700.0f, 2700.0f, 3700.0f, 4950.0f },   // I  (see)
        { 450.0f,  800.0f, 2830.0f, 3500.0f, 4950.0f },   // O  (boat)
        { 325.0f,  700.0f, 2530.0f, 3500.0f, 4950.0f },   // U  (boot)
    };

    /** Matching relative amplitudes, as linear gains (from the published dB
        figures: 0, -6, -32, -20, -50 for A, and so on). */
    static constexpr float kFormantAmplitudes[kNumVowels][kNumFormants] = {
        { 1.0f, 0.501f, 0.025f, 0.100f, 0.003f },   // A
        { 1.0f, 0.063f, 0.032f, 0.018f, 0.001f },   // E
        { 1.0f, 0.100f, 0.032f, 0.016f, 0.001f },   // I
        { 1.0f, 0.355f, 0.158f, 0.040f, 0.002f },   // O
        { 1.0f, 0.251f, 0.032f, 0.010f, 0.001f },   // U
    };

    /** Compensates the summed band-pass gain so the pad is not a volume
        control. Chosen so a full-scale saw stays near full scale. */
    static constexpr float kOutputScale = 2.2f;

    double sampleRateHz = 44100.0;

    std::array<StateVariableFilter, kNumFormants> bandPasses {};
    std::array<float, kNumFormants> formantGains {};

    float padX = 0.5f;
    float padY = 0.5f;
    float throat = 0.0f;
    float resonance = 0.5f;
    bool coefficientsDirty = true;
};

} // namespace gnarl::dsp
