#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/FxDistortion.h"

// FxDistortion.h needs only juce_audio_basics; the spectral assertion here
// needs the FFT, so this test pulls juce_dsp in for itself.
#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <vector>

using namespace gnarl;
using namespace gnarl::dsp;
using Catch::Approx;
using Type = FxDistortion::Type;

namespace
{
    constexpr double kSampleRate = 48000.0;

    std::vector<Type> allTypes()
    {
        std::vector<Type> types;

        for (int i = 0; i < static_cast<int> (Type::count); ++i)
            types.push_back (static_cast<Type> (i));

        return types;
    }

    const char* nameOf (Type type)
    {
        return choices::fxDistortionType[static_cast<int> (type)].toRawUTF8();
    }

    FxDistortion::Settings settingsFor (Type type, float driveDb)
    {
        FxDistortion::Settings settings;
        settings.enabled = true;
        settings.type = type;
        settings.driveDb = driveDb;
        settings.mix = 1.0f;

        return settings;
    }

    /** RMS of a sine driven through the stage, after letting the DC blocker
        and the tilt filter settle. */
    double measureRms (FxDistortion& stage, float amplitude, int settleSamples = 4800)
    {
        const auto increment = juce::MathConstants<float>::twoPi * 220.0f
                             / static_cast<float> (kSampleRate);
        auto phase = 0.0f;

        for (int i = 0; i < settleSamples; ++i)
        {
            stage.processSample (0, std::sin (phase) * amplitude);
            phase += increment;
        }

        double sum = 0.0;
        constexpr int kMeasureSamples = 9600;

        for (int i = 0; i < kMeasureSamples; ++i)
        {
            const auto out = stage.processSample (0, std::sin (phase) * amplitude);
            sum += static_cast<double> (out) * out;
            phase += increment;
        }

        return std::sqrt (sum / kMeasureSamples);
    }
}

TEST_CASE ("The FX distortion is finite at every rate and every curve", "[fx][distortion]")
{
    for (const auto rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        for (const auto type : allTypes())
        {
            FxDistortion stage;
            stage.prepare (rate);

            auto settings = settingsFor (type, 48.0f);
            settings.bias = 0.9f;
            settings.tone = -1.0f;
            stage.setSettings (settings);

            INFO ("rate " << rate << ", curve " << nameOf (type));

            for (int i = 0; i < 4096; ++i)
            {
                // Deliberately hostile: full scale, plus a DC offset, plus
                // silence, in one pass.
                const auto input = i < 2048
                    ? std::sin (static_cast<float> (i) * 0.05f) * 1.5f + 0.4f
                    : 0.0f;

                const auto out = stage.processSample (i % 2, input);
                REQUIRE (std::isfinite (out));
            }
        }
    }
}

TEST_CASE ("Drive does not double as a volume control", "[fx][distortion]")
{
    /*  THE BUG THIS EXISTS FOR, twice over. The voice filter's drive shipped
        as a +15 dB volume control once, and a loose test threshold hid it
        (CLAUDE.md §3). This stage then reproduced the same bug with the sign
        flipped: compensating by the slope AT THE ORIGIN normalises the
        small-signal gain, which over 36 dB of drive is a region the signal has
        already left, and every curve measured 12.5 dB DOWN. Hence the RMS
        match in setSettings, and hence the threshold below being tight enough
        to have caught it.

        Every curve is included, rectify among them. An even curve has no unity
        RMS gain about ZERO - it moves most of the signal's energy to DC, which
        is the exclusion the filter's drive test has to make - but the
        compensation here measures RMS about the MEAN, which is precisely what
        the DC blocker downstream leaves. So the arithmetic works out for the
        even curve too, and nothing needs excluding.
    */
    for (const auto type : allTypes())
    {

        FxDistortion quiet;
        FxDistortion loud;
        quiet.prepare (kSampleRate);
        loud.prepare (kSampleRate);

        quiet.setSettings (settingsFor (type, 0.0f));
        loud.setSettings (settingsFor (type, 36.0f));

        const auto atUnity = measureRms (quiet, 0.3f);
        const auto driven = measureRms (loud, 0.3f);

        REQUIRE (atUnity > 1.0e-4);
        REQUIRE (driven > 1.0e-4);

        const auto changeDb = 20.0 * std::log10 (driven / atUnity);

        INFO ("curve " << nameOf (type) << ": " << changeDb << " dB from 0 to 36 dB of drive");

        // 36 dB of drive is allowed to change the level by a few dB - a
        // saturator does compress - but not by anything like 36.
        CHECK (std::abs (changeDb) < 6.0);
    }
}

