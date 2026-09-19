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

    /** One mod slot's destination.

        SEPARATE FROM `Setting` BECAUSE A DESTINATION IS NOT A PARAMETER. A
        host parameter is a number, and a number indexing a list of
        destinations would silently repoint every saved preset the moment the
        list changed - so destinations are parameter-ID strings living in the
        ValueTree (CLAUDE.md section 4).

        Which means a factory preset built only from `Setting`s has no
        modulation at all, however many mod slots it enables. That was true
        of this entire bank: every slot was enabled, given a depth, and left
        pointing at `none`. `FactoryBankTests` now fails the build if it
        happens again. */
    struct Routing
    {
        int slot;
        /** A parameter ID from ParameterIDs.h. */
        const char* destination;
    };

    /** One point of a drawable LFO curve. Same fields as
        `dsp::LfoCurve::Point`, kept as plain data so the table below reads
        as a shape rather than as constructor calls. */
    struct CurvePoint
    {
        float time;     // 0..1
        float value;    // 0..1
        float tension;  // -1..1, shaping the segment AFTER this point
        bool step;      // hold until the next point, instead of interpolating
    };

    /** A whole drawn curve for one LFO. */
    struct Curve
    {
        int lfo;
        std::vector<CurvePoint> points;
    };

    struct Definition
    {
        const char* name;
        const char* category;
        /** What this patch is for, shown in the browser. */
        const char* description;
        const char* tags;
        std::vector<Setting> settings;
        /** Empty for a patch with no modulation - which should be rare, and
            is a deliberate choice rather than an oversight when it happens. */
        std::vector<Routing> routings {};
        std::vector<Curve> curves {};
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
