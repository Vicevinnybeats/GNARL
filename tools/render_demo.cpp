/**
    Renders GNARL demo clips to .wav, headlessly.

    Exists so the sound can be evaluated without a DAW, which is the whole
    point of a synth and the one thing a unit test cannot tell you. Built only
    when GNARL_BUILD_DEMO_RENDERER is on.

    NOTE ON THE WOBBLE. The LFO engine is Phase 3, so the modulation here is
    applied by setting parameters per block from this file. That is exactly
    what an LFO will do later, at block rate - so these clips are a fair
    preview of the engine's tone, but the real thing will be smoother and
    sample-accurate.
*/
#include "PluginProcessor.h"
#include "preset/FactoryBank.h"
#include "preset/PresetManager.h"
#include "params/ParameterChoices.h"
#include "params/ParameterIDs.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include <cmath>
#include <functional>

using namespace gnarl;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 128;
    constexpr double kTempoBpm = 140.0;

    void setParameter (GnarlProcessor& processor, const char* id, float realValue)
    {
        if (auto* parameter = processor.getValueTreeState().getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (realValue));
    }

    /** Called once per block with the elapsed time, to move parameters. */
    using Automation = std::function<void (GnarlProcessor&, double timeSeconds)>;

    void renderToFile (const juce::File& file,
                       double seconds,
                       const std::function<void (GnarlProcessor&)>& setUpPatch,
                       const std::function<void (juce::MidiBuffer&, int blockIndex)>& midiFor,
                       const Automation& automate)
    {
        GnarlProcessor processor;
        processor.setPlayConfigDetails (0, 2, kSampleRate, kBlockSize);
        processor.prepareToPlay (kSampleRate, kBlockSize);

        setUpPatch (processor);

        // Re-prepared so any wavetable the patch selected is generated on this
        // thread rather than being missing on the first block.
        processor.prepareToPlay (kSampleRate, kBlockSize);

        const auto totalBlocks = static_cast<int> (seconds * kSampleRate / kBlockSize);

        juce::AudioBuffer<float> output (2, totalBlocks * kBlockSize);
        output.clear();

        juce::AudioBuffer<float> block (2, kBlockSize);

        for (int blockIndex = 0; blockIndex < totalBlocks; ++blockIndex)
        {
            const auto time = static_cast<double> (blockIndex * kBlockSize) / kSampleRate;

            automate (processor, time);

            juce::MidiBuffer midi;
            midiFor (midi, blockIndex);

            block.clear();
            processor.processBlock (block, midi);

            for (int channel = 0; channel < 2; ++channel)
                output.copyFrom (channel, blockIndex * kBlockSize, block, channel, 0, kBlockSize);
        }

        const auto peak = output.getMagnitude (0, output.getNumSamples());
        std::printf ("  peak %.3f", peak);

        // Normalised to -1 dBFS so the clips are comparable by ear rather than
        // by whichever patch happened to be hotter.
        if (peak > 0.0f)
            output.applyGain (0.891f / peak);

        file.deleteFile();

        juce::WavAudioFormat format;
        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());

        if (stream == nullptr)
        {
            std::printf ("  FAILED to open %s\n", file.getFullPathName().toRawUTF8());
            return;
        }

        std::unique_ptr<juce::AudioFormatWriter> writer (
            format.createWriterFor (stream.release(), kSampleRate, 2, 24, {}, 0));

        if (writer == nullptr)
        {
            std::printf ("  FAILED to create writer\n");
            return;
        }

        writer->writeFromAudioSampleBuffer (output, 0, output.getNumSamples());
        writer->flush();

        std::printf ("  -> %s\n", file.getFileName().toRawUTF8());
    }

    /** A triplet-grid wobble, 0..1, which is the grid riddim is built on. */
    float tripletWobble (double timeSeconds, double divisionsPerBeat)
    {
        const auto beats = timeSeconds * kTempoBpm / 60.0;
        const auto phase = std::fmod (beats * divisionsPerBeat, 1.0);

        // Asymmetric shape: fast attack, slower fall. A sine wobble sounds
        // like a tremolo; this sounds like a mouth opening.
        const auto shaped = phase < 0.35
                          ? phase / 0.35
                          : 1.0 - (phase - 0.35) / 0.65;

        return static_cast<float> (juce::jlimit (0.0, 1.0, shaped));
    }

    void holdNote (juce::MidiBuffer& midi, int blockIndex, int note)
    {
        if (blockIndex == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, 1.0f), 0);
    }
}

