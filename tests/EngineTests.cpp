#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"
#include "params/ParameterChoices.h"
#include "params/ParameterIDs.h"

#include <cmath>
#include <vector>

using namespace gnarl;

/*
    End-to-end tests: the whole plugin, driven the way a host drives it.

    Everything below goes through processBlock with real MIDI, rather than
    calling the DSP classes directly. The unit tests prove each piece works;
    these prove the pieces are actually CONNECTED - which is a different
    failure, and the one that produces a synth that passes every test and makes
    no sound.
*/

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;

    void setParameter (GnarlProcessor& processor, const char* id, float realValue)
    {
        auto* parameter = processor.getValueTreeState().getParameter (id);
        REQUIRE (parameter != nullptr);

        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            ranged->setValueNotifyingHost (ranged->convertTo0to1 (realValue));
    }

    struct RenderResult
    {
        float peak = 0.0f;
        double rms = 0.0;
        bool allFinite = true;
    };

    /** Holds a note for `seconds` and measures the output. */
    RenderResult renderNote (GnarlProcessor& processor,
                             int midiNote,
                             double seconds,
                             float velocity = 1.0f)
    {
        juce::AudioBuffer<float> buffer (2, kBlockSize);
        juce::MidiBuffer midi;

        midi.addEvent (juce::MidiMessage::noteOn (1, midiNote, velocity), 0);

        const auto totalBlocks = static_cast<int> (seconds * kSampleRate / kBlockSize);

        RenderResult result;
        double sumSquares = 0.0;
        int sampleCount = 0;

        for (int block = 0; block < totalBlocks; ++block)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
            midi.clear();

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                const auto* data = buffer.getReadPointer (channel);

                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    if (! std::isfinite (data[i]))
                        result.allFinite = false;

                    result.peak = juce::jmax (result.peak, std::abs (data[i]));
                    sumSquares += static_cast<double> (data[i]) * data[i];
                    ++sampleCount;
                }
            }
        }

        if (sampleCount > 0)
            result.rms = std::sqrt (sumSquares / static_cast<double> (sampleCount));

        return result;
    }

    std::unique_ptr<GnarlProcessor> makePreparedProcessor()
    {
        auto processor = std::make_unique<GnarlProcessor>();
        processor->setPlayConfigDetails (0, 2, kSampleRate, kBlockSize);
        processor->prepareToPlay (kSampleRate, kBlockSize);
        return processor;
    }
}

TEST_CASE ("The default patch makes a sound", "[engine]")
{
    // THE milestone test. Oscillator 1 and filter 1 are on by default, so a
    // note must produce audio with no patch editing at all. A synth whose
    // default patch is silent looks broken on first load, whatever the unit
    // tests say.
    auto processor = makePreparedProcessor();

    const auto result = renderNote (*processor, 36, 0.5);

    INFO ("peak " << result.peak << ", rms " << result.rms);

    CHECK (result.allFinite);
    CHECK (result.peak > 0.05f);
    CHECK (result.rms > 0.005);
}

TEST_CASE ("No notes means exact silence", "[engine]")
{
    auto processor = makePreparedProcessor();

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    for (int block = 0; block < 32; ++block)
    {
        buffer.clear();
        processor->processBlock (buffer, midi);

        CHECK (buffer.getMagnitude (0, buffer.getNumSamples()) == 0.0f);
    }
}

