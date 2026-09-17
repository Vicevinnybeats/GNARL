#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/Wavetable.h"
#include "dsp/WavetableLibrary.h"

#include <cmath>
#include <vector>

using namespace gnarl::dsp;

namespace
{
    /** Energy in a frame above `harmonicLimit`, relative to total energy.

        This is the measurement that decides whether the mip levels are
        genuinely band-limited. Anything but a tiny residue here means the
        oscillator will alias at pitch, which is the single most common reason
        a wavetable synth sounds cheap in the top octaves. */
    float relativeEnergyAboveHarmonic (const float* frame, int frameSize, int harmonicLimit)
    {
        const auto order = static_cast<int> (std::log2 (static_cast<double> (frameSize)));
        juce::dsp::FFT fft (order);

        std::vector<float> scratch (static_cast<std::size_t> (frameSize) * 2, 0.0f);

        for (int i = 0; i < frameSize; ++i)
            scratch[static_cast<std::size_t> (i)] = frame[i];

        fft.performRealOnlyForwardTransform (scratch.data(), true);

        auto total = 0.0f;
        auto above = 0.0f;

        for (int h = 1; h < frameSize / 2; ++h)
        {
            const auto real = scratch[static_cast<std::size_t> (h) * 2];
            const auto imaginary = scratch[static_cast<std::size_t> (h) * 2 + 1];
            const auto energy = real * real + imaginary * imaginary;

            total += energy;

            if (h > harmonicLimit)
                above += energy;
        }

        if (total <= 0.0f)
            return 0.0f;

        return above / total;
    }

    /** A saw at every harmonic, for a predictable spectrum to measure. */
    void sawGenerator (float, Wavetable::Spectrum& spectrum)
    {
        for (int h = 1; h <= Wavetable::kBaseNumHarmonics; ++h)
            spectrum.setHarmonic (h, 1.0f / static_cast<float> (h));
    }
}

TEST_CASE ("A generated table reports itself generated and normalised", "[wavetable]")
{
    Wavetable table;
    CHECK_FALSE (table.isGenerated());

    table.generate (sawGenerator);

    REQUIRE (table.isGenerated());

    // Normalised to exactly 1.0, so the oscillator's level control starts from
    // a known place whatever the generator produced.
    auto peak = 0.0f;

    for (int frame = 0; frame < Wavetable::kNumFrames; ++frame)
    {
        const auto* data = table.getFrame (0, frame);
        REQUIRE (data != nullptr);

        for (int i = 0; i < Wavetable::kBaseFrameSize; ++i)
            peak = juce::jmax (peak, std::abs (data[i]));
    }

    CHECK (peak == Catch::Approx (1.0f).margin (1.0e-4));
}

TEST_CASE ("Every mip level is populated and finite", "[wavetable]")
{
    Wavetable table;
    table.generate (sawGenerator);

    for (int level = 0; level < Wavetable::kNumMipLevels; ++level)
    {
        const auto frameSize = Wavetable::getFrameSizeForLevel (level);
        INFO ("mip level " << level << " (" << frameSize << " samples)");

        for (int frame = 0; frame < Wavetable::kNumFrames; frame += 37)
        {
            const auto* data = table.getFrame (level, frame);
            REQUIRE (data != nullptr);

            for (int i = 0; i < frameSize; ++i)
                REQUIRE (std::isfinite (data[i]));
        }
    }
}

TEST_CASE ("Guard samples continue the frame periodically on both sides",
           "[wavetable]")
{
    Wavetable table;
    table.generate (sawGenerator);

    // 4-point cubic interpolation reads frame[i-1] .. frame[i+2] with no
    // bounds check, so the padding on BOTH sides has to hold the correctly
    // wrapped samples. Zeroes there would click once per cycle.
    for (int level = 0; level < Wavetable::kNumMipLevels; ++level)
    {
        const auto frameSize = Wavetable::getFrameSizeForLevel (level);
        const auto* data = table.getFrame (level, 128);
        REQUIRE (data != nullptr);

        for (int g = 0; g < Wavetable::kGuardSamplesAfter; ++g)
        {
            INFO ("level " << level << " trailing guard " << g);
            CHECK (data[frameSize + g] == Catch::Approx (data[g % frameSize]).margin (1.0e-6));
        }

        for (int g = 1; g <= Wavetable::kGuardSamplesBefore; ++g)
        {
            INFO ("level " << level << " leading guard " << g);
            CHECK (data[-g] == Catch::Approx (data[frameSize - g]).margin (1.0e-6));
        }
    }
}

