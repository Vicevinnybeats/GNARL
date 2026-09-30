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
#include <map>
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

/*  THE BANK AS FILES, which is a different thing from the bank in the binary.

    The 150 presets are compiled in, so a fresh install already has them and
    this writes nothing the plugin needs. What it produces is a LIBRARY: a
    folder of .gnarl files somebody can back up, put in a shared drive, hand
    to a collaborator, load one of on a machine running an older build, or
    edit and keep beside their own patches. A bank that exists only inside an
    executable is a bank you cannot do any of that with.

    ONE FLAT FOLDER, not one folder per category. Category subfolders were the
    first version and they were wrong for what this is: nobody browses these
    files, they browse the plugin's own browser, which already filters by
    category. What the folder has to be good at is being COPIED - to a backup,
    a shared drive, another machine - and nine nested folders make that a
    thing you can do partially and not notice. The category is in INDEX.txt,
    which is where you look when you want to know it.

    THE FILENAME IS NUMBERED. `sanitiseFilename` strips what a filename cannot
    hold, and two presets could in principle sanitise to the same text -
    silently overwriting each other and producing a "library" of 149. The
    index prefix makes a collision impossible, and it also makes the bank's
    own order visible, which is the order a preset's stored INDEX refers to
    (CLAUDE.md section 3: generated presets are appended, never interleaved). */
/*  THE FACTORY BANK AS DATA THE BROWSER CAN LOAD.
 *
 *  In the plugin a factory preset is a ValueTree the engine applies. In a
 *  browser there is no engine, so loading one changed the name in the status
 *  bar and NOTHING ELSE - every factory preset sounded identical, because
 *  none of them was doing anything. Reported as "every preset sounds the
 *  same when I change it", which was exactly right.
 *
 *  A user's own patch already round-trips, because the browser captures the
 *  live parameters when it saves. The factory bank cannot work that way: it
 *  is built by C++ that does not run there. So it is dumped here, at build
 *  time, from the same FactoryBank::build the plugin uses - one source, no
 *  second list to drift (section 4's argument for the generated ID files).
 *
 *  NORMALISED values, which is what crosses the relay and what the UI's own
 *  snapshot format stores. Converting to real units here would duplicate
 *  every range and skew a third time.
 */
void writeUiPresets (const juce::File& outputDirectory)
{
    GnarlProcessor builder;
    builder.setPlayConfigDetails (0, 2, kSampleRate, kBlockSize);
    builder.prepareToPlay (kSampleRate, kBlockSize);

    auto& apvts = builder.getValueTreeState();
    const auto bank = preset::FactoryBank::build (apvts, apvts.copyState());

    /*  THE DEFAULTS FIRST, so each preset can be stored as the DIFFERENCE
        from them. Written in full, every preset is 430 numbers and the bank
        is 1.7 MB - shipped in a bundle that is otherwise about 2 MB, to say
        four hundred times over that a parameter is where it already was.

        Most presets touch a couple of dozen controls. Storing the delta cuts
        it by an order of magnitude, and the loader applies defaults before
        the delta, so loading B after A cannot leave A's values behind - which
        a sparse patch WOULD do without that reset. */
    std::map<juce::String, float> defaults;

    for (auto* parameter : builder.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            defaults[withId->paramID] = withId->getValue();

    juce::StringArray entries;

    for (const auto& tree : bank)
    {
        const auto metadata = preset::readMetadata (tree);

        /*  `getPresets().apply`, NOT `apvts.replaceState`. The first draft
            called replaceState directly - the same call applyPresetState
            makes - and every one of the 160 presets came out IDENTICAL:
            190 identical pairs among the first twenty, and zero parameters
            differing between Triplet Growl and Sixteenth Wobble. It was
            reading the default state a hundred and sixty times.

            The preset manager's apply is the path the renderer uses and the
            one that demonstrably produces different audio per preset, so it
            is the one to trust. Checked rather than assumed, which is the
            only reason this was caught before shipping a bank of clones. */
        builder.getPresets().apply (tree);

        juce::StringArray pairs;

        for (auto* parameter : builder.getParameters())
        {
            auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter);

            if (withId == nullptr)
                continue;

            const auto value = withId->getValue();
            const auto found = defaults.find (withId->paramID);

            //  1e-6: these are normalised 0..1 and written to six places, so
            //  anything smaller is the serialisation rather than the patch.
            if (found != defaults.end() && std::abs (found->second - value) < 1.0e-6f)
                continue;

            pairs.add ("\"" + withId->paramID + "\":" + juce::String (value, 6));
        }

        entries.add ("  {\"name\": " + metadata.name.quoted()
                     + ", \"category\": " + metadata.category.quoted()
                     + ", \"values\": {" + pairs.joinIntoString (",") + "}}");
    }

    const auto file = outputDirectory.getChildFile ("factoryPresets.json");
    file.replaceWithText ("[\n" + entries.joinIntoString (",\n") + "\n]\n");

    std::printf ("wrote %d presets -> %s\n",
                 static_cast<int> (entries.size()),
                 file.getFullPathName().toRawUTF8());
}

