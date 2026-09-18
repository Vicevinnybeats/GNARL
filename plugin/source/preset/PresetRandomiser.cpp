#include "PresetRandomiser.h"

#include "../params/ParameterChoices.h"

namespace gnarl::preset
{

namespace
{
    bool startsWith (const juce::String& id, const char* prefix)
    {
        return id.startsWith (prefix);
    }

    bool endsWith (const juce::String& id, const char* suffix)
    {
        return id.endsWith (suffix);
    }
}

PresetRandomiser::Policy PresetRandomiser::classify (const juce::String& id)
{
    /*  THE PLAYER'S SETUP, NOT THE SOUND. A randomise that moves any of these
        breaks the instrument rather than varying it: a quiet patch because
        the dice chose -48 dB of master gain reads as a bug, and a patch that
        silently switched to mono legato reads as one too. */
    for (const auto* frozen : { pid::masterGain, pid::bypass, pid::maxVoices,
                                pid::polyMode, pid::pitchBendRange,
                                pid::oversampling, pid::glideAlways,
                                pid::velocityCurve })
    {
        if (id == frozen)
            return Policy::frozen;
    }

    // Booleans. Named consistently, which is what makes this a suffix test
    // rather than a table: every enable in the layout ends in _enabled.
    if (endsWith (id, "_enabled") || endsWith (id, "_sync")
        || endsWith (id, "_ping_pong") || endsWith (id, "_bipolar")
        || endsWith (id, "_phase_random"))
    {
        return Policy::gated;
    }

    // Frequency- and time-like. A cutoff may go to 20 Hz because a user may
    // want that; the dice should not, because the roll is wasted.
    if (endsWith (id, "_cutoff") || endsWith (id, "_freq")
        || endsWith (id, "_centre") || endsWith (id, "_rate")
        || endsWith (id, "_attack") || endsWith (id, "_decay")
        || endsWith (id, "_release") || endsWith (id, "_hold")
        || endsWith (id, "_delay") || endsWith (id, "_time_ms")
        || endsWith (id, "_low_cut") || endsWith (id, "_high_cut")
        || endsWith (id, "_crossover_low") || endsWith (id, "_crossover_high")
        || id == pid::glideTime)
    {
        return Policy::ranged;
    }

    return Policy::free;
}

float PresetRandomiser::getProbability (const juce::String& id)
{
    /*  FOURTEEN EFFECTS AT EVEN ODDS GIVES SEVEN EFFECTS ON, which is mush.
        Each of these is how often the thing should be on in a patch somebody
        would keep, not how often it is interesting on its own. */

    // The first oscillator is the instrument. A patch with no oscillator is
    // silence, which is never a useful roll.
    if (id == pid::osc[0].enabled)
        return 1.0f;

    if (id == pid::osc[1].enabled)
        return 0.55f;

    if (id == pid::sub.enabled)
        return 0.6f;

    if (id == pid::noise.enabled)
        return 0.3f;

    // The first filter is most of what makes a growl; the second is a
    // flourish.
    if (id == pid::filter[0].enabled)
        return 0.85f;

    if (id == pid::filter[1].enabled)
        return 0.35f;

    // On in nearly every riddim patch, which is why it is built in rather
    // than occupying a rack slot.
    if (id == pid::ott.enabled)
        return 0.7f;

    // Drive and tone shaping: common. Space: occasional. A patch with reverb
    // AND delay AND chorus AND phaser is a wash.
    if (id == pid::fxDistortion[0].enabled) return 0.55f;
    if (id == pid::fxDistortion[1].enabled) return 0.2f;
    if (id == pid::fxEq[0].enabled)         return 0.4f;
    if (id == pid::fxEq[1].enabled)         return 0.2f;
    if (id == pid::fxFilter[0].enabled)     return 0.3f;
    if (id == pid::fxFilter[1].enabled)     return 0.12f;
    if (id == pid::fxChorus.enabled)        return 0.2f;
    if (id == pid::fxFlanger.enabled)       return 0.12f;
    if (id == pid::fxPhaser.enabled)        return 0.12f;
    if (id == pid::fxHyper.enabled)         return 0.25f;
    if (id == pid::fxDimension.enabled)     return 0.3f;
    if (id == pid::fxDelay.enabled)         return 0.25f;
    if (id == pid::fxReverb.enabled)        return 0.3f;

    // The limiter is a safety net, and a randomiser that can produce a patch
    // with no ceiling is a randomiser that can produce a patch that clips.
    if (id == pid::fxLimiter.enabled)
        return 0.85f;

    // A mod slot that is off does nothing, and a matrix with sixteen live
    // slots is not modulation, it is weather.
    for (std::size_t i = 0; i < pid::kNumModSlots; ++i)
        if (id == pid::modSlot[i].enabled)
            return i < 4 ? 0.5f : 0.15f;

    // Tempo sync on by default for anything that has the option: a delay or
    // LFO that is not in time with the track is usually a mistake.
    if (endsWith (id, "_sync"))
        return 0.75f;

    return 0.5f;
}

juce::Range<float> PresetRandomiser::getRange (const juce::String& id)
{
    /*  Sub-ranges in NORMALISED space, because that is what the caller sets.
        Every one of these is "the part of the knob worth rolling", not the
        part the knob allows - the difference is the whole point of the RANGED
        policy. */

    // Cutoffs: the range is skewed with 1 kHz at the midpoint, so 0.25..0.95
    // is roughly 90 Hz to 13 kHz. Below that the patch is inaudible; above
    // it, the filter is not doing anything.
    if (endsWith (id, "_cutoff"))
        return { 0.25f, 0.95f };

    // An envelope attack of eight seconds is not a patch, it is a mistake.
    // 0..0.45 on the skewed range is up to about 250 ms.
    if (endsWith (id, "_attack"))
        return { 0.0f, 0.45f };

    // Decay and release want to be short-to-medium for this music.
    if (endsWith (id, "_decay") || endsWith (id, "_release"))
        return { 0.1f, 0.7f };

    if (endsWith (id, "_hold") || endsWith (id, "_delay"))
        return { 0.0f, 0.3f };

    // LFO and FX modulation rates: the wobble range. The audio-rate end is a
    // special effect, not something to land on by accident.
    if (endsWith (id, "_rate"))
        return { 0.2f, 0.75f };

    // A cut that has swallowed the whole band is a silent patch.
    if (endsWith (id, "_low_cut"))
        return { 0.0f, 0.4f };

    if (endsWith (id, "_high_cut"))
        return { 0.6f, 1.0f };

    // Portamento: mostly short. A one-second glide on every note is a
    // specific choice, not a default.
    if (id == pid::glideTime)
        return { 0.0f, 0.35f };

    return { 0.15f, 0.9f };
}

bool PresetRandomiser::isSectionEnabled (const juce::String& id,
                                         const Options& options)
{
    if (startsWith (id, "osc") || startsWith (id, "sub_")
        || startsWith (id, "noise_"))
    {
        return options.oscillators;
    }

    if (startsWith (id, "filter"))
        return options.filters;

    if (startsWith (id, "env"))
        return options.envelopes;

    if (startsWith (id, "lfo"))
        return options.lfos;

    if (startsWith (id, "mod_slot") || startsWith (id, "macro"))
        return options.modMatrix;

    if (startsWith (id, "fx_") || startsWith (id, "ott_"))
        return options.fx;

    // Anything unclassified is global, and the global parameters are frozen
    // anyway - so this is the safe answer rather than a gap.
    return true;
}

void PresetRandomiser::apply (juce::AudioProcessorValueTreeState& state,
                              const Options& options,
                              juce::Random& random)
{
    const auto amount = juce::jlimit (0.0f, 1.0f, options.amount);

    if (amount <= 0.0f)
        return;

    auto* processor = dynamic_cast<juce::AudioProcessor*> (&state.processor);

    if (processor == nullptr)
        return;

    for (auto* parameter : processor->getParameters())
    {
        auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter);

        if (withID == nullptr)
            continue;

        const auto& id = withID->paramID;
        const auto policy = classify (id);

        if (policy == Policy::frozen || ! isSectionEnabled (id, options))
            continue;

        const auto currentValue = parameter->getValue();
        auto drawn = currentValue;

        switch (policy)
        {
            case Policy::gated:
                drawn = random.nextFloat() < getProbability (id) ? 1.0f : 0.0f;
                break;

            case Policy::ranged:
            {
                const auto range = getRange (id);
                drawn = range.getStart()
                      + random.nextFloat() * range.getLength();
                break;
            }

            case Policy::free:
                drawn = random.nextFloat();
                break;

            case Policy::frozen:
            default:
                continue;
        }

        /*  BLENDED TOWARDS WHAT IS LOADED, so `amount` is a dial rather than
            a switch. A boolean blends too, which sounds wrong and is right:
            at 20% there is a 20% chance the new state is taken at all, which
            is exactly what "nudge this patch" means for something that cannot
            be half on. */
        auto next = drawn;

        if (amount < 1.0f)
        {
            if (policy == Policy::gated)
                next = random.nextFloat() < amount ? drawn : currentValue;
            else
                next = currentValue + (drawn - currentValue) * amount;
        }

        parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, next));
    }
}

} // namespace gnarl::preset
