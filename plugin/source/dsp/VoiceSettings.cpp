#include "VoiceSettings.h"

#include <cmath>

namespace gnarl::dsp
{

namespace
{
    float addUnipolar (float base, float offset) noexcept
    {
        return juce::jlimit (0.0f, 1.0f, base + offset);
    }

    float addBipolar (float base, float offset) noexcept
    {
        return juce::jlimit (-1.0f, 1.0f, base + offset);
    }

    /** Cutoff is modulated in OCTAVES, not in Hz.

        A full-depth LFO on a cutoff in Hz spends almost all its travel above
        10 kHz, where nothing is audible, and crosses the musically useful
        range in a few percent of its motion. In octaves the same LFO sweeps
        evenly across the keyboard, which is what a filter wobble has to do. */
    float modulateCutoff (float baseHz, float offset) noexcept
    {
        constexpr float kOctavesAtFullDepth = 6.0f;

        return juce::jlimit (20.0f, 20000.0f,
                             baseHz * std::exp2 (offset * kOctavesAtFullDepth));
    }
}

void applyModulation (const VoiceSettings& base,
                      const ModulationOffsets& offsets,
                      VoiceSettings::OscillatorState (&oscillators)[pid::kNumOscillators],
                      VoiceSettings::SubState& sub,
                      VoiceSettings::NoiseState& noise,
                      FilterSlot::Settings (&filters)[pid::kNumFilters]) noexcept
{
    for (std::size_t i = 0; i < pid::kNumOscillators; ++i)
        oscillators[i] = base.oscillators[i];

    sub = base.sub;
    noise = base.noise;

    for (std::size_t i = 0; i < pid::kNumFilters; ++i)
        filters[i] = base.filters[i];

    // --- Oscillator 1 ------------------------------------------------------
    {
        auto& target = oscillators[0].settings;

        target.tablePosition = addUnipolar (target.tablePosition,
            offsets.get (ModDestination::osc1TablePosition));
        target.warpAmount = addBipolar (target.warpAmount,
            offsets.get (ModDestination::osc1WarpAmount));
        target.level = addUnipolar (target.level,
            offsets.get (ModDestination::osc1Level));
        target.pan = addBipolar (target.pan,
            offsets.get (ModDestination::osc1Pan));
        target.unisonDetune = addUnipolar (target.unisonDetune,
            offsets.get (ModDestination::osc1UnisonDetune));

        // Pitch in semitones, so a depth of 1 is two octaves - wide enough
        // for a sweep, narrow enough that a small depth is still playable.
        oscillators[0].pitchOffsetSemitones +=
            offsets.get (ModDestination::osc1PitchSemi) * 24.0f
            + offsets.get (ModDestination::osc1PitchFine);

        target.grainSizeMs = juce::jlimit (1.0f, 500.0f,
            target.grainSizeMs + offsets.get (ModDestination::osc1GrainSize) * 250.0f);
        target.grainDensityHz = juce::jlimit (1.0f, 200.0f,
            target.grainDensityHz + offsets.get (ModDestination::osc1GrainDensity) * 100.0f);
    }

    // --- Oscillator 2 ------------------------------------------------------
    {
        auto& target = oscillators[1].settings;

        target.tablePosition = addUnipolar (target.tablePosition,
            offsets.get (ModDestination::osc2TablePosition));
        target.warpAmount = addBipolar (target.warpAmount,
            offsets.get (ModDestination::osc2WarpAmount));
        target.level = addUnipolar (target.level,
            offsets.get (ModDestination::osc2Level));
        target.pan = addBipolar (target.pan,
            offsets.get (ModDestination::osc2Pan));
        target.unisonDetune = addUnipolar (target.unisonDetune,
            offsets.get (ModDestination::osc2UnisonDetune));

        oscillators[1].pitchOffsetSemitones +=
            offsets.get (ModDestination::osc2PitchSemi) * 24.0f
            + offsets.get (ModDestination::osc2PitchFine);
    }

    sub.settings.level = addUnipolar (sub.settings.level,
        offsets.get (ModDestination::subLevel));
    noise.level = addUnipolar (noise.level,
        offsets.get (ModDestination::noiseLevel));

    // --- Filters -----------------------------------------------------------
    filters[0].cutoffHz = modulateCutoff (filters[0].cutoffHz,
        offsets.get (ModDestination::filter1Cutoff));
    filters[0].resonance = addUnipolar (filters[0].resonance,
        offsets.get (ModDestination::filter1Resonance));
    filters[0].drive = addUnipolar (filters[0].drive,
        offsets.get (ModDestination::filter1Drive));
    filters[0].mix = addUnipolar (filters[0].mix,
        offsets.get (ModDestination::filter1Mix));
    filters[0].formantX = addUnipolar (filters[0].formantX,
        offsets.get (ModDestination::filter1FormantX));
    filters[0].formantY = addUnipolar (filters[0].formantY,
        offsets.get (ModDestination::filter1FormantY));
    filters[0].formantThroat = addBipolar (filters[0].formantThroat,
        offsets.get (ModDestination::filter1FormantThroat));

    filters[1].cutoffHz = modulateCutoff (filters[1].cutoffHz,
        offsets.get (ModDestination::filter2Cutoff));
    filters[1].resonance = addUnipolar (filters[1].resonance,
        offsets.get (ModDestination::filter2Resonance));
    filters[1].drive = addUnipolar (filters[1].drive,
        offsets.get (ModDestination::filter2Drive));
    filters[1].mix = addUnipolar (filters[1].mix,
        offsets.get (ModDestination::filter2Mix));
}

} // namespace gnarl::dsp
