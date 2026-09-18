#include "PresetMorph.h"

#include "../params/ParameterChoices.h"

namespace gnarl::preset
{

bool PresetMorph::isDiscrete (const juce::String& id)
{
    /*  Anything whose value names a THING rather than measuring one. There is
        nothing half way between tanh and bitcrush, between poly and legato,
        or between on and off - and an interpolation that passes through the
        entries in between lands on a third thing nobody asked for. */
    return id.endsWith ("_enabled")
        || id.endsWith ("_type")
        || id.endsWith ("_mode")
        || id.endsWith ("_shape")
        || id.endsWith ("_curve")
        || id.endsWith ("_waveform")
        || id.endsWith ("_source")
        || id.endsWith ("_division")
        || id.endsWith ("_sync")
        || id.endsWith ("_ping_pong")
        || id.endsWith ("_bipolar")
        || id.endsWith ("_phase_random")
        || id.endsWith ("_voices")
        || id.endsWith ("_stages")
        || id.endsWith ("_wavetable")
        || id == pid::polyMode
        || id == pid::oversampling
        || id == pid::velocityCurve
        || id == pid::filterRouting
        || id == pid::maxVoices
        || id == pid::glideAlways;
}

std::map<juce::String, float> PresetMorph::readNormalised (
    juce::AudioProcessorValueTreeState& state, const juce::ValueTree& presetTree)
{
    std::map<juce::String, float> values;

    const auto stateTree = readState (presetTree);

    if (! stateTree.isValid())
        return values;

    /*  Read out of the TREE rather than by applying the preset and reading the
        parameters. Applying would mean loading both patches to morph between
        them, which on a preset that changes a wavetable would kick off a
        generation pass per endpoint - and the endpoints are not what the user
        is going to hear. */
    for (auto child : stateTree)
    {
        if (! child.hasProperty ("id") || ! child.hasProperty ("value"))
            continue;

        const auto id = child.getProperty ("id").toString();
        const auto raw = static_cast<float> (child.getProperty ("value"));

        if (auto* parameter = state.getParameter (id))
            values[id] = parameter->convertTo0to1 (raw);
    }

    return values;
}

bool PresetMorph::apply (juce::AudioProcessorValueTreeState& state,
                         const juce::ValueTree& a,
                         const juce::ValueTree& b,
                         float position)
{
    if (! isPreset (a) || ! isPreset (b))
        return false;

    const auto from = readNormalised (state, a);
    const auto to = readNormalised (state, b);

    // A preset with no parameters in it is not something to morph from.
    if (from.empty() || to.empty())
        return false;

    const auto blend = juce::jlimit (0.0f, 1.0f, position);

    auto* processor = dynamic_cast<juce::AudioProcessor*> (&state.processor);

    if (processor == nullptr)
        return false;

    for (auto* parameter : processor->getParameters())
    {
        auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter);

        if (withID == nullptr)
            continue;

        const auto& id = withID->paramID;

        const auto inA = from.find (id);
        const auto inB = to.find (id);

        // A parameter neither preset mentions is left where it is: it is
        // either new since both were saved, or frozen. Either way, inventing a
        // value for it is not morphing.
        if (inA == from.end() && inB == to.end())
            continue;

        const auto valueA = inA != from.end() ? inA->second : parameter->getValue();
        const auto valueB = inB != to.end() ? inB->second : parameter->getValue();

        const auto morphed = isDiscrete (id)
                           // Steps at the half-way point, so the character
                           // changes once, in the middle.
                           ? (blend < 0.5f ? valueA : valueB)
                           : valueA + (valueB - valueA) * blend;

        parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, morphed));
    }

    return true;
}

} // namespace gnarl::preset
