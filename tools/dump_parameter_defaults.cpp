/**
    Dumps every parameter's range, default and unit to JSON.

    The browser preview of the UI has no plugin behind it, so its mock relay
    has no idea what any parameter's range or default is - every knob reads 0%
    and every readout is meaningless. That makes the preview useless for
    judging layout and actively misleading in a screenshot.

    This dumps the real values from the real APVTS, so the preview is seeded
    from the same single source of truth as the plugin. Built only when
    GNARL_BUILD_DEMO_RENDERER is on; the output is committed.
*/
#include "PluginProcessor.h"

#include <juce_events/juce_events.h>

#include <cstdio>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    gnarl::GnarlProcessor processor;

    juce::DynamicObject::Ptr root (new juce::DynamicObject());

    for (auto* parameter : processor.getParameters())
    {
        auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter);

        if (withID == nullptr)
            continue;

        juce::DynamicObject::Ptr entry (new juce::DynamicObject());

        entry->setProperty ("name", withID->getName (64));
        entry->setProperty ("label", withID->getLabel());
        entry->setProperty ("default", parameter->getDefaultValue());
        entry->setProperty ("steps", parameter->getNumSteps());

        // The formatted text the host would show, at the default and at both
        // extremes, so the UI can display real units without reimplementing
        // every formatter in TypeScript.
        entry->setProperty ("textAtDefault", parameter->getText (parameter->getDefaultValue(), 32));
        entry->setProperty ("textAtMin", parameter->getText (0.0f, 32));
        entry->setProperty ("textAtMax", parameter->getText (1.0f, 32));

        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
        {
            const auto& range = ranged->getNormalisableRange();
            entry->setProperty ("min", range.start);
            entry->setProperty ("max", range.end);
            entry->setProperty ("skew", range.skew);
            entry->setProperty ("interval", range.interval);
        }

        root->setProperty (withID->paramID, juce::var (entry.get()));
    }

    const auto json = juce::JSON::toString (juce::var (root.get()), false);

    if (argc > 1)
    {
        juce::File file { juce::String (argv[1]) };
        file.replaceWithText (json + "\n");
        std::printf ("wrote %d parameters to %s\n",
                     processor.getParameters().size(),
                     file.getFullPathName().toRawUTF8());
    }
    else
    {
        std::printf ("%s\n", json.toRawUTF8());
    }

    return 0;
}