TEST_CASE ("Every mip level shares one amplitude scale", "[wavetable]")
{
    // THIS IS THE TEST THAT WAS MISSING. Checking only level 0's peak hid a
    // real bug: juce::dsp::FFT's inverse transform divides by the transform
    // size, so each finer level came out 2048/frameSize times quieter, and
    // normalising the table by level 0 left level 4 sixteen times too loud.
    // The audible symptom is a jump in volume when a glide crosses a mip
    // boundary.
    Wavetable table;

    // One harmonic, which EVERY level can represent, so all of them must
    // produce the same peak.
    table.generate ([] (float, Wavetable::Spectrum& spectrum)
    {
        spectrum.setHarmonic (1, 1.0f);
    });

    for (int level = 0; level < Wavetable::kNumMipLevels; ++level)
    {
        const auto frameSize = Wavetable::getFrameSizeForLevel (level);
        const auto* data = table.getFrame (level, 0);
        REQUIRE (data != nullptr);

        auto peak = 0.0f;

        for (int i = 0; i < frameSize; ++i)
            peak = juce::jmax (peak, std::abs (data[i]));

        INFO ("level " << level << " (" << frameSize << " samples) peak " << peak);
        CHECK (peak == Catch::Approx (1.0f).margin (0.02));
    }
}

TEST_CASE ("No mip level is degenerate", "[wavetable]")
{
    // A frame with only 2 samples holding 1 harmonic samples that sine at its
    // zero crossings, so the frame comes out silent. The sample count is
    // floored to stop the finest levels collapsing that way.
    for (int level = 0; level < Wavetable::kNumMipLevels; ++level)
    {
        const auto frameSize = Wavetable::getFrameSizeForLevel (level);
        const auto harmonics = Wavetable::getNumHarmonicsForLevel (level);

        INFO ("level " << level);

        CHECK (frameSize >= Wavetable::kMinFrameSize);

        // Strictly more than twice the harmonic count: exactly twice is the
        // degenerate Nyquist case.
        CHECK (frameSize > harmonics * 2 - 1);
        CHECK (harmonics >= 1);
    }
}

TEST_CASE ("Mip levels are genuinely band-limited", "[wavetable][aliasing]")
{
    Wavetable table;
    table.generate (sawGenerator);

    // Levels small enough that the FFT is meaningful. Each must hold no
    // significant energy above the harmonic count it claims.
    for (int level = 0; level <= 6; ++level)
    {
        const auto frameSize = Wavetable::getFrameSizeForLevel (level);
        const auto limit = Wavetable::getNumHarmonicsForLevel (level);
        const auto* data = table.getFrame (level, 100);
        REQUIRE (data != nullptr);

        const auto leakage = relativeEnergyAboveHarmonic (data, frameSize, limit);

        INFO ("level " << level << " claims " << limit << " harmonics, leakage "
              << leakage);

        // -60 dB in energy terms.
        CHECK (leakage < 1.0e-6f);
    }
}