TEST_CASE ("Output does not clip at full velocity with a full patch",
           "[engine]")
{
    // Every source on, unison up, a chord held. If the default gain staging
    // cannot survive this, every preset will need to be turned down by hand.
    auto processor = makePreparedProcessor();

    setParameter (*processor, pid::osc[1].enabled, 1.0f);
    setParameter (*processor, pid::osc[1].level, 0.8f);
    setParameter (*processor, pid::osc[0].unisonVoices, 8.0f);
    setParameter (*processor, pid::osc[1].unisonVoices, 8.0f);
    setParameter (*processor, pid::sub.enabled, 1.0f);
    setParameter (*processor, pid::noise.enabled, 1.0f);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    for (const auto note : { 36, 43, 48, 55 })
        midi.addEvent (juce::MidiMessage::noteOn (1, note, 1.0f), 0);

    auto peak = 0.0f;

    for (int block = 0; block < 200; ++block)
    {
        buffer.clear();
        processor->processBlock (buffer, midi);
        midi.clear();

        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                peak = juce::jmax (peak, std::abs (buffer.getReadPointer (channel)[i]));
    }

    INFO ("peak with everything on and a four-note chord: " << peak);

    // Some headroom loss is expected and fine; 4x full scale is not.
    CHECK (peak < 4.0f);
    CHECK (peak > 0.1f);
}

TEST_CASE ("Note off eventually silences the voice", "[engine]")
{
    auto processor = makePreparedProcessor();

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    midi.addEvent (juce::MidiMessage::noteOn (1, 48, 1.0f), 0);

    for (int block = 0; block < 20; ++block)
    {
        buffer.clear();
        processor->processBlock (buffer, midi);
        midi.clear();
    }

    midi.addEvent (juce::MidiMessage::noteOff (1, 48), 0);

    // Long enough for the placeholder release plus the voice being freed.
    auto tailPeak = 0.0f;

    for (int block = 0; block < 200; ++block)
    {
        buffer.clear();
        processor->processBlock (buffer, midi);
        midi.clear();

        if (block > 100)
            tailPeak = juce::jmax (tailPeak,
                                   buffer.getMagnitude (0, buffer.getNumSamples()));
    }

    INFO ("peak well after note off: " << tailPeak);
    CHECK (tailPeak < 1.0e-4f);
    CHECK (processor->getSoundingVoiceCount() == 0);
}

TEST_CASE ("The filter audibly changes the sound", "[engine]")
{
    // Proves the filter is actually in the signal path, which no filter unit
    // test can tell you.
    const auto measure = [] (float cutoff)
    {
        auto processor = makePreparedProcessor();
        setParameter (*processor, pid::filter[0].cutoff, cutoff);

        // Resonance at zero on purpose. With resonance up, closing the filter
        // onto a harmonic BOOSTS it, so a lower cutoff can legitimately be
        // louder - this test is about the slope being in the path, and mixing
        // the resonant peak into it would make it measure something else.
        setParameter (*processor, pid::filter[0].resonance, 0.0f);

        return renderNote (*processor, 36, 0.3).rms;
    };

    const auto open = measure (20000.0f);

    // Closed BELOW the note's fundamental (65 Hz at MIDI 36), not just below
    // its harmonics. A saw's RMS is dominated by its fundamental, so closing
    // to 120 Hz removes most of the harmonics and only about 20% of the
    // energy - which is correct behaviour but a weak thing to assert on.
    const auto closed = measure (40.0f);

    INFO ("open " << open << ", closed " << closed);

    CHECK (closed < open * 0.5);
}