TEST_CASE ("Drive adds harmonics rather than just level", "[fx][distortion]")
{
    // The other half of the previous test: compensation must not be achieved
    // by the stage doing nothing.
    FxDistortion clean;
    FxDistortion dirty;
    clean.prepare (kSampleRate);
    dirty.prepare (kSampleRate);

    clean.setSettings (settingsFor (Type::tanh, 0.0f));
    dirty.setSettings (settingsFor (Type::hardClip, 36.0f));

    const auto capture = [] (FxDistortion& stage)
    {
        constexpr int kSize = 8192;
        std::vector<float> samples;
        samples.reserve (kSize);

        const auto increment = juce::MathConstants<float>::twoPi * 220.0f
                             / static_cast<float> (kSampleRate);
        auto phase = 0.0f;

        for (int i = 0; i < 4800; ++i)
        {
            stage.processSample (0, std::sin (phase) * 0.5f);
            phase += increment;
        }

        for (int i = 0; i < kSize; ++i)
        {
            samples.push_back (stage.processSample (0, std::sin (phase) * 0.5f));
            phase += increment;
        }

        return samples;
    };

    const auto cleanSamples = capture (clean);
    const auto dirtySamples = capture (dirty);

    // Crest factor: a sine is about 1.41, and a hard-clipped one approaches 1.
    const auto crest = [] (const std::vector<float>& samples)
    {
        double sum = 0.0;
        auto peak = 0.0f;

        for (const auto s : samples)
        {
            sum += static_cast<double> (s) * s;
            peak = juce::jmax (peak, std::abs (s));
        }

        return peak / std::sqrt (sum / samples.size());
    };

    const auto cleanCrest = crest (cleanSamples);
    const auto dirtyCrest = crest (dirtySamples);

    INFO ("crest factor: clean " << cleanCrest << ", clipped " << dirtyCrest);

    CHECK (cleanCrest > 1.35);
    CHECK (dirtyCrest < 1.2);
}

TEST_CASE ("Bias puts even harmonics in", "[fx][distortion]")
{
    /*  A symmetric curve makes only ODD harmonics. Bias is what puts even
        ones in, and it is the difference between "fuzzy" and "growling", so
        this measures the second harmonic rather than asserting the parameter
        was read.
    */
    const auto secondHarmonicLevel = [] (float bias)
    {
        FxDistortion stage;
        stage.prepare (kSampleRate);

        auto settings = settingsFor (Type::tanh, 24.0f);
        settings.bias = bias;
        stage.setSettings (settings);

        constexpr int fftOrder = 13;
        constexpr int fftSize = 1 << fftOrder;

        const auto fundamental = 250.0f;
        const auto increment = juce::MathConstants<float>::twoPi * fundamental
                             / static_cast<float> (kSampleRate);
        auto phase = 0.0f;

        for (int i = 0; i < 9600; ++i)
        {
            stage.processSample (0, std::sin (phase) * 0.5f);
            phase += increment;
        }

        std::vector<float> scratch (static_cast<std::size_t> (fftSize) * 2, 0.0f);

        for (int i = 0; i < fftSize; ++i)
        {
            // Blackman-Harris, not Hann: CLAUDE.md §8. Hann's -31 dB sidelobes
            // leak enough to fake a harmonic that is not there.
            const auto t = static_cast<float> (i) / static_cast<float> (fftSize - 1);
            const auto window = 0.35875f
                              - 0.48829f * std::cos (juce::MathConstants<float>::twoPi * t)
                              + 0.14128f * std::cos (2.0f * juce::MathConstants<float>::twoPi * t)
                              - 0.01168f * std::cos (3.0f * juce::MathConstants<float>::twoPi * t);

            scratch[static_cast<std::size_t> (i)] =
                stage.processSample (0, std::sin (phase) * 0.5f) * window;
            phase += increment;
        }

        juce::dsp::FFT fft (fftOrder);
        fft.performRealOnlyForwardTransform (scratch.data(), true);

        const auto binHz = static_cast<float> (kSampleRate) / static_cast<float> (fftSize);

        const auto peakAround = [&] (float frequency)
        {
            const auto centre = static_cast<int> (frequency / binHz);
            auto peak = 0.0f;

            for (int bin = juce::jmax (1, centre - 4); bin <= centre + 4; ++bin)
            {
                const auto real = scratch[static_cast<std::size_t> (bin) * 2];
                const auto imaginary = scratch[static_cast<std::size_t> (bin) * 2 + 1];
                peak = juce::jmax (peak, std::sqrt (real * real + imaginary * imaginary));
            }

            return peak;
        };

        const auto first = peakAround (fundamental);
        const auto second = peakAround (fundamental * 2.0f);

        return first > 0.0f ? second / first : 0.0f;
    };

    const auto symmetric = secondHarmonicLevel (0.0f);
    const auto biased = secondHarmonicLevel (0.8f);

    INFO ("second harmonic relative to the fundamental: symmetric " << symmetric
          << ", biased " << biased);

    // Symmetric tanh should have essentially no second harmonic.
    CHECK (symmetric < 0.02f);

    // And bias should put a clearly measurable one in.
    CHECK (biased > 0.05f);
}