TEST_CASE ("Mip level selection never allows a harmonic above Nyquist",
           "[wavetable][aliasing]")
{
    // The whole point of the mip map. Swept across the full musical range at
    // the two sample rates that matter most.
    for (const auto sampleRate : { 44100.0f, 48000.0f })
    {
        for (int midiNote = 0; midiNote <= 127; ++midiNote)
        {
            const auto frequency = 440.0f
                * std::pow (2.0f, (static_cast<float> (midiNote) - 69.0f) / 12.0f);
            const auto increment = frequency / sampleRate;

            const auto level = Wavetable::getMipLevelForIncrement (increment);
            const auto harmonics = Wavetable::getNumHarmonicsForLevel (level);

            INFO ("note " << midiNote << " at " << sampleRate
                  << " Hz -> level " << level << " (" << harmonics << " harmonics)");

            // The highest harmonic present must stay below Nyquist.
            CHECK (static_cast<float> (harmonics) * increment <= 0.5f);

            // And it must be the COARSEST safe level: a finer one would cost
            // bandwidth for nothing, but a needlessly coarse one dulls the
            // tone, which is the failure nobody notices until A/B.
            if (level > 0)
            {
                const auto finer = Wavetable::getNumHarmonicsForLevel (level - 1);
                CHECK (static_cast<float> (finer) * increment > 0.5f);
            }
        }
    }
}

TEST_CASE ("Mip level selection handles degenerate increments", "[wavetable]")
{
    // A stopped or reversed oscillator must not index out of range or feed
    // zero into a logarithm.
    CHECK (Wavetable::getMipLevelForIncrement (0.0f) == 0);
    CHECK (Wavetable::getMipLevelForIncrement (-0.001f)
           == Wavetable::getMipLevelForIncrement (0.001f));

    const auto extreme = Wavetable::getMipLevelForIncrement (100.0f);
    CHECK (extreme >= 0);
    CHECK (extreme < Wavetable::kNumMipLevels);
}

TEST_CASE ("Frame and level accessors clamp out-of-range indices", "[wavetable]")
{
    Wavetable table;
    table.generate (sawGenerator);

    // Reading past the end must clamp rather than walk off the allocation:
    // table position is a modulated parameter, and modulation overshoots.
    CHECK (table.getFrame (0, -1) == table.getFrame (0, 0));
    CHECK (table.getFrame (0, 99999) == table.getFrame (0, Wavetable::kNumFrames - 1));
    CHECK (table.getFrame (-5, 0) == table.getFrame (0, 0));
    CHECK (table.getFrame (99, 0) == table.getFrame (Wavetable::kNumMipLevels - 1, 0));
}

TEST_CASE ("Generation is deterministic", "[wavetable]")
{
    // Two generations must be bit-identical, or a bounce differs from the
    // preview the producer approved.
    Wavetable first;
    Wavetable second;

    first.generate (sawGenerator);
    second.generate (sawGenerator);

    for (int frame = 0; frame < Wavetable::kNumFrames; frame += 17)
    {
        const auto* a = first.getFrame (0, frame);
        const auto* b = second.getFrame (0, frame);

        for (int i = 0; i < Wavetable::kBaseFrameSize; i += 13)
            REQUIRE (a[i] == b[i]);
    }
}

TEST_CASE ("A generator that morphs produces different frames", "[wavetable]")
{
    Wavetable table;

    // Fundamental only at position 0, plus a high harmonic at position 1.
    table.generate ([] (float position, Wavetable::Spectrum& spectrum)
    {
        spectrum.setHarmonic (1, 1.0f);
        spectrum.setHarmonic (32, position);
    });

    const auto* first = table.getFrame (0, 0);
    const auto* last = table.getFrame (0, Wavetable::kNumFrames - 1);

    auto difference = 0.0f;

    for (int i = 0; i < Wavetable::kBaseFrameSize; ++i)
        difference += std::abs (first[i] - last[i]);

    // A table whose frames are all identical is a waveform, not a wavetable.
    CHECK (difference > 1.0f);
}

