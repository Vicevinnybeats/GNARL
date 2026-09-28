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

    /** Renders a patch and hands back the audio, un-normalised. */
    juce::AudioBuffer<float> renderToBuffer (
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

        return output;
    }

    void renderToFile (const juce::File& file,
                       double seconds,
                       const std::function<void (GnarlProcessor&)>& setUpPatch,
                       const std::function<void (juce::MidiBuffer&, int blockIndex)>& midiFor,
                       const Automation& automate)
    {
        auto output = renderToBuffer (seconds, setUpPatch, midiFor, automate);

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

        /*  THE AUDITION NOTE COMES FROM THE CATEGORY, and it has to.

            Everything here was rendered at MIDI 36 while the bank was all
            bass patches, and that quietly ruined the graintable pack: a pad
            through the formant filter at C1 has its harmonics far below the
            formant frequencies, so the filter attenuates almost all of it.
            "Grain Choir" came out at 0.008 peak and looked like a broken
            preset rather than a pad being auditioned two octaves below where
            anybody would play it.

            The mapping lives in FactoryBank so the tests audition at exactly
            the same note this does. They had a copy each once, and disagreed
            silently. */
        const auto note = preset::FactoryBank::getAuditionNote (
            preset::readMetadata (bank[i]).category);

        // Three seconds: long enough for the pad's 850 ms attack to arrive and
        // for a delay or reverb tail to be audible after the note ends.
        renderToFile (file, 3.0,
            [&tree] (GnarlProcessor& p) { p.getPresets().apply (tree); },
            [note] (juce::MidiBuffer& midi, int blockIndex) { holdNote (midi, blockIndex, note); },
            [] (GnarlProcessor&, double) {});
    }
}


/*  ONE FILE TO AUDITION THE WHOLE BANK.

    A hundred and fifty separate .wavs is a directory, not something anybody
    listens to. This writes a single continuous take with every preset played
    in turn and a short gap between them, plus an index naming what is at each
    timestamp - so judging the bank is scrubbing through one file with the
    index open beside it, which is what a person actually does.

    GROUPED BY CATEGORY, deliberately. Hearing eighteen subs in a row is how
    you notice that three of them are the same patch; hearing them scattered
    between growls is how you miss it.

    AND NOT NORMALISED PER PRESET, which the individual .wavs above are. Those
    exist to judge one patch, so making them comparable by ear is right. This
    one exists to judge the BANK, and the first thing worth knowing is whether
    a patch arrives thirty decibels under its neighbour - which per-preset
    normalisation is precisely what hides. One gain is applied to the whole
    take at the end. */