TEST_CASE ("Mix blends, and a disabled stage is exactly transparent", "[fx][distortion]")
{
    FxDistortion stage;
    stage.prepare (kSampleRate);

    auto settings = settingsFor (Type::hardClip, 36.0f);
    settings.mix = 0.0f;
    stage.setSettings (settings);

    // Mix at zero is the dry signal, bit for bit: the wet path is computed and
    // then contributes nothing.
    for (int i = 0; i < 256; ++i)
    {
        const auto input = std::sin (static_cast<float> (i) * 0.1f) * 0.7f;
        CHECK (stage.processSample (0, input) == Approx (input));
    }

    settings.enabled = false;
    stage.setSettings (settings);

    // Disabled is transparent without even running the stage, so the DC
    // blocker cannot contribute a slow drift to a bypassed effect.
    for (int i = 0; i < 256; ++i)
    {
        const auto input = std::sin (static_cast<float> (i) * 0.1f) * 0.7f;
        CHECK (stage.processSample (0, input) == Approx (input));
    }
}

TEST_CASE ("The FX distortion blocks DC", "[fx][distortion]")
{
    // Bias and the even-harmonic curves both shift the signal off zero, and DC
    // reaching the master is headroom thrown away for something inaudible.
    FxDistortion stage;
    stage.prepare (kSampleRate);

    auto settings = settingsFor (Type::rectify, 24.0f);
    settings.bias = 0.7f;
    stage.setSettings (settings);

    const auto increment = juce::MathConstants<float>::twoPi * 200.0f
                         / static_cast<float> (kSampleRate);
    auto phase = 0.0f;

    for (int i = 0; i < 48000; ++i)
    {
        stage.processSample (0, std::sin (phase) * 0.5f);
        phase += increment;
    }

    double sum = 0.0;
    constexpr int kMeasureSamples = 48000;

    for (int i = 0; i < kMeasureSamples; ++i)
    {
        sum += stage.processSample (0, std::sin (phase) * 0.5f);
        phase += increment;
    }

    const auto mean = sum / kMeasureSamples;
    INFO ("mean output over one second: " << mean);

    CHECK (std::abs (mean) < 0.01);
}

TEST_CASE ("A parameter sweep never produces a NaN", "[fx][distortion]")
{
    FxDistortion stage;
    stage.prepare (kSampleRate);

    for (const auto type : allTypes())
    {
        for (int step = 0; step <= 40; ++step)
        {
            const auto t = static_cast<float> (step) / 40.0f;

            auto settings = settingsFor (type, t * 48.0f);
            settings.tone = t * 2.0f - 1.0f;
            settings.bias = 1.0f - t * 2.0f;
            settings.mix = t;
            settings.outputDb = t * 48.0f - 24.0f;
            stage.setSettings (settings);

            for (int i = 0; i < 64; ++i)
            {
                const auto out = stage.processSample (i % 2,
                    std::sin (static_cast<float> (i) * 0.3f) * (0.2f + t));
                REQUIRE (std::isfinite (out));
            }
        }
    }
}
