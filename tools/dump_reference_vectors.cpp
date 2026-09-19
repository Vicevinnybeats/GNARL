/**
    Dumps reference vectors for the TypeScript mirrors.

    WHY THIS EXISTS. Two pieces of TypeScript duplicate C++ logic on purpose,
    because the UI has to show what the engine does and the engine's copy is on
    the audio thread:

      - `ui/src/bridge/warp.ts` is a port of WarpProcessor.h, so the wavetable
        display can draw the warped waveform.
      - `ui/src/bridge/formatters.ts` mirrors the formatters in
        ParameterRanges.h, so a knob's readout says what the host's automation
        panel says.

    Duplicated logic is a liability, and the liability is not that it is
    written twice - it is that it can drift and nothing notices. warp.ts
    claimed in its own header comment to be checked against reference values
    dumped from C++; there was no dumper, no reference file and no test. This
    is that dumper, and `ui/scripts/check-reference.mjs` is that check.

    THE FORMATTERS ARE SAMPLED RATHER THAN NAMED. There is no way to ask a
    juce::AudioProcessorParameter which formatting function it was given, so
    this dumps what it actually PRODUCES at a spread of points across its
    range. The TypeScript side infers which formatter to use and the check
    compares every parameter at every sampled point - so an inference that
    picks the wrong formatter fails loudly rather than being trusted.

    Built with the demo renderer, and the output is committed.
*/
#include "PluginProcessor.h"
#include "dsp/WarpProcessor.h"
#include "params/ParameterChoices.h"

#include <juce_events/juce_events.h>

#include <cstdio>

using namespace gnarl;

namespace
{
    /** Points across each parameter's range. Enough to catch a formatter that
        is right at the ends and wrong in the middle - which is exactly what a
        threshold-based formatter (kHz above 1000, Hz below) would be. */
    constexpr int kFormatSamples = 21;

    /** Phase points per warp mode. The warps have corners - sync wraps,
        quantize steps - so the sampling has to be dense enough to land on
        both sides of one. */
    constexpr int kPhaseSamples = 33;
    constexpr int kAmountSamples = 5;
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    GnarlProcessor processor;

    juce::DynamicObject::Ptr root (new juce::DynamicObject());

    // --- Formatters --------------------------------------------------------

    juce::DynamicObject::Ptr formatting (new juce::DynamicObject());

    for (auto* parameter : processor.getParameters())
    {
        auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter);

        if (withID == nullptr)
            continue;

        juce::Array<juce::var> samples;

        for (int i = 0; i < kFormatSamples; ++i)
        {
            const auto normalised = static_cast<float> (i)
                                  / static_cast<float> (kFormatSamples - 1);

            juce::DynamicObject::Ptr point (new juce::DynamicObject());
            point->setProperty ("n", normalised);
            point->setProperty ("text", parameter->getText (normalised, 32));

            samples.add (juce::var (point.get()));
        }

        formatting->setProperty (withID->paramID, samples);
    }

    root->setProperty ("formatting", juce::var (formatting.get()));

    // --- Warps -------------------------------------------------------------

    juce::Array<juce::var> warps;

    for (int mode = 0; mode < static_cast<int> (choices::WarpMode::count); ++mode)
    {
        for (int a = 0; a < kAmountSamples; ++a)
        {
            const auto amount = static_cast<float> (a)
                              / static_cast<float> (kAmountSamples - 1);

            juce::Array<juce::var> phases;

            for (int p = 0; p < kPhaseSamples; ++p)
            {
                const auto phase = static_cast<float> (p)
                                 / static_cast<float> (kPhaseSamples - 1);

                juce::DynamicObject::Ptr point (new juce::DynamicObject());
                point->setProperty ("phase", phase);
                point->setProperty ("out", dsp::warp::applyPhase (
                    phase, static_cast<choices::WarpMode> (mode), amount));

                phases.add (juce::var (point.get()));
            }

            juce::DynamicObject::Ptr entry (new juce::DynamicObject());
            entry->setProperty ("mode", mode);
            entry->setProperty ("amount", amount);
            entry->setProperty ("bandwidth", dsp::warp::getBandwidthExpansion (
                static_cast<choices::WarpMode> (mode), amount));
            entry->setProperty ("points", phases);

            warps.add (juce::var (entry.get()));
        }
    }

    root->setProperty ("warp", warps);

    const auto json = juce::JSON::toString (juce::var (root.get()), false);

    if (argc > 1)
    {
        juce::File file { juce::String (argv[1]) };
        file.replaceWithText (json + "\n");
        std::printf ("wrote reference vectors to %s\n",
                     file.getFullPathName().toRawUTF8());
    }
    else
    {
        std::printf ("%s\n", json.toRawUTF8());
    }

    return 0;
}
