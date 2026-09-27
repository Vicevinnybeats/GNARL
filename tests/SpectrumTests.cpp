#include <catch2/catch_test_macros.hpp>

#include "dsp/SpectrumAnalyser.h"

#include <cmath>

using namespace gnarl;

/*
    The spectrum display's analyser.

    WHAT IS WORTH ASSERTING about a picture is not "the numbers are right" -
    a display is allowed to be approximate - but that it POINTS AT THE RIGHT
    PLACE. A spectrum whose peak lands an octave off is worse than no
    spectrum: it would have somebody EQ the wrong band and trust it.

    So these tests feed tones at known frequencies and check the peak bin
    maps back to that frequency, and feed silence and check the display is
    at its floor rather than showing the transform's own noise.
*/

namespace
{
    constexpr double kSampleRate = 48000.0;

    /** Does the drawn bin the peak landed in actually COVER this frequency?

        THE FIRST VERSION OF THIS TEST RE-DERIVED the analyser's mapping from
        the log formula and compared bin numbers. It failed by 27 bins - and
        it was right to: the mapping is not purely logarithmic, because at
        2048 points a log curve asks for resolution the bottom of the FFT
        does not have and the monotonic pass spreads those bins out one FFT
        bin at a time. The re-derivation was asserting a formula the code
        deliberately does not follow.

        Asking the analyser which FFT bins a drawn bin covers tests the
        property that matters - the peak points at the right frequency -
        without either copying the mapping or assuming a shape it never
        claimed. */
    bool binCovers (const dsp::SpectrumAnalyser& analyser, int bin, double frequencyHz)
    {
        const auto [first, last] = analyser.getBinRange (bin);

        const auto lowHz = first * kSampleRate / dsp::SpectrumAnalyser::kFftSize;
        const auto highHz = last * kSampleRate / dsp::SpectrumAnalyser::kFftSize;

        // One FFT bin of slack either side: a tone rarely sits exactly on an
        // edge and its energy leaks into the neighbouring bin.
        const auto slack = kSampleRate / dsp::SpectrumAnalyser::kFftSize;

        return frequencyHz >= lowHz - slack && frequencyHz <= highHz + slack;
    }

    void pushTone (dsp::SpectrumAnalyser& analyser, double frequencyHz,
                   float amplitude = 0.5f)
    {
        // More than the ring holds, so what is analysed is steady state.
        constexpr int kBlock = 256;
        std::array<float, kBlock> block {};

        double phase = 0.0;
        const auto increment = frequencyHz / kSampleRate;

        for (int b = 0; b < 40; ++b)
        {
            for (int i = 0; i < kBlock; ++i)
            {
                block[static_cast<std::size_t> (i)] =
                    amplitude * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * phase));

                phase += increment;

                if (phase >= 1.0)
                    phase -= 1.0;
            }

            analyser.pushBlock (block.data(), nullptr, kBlock);
        }
    }

    int peakBin (const std::array<float, dsp::SpectrumAnalyser::kNumBins>& bins)
    {
        int best = 0;

        for (int i = 1; i < dsp::SpectrumAnalyser::kNumBins; ++i)
            if (bins[static_cast<std::size_t> (i)] > bins[static_cast<std::size_t> (best)])
                best = i;

        return best;
    }
}

TEST_CASE ("The peak lands where the tone is", "[spectrum]")
{
    /*  THE ONE THAT MATTERS. A display whose peak is an octave off is worse
        than no display - somebody would EQ the wrong band and believe it.

        Checked across the range a riddim patch actually occupies, from a
        sub fundamental to the top of a screech. */
    for (const auto frequency : { 55.0, 110.0, 440.0, 1000.0, 4000.0, 10000.0 })
    {
        dsp::SpectrumAnalyser analyser;
        pushTone (analyser, frequency);

        std::array<float, dsp::SpectrumAnalyser::kNumBins> bins {};
        REQUIRE (analyser.read (bins));

        const auto peak = peakBin (bins);
        const auto [first, last] = analyser.getBinRange (peak);

        INFO (frequency << " Hz -> drawn bin " << peak
                        << " covering FFT bins " << first << ".." << last);

        CHECK (binCovers (analyser, peak, frequency));
    }
}

TEST_CASE ("Bins rise monotonically with frequency", "[spectrum]")
{
    /*  Not just "near the right place" for each tone independently - the
        ORDER has to hold, or the display could be mirrored or scrambled and
        still pass the case above at every point it was sampled. */
    auto previous = -1;

    for (const auto frequency : { 60.0, 250.0, 1000.0, 5000.0, 15000.0 })
    {
        dsp::SpectrumAnalyser analyser;
        pushTone (analyser, frequency);

        std::array<float, dsp::SpectrumAnalyser::kNumBins> bins {};
        REQUIRE (analyser.read (bins));

        const auto peak = peakBin (bins);

        INFO (frequency << " Hz lands in bin " << peak);
        CHECK (peak > previous);

        previous = peak;
    }
}