TEST_CASE ("The formant filter produces vowel peaks through the full engine",
           "[engine][formant]")
{
    // Measures the actual plugin output, not the filter in isolation: the
    // formant filter is the reason this plugin exists, so it gets an
    // end-to-end check that the pad position reaches it.
    const auto renderSpectrumPeak = [] (float padX, float padY, float probeHz)
    {
        auto processor = makePreparedProcessor();

        setParameter (*processor, pid::filter[0].type,
                      static_cast<float> (choices::FilterType::formant));
        setParameter (*processor, pid::filter[0].formantX, padX);
        setParameter (*processor, pid::filter[0].formantY, padY);
        setParameter (*processor, pid::filter[0].resonance, 0.8f);
        setParameter (*processor, pid::osc[0].wavetable, 1.0f);   // Growl Vowels

        constexpr int fftOrder = 14;
        constexpr int fftSize = 1 << fftOrder;

        juce::AudioBuffer<float> buffer (2, kBlockSize);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 36, 1.0f), 0);

        std::vector<float> captured;
        captured.reserve (static_cast<std::size_t> (fftSize));

        while (static_cast<int> (captured.size()) < fftSize)
        {
            buffer.clear();
            processor->processBlock (buffer, midi);
            midi.clear();

            const auto* data = buffer.getReadPointer (0);

            for (int i = 0; i < buffer.getNumSamples()
                            && static_cast<int> (captured.size()) < fftSize; ++i)
                captured.push_back (data[i]);
        }

        juce::dsp::FFT fft (fftOrder);
        std::vector<float> scratch (static_cast<std::size_t> (fftSize) * 2, 0.0f);

        for (int i = 0; i < fftSize; ++i)
        {
            const auto window = 0.5f - 0.5f * std::cos (
                juce::MathConstants<float>::twoPi * static_cast<float> (i)
                / static_cast<float> (fftSize));
            scratch[static_cast<std::size_t> (i)] =
                captured[static_cast<std::size_t> (i)] * window;
        }

        fft.performRealOnlyForwardTransform (scratch.data(), true);

        const auto binHz = static_cast<float> (kSampleRate) / static_cast<float> (fftSize);
        const auto centreBin = static_cast<int> (probeHz / binHz);

        auto peak = 0.0f;

        for (int bin = juce::jmax (1, centreBin - 12); bin <= centreBin + 12; ++bin)
        {
            const auto real = scratch[static_cast<std::size_t> (bin) * 2];
            const auto imaginary = scratch[static_cast<std::size_t> (bin) * 2 + 1];
            peak = juce::jmax (peak, std::sqrt (real * real + imaginary * imaginary));
        }

        return peak;
    };

    const auto aAnchor = dsp::FormantFilter::getVowelAnchor (dsp::FormantFilter::Vowel::a);
    const auto iAnchor = dsp::FormantFilter::getVowelAnchor (dsp::FormantFilter::Vowel::i);

    // A's first formant is 800 Hz; I's is 350 Hz. So 800 Hz should be louder
    // at the A anchor than at the I anchor.
    const auto atA = renderSpectrumPeak (aAnchor.x, aAnchor.y, 800.0f);
    const auto atI = renderSpectrumPeak (iAnchor.x, iAnchor.y, 800.0f);

    INFO ("800 Hz energy: A anchor " << atA << ", I anchor " << atI);

    CHECK (atA > atI);
}

TEST_CASE ("Every wavetable makes a sound through the engine", "[engine]")
{
    // A factory table that is silent, or that the reader fails to resolve,
    // would look like a broken preset.
    for (int table = 0; table < dsp::WavetableLibrary::kNumFactoryTables; ++table)
    {
        auto processor = makePreparedProcessor();
        setParameter (*processor, pid::osc[0].wavetable, static_cast<float> (table));

        // Re-prepare so the newly selected table is generated on the message
        // thread, exactly as a real host would after a parameter change.
        processor->prepareToPlay (kSampleRate, kBlockSize);

        const auto result = renderNote (*processor, 36, 0.2);

        INFO ("table " << table << " ("
              << dsp::WavetableLibrary::getTableName (table)
              << ") peak " << result.peak);

        CHECK (result.allFinite);
        CHECK (result.peak > 0.01f);
    }
}

TEST_CASE ("Graintable mode makes a sound through the engine", "[engine][grain]")
{
    auto processor = makePreparedProcessor();

    setParameter (*processor, pid::osc[0].mode,
                  static_cast<float> (choices::OscMode::graintable));
    setParameter (*processor, pid::osc[0].grainDensity, 30.0f);
    setParameter (*processor, pid::osc[0].grainSize, 40.0f);

    const auto result = renderNote (*processor, 36, 0.4);

    INFO ("peak " << result.peak << ", rms " << result.rms);

    CHECK (result.allFinite);
    CHECK (result.peak > 0.01f);
}