void writePresetLibrary (const juce::File& outputDirectory)
{
    GnarlProcessor builder;
    builder.setPlayConfigDetails (0, 2, kSampleRate, kBlockSize);
    builder.prepareToPlay (kSampleRate, kBlockSize);

    const auto bank = preset::FactoryBank::build (
        builder.getValueTreeState(), builder.getValueTreeState().copyState());

    const auto root = outputDirectory.getChildFile ("Presets");
    root.createDirectory();

    juce::StringArray index;
    index.add ("# GNARL factory preset library");
    index.add ("# " + juce::String (static_cast<int> (bank.size()))
               + " presets, .gnarl format, one flat folder");
    index.add ("#");
    index.add ("# These are ALREADY IN THE PLUGIN. This folder is a copy to");
    index.add ("# back up or share, not something to install.");
    index.add ("");
    index.add ("file                                  category   name");

    int written = 0;

    for (std::size_t i = 0; i < bank.size(); ++i)
    {
        const auto metadata = preset::readMetadata (bank[i]);

        const auto name = juce::String (static_cast<int> (i) + 1).paddedLeft ('0', 3)
                        + " " + preset::sanitiseFilename (metadata.name);

        const auto file = root.getChildFile (name + "." + preset::kFileExtension);

        /*  replaceWithText, not appendText: this tool is re-run over an
            existing directory whenever the bank changes, and appending would
            produce a file holding two presets, which parses as neither. */
        if (file.replaceWithText (preset::toText (bank[i])))
        {
            ++written;
            index.add ((name + ".gnarl").paddedRight (' ', 38)
                       + metadata.category.paddedRight (' ', 11) + metadata.name);
        }
        else
        {
            std::printf ("  could not write %s\n", file.getFullPathName().toRawUTF8());
        }
    }

    root.getChildFile ("INDEX.txt").replaceWithText (index.joinIntoString ("\n") + "\n");

    /*  A READ-BACK, not a claim. Writing a file that does not parse is the
        one failure this tool can produce that looks exactly like success, so
        every file is loaded again and checked to be a preset. */
    int verified = 0;

    for (const auto& file : root.findChildFiles (juce::File::findFiles, true,
                                                 preset::kFileWildcard))
        if (preset::isPreset (preset::fromText (file.loadFileAsString())))
            ++verified;

    std::printf ("Wrote %d presets to %s\n", written,
                 root.getFullPathName().toRawUTF8());
    std::printf ("Verified %d of %d parse back as presets\n", verified, written);

    if (verified != written)
        std::printf ("  WARNING: %d file(s) did not parse back\n", written - verified);
}

/*  MEASURE THE BANK THE SAME WAY THE REFERENCES WERE MEASURED.

    The client's tracks were analysed for band balance, crest factor and the
    modulation rate of the 220-1200 Hz band. Those numbers are the target, and
    until now there was no way to ask what the BANK measures - so "this patch
    does not sound like the reference" could only ever be answered by ear, one
    patch at a time, by somebody who had both in front of them.

    This renders each preset at the note its category implies and prints the
    same columns. It does not say whether a patch is good; taste is not
    measurable. It says whether a patch is in the same PLACE as the material,
    which is a different question and one that has an answer.

    The band energies are SUMMED over their bins, never averaged - a mean
    divides out the band's width, and narrow low bands then come back as
    thousands of per cent. That error was made once already on the reference
    side and is easy to repeat here.

    The wobble rate needs a hop that can SEE it: a 1/8 triplet at 150 BPM is
    7.5 Hz, so a hop of 8192 samples (Nyquist 1.35 Hz) reports the band edge
    instead of the music, which is exactly what happened the first time. 256
    gives a Nyquist near 94 Hz.  */