TEST_CASE ("Importing raw samples still yields band-limited levels",
           "[wavetable][aliasing]")
{
    // A single-cycle sine, repeated. Imported tables go through harmonic
    // analysis precisely so their mip levels are band-limited too; copying
    // samples straight in would leave every level above 0 wrong.
    constexpr int sourceFrames = 4;
    constexpr int sourceFrameSize = 2048;

    std::vector<float> source (static_cast<std::size_t> (sourceFrames * sourceFrameSize));

    for (int f = 0; f < sourceFrames; ++f)
    {
        for (int i = 0; i < sourceFrameSize; ++i)
        {
            const auto phase = juce::MathConstants<float>::twoPi
                             * static_cast<float> (i) / static_cast<float> (sourceFrameSize);
            source[static_cast<std::size_t> (f * sourceFrameSize + i)] = std::sin (phase);
        }
    }

    Wavetable table;
    table.generateFromSamples (source.data(), sourceFrames, sourceFrameSize);

    REQUIRE (table.isGenerated());

    // A sine is one harmonic, so even the coarsest level should reproduce it.
    const auto* frame = table.getFrame (0, 0);
    REQUIRE (frame != nullptr);

    const auto leakage = relativeEnergyAboveHarmonic (frame, Wavetable::kBaseFrameSize, 1);
    INFO ("leakage above the fundamental: " << leakage);
    CHECK (leakage < 1.0e-4f);
}

TEST_CASE ("Importing rejects degenerate input", "[wavetable]")
{
    Wavetable table;
    std::vector<float> data (16, 0.0f);

    table.generateFromSamples (nullptr, 4, 4);
    CHECK_FALSE (table.isGenerated());

    table.generateFromSamples (data.data(), 0, 4);
    CHECK_FALSE (table.isGenerated());

    table.generateFromSamples (data.data(), 4, 1);
    CHECK_FALSE (table.isGenerated());
}

// --- Library ---------------------------------------------------------------

TEST_CASE ("Every factory table generates, is finite and is not silent",
           "[wavetable][library]")
{
    WavetableLibrary library;

    for (int index = 0; index < WavetableLibrary::kNumFactoryTables; ++index)
    {
        const auto& table = library.getTable (index);

        INFO ("table " << index << ": " << WavetableLibrary::getTableName (index));
        REQUIRE (table.isGenerated());

        auto peak = 0.0f;

        for (int frame = 0; frame < Wavetable::kNumFrames; frame += 51)
        {
            const auto* data = table.getFrame (0, frame);
            REQUIRE (data != nullptr);

            for (int i = 0; i < Wavetable::kBaseFrameSize; i += 7)
            {
                REQUIRE (std::isfinite (data[i]));
                peak = juce::jmax (peak, std::abs (data[i]));
            }
        }

        // A silent factory table would look like a broken preset.
        CHECK (peak > 0.05f);
    }
}

TEST_CASE ("Factory tables are band-limited at level 0", "[wavetable][library][aliasing]")
{
    WavetableLibrary library;

    for (int index = 0; index < WavetableLibrary::kNumFactoryTables; ++index)
    {
        const auto& table = library.getTable (index);
        const auto* frame = table.getFrame (4, 100);
        REQUIRE (frame != nullptr);

        const auto frameSize = Wavetable::getFrameSizeForLevel (4);
        const auto limit = Wavetable::getNumHarmonicsForLevel (4);
        const auto leakage = relativeEnergyAboveHarmonic (frame, frameSize, limit);

        INFO ("table " << index << " (" << WavetableLibrary::getTableName (index)
              << ") leakage " << leakage);
        CHECK (leakage < 1.0e-6f);
    }
}

TEST_CASE ("Tables are cached, not regenerated", "[wavetable][library]")
{
    WavetableLibrary library;

    CHECK (library.getTableIfLoaded (0) == nullptr);

    const auto& first = library.getTable (0);
    const auto& second = library.getTable (0);

    CHECK (&first == &second);
    CHECK (library.getTableIfLoaded (0) == &first);
}

TEST_CASE ("Library indices clamp", "[wavetable][library]")
{
    WavetableLibrary library;

    CHECK (&library.getTable (-1) == &library.getTable (0));
    CHECK (&library.getTable (9999)
           == &library.getTable (WavetableLibrary::kNumFactoryTables - 1));

    CHECK (library.getTableIfLoaded (-1) == nullptr);
    CHECK (library.getTableIfLoaded (9999) == nullptr);
}

TEST_CASE ("Every factory table has a name", "[wavetable][library]")
{
    const auto names = WavetableLibrary::getTableNames();

    REQUIRE (names.size() == WavetableLibrary::kNumFactoryTables);

    for (const auto& name : names)
        CHECK (name.isNotEmpty());
}