/*  Renders the factory bank.

    THE FACTORY PATCHES ARE CONSTRUCTED FROM WHAT THE DSP DOES, not tuned by
    ear - code cannot listen. This is how they get judged: every preset in the
    bank rendered to a .wav, held for a couple of seconds, so a sound designer
    can hear what the table in FactoryBank.cpp actually produces and move the
    numbers. Without this the bank is a set of plausible-looking constants. */
void renderFactoryBank (const juce::File& outputDirectory)
{
    GnarlProcessor builder;
    builder.setPlayConfigDetails (0, 2, kSampleRate, kBlockSize);
    builder.prepareToPlay (kSampleRate, kBlockSize);

    const auto bank = preset::FactoryBank::build (
        builder.getValueTreeState(), builder.getValueTreeState().copyState());

    const auto definitions = preset::FactoryBank::getDefinitions();

    std::printf ("\nRendering the factory bank (%d presets)\n",
                 static_cast<int> (bank.size()));

    for (std::size_t i = 0; i < bank.size() && i < definitions.size(); ++i)
    {
        const auto name = juce::String (definitions[i].name);

        std::printf ("  %s", name.toRawUTF8());

        const auto file = outputDirectory.getChildFile (
            "gnarl-bank-" + juce::String (static_cast<int> (i) + 1).paddedLeft ('0', 2)
            + "-" + name.toLowerCase().replaceCharacter (' ', '-') + ".wav");

        const auto tree = bank[i];

        // Three seconds: long enough for the pad's 850 ms attack to arrive and
        // for a delay or reverb tail to be audible after the note ends.
        renderToFile (file, 3.0,
            [&tree] (GnarlProcessor& p) { p.getPresets().apply (tree); },
            [] (juce::MidiBuffer& midi, int blockIndex) { holdNote (midi, blockIndex, 36); },
            [] (GnarlProcessor&, double) {});
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // --bank renders the factory presets instead of the demo clips. Separate
    // because the demo clips automate parameters by hand from this file and
    // the bank does not - a factory preset has to sound right without anything
    // driving it.
    auto bankOnly = false;
    juce::File outputDirectory = juce::File::getCurrentWorkingDirectory();

    for (int i = 1; i < argc; ++i)
    {
        const juce::String argument { argv[i] };

        if (argument == "--bank")
            bankOnly = true;
        else
            outputDirectory = juce::File (argument);
    }

    outputDirectory.createDirectory();

    if (bankOnly)
    {
        renderFactoryBank (outputDirectory);
        std::printf ("Done.\n");
        return 0;
    }

    std::printf ("Rendering GNARL demo clips to %s\n",
                 outputDirectory.getFullPathName().toRawUTF8());

    // --- 1. Formant growl --------------------------------------------------
    // The sound the plugin exists for: a vowel filter wobbling on a triplet
    // grid over a harmonically dense table.
    std::printf ("1. formant growl");
    renderToFile (outputDirectory.getChildFile ("gnarl-01-formant-growl.wav"), 4.0,
        [] (GnarlProcessor& p)
        {
            setParameter (p, pid::osc[0].wavetable, 18.0f);   // Growl Morph
            setParameter (p, pid::osc[0].level, 0.9f);
            setParameter (p, pid::osc[0].unisonVoices, 4.0f);
            setParameter (p, pid::osc[0].unisonDetune, 0.18f);
            setParameter (p, pid::osc[0].unisonSpread, 0.6f);

            setParameter (p, pid::sub.enabled, 1.0f);
            setParameter (p, pid::sub.level, 0.55f);
            setParameter (p, pid::sub.octave, -1.0f);

            setParameter (p, pid::filter[0].type,
                          static_cast<float> (choices::FilterType::formant));
            setParameter (p, pid::filter[0].resonance, 0.85f);
            setParameter (p, pid::filter[0].drive, 0.35f);
            setParameter (p, pid::filter[0].driveCurve,
                          static_cast<float> (choices::DriveCurve::tube));
        },
        [] (juce::MidiBuffer& midi, int blockIndex) { holdNote (midi, blockIndex, 31); },
        [] (GnarlProcessor& p, double time)
        {
            // 1/6 notes: the triplet grid.
            const auto wobble = tripletWobble (time, 3.0);

            setParameter (p, pid::filter[0].formantX, wobble);
            setParameter (p, pid::filter[0].formantY, 0.2f + wobble * 0.7f);
            setParameter (p, pid::filter[0].formantThroat, -0.3f + wobble * 0.5f);
            setParameter (p, pid::osc[0].tablePos, wobble * 0.8f);
        });

    // --- 2. Ladder wobble bass ---------------------------------------------
    std::printf ("2. ladder wobble");
    renderToFile (outputDirectory.getChildFile ("gnarl-02-ladder-wobble.wav"), 4.0,
        [] (GnarlProcessor& p)
        {
            setParameter (p, pid::osc[0].wavetable, 14.0f);   // Wobble Bass
            setParameter (p, pid::osc[0].level, 0.9f);
            setParameter (p, pid::osc[0].unisonVoices, 2.0f);
            setParameter (p, pid::osc[0].unisonDetune, 0.1f);

            setParameter (p, pid::sub.enabled, 1.0f);
            setParameter (p, pid::sub.level, 0.7f);

            setParameter (p, pid::filter[0].type,
                          static_cast<float> (choices::FilterType::ladderLowPass));
            setParameter (p, pid::filter[0].resonance, 0.7f);
            setParameter (p, pid::filter[0].drive, 0.5f);
        },
        [] (juce::MidiBuffer& midi, int blockIndex) { holdNote (midi, blockIndex, 29); },
        [] (GnarlProcessor& p, double time)
        {
            const auto wobble = tripletWobble (time, 2.0);   // 1/4 notes
            setParameter (p, pid::filter[0].cutoff, 90.0f * std::exp2 (wobble * 5.0f));
            setParameter (p, pid::osc[0].tablePos, wobble);
        });

    // --- 3. Screech lead ---------------------------------------------------
    std::printf ("3. screech lead");
    renderToFile (outputDirectory.getChildFile ("gnarl-03-screech.wav"), 4.0,
        [] (GnarlProcessor& p)
        {
            setParameter (p, pid::osc[0].wavetable, 4.0f);    // Odd Screech
            setParameter (p, pid::osc[0].unisonVoices, 7.0f);
            setParameter (p, pid::osc[0].unisonDetune, 0.35f);
            setParameter (p, pid::osc[0].unisonSpread, 0.9f);
            setParameter (p, pid::osc[0].warpMode,
                          static_cast<float> (choices::WarpMode::sync));

            setParameter (p, pid::filter[0].type,
                          static_cast<float> (choices::FilterType::bandPass24));
            setParameter (p, pid::filter[0].resonance, 0.6f);
            setParameter (p, pid::filter[0].drive, 0.4f);
        },
        [] (juce::MidiBuffer& midi, int blockIndex) { holdNote (midi, blockIndex, 55); },
        [] (GnarlProcessor& p, double time)
        {
            const auto wobble = tripletWobble (time, 6.0);    // 1/12 notes
            setParameter (p, pid::filter[0].cutoff, 700.0f * std::exp2 (wobble * 3.0f));
            setParameter (p, pid::osc[0].warpAmount, wobble * 0.8f);
            setParameter (p, pid::osc[0].tablePos, wobble);
        });

    // --- 4. Graintable texture ---------------------------------------------
    std::printf ("4. graintable");
    renderToFile (outputDirectory.getChildFile ("gnarl-04-graintable.wav"), 4.0,
        [] (GnarlProcessor& p)
        {
            setParameter (p, pid::osc[0].wavetable, 1.0f);    // Growl Vowels
            setParameter (p, pid::osc[0].mode,
                          static_cast<float> (choices::OscMode::graintable));
            setParameter (p, pid::osc[0].grainSize, 55.0f);
            setParameter (p, pid::osc[0].grainDensity, 28.0f);
            setParameter (p, pid::osc[0].grainPitchJitter, 0.12f);
            setParameter (p, pid::osc[0].unisonVoices, 3.0f);
            setParameter (p, pid::osc[0].unisonDetune, 0.2f);

            setParameter (p, pid::filter[0].type,
                          static_cast<float> (choices::FilterType::formant));
            setParameter (p, pid::filter[0].resonance, 0.7f);
        },
        [] (juce::MidiBuffer& midi, int blockIndex) { holdNote (midi, blockIndex, 36); },
        [] (GnarlProcessor& p, double time)
        {
            const auto wobble = tripletWobble (time, 1.5);
            setParameter (p, pid::filter[0].formantX, wobble);
            setParameter (p, pid::filter[0].formantY, 1.0f - wobble);
            setParameter (p, pid::osc[0].grainPosJitter, wobble * 0.5f);
        });

    renderFactoryBank (outputDirectory);

    std::printf ("Done.\n");
    return 0;
}