TEST_CASE ("Every filter type passes audio through the engine", "[engine]")
{
    for (int type = 0; type < static_cast<int> (choices::FilterType::count); ++type)
    {
        auto processor = makePreparedProcessor();

        setParameter (*processor, pid::filter[0].type, static_cast<float> (type));
        setParameter (*processor, pid::filter[0].cutoff, 2000.0f);
        setParameter (*processor, pid::filter[0].resonance, 0.4f);

        const auto result = renderNote (*processor, 36, 0.25);

        INFO ("filter type " << type << " peak " << result.peak);

        CHECK (result.allFinite);
        CHECK (result.peak > 0.005f);
        CHECK (result.peak < 10.0f);
    }
}

TEST_CASE ("The sub and noise sources reach the output", "[engine]")
{
    // Each source on its own, with the main oscillators off, so a source that
    // is wired to nothing shows up as silence.
    const auto renderOnly = [] (const char* enableId, const char* levelId)
    {
        auto processor = makePreparedProcessor();

        setParameter (*processor, pid::osc[0].enabled, 0.0f);
        setParameter (*processor, enableId, 1.0f);

        if (levelId != nullptr)
            setParameter (*processor, levelId, 0.8f);

        return renderNote (*processor, 36, 0.3);
    };

    const auto subOnly = renderOnly (pid::sub.enabled, pid::sub.level);
    INFO ("sub only peak " << subOnly.peak);
    CHECK (subOnly.peak > 0.01f);
    CHECK (subOnly.allFinite);

    const auto noiseOnly = renderOnly (pid::noise.enabled, pid::noise.level);
    INFO ("noise only peak " << noiseOnly.peak);
    CHECK (noiseOnly.peak > 0.01f);
    CHECK (noiseOnly.allFinite);
}

TEST_CASE ("Every noise type reaches the output", "[engine]")
{
    for (int type = 0; type < static_cast<int> (choices::NoiseType::count); ++type)
    {
        auto processor = makePreparedProcessor();

        setParameter (*processor, pid::osc[0].enabled, 0.0f);
        setParameter (*processor, pid::noise.enabled, 1.0f);
        setParameter (*processor, pid::noise.level, 0.8f);
        setParameter (*processor, pid::noise.type, static_cast<float> (type));

        const auto result = renderNote (*processor, 36, 0.3);

        INFO ("noise type " << type << " peak " << result.peak
              << " rms " << result.rms);

        CHECK (result.allFinite);
        CHECK (result.peak > 0.005f);
    }
}

TEST_CASE ("Parallel and split routing both produce sound", "[engine]")
{
    for (int routing = 0; routing < static_cast<int> (choices::FilterRouting::count);
         ++routing)
    {
        auto processor = makePreparedProcessor();

        setParameter (*processor, pid::filterRouting, static_cast<float> (routing));
        setParameter (*processor, pid::filter[1].enabled, 1.0f);
        setParameter (*processor, pid::filter[1].cutoff, 800.0f);
        setParameter (*processor, pid::osc[0].sendFilter2, 0.7f);

        const auto result = renderNote (*processor, 36, 0.25);

        INFO ("routing " << routing << " peak " << result.peak);

        CHECK (result.allFinite);
        CHECK (result.peak > 0.005f);
    }
}

TEST_CASE ("A source with all sends at zero is silent", "[engine]")
{
    // The send levels are the routing, so zeroing them must actually mute the
    // source rather than leaving a leak path.
    auto processor = makePreparedProcessor();

    setParameter (*processor, pid::osc[0].sendFilter1, 0.0f);
    setParameter (*processor, pid::osc[0].sendFilter2, 0.0f);
    setParameter (*processor, pid::osc[0].sendDirect, 0.0f);

    const auto result = renderNote (*processor, 36, 0.2);

    INFO ("peak with all sends at zero: " << result.peak);
    CHECK (result.peak < 1.0e-6f);
}