void measureBank (const juce::File& outputDirectory, int stride)
{
    GnarlProcessor builder;
    builder.setPlayConfigDetails (0, 2, kSampleRate, kBlockSize);
    builder.prepareToPlay (kSampleRate, kBlockSize);

    const auto bank = preset::FactoryBank::build (
        builder.getValueTreeState(), builder.getValueTreeState().copyState());

    const auto definitions = preset::FactoryBank::getDefinitions();
    const auto count = std::min (bank.size(), definitions.size());

    constexpr int kFftOrder = 15;                 // 32768, ~1.5 Hz per bin
    constexpr int kFftSize = 1 << kFftOrder;

    juce::dsp::FFT fft { kFftOrder };
    std::vector<float> window (kFftSize);

    for (int i = 0; i < kFftSize; ++i)
        window[static_cast<std::size_t> (i)] =
            0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / (kFftSize - 1));

    struct Band { const char* name; float lo; float hi; };
    const Band bands[] = { { "sub", 20.0f, 80.0f }, { "low", 80.0f, 220.0f },
                           { "growl", 220.0f, 1200.0f }, { "upper", 1200.0f, 5000.0f },
                           { "air", 5000.0f, 16000.0f } };

    std::printf ("%-26s %6s %6s %6s %6s %6s %6s %8s\n",
                 "preset", "sub", "low", "growl", "upper", "air", "crest", "wobble");

    std::vector<double> subs, growls, crests, rates;

    for (std::size_t i = 0; i < count; i += static_cast<std::size_t> (stride))
    {
        const auto tree = bank[i];
        const auto meta = preset::readMetadata (tree);
        const auto note = preset::FactoryBank::getAuditionNote (meta.category);

        auto rendered = renderToBuffer (4.0,
            [&tree] (GnarlProcessor& p) { p.getPresets().apply (tree); },
            [note] (juce::MidiBuffer& midi, int blockIndex) { holdNote (midi, blockIndex, note); },
            [] (GnarlProcessor&, double) {});

        const auto n = rendered.getNumSamples();

        if (n < kFftSize)
            continue;

        //  Mono sum, skipping the attack so this measures the sustained patch.
        const auto skip = static_cast<int> (0.5 * kSampleRate);
        std::vector<float> mono (static_cast<std::size_t> (n - skip));

        for (int j = skip; j < n; ++j)
            mono[static_cast<std::size_t> (j - skip)] =
                0.5f * (rendered.getSample (0, j) + rendered.getSample (1, j));

        //  ---- band balance, averaged over overlapping frames -------------
        std::vector<float> power (static_cast<std::size_t> (kFftSize / 2 + 1), 0.0f);
        int frames = 0;

        for (std::size_t off = 0; off + kFftSize < mono.size(); off += kFftSize / 2)
        {
            std::vector<float> fftData (static_cast<std::size_t> (2 * kFftSize), 0.0f);

            for (int j = 0; j < kFftSize; ++j)
                fftData[static_cast<std::size_t> (j)] =
                    mono[off + static_cast<std::size_t> (j)] * window[static_cast<std::size_t> (j)];

            fft.performFrequencyOnlyForwardTransform (fftData.data());

            for (std::size_t b = 0; b < power.size(); ++b)
                power[b] += fftData[b] * fftData[b];

            ++frames;
        }

        if (frames == 0)
            continue;

        double total = 0.0;

        for (auto& v : power)
        {
            v /= static_cast<float> (frames);
            total += v;
        }

        if (total <= 0.0)
            continue;

        double pct[5] {};

        for (int b = 0; b < 5; ++b)
        {
            const auto loBin = static_cast<std::size_t> (bands[b].lo * kFftSize / kSampleRate);
            const auto hiBin = std::min (static_cast<std::size_t> (bands[b].hi * kFftSize / kSampleRate),
                                         power.size() - 1);
            double sum = 0.0;

            for (auto k = loBin; k < hiBin; ++k)   // SUMMED, not averaged
                sum += power[k];

            pct[b] = 100.0 * sum / total;
        }

        //  ---- crest ------------------------------------------------------
        double sumSq = 0.0;
        float peak = 0.0f;

        for (auto v : mono)
        {
            sumSq += static_cast<double> (v) * v;
            peak = std::max (peak, std::abs (v));
        }

        const auto rms = std::sqrt (sumSq / static_cast<double> (mono.size()));
        const auto crest = rms > 0.0 ? 20.0 * std::log10 (peak / rms) : 0.0;

        //  ---- wobble rate of the growl band ------------------------------
        //  A one-pole band-pass is enough here: the question is how fast the
        //  envelope moves, not its exact shape.
        double bp = 0.0, lp = 0.0;
        constexpr int kHop = 256;
        std::vector<double> env;
        double acc = 0.0;
        int accN = 0;

        for (auto v : mono)
        {
            lp += 0.035 * (v - lp);           // ~270 Hz
            bp += 0.28 * (lp - bp);           // envelope of the low-mid
            acc += std::abs (lp - bp);
            if (++accN == kHop) { env.push_back (acc / kHop); acc = 0.0; accN = 0; }
        }

        double rate = 0.0;

        if (env.size() > 64)
        {
            double mean = 0.0;
            for (auto v : env) mean += v;
            mean /= static_cast<double> (env.size());
            for (auto& v : env) v -= mean;

            //  Goertzel over the musical wobble range, which is cheaper and
            //  clearer than an FFT of 200 points.
            const auto envRate = kSampleRate / kHop;
            double best = 0.0;

            for (double f = 1.5; f <= 16.0; f += 0.05)
            {
                const auto w = juce::MathConstants<double>::twoPi * f / envRate;
                double s1 = 0.0, s2 = 0.0;
                const auto c = 2.0 * std::cos (w);

                for (auto v : env) { const auto s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }

                const auto mag = s1 * s1 + s2 * s2 - c * s1 * s2;

                if (mag > best) { best = mag; rate = f; }
            }
        }

        std::printf ("%-26s %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f %7.2fHz\n",
                     meta.name.substring (0, 25).toRawUTF8(),
                     pct[0], pct[1], pct[2], pct[3], pct[4], crest, rate);

        subs.push_back (pct[0]);
        growls.push_back (pct[2]);
        crests.push_back (crest);
        rates.push_back (rate);
    }

    const auto report = [] (const char* label, std::vector<double> v, const char* target)
    {
        if (v.empty()) return;
        std::sort (v.begin(), v.end());
        std::printf ("%-8s min %6.2f   max %6.2f   median %6.2f      reference: %s\n",
                     label, v.front(), v.back(), v[v.size() / 2], target);
    };

    std::printf ("\n");
    report ("sub",   subs,   "58-85%, median 70");
    report ("growl", growls, "3.7-35%, median 9.9");
    report ("crest", crests, "8.9-14.5 dB, median 11");
    report ("rate",  rates,  "2.35-7.10 Hz, clustered at 2.35 and 4.65");

    juce::ignoreUnused (outputDirectory);
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
    /*  --presets writes the bank as .gnarl FILES rather than as audio: a
        library to back up, share or load on another machine. */
    auto presetsOnly = false;
    /*  --measure prints the band balance, crest and wobble rate of the bank
        in the same columns the reference tracks were measured in. */
    /*  --ui-presets writes the bank as JSON for the browser build, so a
        factory preset there loads its SOUND rather than only its name. */
    auto uiPresetsOnly = false;
    auto measureOnly = false;
    auto measureStride = 7;
    juce::File outputDirectory = juce::File::getCurrentWorkingDirectory();

    for (int i = 1; i < argc; ++i)
    {
        const juce::String argument { argv[i] };

        if (argument == "--bank")
            bankOnly = true;
        else if (argument == "--audition")
            auditionOnly = true;
        else if (argument == "--presets")
            presetsOnly = true;
        else if (argument == "--ui-presets")
            uiPresetsOnly = true;
        else if (argument == "--measure")
            measureOnly = true;
        else if (argument.startsWith ("--stride="))
            measureStride = std::max (1, argument.fromFirstOccurrenceOf ("=", false, false).getIntValue());
        else
            outputDirectory = juce::File (argument);
    }

    outputDirectory.createDirectory();

    if (measureOnly)
    {
        measureBank (outputDirectory, measureStride);
        return 0;
    }

    if (uiPresetsOnly)
    {
        writeUiPresets (outputDirectory);
        std::printf ("Done.\n");
        return 0;
    }

    if (presetsOnly)
    {
        writePresetLibrary (outputDirectory);
        std::printf ("Done.\n");
        return 0;
    }

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
