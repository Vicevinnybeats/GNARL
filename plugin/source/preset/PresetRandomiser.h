#pragma once

#include "../params/ParameterIDs.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace gnarl::preset
{

/**
    Randomise.

    RANDOMISING 430 PARAMETERS UNIFORMLY GIVES NOISE, EVERY TIME. That is the
    whole design problem here, and it is not a tuning detail: a uniform draw
    puts the filter at 20 Hz as often as at 2 kHz, turns all fourteen effects
    on at once, sets the amp envelope's attack to eight seconds, and picks a
    master gain. The result is not a patch anyone would keep, and a randomise
    button people press once and never again might as well not exist.

    So every parameter falls into one of four policies, decided from its ID:

      - FROZEN. Never touched. The master fader, the voice count, the poly
        mode, the pitch-bend range, the oversampling factor: these are the
        player's setup rather than the sound, and a randomise that moves them
        is a randomise that breaks the instrument. Getting a quiet patch
        because the dice chose -48 dB reads as a bug, not as variety.
      - GATED. Booleans, drawn against a PROBABILITY rather than a coin flip.
        Fourteen effects at even odds gives seven effects on, which is mush.
        Each family carries its own likelihood: an oscillator is usually on, a
        reverb sometimes, a bitcrusher rarely.
      - RANGED. Anything frequency- or time-like, drawn inside a musically
        useful SUB-range of what the parameter allows. A cutoff is allowed to
        go to 20 Hz because a user may want that; the dice should not, because
        below about 80 Hz the patch is inaudible and the roll is wasted.
      - FREE. Everything else, drawn uniformly across its normalised range.
        Depths, mixes, pans, curve amounts: these are genuinely fine anywhere.

    AND IT IS AN AMOUNT, NOT A BUTTON. `amount` blends each drawn value
    towards the patch that is already loaded, so 15% is a nudge and 100% is a
    fresh patch. A nudge is what a producer actually reaches for - "this is
    close, give me a variation" - and it is the setting that makes the feature
    worth having rather than a novelty.

    MESSAGE THREAD. This writes parameters through the host, so it belongs
    where a UI gesture belongs.
*/
class PresetRandomiser
{
public:
    struct Options
    {
        /** 0..1. How far each parameter moves from where it is now. */
        float amount = 1.0f;

        /** Sections to leave completely alone, so "randomise the FX only" is
            possible - which is most of how this gets used once someone has a
            patch they like. */
        bool oscillators = true;
        bool filters = true;
        bool envelopes = true;
        bool lfos = true;
        bool modMatrix = true;
        bool fx = true;
    };

    /** MESSAGE THREAD. Randomises `state`'s parameters in place.

        Takes an explicit seed so a randomise is reproducible in a test. A
        randomise nobody can reproduce is a randomise nobody can debug when a
        customer says it made something horrible. */
    static void apply (juce::AudioProcessorValueTreeState& state,
                       const Options& options,
                       juce::Random& random);

    /** Which policy an ID falls under. Public so the tests can assert the
        classification directly rather than inferring it from outcomes - the
        interesting failures here are miscategorisations, and a statistical
        test for those needs thousands of draws to see what one lookup says. */
    enum class Policy
    {
        frozen,
        gated,
        ranged,
        free
    };

    static Policy classify (const juce::String& parameterId);

    /** For a GATED id, how likely it is to come out on. */
    static float getProbability (const juce::String& parameterId);

    /** For a RANGED id, the sub-range of 0..1 the dice may use. */
    static juce::Range<float> getRange (const juce::String& parameterId);

private:
    static bool isSectionEnabled (const juce::String& parameterId,
                                  const Options& options);
};

} // namespace gnarl::preset
