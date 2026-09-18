#pragma once

#include "FilterSlot.h"
#include "NoiseGenerator.h"
#include "WavetableOscillator.h"
#include "../params/ParameterChoices.h"
#include "../params/ParameterIDs.h"

#include <array>

namespace gnarl::dsp
{

class Wavetable;

/**
    Everything a voice needs for one block, read from the parameters ONCE.

    The processor fills this per block; the voices read it. Sixteen voices each
    reading the parameter tree themselves would do the same string-free atomic
    loads sixteen times over and, worse, could see different values within one
    block if a parameter changed mid-loop - so two notes of the same chord
    would be filtered differently.
*/
struct VoiceSettings
{
    /** Send levels from one source into the two filters and straight out. */
    struct Sends
    {
        float toFilter1 = 1.0f;
        float toFilter2 = 0.0f;
        float direct = 0.0f;

        bool isSilent() const noexcept
        {
            return toFilter1 <= 0.0f && toFilter2 <= 0.0f && direct <= 0.0f;
        }
    };

    struct OscillatorState
    {
        bool enabled = false;
        WavetableOscillator::Settings settings {};
        Sends sends {};

        /** Coarse + fine pitch offset, in semitones. */
        float pitchOffsetSemitones = 0.0f;

        /** The table to read. Owned by the library, borrowed here. */
        const Wavetable* table = nullptr;
    };

    struct SubState
    {
        bool enabled = false;
        WavetableOscillator::Settings settings {};
        Sends sends {};
        float pitchOffsetSemitones = -12.0f;
        const Wavetable* table = nullptr;
    };

    struct NoiseState
    {
        bool enabled = false;
        choices::NoiseType type = choices::NoiseType::white;
        float level = 0.3f;
        float pan = 0.0f;
        float pitchOffsetSemitones = 0.0f;
        Sends sends {};
    };

    std::array<OscillatorState, pid::kNumOscillators> oscillators {};
    SubState sub {};
    NoiseState noise {};

    std::array<FilterSlot::Settings, pid::kNumFilters> filters {};
    std::array<bool, pid::kNumFilters> filterEnabled { true, false };
    choices::FilterRouting filterRouting = choices::FilterRouting::series;

    /** Per-voice detune and drift depth, 0..1. */
    float analogDrift = 0.15f;

    /** Cents of detune at analogDrift 1. Small: this is glue, not an effect. */
    static constexpr float kMaxDriftCents = 12.0f;
};

} // namespace gnarl::dsp