void renderBankAudition (const juce::File& outputDirectory)
{
    GnarlProcessor builder;
    builder.setPlayConfigDetails (0, 2, kSampleRate, kBlockSize);
    builder.prepareToPlay (kSampleRate, kBlockSize);

    const auto bank = preset::FactoryBank::build (
        builder.getValueTreeState(), builder.getValueTreeState().copyState());

    const auto definitions = preset::FactoryBank::getDefinitions();

    const auto count = std::min (bank.size(), definitions.size());

    // Stable sort by category, so the bank's own order survives within a
    // category and a preset can still be found by the number it ships under.
    std::vector<std::size_t> order (count);
    std::iota (order.begin(), order.end(), std::size_t { 0 });

    std::stable_sort (order.begin(), order.end(),
        [&definitions] (std::size_t a, std::size_t b)
        {
            return juce::String (definitions[a].category)
                 < juce::String (definitions[b].category);
        });

    constexpr double kHold = 2.4;      // long enough to hear a growl cycle
    constexpr double kGap = 0.35;      // and to tell one patch from the next

    const auto holdSamples = static_cast<int> (kHold * kSampleRate);
    const auto gapSamples = static_cast<int> (kGap * kSampleRate);
    const auto stride = holdSamples + gapSamples;

    juce::AudioBuffer<float> sheet (2, stride * static_cast<int> (count));
    sheet.clear();

    juce::StringArray index;
    index.add ("# GNARL factory bank audition");
    index.add ("# " + juce::String (static_cast<int> (count)) + " presets, "
               + juce::String (kHold, 1) + "s each");
    index.add ("");
    index.add ("time      peak   category   name");

    std::printf ("\nRendering the bank audition (%d presets)\n",
                 static_cast<int> (count));

    for (std::size_t position = 0; position < order.size(); ++position)
    {
        const auto i = order[position];
        const auto& definition = definitions[i];
        const auto tree = bank[i];

        const auto note = preset::FactoryBank::getAuditionNote (
            preset::readMetadata (bank[i]).category);

        auto rendered = renderToBuffer (kHold,
            [&tree] (GnarlProcessor& p) { p.getPresets().apply (tree); },
            [note] (juce::MidiBuffer& midi, int blockIndex) { holdNote (midi, blockIndex, note); },
            [] (GnarlProcessor&, double) {});

        const auto peak = rendered.getMagnitude (0, rendered.getNumSamples());
        const auto start = stride * static_cast<int> (position);
        const auto length = std::min (holdSamples, rendered.getNumSamples());

        for (int channel = 0; channel < 2; ++channel)
            sheet.copyFrom (channel, start, rendered, channel, 0, length);

        /*  A SHORT FADE AT EACH END. The note is cut off rather than released,
            so without this every preset ends in a click - and a hundred and
            fifty clicks is the only thing you would hear. */
        const auto fade = static_cast<int> (0.008 * kSampleRate);
        sheet.applyGainRamp (start, fade, 0.0f, 1.0f);
        sheet.applyGainRamp (start + length - fade, fade, 1.0f, 0.0f);

        const auto seconds = static_cast<double> (start) / kSampleRate;
        const auto minutes = static_cast<int> (seconds) / 60;

        index.add (juce::String (minutes).paddedLeft ('0', 2) + ":"
                   + juce::String (seconds - minutes * 60, 1).paddedLeft ('0', 4)
                   + "  " + juce::String (peak, 3).paddedLeft (' ', 5)
                   + "  " + juce::String (definition.category).paddedRight (' ', 9)
                   + "  " + juce::String (definition.name));

        std::printf ("  %3d  %-8s  %-22s peak %.3f\n",
                     static_cast<int> (position) + 1,
                     definition.category,
                     definition.name,
                     peak);
    }

    // One gain for the whole take, so the relative levels survive.
    const auto peak = sheet.getMagnitude (0, sheet.getNumSamples());

    if (peak > 0.0f)
        sheet.applyGain (0.891f / peak);

    const auto file = outputDirectory.getChildFile ("gnarl-bank-audition.wav");
    file.deleteFile();

    juce::WavAudioFormat format;

    if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
    {
        if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (
                format.createWriterFor (stream.get(), kSampleRate, 2, 16, {}, 0)))
        {
            stream.release();
            writer->writeFromAudioSampleBuffer (sheet, 0, sheet.getNumSamples());
        }
    }

    outputDirectory.getChildFile ("gnarl-bank-audition.txt")
        .replaceWithText (index.joinIntoString ("\n") + "\n");

    std::printf ("\n  %s  (%.1f minutes)\n",
                 file.getFullPathName().toRawUTF8(),
                 static_cast<double> (sheet.getNumSamples()) / kSampleRate / 60.0);
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // --bank renders the factory presets instead of the demo clips. Separate
    // because the demo clips automate parameters by hand from this file and
    // the bank does not - a factory preset has to sound right without anything
    // driving it.
    auto bankOnly = false;
    /*  --audition writes ONE file with the whole bank in it, grouped by
        category, plus an index of timestamps. That is the one somebody
        listens to; --bank writes the hundred and fifty separate files, which
        is what you want when you are working on a single patch. */
    auto auditionOnly = false;
    juce::File outputDirectory = juce::File::getCurrentWorkingDirectory();

    for (int i = 1; i < argc; ++i)
    {
        const juce::String argument { argv[i] };

        if (argument == "--bank")
            bankOnly = true;
        else if (argument == "--audition")
            auditionOnly = true;
        else
            outputDirectory = juce::File (argument);
    }

    outputDirectory.createDirectory();

    if (auditionOnly)
    {
        renderBankAudition (outputDirectory);
        std::printf ("Done.\n");
        return 0;
    }

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