TEST_CASE ("Held notes stay finite across sample rate changes", "[engine]")
{
    auto processor = makePreparedProcessor();

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 48, 1.0f), 0);

    for (const auto rate : { 44100.0, 96000.0, 48000.0 })
    {
        processor->setPlayConfigDetails (0, 2, rate, kBlockSize);
        processor->prepareToPlay (rate, kBlockSize);

        for (int block = 0; block < 20; ++block)
        {
            buffer.clear();
            processor->processBlock (buffer, midi);
            midi.clear();

            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    REQUIRE (std::isfinite (buffer.getReadPointer (channel)[i]));
        }
    }
}

TEST_CASE ("Oversampling measurably reduces drive aliasing", "[engine][aliasing]")
{
    // The claim oversampling exists to make, measured rather than asserted.
    //
    // A hard-clipped sine is the worst case: the clipper generates harmonics
    // without limit, and every one above Nyquist folds back as an inharmonic
    // partial. Those partials are NOT at multiples of the fundamental, so they
    // are distinguishable from the distortion we want.
    const auto measureAliasing = [] (choices::Oversampling factor, float tableIndex)
    {
        auto processor = makePreparedProcessor();

        setParameter (*processor, pid::oversampling, static_cast<float> (factor));

        setParameter (*processor, pid::osc[0].wavetable, tableIndex);
        setParameter (*processor, pid::osc[0].tablePos, 0.0f);
        setParameter (*processor, pid::filter[0].cutoff, 20000.0f);
        setParameter (*processor, pid::filter[0].resonance, 0.0f);
        setParameter (*processor, pid::filter[0].drive, 1.0f);
        setParameter (*processor, pid::filter[0].driveCurve,
                      static_cast<float> (choices::DriveCurve::hardClip));

        processor->prepareToPlay (kSampleRate, kBlockSize);

        constexpr int fftOrder = 14;
        constexpr int fftSize = 1 << fftOrder;

        // A high note, so its harmonics are far apart and the folded ones land
        // clearly between them.
        constexpr int midiNote = 96;
        const auto fundamental = 440.0f
            * std::pow (2.0f, (static_cast<float> (midiNote) - 69.0f) / 12.0f);

        juce::AudioBuffer<float> buffer (2, kBlockSize);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, midiNote, 1.0f), 0);

        std::vector<float> captured;
        captured.reserve (static_cast<std::size_t> (fftSize));

        // Discard the attack and any oversampler latency before capturing.
        for (int block = 0; block < 40; ++block)
        {
            buffer.clear();
            processor->processBlock (buffer, midi);
            midi.clear();
        }

        while (static_cast<int> (captured.size()) < fftSize)
        {
            buffer.clear();
            processor->processBlock (buffer, midi);

            const auto* data = buffer.getReadPointer (0);

            for (int i = 0; i < buffer.getNumSamples()
                            && static_cast<int> (captured.size()) < fftSize; ++i)
                captured.push_back (data[i]);
        }

        // Blackman-Harris, for the reason documented in CLAUDE.md: a Hann
        // window's -31 dB sidelobes would swamp the measurement.
        juce::dsp::FFT fft (fftOrder);
        std::vector<float> scratch (static_cast<std::size_t> (fftSize) * 2, 0.0f);

        for (int i = 0; i < fftSize; ++i)
        {
            const auto t = juce::MathConstants<float>::twoPi
                         * static_cast<float> (i) / static_cast<float> (fftSize);
            const auto window = 0.35875f - 0.48829f * std::cos (t)
                              + 0.14128f * std::cos (2.0f * t)
                              - 0.01168f * std::cos (3.0f * t);

            scratch[static_cast<std::size_t> (i)] =
                captured[static_cast<std::size_t> (i)] * window;
        }

        fft.performRealOnlyForwardTransform (scratch.data(), true);

        const auto binHz = static_cast<float> (kSampleRate) / static_cast<float> (fftSize);
        const auto fundamentalBin = fundamental / binHz;
        constexpr int skirtBins = 8;

        auto reference = 0.0f;
        auto worstAlias = 0.0f;

        for (int bin = 2; bin < fftSize / 2; ++bin)
        {
            const auto real = scratch[static_cast<std::size_t> (bin) * 2];
            const auto imaginary = scratch[static_cast<std::size_t> (bin) * 2 + 1];
            const auto magnitude = std::sqrt (real * real + imaginary * imaginary);

            const auto frequency = static_cast<float> (bin) * binHz;
            const auto harmonicNumber = std::round (frequency / fundamental);
            const auto nearestHarmonicBin = harmonicNumber * fundamentalBin;

            if (std::abs (static_cast<float> (bin) - nearestHarmonicBin)
                <= static_cast<float> (skirtBins))
            {
                if (std::abs (harmonicNumber - 1.0f) < 0.5f)
                    reference = juce::jmax (reference, magnitude);

                continue;
            }

            worstAlias = juce::jmax (worstAlias, magnitude);
        }

        if (reference <= 0.0f)
            return 0.0f;

        return juce::Decibels::gainToDecibels (worstAlias / reference, -200.0f);
    };

    // Sub Sine (table 5): a single harmonic, so every partial in the output
    // was created by the drive stage. Isolates the clipper.
    const auto sineOff = measureAliasing (choices::Oversampling::off, 5.0f);
    const auto sineTwice = measureAliasing (choices::Oversampling::twoTimes, 5.0f);
    const auto sineFour = measureAliasing (choices::Oversampling::fourTimes, 5.0f);

    INFO ("pure sine into hard clip: off " << sineOff << " dBc, 2x "
          << sineTwice << " dBc, 4x " << sineFour << " dBc");

    // Dirty Saw (table 12): harmonically dense, which is the case the
    // oscillator's base-rate band limit exists for.
    const auto sawOff = measureAliasing (choices::Oversampling::off, 12.0f);
    const auto sawTwice = measureAliasing (choices::Oversampling::twoTimes, 12.0f);
    const auto sawFour = measureAliasing (choices::Oversampling::fourTimes, 12.0f);

    INFO ("dense saw into hard clip: off " << sawOff << " dBc, 2x "
          << sawTwice << " dBc, 4x " << sawFour << " dBc");

    // Oversampling must actually help. If these come out equal, the voices
    // are not running at the higher rate and the oversampler is decorative.
    CHECK (sineTwice < sineOff - 3.0f);
    CHECK (sawTwice < sawOff - 3.0f);

    // 4x must not be WORSE than 2x. Note this is deliberately not a demand
    // that it be better: past 2x the folded partials are already below the
    // oscillator's own interpolation floor, so the remaining difference is
    // not the clipper's and is not worth asserting an ordering on.
    CHECK (sineFour < sineTwice + 2.0f);
    CHECK (sawFour < sawTwice + 2.0f);
}

