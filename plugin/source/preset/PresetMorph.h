#pragma once

#include "PresetFormat.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <map>

namespace gnarl::preset
{

/**
    Morph between two presets.

    NOT EVERY PARAMETER CAN BE HALF WAY BETWEEN TWO VALUES, and that is the
    only interesting thing about this. A cutoff can: half way between 200 Hz
    and 2 kHz is a real filter setting. A distortion CURVE cannot - there is
    nothing half way between tanh and bitcrush, and an implementation that
    interpolates the choice index lands on `fold` on the way past, which is
    neither of the two things the user asked to morph between.

    So continuous parameters are interpolated and DISCRETE ones step at the
    half-way point: below 0.5 you get A's choice, above it B's. A morph
    control therefore sweeps smoothly and changes character once, in the
    middle, which is what a morph between two patches actually sounds like.

    THE SAME GOES FOR THE STATE THAT IS NOT A PARAMETER. The FX chain order is
    a permutation and the mod slots' destinations are parameter-ID strings -
    neither has a midpoint. They step with everything else discrete, so the
    morph takes one side's routing wholesale rather than assembling a chain
    from halves of two.

    MESSAGE THREAD.
*/
class PresetMorph
{
public:
    /** True for an ID whose value has no meaningful midpoint. */
    static bool isDiscrete (const juce::String& parameterId);

    /** MESSAGE THREAD. Writes the morph of `a` and `b` at `position` (0 gives
        a, 1 gives b) into `state`.

        Both trees must be presets. Returns false and writes nothing if either
        is not - a half-applied morph is worse than none. */
    static bool apply (juce::AudioProcessorValueTreeState& state,
                       const juce::ValueTree& a,
                       const juce::ValueTree& b,
                       float position);

private:
    /** The parameter values out of a preset's state tree, by ID, already
        normalised. */
    static std::map<juce::String, float> readNormalised (
        juce::AudioProcessorValueTreeState& state, const juce::ValueTree& preset);
};

} // namespace gnarl::preset
