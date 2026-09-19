#pragma once

#include "PresetFormat.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace gnarl::preset
{

/**
    The presets that ship in the binary.

    WHAT THESE ARE AND ARE NOT. Each one is built from deliberate parameter
    choices that exercise a specific part of the architecture - the formant
    filter under a drawn LFO, two detuned oscillators through one filter, the
    graintable mode, a mid/side widener on a centred patch. They are
    constructed from what the DSP does, and every one of them says in a
    comment what it is for.

    They have NOT been listened to and adjusted by ear, because that is not
    something code can do. Rendering them is: `GnarlRenderDemo --bank` writes
    every one to a .wav, which is how they get judged and revised. Treat the
    numbers here as a starting bank that a sound designer should then move,
    not as finished patches.

    LEGALLY, EVERYTHING HERE IS OURS. The wavetables are the ones the plugin
    generates; no table, sample or parameter set comes from Serum, Vital,
    Massive or anything else commercial (CLAUDE.md section 9). The category
    names follow the convention those products share, which is an information
    architecture and free to use.

    BUILT FROM THE DEFAULT STATE, NOT FROM A PARTIAL TREE. A preset has to be
    a COMPLETE state: APVTS creates a node for any parameter missing from a
    tree it is given, using the value that parameter currently holds - so a
    partial factory preset would inherit whatever the last patch had for
    everything it did not mention, and would sound different depending on what
    you loaded before it. Each preset here is therefore the plugin's default
    state with a named set of overrides applied.
*/
class FactoryBank
{
public:
    /** One parameter override, as a real (not normalised) value. */
    struct Setting
    {
        const char* id;
        float value;
    };

    struct Definition
    {
        const char* name;
        const char* category;
        /** What this patch is for, shown in the browser. */
        const char* description;
        const char* tags;
        std::vector<Setting> settings;
    };

    /** The bank's definitions. Static data, no processor needed. */
    static std::vector<Definition> getDefinitions();

    /** Builds the bank as preset trees.

        `defaultState` must be the plugin's state BEFORE any editing - the
        constructor's copyState() - for the reason in the class comment. */
    static std::vector<juce::ValueTree> build (
        const juce::AudioProcessorValueTreeState& state,
        const juce::ValueTree& defaultState);

    static int getCount();
};

} // namespace gnarl::preset