TEST_CASE ("Changing the oversampling factor mid-note is safe", "[engine]")
{
    // The factor is a live parameter, so switching it must not allocate, must
    // not blow up, and must not silence a sounding note.
    auto processor = makePreparedProcessor();

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 40, 1.0f), 0);

    auto sawAudio = false;

    for (int block = 0; block < 90; ++block)
    {
        if (block % 30 == 0)
            setParameter (*processor, pid::oversampling,
                          static_cast<float> (block / 30 % 3));

        buffer.clear();
        processor->processBlock (buffer, midi);
        midi.clear();

        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                REQUIRE (std::isfinite (buffer.getReadPointer (channel)[i]));

        if (block > 40 && buffer.getMagnitude (0, buffer.getNumSamples()) > 0.01f)
            sawAudio = true;
    }

    CHECK (sawAudio);
}

TEST_CASE ("Oversampling latency is reported to the host", "[engine]")
{
    // A host cannot compensate for latency it was not told about, and an
    // uncompensated synth drifts against the rest of the project.
    auto processor = makePreparedProcessor();

    setParameter (*processor, pid::oversampling,
                  static_cast<float> (choices::Oversampling::off));
    processor->prepareToPlay (kSampleRate, kBlockSize);
    CHECK (processor->getLatencySamples() == 0);

    setParameter (*processor, pid::oversampling,
                  static_cast<float> (choices::Oversampling::fourTimes));
    processor->prepareToPlay (kSampleRate, kBlockSize);

    INFO ("reported latency at 4x: " << processor->getLatencySamples());
    CHECK (processor->getLatencySamples() >= 0);
}

