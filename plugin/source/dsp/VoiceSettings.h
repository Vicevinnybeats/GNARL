#pragma once

#include "Envelope.h"
#include "FilterSlot.h"
#include "Lfo.h"
#include "ModMatrix.h"
#include "Modulation.h"
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

    std::array<Envelope::Settings, pid::kNumEnvelopes> envelopes {};
    std::array<Lfo::Settings, pid::kNumLfos> lfos {};

    /** Curves for the drawable LFOs, copied by value from the editor. */
    std::array<LfoCurve, pid::kNumLfos> lfoCurves {};

    ModMatrix modMatrix {};

    /** Macro knob values, 0..1. Global, so every voice sees the same macro. */
    std::array<float, pid::kNumMacros> macros {};

    /** MIDI controllers, 0..1, global. */
    float modWheel = 0.0f;
    float pitchBend = 0.5f;
    float aftertouch = 0.0f;

    /** Host transport, for the LFOs' free-run phase lock. */
    Lfo::TransportInfo transport {};

    /** Per-voice detune and drift depth, 0..1. */
    float analogDrift = 0.15f;

    /** Cents of detune at analogDrift 1. Small: this is glue, not an effect. */
    static constexpr float kMaxDriftCents = 12.0f;
};

/**
    Applies modulation offsets to a copy of the fields a voice actually reads.

    Full-range offsets: a depth of 1 sweeps the destination across its whole
    range, because a modulation control that only reaches part of the way is a
    control the user has to compensate for. Everything is clamped to the
    parameter's legal range afterwards, since modulation overshoots by design.
*/
void applyModulation (const VoiceSettings& base,
                      const ModulationOffsets& offsets,
                      VoiceSettings::OscillatorState (&oscillators)[pid::kNumOscillators],
                      VoiceSettings::SubState& sub,
                      VoiceSettings::NoiseState& noise,
                      FilterSlot::Settings (&filters)[pid::kNumFilters]) noexcept;

} // namespace gnarl::dsp
