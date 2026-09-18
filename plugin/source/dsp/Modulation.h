#pragma once

#include <juce_core/juce_core.h>

#include <array>

namespace gnarl::dsp
{

/**
    Modulation destinations.

    WHY AN ENUM RATHER THAN "ANY PARAMETER". The mod slot's destination is
    STORED as a parameter-ID string (see ParameterIDs.h), which keeps presets
    stable and leaves the door open to widening this list. But the audio
    thread cannot look up a string, so the message thread resolves the string
    to one of these before publishing it.

    The list covers the parameters that are musically worth modulating. That
    is a v1 scope decision, not a limitation of the storage format: adding a
    destination is appending an entry here and a case in the apply function,
    and every preset that used the old list still loads because it stored
    strings.
*/
enum class ModDestination
{
    none = 0,

    // Oscillator 1
    osc1TablePosition,
    osc1WarpAmount,
    osc1Level,
    osc1Pan,
    osc1PitchSemi,
    osc1PitchFine,
    osc1UnisonDetune,
    osc1GrainSize,
    osc1GrainDensity,

    // Oscillator 2
    osc2TablePosition,
    osc2WarpAmount,
    osc2Level,
    osc2Pan,
    osc2PitchSemi,
    osc2PitchFine,
    osc2UnisonDetune,

    // Sub and noise
    subLevel,
    noiseLevel,

    // Filter 1
    filter1Cutoff,
    filter1Resonance,
    filter1Drive,
    filter1Mix,
    filter1FormantX,
    filter1FormantY,
    filter1FormantThroat,

    // Filter 2
    filter2Cutoff,
    filter2Resonance,
    filter2Drive,
    filter2Mix,

    count
};

/**
    Modulation offsets for one voice, for one chunk.

    Kept as a small flat array indexed by destination rather than a copy of
    the whole VoiceSettings. Copying VoiceSettings per voice per chunk would
    be 16 voices x 8 chunks x a few hundred bytes every block, to change a
    handful of fields.
*/
struct ModulationOffsets
{
    std::array<float, static_cast<std::size_t> (ModDestination::count)> values {};

    void clear() noexcept { values.fill (0.0f); }

    float get (ModDestination destination) const noexcept
    {
        return values[static_cast<std::size_t> (destination)];
    }

    void add (ModDestination destination, float amount) noexcept
    {
        if (destination == ModDestination::none)
            return;

        values[static_cast<std::size_t> (destination)] += amount;
    }
};

/** Maps a parameter ID to a destination. MESSAGE THREAD only: it compares
    strings. Returns `none` for an unmodulatable or unknown parameter, which
    is also what makes an old preset referring to a since-removed parameter
    load harmlessly. */
ModDestination getDestinationForParameterID (const juce::String& parameterID);

/** The parameter ID a destination corresponds to, for the UI's labels and for
    round-tripping a preset. */
juce::String getParameterIDForDestination (ModDestination destination);

/** Human-readable name, for the mod matrix's destination column. */
juce::String getDestinationDisplayName (ModDestination destination);

/** Every destination, for the UI's picker. */
juce::StringArray getAllDestinationDisplayNames();

} // namespace gnarl::dsp