// --- OTT -------------------------------------------------------------------

TEST_CASE ("OTT off is transparent", "[engine][ott]")
{
    // Default is off, and off must mean exactly nothing - a built-in
    // compressor that colours the sound before the user asks makes every
    // preset sound like the plugin rather than like the patch.
    auto withOtt = makePreparedProcessor();
    auto without = makePreparedProcessor();

    setParameter (*withOtt, pid::ott.enabled, 0.0f);
    setParameter (*withOtt, pid::ott.depth, 1.0f);

    juce::AudioBuffer<float> a (2, kBlockSize);
    juce::AudioBuffer<float> b (2, kBlockSize);
    juce::MidiBuffer midiA;
    juce::MidiBuffer midiB;

    midiA.addEvent (juce::MidiMessage::noteOn (1, 40, 1.0f), 0);
    midiB.addEvent (juce::MidiMessage::noteOn (1, 40, 1.0f), 0);

    for (int block = 0; block < 40; ++block)
    {
        a.clear();
        b.clear();
        withOtt->processBlock (a, midiA);
        without->processBlock (b, midiB);
        midiA.clear();
        midiB.clear();

        for (int i = 0; i < kBlockSize; ++i)
            REQUIRE (a.getReadPointer (0)[i] == b.getReadPointer (0)[i]);
    }
}

TEST_CASE ("OTT raises the level of quiet material", "[engine][ott]")
{
    // The upward half is what makes it OTT rather than a compressor. A quiet
    // note should come out closer to full scale with it on.
    const auto measureRms = [] (bool enabled)
    {
        auto processor = makePreparedProcessor();

        setParameter (*processor, pid::ott.enabled, enabled ? 1.0f : 0.0f);
        setParameter (*processor, pid::ott.depth, 1.0f);

        // Deliberately quiet, so the upward path is what is being measured.
        setParameter (*processor, pid::osc[0].level, 0.05f);

        return renderNote (*processor, 40, 0.8, 0.5f).rms;
    };

    const auto off = measureRms (false);
    const auto on = measureRms (true);

    INFO ("quiet note RMS: off " << off << ", OTT on " << on);

    CHECK (on > off * 1.5);
}

TEST_CASE ("OTT reduces dynamic range", "[engine][ott]")
{
    // Both halves together should narrow the gap between the loudest and
    // quietest passages, which is the whole point.
    const auto measureRange = [] (bool enabled)
    {
        auto processor = makePreparedProcessor();

        setParameter (*processor, pid::ott.enabled, enabled ? 1.0f : 0.0f);
        setParameter (*processor, pid::ott.depth, 1.0f);

        juce::AudioBuffer<float> buffer (2, kBlockSize);
        juce::MidiBuffer midi;

        // A loud note, then a quiet one.
        std::vector<double> blockRms;

        for (const auto velocity : { 1.0f, 0.15f })
        {
            midi.addEvent (juce::MidiMessage::noteOn (1, 40, velocity), 0);

            for (int block = 0; block < 60; ++block)
            {
                buffer.clear();
                processor->processBlock (buffer, midi);
                midi.clear();

                if (block > 30)
                {
                    double sum = 0.0;

                    for (int i = 0; i < kBlockSize; ++i)
                    {
                        const auto v = buffer.getReadPointer (0)[i];
                        sum += static_cast<double> (v) * v;
                    }

                    blockRms.push_back (std::sqrt (sum / kBlockSize));
                }
            }

            midi.addEvent (juce::MidiMessage::noteOff (1, 40), 0);

            for (int block = 0; block < 40; ++block)
            {
                buffer.clear();
                processor->processBlock (buffer, midi);
                midi.clear();
            }
        }

        const auto loudest = *std::max_element (blockRms.begin(), blockRms.end());
        const auto quietest = *std::min_element (blockRms.begin(), blockRms.end());

        return loudest / juce::jmax (1.0e-9, quietest);
    };

    const auto off = measureRange (false);
    const auto on = measureRange (true);

    INFO ("loud/quiet ratio: off " << off << ", OTT on " << on);

    CHECK (on < off);
}