TEST_CASE ("A louder tone reads louder", "[spectrum]")
{
    dsp::SpectrumAnalyser loud;
    dsp::SpectrumAnalyser quiet;

    pushTone (loud, 440.0, 0.5f);
    pushTone (quiet, 440.0, 0.05f);

    std::array<float, dsp::SpectrumAnalyser::kNumBins> loudBins {};
    std::array<float, dsp::SpectrumAnalyser::kNumBins> quietBins {};

    REQUIRE (loud.read (loudBins));
    REQUIRE (quiet.read (quietBins));

    const auto difference = loudBins[static_cast<std::size_t> (peakBin (loudBins))]
                          - quietBins[static_cast<std::size_t> (peakBin (quietBins))];

    // A factor of ten is 20 dB. Generous bounds: this asserts the scale is
    // dB and the right way up, not that the calibration is exact.
    INFO ("difference: " << difference << " dB");
    CHECK (difference > 15.0f);
    CHECK (difference < 25.0f);
}

TEST_CASE ("Silence reads as the floor, not as noise", "[spectrum]")
{
    /*  An analyser that showed its own transform noise would draw a
        permanent carpet along the bottom of the display, which reads as the
        synth being noisy. */
    dsp::SpectrumAnalyser analyser;

    std::array<float, 256> silence {};

    for (int i = 0; i < 40; ++i)
        analyser.pushBlock (silence.data(), nullptr, 256);

    std::array<float, dsp::SpectrumAnalyser::kNumBins> bins {};
    analyser.read (bins);

    for (int i = 0; i < dsp::SpectrumAnalyser::kNumBins; ++i)
    {
        INFO ("bin " << i);
        CHECK (bins[static_cast<std::size_t> (i)] == dsp::SpectrumAnalyser::kFloorDb);
    }
}

TEST_CASE ("Nothing pushed yet draws nothing", "[spectrum]")
{
    // Rather than a frame of whatever the buffers happened to contain.
    dsp::SpectrumAnalyser analyser;

    std::array<float, dsp::SpectrumAnalyser::kNumBins> bins {};
    CHECK_FALSE (analyser.read (bins));
}

TEST_CASE ("Every drawn bin covers at least one FFT bin", "[spectrum]")
{
    /*  A logarithmic grouping rounds several low bins onto the same FFT bin.
        If a group came out empty it would read as a permanent notch at
        whatever frequency the rounding collapsed - a hole in the display
        that no signal could ever fill. */
    dsp::SpectrumAnalyser analyser;

    // Broadband, so every bin has something in it if it can have anything.
    juce::Random random { 1234 };
    std::array<float, 256> noise {};

    for (int b = 0; b < 40; ++b)
    {
        for (auto& sample : noise)
            sample = random.nextFloat() * 0.5f - 0.25f;

        analyser.pushBlock (noise.data(), nullptr, 256);
    }

    std::array<float, dsp::SpectrumAnalyser::kNumBins> bins {};
    REQUIRE (analyser.read (bins));

    for (int i = 0; i < dsp::SpectrumAnalyser::kNumBins; ++i)
    {
        INFO ("bin " << i << " reads " << bins[static_cast<std::size_t> (i)] << " dB");
        CHECK (bins[static_cast<std::size_t> (i)] > dsp::SpectrumAnalyser::kFloorDb);
    }
}

TEST_CASE ("A panned signal still shows up", "[spectrum]")
{
    /*  The analyser takes the mono SUM, not a channel. A spectrum of the
        left channel alone would show a hole wherever the patch panned
        something right, which reads as a bug in the synth rather than as a
        property of the picture. */
    dsp::SpectrumAnalyser analyser;

    constexpr int kBlock = 256;
    std::array<float, kBlock> left {};
    std::array<float, kBlock> right {};

    double phase = 0.0;

    for (int b = 0; b < 40; ++b)
    {
        for (int i = 0; i < kBlock; ++i)
        {
            // Hard right: left is silent.
            right[static_cast<std::size_t> (i)] =
                0.5f * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * phase));

            phase += 440.0 / kSampleRate;

            if (phase >= 1.0)
                phase -= 1.0;
        }

        analyser.pushBlock (left.data(), right.data(), kBlock);
    }

    std::array<float, dsp::SpectrumAnalyser::kNumBins> bins {};
    REQUIRE (analyser.read (bins));

    CHECK (binCovers (analyser, peakBin (bins), 440.0));
}