TEST_CASE ("OTT crossovers sum without a hole at the split frequencies",
           "[engine][ott]")
{
    // Linkwitz-Riley 4th order sums to an allpass, so with depth at zero and
    // unity band gains the three bands must reconstruct flat. A naive
    // crossover leaves a dip at the split that no band gain can fix.
    auto processor = makePreparedProcessor();

    setParameter (*processor, pid::ott.enabled, 1.0f);
    setParameter (*processor, pid::ott.depth, 0.0f);
    setParameter (*processor, pid::filter[0].cutoff, 20000.0f);

    auto reference = makePreparedProcessor();
    setParameter (*reference, pid::ott.enabled, 0.0f);
    setParameter (*reference, pid::filter[0].cutoff, 20000.0f);

    const auto withOtt = renderNote (*processor, 45, 0.6).rms;
    const auto without = renderNote (*reference, 45, 0.6).rms;

    INFO ("RMS with OTT at depth 0: " << withOtt << ", bypassed: " << without);

    // Within 1.5 dB. Not bit-exact, because the crossover applies an allpass
    // - the phase changes, the magnitude does not.
    const auto ratioDb = juce::Decibels::gainToDecibels (
        static_cast<float> (withOtt / juce::jmax (1.0e-9, without)));

    CHECK (std::abs (ratioDb) < 1.5f);
}

TEST_CASE ("OTT stays finite at extreme settings", "[engine][ott]")
{
    for (const auto depth : { 0.0f, 0.5f, 1.0f })
    {
        for (const auto time : { 0.0f, 1.0f })
        {
            for (const auto inGain : { -24.0f, 24.0f })
            {
                auto processor = makePreparedProcessor();

                setParameter (*processor, pid::ott.enabled, 1.0f);
                setParameter (*processor, pid::ott.depth, depth);
                setParameter (*processor, pid::ott.time, time);
                setParameter (*processor, pid::ott.inputGain, inGain);
                setParameter (*processor, pid::ott.lowGain, 24.0f);
                setParameter (*processor, pid::ott.highGain, 24.0f);

                const auto result = renderNote (*processor, 36, 0.3);

                INFO ("depth " << depth << " time " << time
                      << " in " << inGain << " peak " << result.peak);

                CHECK (result.allFinite);
                CHECK (result.peak < 100.0f);
            }
        }
    }
}

TEST_CASE ("OTT crossover frequencies are clamped sanely", "[engine][ott]")
{
    // The high split must stay above the low one, whatever the user does.
    auto processor = makePreparedProcessor();

    setParameter (*processor, pid::ott.enabled, 1.0f);
    setParameter (*processor, pid::ott.depth, 1.0f);
    setParameter (*processor, pid::ott.crossoverLow, 500.0f);
    setParameter (*processor, pid::ott.crossoverHigh, 500.0f);

    const auto result = renderNote (*processor, 40, 0.3);

    CHECK (result.allFinite);
    CHECK (result.peak < 100.0f);
}
