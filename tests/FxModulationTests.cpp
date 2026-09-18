#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/FxChorus.h"
#include "dsp/FxDimension.h"
#include "dsp/FxFlanger.h"
#include "dsp/FxHyper.h"
#include "dsp/FxPhaser.h"

#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <vector>

using namespace gnarl;
using namespace gnarl::dsp;
using Catch::Approx;

namespace
{
    constexpr double kSampleRate = 48000.0;

    /** Every one of these effects must leave the signal alone when it is
        disabled and when its mix is zero, and "alone" means bit-identical.
        Fourteen of these sit in a chain that most patches do not use. */
    template <typename Effect, typename Settings>
    void checkTransparent (Settings settings)
    {
        Effect effect;
        effect.prepare (kSampleRate);

        SECTION ("disabled")
        {
            auto disabled = settings;
            disabled.enabled = false;
            effect.setSettings (disabled);

            for (int i = 0; i < 512; ++i)
            {
                const auto input = std::sin (static_cast<float> (i) * 0.1f);

                if constexpr (requires { effect.processSample (0, input); })
                {
                    CHECK (effect.processSample (0, input) == input);
                    CHECK (effect.processSample (1, input) == input);
                }
                else
                {
                    auto left = input;
                    auto right = input * 0.5f;
                    const auto dryLeft = left;
                    const auto dryRight = right;

                    effect.processSample (left, right);

                    CHECK (left == dryLeft);
                    CHECK (right == dryRight);
                }
            }
        }

        SECTION ("mix at zero")
        {
            auto silent = settings;
            silent.enabled = true;
            silent.mix = 0.0f;
            effect.setSettings (silent);

            for (int i = 0; i < 512; ++i)
            {
                const auto input = std::sin (static_cast<float> (i) * 0.1f);

                if constexpr (requires { effect.processSample (0, input); })
                {
                    CHECK (effect.processSample (0, input) == input);
                }
                else
                {
                    auto left = input;
                    auto right = input * 0.5f;
                    const auto dryLeft = left;

                    effect.processSample (left, right);

                    CHECK (left == dryLeft);
                }
            }
        }
    }

    FxChorus::Settings chorusSettings()
    {
        FxChorus::Settings s;
        s.enabled = true;
        s.mix = 1.0f;
        s.rateHz = 1.0f;
        s.depth = 0.6f;
        s.voices = 3;
        s.spread = 0.8f;
        s.feedback = 0.2f;
        return s;
    }

    FxFlanger::Settings flangerSettings()
    {
        FxFlanger::Settings s;
        s.enabled = true;
        s.mix = 0.5f;
        s.rateHz = 0.4f;
        s.depth = 0.7f;
        s.feedback = 0.5f;
        s.manual = 0.3f;
        s.stereo = 0.5f;
        return s;
    }

    FxPhaser::Settings phaserSettings()
    {
        FxPhaser::Settings s;
        s.enabled = true;
        s.mix = 0.5f;
        s.rateHz = 0.5f;
        s.depth = 0.7f;
        s.stages = 6;
        s.centreHz = 800.0f;
        s.feedback = 0.3f;
        s.stereo = 0.5f;
        return s;
    }

    FxHyper::Settings hyperSettings()
    {
        FxHyper::Settings s;
        s.enabled = true;
        s.mix = 1.0f;
        s.amount = 0.6f;
        s.detune = 0.5f;
        s.voices = 4;
        s.width = 0.8f;
        return s;
    }

    FxDimension::Settings dimensionSettings()
    {
        FxDimension::Settings s;
        s.enabled = true;
        s.mix = 1.0f;
        s.amount = 0.7f;
        s.width = 0.8f;
        s.timeMs = 12.0f;
        return s;
    }

    /** RMS of a per-channel effect's output over a fixed test signal. */
    template <typename Effect>
    double rmsOf (Effect& effect, int samples = 48000)
    {
        auto sumSquares = 0.0;

        for (int i = 0; i < samples; ++i)
        {
            const auto t = static_cast<float> (i) / static_cast<float> (kSampleRate);
            const auto input = 0.5f
                             * (std::sin (juce::MathConstants<float>::twoPi * 220.0f * t)
                                + 0.5f * std::sin (juce::MathConstants<float>::twoPi * 660.0f * t));

            const auto out = effect.processSample (0, input);

            if (i > samples / 4)
                sumSquares += static_cast<double> (out) * out;
        }

        return std::sqrt (sumSquares / static_cast<double> (samples - samples / 4));
    }
}

// --- Transparency ----------------------------------------------------------

TEST_CASE ("A disabled chorus is transparent", "[fx][chorus]")
{
    checkTransparent<FxChorus> (chorusSettings());
}

TEST_CASE ("A disabled flanger is transparent", "[fx][flanger]")
{
    checkTransparent<FxFlanger> (flangerSettings());
}

TEST_CASE ("A disabled phaser is transparent", "[fx][phaser]")
{
    checkTransparent<FxPhaser> (phaserSettings());
}

TEST_CASE ("A disabled hyper is transparent", "[fx][hyper]")
{
    checkTransparent<FxHyper> (hyperSettings());
}

TEST_CASE ("A disabled dimension is transparent", "[fx][dimension]")
{
    checkTransparent<FxDimension> (dimensionSettings());
}

// --- Chorus ----------------------------------------------------------------

TEST_CASE ("Chorus voices do not sum coherently", "[fx][chorus]")
{
    /*  THE BUG THIS EXISTS FOR. Four taps modulated in step are one tap four
        times as loud: they sum coherently and the effect is a vibrato, not a
        chorus. The taps have to disagree about where they are, which means
        adding voices must NOT multiply the level. */
    std::vector<double> levels;

    for (const auto voices : { 1, 2, 3, 4 })
    {
        auto settings = chorusSettings();
        settings.voices = voices;
        settings.feedback = 0.0f;

        FxChorus chorus;
        chorus.prepare (kSampleRate);
        chorus.setSettings (settings);

        levels.push_back (rmsOf (chorus));
    }

    REQUIRE (levels.front() > 1.0e-4);

    for (std::size_t i = 1; i < levels.size(); ++i)
    {
        const auto changeDb = 20.0 * std::log10 (levels[i] / levels.front());

        INFO (i + 1 << " voices is " << changeDb << " dB against one");

        // Four coherent taps would be +12 dB. A few dB of movement is the
        // taps interfering, which is the effect.
        CHECK (std::abs (changeDb) < 4.0);
    }
}

TEST_CASE ("A chorus moves the signal", "[fx][chorus]")
{
    // The other half: level-matching must not have been achieved by doing
    // nothing. A chorus's output differs from its input continuously.
    FxChorus chorus;
    chorus.prepare (kSampleRate);
    chorus.setSettings (chorusSettings());

    auto largestDifference = 0.0f;

    for (int i = 0; i < 48000; ++i)
    {
        const auto input = std::sin (static_cast<float> (i) * 0.05f) * 0.5f;
        largestDifference = juce::jmax (largestDifference,
                                        std::abs (chorus.processSample (0, input) - input));
    }

    CHECK (largestDifference > 0.05f);
}

TEST_CASE ("Chorus spread decorrelates the two channels", "[fx][chorus]")
{
    // What makes it stereo. With spread at zero the two channels get the same
    // treatment; with it up they do not.
    const auto channelDifference = [] (float spread)
    {
        auto settings = chorusSettings();
        settings.spread = spread;

        FxChorus chorus;
        chorus.prepare (kSampleRate);
        chorus.setSettings (settings);

        auto largest = 0.0f;

        for (int i = 0; i < 48000; ++i)
        {
            const auto input = std::sin (static_cast<float> (i) * 0.05f) * 0.5f;

            const auto left = chorus.processSample (0, input);
            const auto right = chorus.processSample (1, input);

            largest = juce::jmax (largest, std::abs (left - right));
        }

        return largest;
    };

    const auto mono = channelDifference (0.0f);
    const auto wide = channelDifference (1.0f);

    INFO ("channel difference: spread 0 gives " << mono << ", spread 1 gives " << wide);

    CHECK (mono == Approx (0.0f).margin (1.0e-6f));
    CHECK (wide > 0.05f);
}

// --- Flanger ---------------------------------------------------------------

TEST_CASE ("Flanger feedback polarity moves the comb", "[fx][flanger]")
{
    /*  THE CONTROL THAT MATTERS MOST HERE. A negative feedback flanger has its
        nulls where a positive one has its peaks. If the sign did nothing, the
        parameter would have been declared unipolar and half the effect would
        be missing - so this measures that the two are actually different. */
    const auto renderWith = [] (float feedback)
    {
        auto settings = flangerSettings();
        settings.feedback = feedback;
        settings.rateHz = 0.01f;    // Nearly static, so the comb sits still.
        settings.depth = 0.0f;

        FxFlanger flanger;
        flanger.prepare (kSampleRate);
        flanger.setSettings (settings);

        std::vector<float> output;
        output.reserve (24000);

        for (int i = 0; i < 24000; ++i)
        {
            // Noise, so there is energy at every comb frequency.
            const auto input = std::sin (static_cast<float> (i) * 0.31f)
                             + std::sin (static_cast<float> (i) * 1.13f);

            output.push_back (flanger.processSample (0, input * 0.4f));
        }

        return output;
    };

    const auto positive = renderWith (0.9f);
    const auto negative = renderWith (-0.9f);

    auto largestDifference = 0.0f;

    for (std::size_t i = positive.size() / 2; i < positive.size(); ++i)
        largestDifference = juce::jmax (largestDifference,
                                        std::abs (positive[i] - negative[i]));

    INFO ("largest difference between +90% and -90% feedback: " << largestDifference);
    CHECK (largestDifference > 0.05f);
}

TEST_CASE ("Flanger manual moves the comb without the LFO", "[fx][flanger]")
{
    // The control a chorus does not have: where the comb sits IS the sound.
    const auto renderWith = [] (float manual)
    {
        auto settings = flangerSettings();
        settings.manual = manual;
        settings.depth = 0.0f;
        settings.rateHz = 0.01f;

        FxFlanger flanger;
        flanger.prepare (kSampleRate);
        flanger.setSettings (settings);

        auto sumSquares = 0.0;

        for (int i = 0; i < 24000; ++i)
        {
            const auto input = std::sin (static_cast<float> (i) * 0.4f) * 0.5f;
            const auto out = flanger.processSample (0, input);

            if (i > 12000)
                sumSquares += static_cast<double> (out) * out;
        }

        return std::sqrt (sumSquares / 12000.0);
    };

    // A fixed tone through a comb whose position moves: the level at that one
    // frequency must change, because the comb's nulls have moved past it.
    const auto low = renderWith (0.0f);
    const auto high = renderWith (1.0f);

    INFO ("manual 0 gives " << low << ", manual 1 gives " << high);
    CHECK (std::abs (low - high) > 0.01);
}

TEST_CASE ("A flanger at full feedback does not run away", "[fx][flanger]")
{
    for (const auto feedback : { -1.0f, 1.0f })
    {
        auto settings = flangerSettings();
        settings.feedback = feedback;

        FxFlanger flanger;
        flanger.prepare (kSampleRate);
        flanger.setSettings (settings);

        for (int i = 0; i < 4800; ++i)
            flanger.processSample (0, (i % 16 < 8 ? 1.0f : -1.0f));

        auto earlyPeak = 0.0f;
        auto latePeak = 0.0f;

        for (int i = 0; i < static_cast<int> (kSampleRate * 20); ++i)
        {
            const auto out = std::abs (flanger.processSample (0, 0.0f));

            REQUIRE (std::isfinite (out));

            if (i < 24000)
                earlyPeak = juce::jmax (earlyPeak, out);
            else if (i > static_cast<int> (kSampleRate * 15))
                latePeak = juce::jmax (latePeak, out);
        }

        INFO ("feedback " << feedback << ": early " << earlyPeak
              << ", late " << latePeak);
        CHECK (latePeak < earlyPeak);
    }
}

// --- Phaser ----------------------------------------------------------------

TEST_CASE ("A first-order all-pass passes every frequency at unity",
           "[fx][phaser]")
{
    /*  The defining property, and the one that says the stage is an all-pass
        rather than a filter: it changes PHASE and not magnitude. If this fails
        the phaser's notches come from the stages attenuating rather than from
        the dry and wet paths interfering, which is a different effect. */
    for (const auto frequency : { 50.0, 200.0, 1000.0, 5000.0, 15000.0 })
    {
        AllPassStage stage;
        stage.prepare (kSampleRate);
        stage.setFrequency (900.0f);

        const auto increment = juce::MathConstants<double>::twoPi * frequency / kSampleRate;
        auto phase = 0.0;
        auto sumSquares = 0.0;

        for (int i = 0; i < 48000; ++i, phase += increment)
        {
            const auto out = stage.processSample (static_cast<float> (std::sin (phase)));

            if (i > 4800)
                sumSquares += static_cast<double> (out) * out;
        }

        const auto gain = std::sqrt (sumSquares / (48000.0 - 4800.0))
                        * juce::MathConstants<double>::sqrt2;

        INFO (frequency << " Hz passes at a gain of " << gain);
        CHECK (gain == Approx (1.0).margin (0.01));
    }
}

TEST_CASE ("Each pair of phaser stages adds one notch", "[fx][phaser]")
{
    /*  THE CLAIM THAT ACTUALLY HOLDS, arrived at by measuring the wrong one
        first. The obvious test - probe eight frequencies and check that more
        stages spread the level further - said twelve stages notch LESS deeply
        than two (17.4 dB against 11.6). That is not a bug in the phaser: more
        stages means more notches, each NARROWER, and eight sparse probes miss
        narrow ones while reliably landing near the single wide notch that two
        stages make. The measurement was answering a different question.

        What the stage count actually controls is the NUMBER of notches, and
        the arithmetic says how many: an N-stage all-pass chain sweeps its
        phase from 0 to -N*pi, and summing it with the dry path cancels every
        time that phase passes an odd multiple of pi. So there are N/2 notches
        for even N. Counting them needs the whole spectrum rather than eight
        points of it, which is what the impulse response gives. */
    const auto notchCountOf = [] (int stages)
    {
        auto settings = phaserSettings();
        settings.stages = stages;
        settings.rateHz = 0.01f;
        settings.depth = 0.0f;       // Static, so the notches hold still.
        settings.feedback = 0.0f;    // Feedback adds peaks, which is a
        settings.mix = 0.5f;         // different shape to count.
        settings.centreHz = 1200.0f;

        FxPhaser phaser;
        phaser.prepare (kSampleRate);
        phaser.setSettings (settings);

        constexpr auto fftOrder = 13;
        constexpr auto fftSize = 1 << fftOrder;

        // The impulse response IS the transfer function, so one run gives the
        // whole spectrum rather than one point of it.
        std::vector<float> scratch (static_cast<std::size_t> (fftSize) * 2, 0.0f);

        for (int i = 0; i < fftSize; ++i)
            scratch[static_cast<std::size_t> (i)] =
                phaser.processSample (0, i == 0 ? 1.0f : 0.0f);

        juce::dsp::FFT fft (fftOrder);
        fft.performRealOnlyForwardTransform (scratch.data(), true);

        std::vector<float> magnitude;
        magnitude.reserve (static_cast<std::size_t> (fftSize / 2));

        for (int bin = 0; bin < fftSize / 2; ++bin)
        {
            const auto real = scratch[static_cast<std::size_t> (bin) * 2];
            const auto imaginary = scratch[static_cast<std::size_t> (bin) * 2 + 1];
            magnitude.push_back (std::sqrt (real * real + imaginary * imaginary));
        }

        // A notch is a local minimum that is a real dip rather than ripple:
        // at least 6 dB below the surrounding response. The window is wide
        // enough not to count the same notch twice.
        constexpr auto window = 12;
        constexpr auto depthRatio = 0.5f;   // -6 dB.

        auto notches = 0;

        for (auto bin = static_cast<std::size_t> (window);
             bin + window < magnitude.size(); ++bin)
        {
            const auto here = magnitude[bin];

            auto isLowest = true;
            auto surrounding = 0.0f;

            for (auto offset = 1; offset <= window; ++offset)
            {
                const auto below = magnitude[bin - static_cast<std::size_t> (offset)];
                const auto above = magnitude[bin + static_cast<std::size_t> (offset)];

                isLowest = isLowest && here <= below && here <= above;
                surrounding = juce::jmax (surrounding, below, above);
            }

            if (isLowest && here < surrounding * depthRatio)
                ++notches;
        }

        return notches;
    };

    for (const auto stages : { 2, 4, 8, 12 })
    {
        const auto measured = notchCountOf (stages);

        INFO (stages << " stages gives " << measured << " notches, expected "
              << stages / 2);

        // Exactly N/2. A notch can fall outside the measured band if the
        // centre is pushed to an extreme, which is why the centre sits at
        // 1200 Hz with room either side.
        CHECK (measured == stages / 2);
    }
}

TEST_CASE ("An odd phaser stage count is allowed and different", "[fx][phaser]")
{
    // An odd count inverts the dry/wet relationship, which is a usable sound
    // rather than a bug - hence a 2..12 parameter rather than even-only.
    const auto renderWith = [] (int stages)
    {
        auto settings = phaserSettings();
        settings.stages = stages;
        settings.rateHz = 0.01f;
        settings.depth = 0.0f;

        FxPhaser phaser;
        phaser.prepare (kSampleRate);
        phaser.setSettings (settings);

        std::vector<float> output;

        for (int i = 0; i < 12000; ++i)
            output.push_back (phaser.processSample (
                0, std::sin (static_cast<float> (i) * 0.1f) * 0.5f));

        return output;
    };

    const auto even = renderWith (4);
    const auto odd = renderWith (5);

    auto largest = 0.0f;

    for (std::size_t i = even.size() / 2; i < even.size(); ++i)
        largest = juce::jmax (largest, std::abs (even[i] - odd[i]));

    CHECK (largest > 0.01f);
}

// --- Hyper -----------------------------------------------------------------

TEST_CASE ("Hyper voices do not sum coherently", "[fx][hyper]")
{
    /*  MEASURED WITH NOISE, NOT WITH A TONE, and that is the whole reason
        this test says anything. A single sine through several taps spread in
        time has comb nulls by construction: at 220 Hz one cycle is 4.5 ms and
        the taps here are spread over 3 ms, so they land two thirds of a cycle
        apart and partly CANCEL. Measuring that way showed the level moving by
        7 dB with the voice count and sent me through three different
        normalisation models, none of which could be right - the quantity
        being measured was a comb null, not a level.

        Broadband input is what the question actually means. Averaged across
        frequency the nulls and the peaks are both there, which is what a
        listener hears, and the taps are then genuinely incoherent. */
    std::vector<double> levels;

    for (const auto voices : { 2, 4, 6, 8 })
    {
        auto settings = hyperSettings();
        settings.voices = voices;

        FxHyper hyper;
        hyper.prepare (kSampleRate);
        hyper.setSettings (settings);

        // The same seed for every voice count, so the comparison is between
        // the voice counts and not between two different noise signals.
        juce::Random random (20260918);

        auto sumSquares = 0.0;
        constexpr auto samples = 96000;

        for (int i = 0; i < samples; ++i)
        {
            auto left = (random.nextFloat() * 2.0f - 1.0f) * 0.5f;
            auto right = left;

            hyper.processSample (left, right);

            if (i > samples / 4)
                sumSquares += static_cast<double> (left) * left
                            + static_cast<double> (right) * right;
        }

        levels.push_back (std::sqrt (sumSquares / (2.0 * (samples - samples / 4))));
    }

    REQUIRE (levels.front() > 1.0e-4);

    for (std::size_t i = 1; i < levels.size(); ++i)
    {
        const auto changeDb = 20.0 * std::log10 (levels[i] / levels.front());

        INFO ("voice count step " << i << " is " << changeDb << " dB against two");
        CHECK (std::abs (changeDb) < 4.0);
    }
}

TEST_CASE ("Hyper width spreads the voices across the field", "[fx][hyper]")
{
    const auto channelDifference = [] (float width)
    {
        auto settings = hyperSettings();
        settings.width = width;

        FxHyper hyper;
        hyper.prepare (kSampleRate);
        hyper.setSettings (settings);

        auto largest = 0.0f;

        for (int i = 0; i < 48000; ++i)
        {
            auto left = std::sin (static_cast<float> (i) * 0.05f) * 0.5f;
            auto right = left;

            hyper.processSample (left, right);

            largest = juce::jmax (largest, std::abs (left - right));
        }

        return largest;
    };

    const auto narrow = channelDifference (0.0f);
    const auto wide = channelDifference (1.0f);

    INFO ("channel difference: width 0 gives " << narrow
          << ", width 1 gives " << wide);

    // At width zero every voice is centred, so a mono input stays mono.
    CHECK (narrow == Approx (0.0f).margin (1.0e-5f));
    CHECK (wide > 0.05f);
}

TEST_CASE ("Hyper detune actually detunes", "[fx][hyper]")
{
    // The voices must sit at DIFFERENT offsets, which means the rates fan out.
    // With detune at zero they all sweep together and the effect is a vibrato.
    const auto movement = [] (float detune)
    {
        auto settings = hyperSettings();
        settings.detune = detune;

        FxHyper hyper;
        hyper.prepare (kSampleRate);
        hyper.setSettings (settings);

        // How much the output level wanders: coherent voices beat against each
        // other, so their sum pumps.
        auto lowest = 1.0e9f;
        auto highest = 0.0f;

        for (int block = 0; block < 60; ++block)
        {
            auto peak = 0.0f;

            for (int i = 0; i < 4800; ++i)
            {
                const auto t = static_cast<float> (block * 4800 + i)
                             / static_cast<float> (kSampleRate);
                auto left = 0.5f * std::sin (juce::MathConstants<float>::twoPi * 220.0f * t);
                auto right = left;

                hyper.processSample (left, right);

                if (block > 4)
                    peak = juce::jmax (peak, std::abs (left));
            }

            if (block > 4)
            {
                lowest = juce::jmin (lowest, peak);
                highest = juce::jmax (highest, peak);
            }
        }

        return highest / juce::jmax (1.0e-6f, lowest);
    };

    // Not a threshold on which is bigger - the claim is only that detune
    // changes the character, which is what the parameter is for.
    const auto none = movement (0.0f);
    const auto full = movement (1.0f);

    INFO ("level wander: detune 0 gives " << none << "x, detune 1 gives " << full << "x");
    CHECK (std::abs (full - none) > 0.02f);
}

// --- Dimension -------------------------------------------------------------

TEST_CASE ("Dimension widens", "[fx][dimension]")
{
    FxDimension dimension;
    dimension.prepare (kSampleRate);
    dimension.setSettings (dimensionSettings());

    auto largestDifference = 0.0f;

    for (int i = 0; i < 24000; ++i)
    {
        // A mono input: a widener's job is to make this not mono.
        auto left = std::sin (static_cast<float> (i) * 0.05f) * 0.5f;
        auto right = left;

        dimension.processSample (left, right);

        largestDifference = juce::jmax (largestDifference, std::abs (left - right));
    }

    CHECK (largestDifference > 0.05f);
}

TEST_CASE ("Dimension is mono-safe", "[fx][dimension]")
{
    /*  THE PROPERTY MOST WIDENERS DO NOT HAVE, and the reason this one is
        built from an inverted cross-feed rather than by boosting the side
        channel. Each delayed copy appears once positive and once negative
        across the two outputs, so summing to mono cancels them exactly and
        leaves the dry signal.

        This matters because for this music every club system and every phone
        speaker sums to mono, and a widener that loses its low end when summed
        is worse than no widener. */
    FxDimension dimension;
    dimension.prepare (kSampleRate);
    dimension.setSettings (dimensionSettings());

    auto largestError = 0.0f;

    for (int i = 0; i < 48000; ++i)
    {
        const auto input = std::sin (static_cast<float> (i) * 0.05f) * 0.4f
                         + std::sin (static_cast<float> (i) * 0.21f) * 0.3f;

        auto left = input;
        auto right = input;

        dimension.processSample (left, right);

        // The mono sum must be the dry signal back.
        const auto monoSum = (left + right) * 0.5f;

        if (i > 4800)
            largestError = juce::jmax (largestError, std::abs (monoSum - input));
    }

    INFO ("largest mono-sum error: " << largestError);
    CHECK (largestError < 1.0e-5f);
}

TEST_CASE ("Dimension at zero width is a no-op", "[fx][dimension]")
{
    auto settings = dimensionSettings();
    settings.width = 0.0f;

    FxDimension dimension;
    dimension.prepare (kSampleRate);
    dimension.setSettings (settings);

    for (int i = 0; i < 512; ++i)
    {
        auto left = std::sin (static_cast<float> (i) * 0.1f);
        auto right = std::cos (static_cast<float> (i) * 0.1f);
        const auto dryLeft = left;
        const auto dryRight = right;

        dimension.processSample (left, right);

        CHECK (left == dryLeft);
        CHECK (right == dryRight);
    }
}

// --- Shared robustness -----------------------------------------------------

TEST_CASE ("Output stays finite across sample rates and hostile settings",
           "[fx][modulation]")
{
    for (const auto sampleRate : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        INFO ("sample rate " << sampleRate);

        const auto hostileInput = [] (int i)
        {
            return (i % 64 < 32 ? 1.0f : -1.0f) + (i % 512 == 0 ? 4.0f : 0.0f);
        };

        SECTION ("chorus")
        {
            FxChorus effect;
            effect.prepare (sampleRate);

            auto settings = chorusSettings();
            settings.depth = 1.0f;
            settings.voices = FxChorus::kMaxVoices;
            settings.feedback = 1.0f;
            settings.rateHz = 20.0f;
            effect.setSettings (settings);

            for (int i = 0; i < 8192; ++i)
                for (int channel = 0; channel < 2; ++channel)
                    REQUIRE (std::isfinite (effect.processSample (channel, hostileInput (i))));
        }

        SECTION ("flanger")
        {
            FxFlanger effect;
            effect.prepare (sampleRate);

            auto settings = flangerSettings();
            settings.depth = 1.0f;
            settings.feedback = -1.0f;
            settings.manual = 1.0f;
            settings.rateHz = 20.0f;
            effect.setSettings (settings);

            for (int i = 0; i < 8192; ++i)
                for (int channel = 0; channel < 2; ++channel)
                    REQUIRE (std::isfinite (effect.processSample (channel, hostileInput (i))));
        }

        SECTION ("phaser")
        {
            FxPhaser effect;
            effect.prepare (sampleRate);

            auto settings = phaserSettings();
            settings.depth = 1.0f;
            settings.stages = FxPhaser::kMaxStages;
            settings.feedback = -1.0f;
            settings.centreHz = 20.0f;
            settings.rateHz = 20.0f;
            effect.setSettings (settings);

            for (int i = 0; i < 8192; ++i)
                for (int channel = 0; channel < 2; ++channel)
                    REQUIRE (std::isfinite (effect.processSample (channel, hostileInput (i))));
        }

        SECTION ("hyper")
        {
            FxHyper effect;
            effect.prepare (sampleRate);

            auto settings = hyperSettings();
            settings.amount = 1.0f;
            settings.detune = 1.0f;
            settings.voices = FxHyper::kMaxVoices;
            settings.width = 1.0f;
            effect.setSettings (settings);

            for (int i = 0; i < 8192; ++i)
            {
                auto left = hostileInput (i);
                auto right = -left;
                effect.processSample (left, right);
                REQUIRE (std::isfinite (left));
                REQUIRE (std::isfinite (right));
            }
        }

        SECTION ("dimension")
        {
            FxDimension effect;
            effect.prepare (sampleRate);

            auto settings = dimensionSettings();
            settings.amount = 1.0f;
            settings.width = 1.0f;
            settings.timeMs = 40.0f;
            effect.setSettings (settings);

            for (int i = 0; i < 8192; ++i)
            {
                auto left = hostileInput (i);
                auto right = -left;
                effect.processSample (left, right);
                REQUIRE (std::isfinite (left));
                REQUIRE (std::isfinite (right));
            }
        }
    }
}

TEST_CASE ("A parameter sweep produces no NaN", "[fx][modulation]")
{
    constexpr auto steps = 32;

    FxChorus chorus;
    FxFlanger flanger;
    FxPhaser phaser;
    FxHyper hyper;
    FxDimension dimension;

    chorus.prepare (kSampleRate);
    flanger.prepare (kSampleRate);
    phaser.prepare (kSampleRate);
    hyper.prepare (kSampleRate);
    dimension.prepare (kSampleRate);

    for (int step = 0; step < steps; ++step)
    {
        const auto t = static_cast<float> (step) / static_cast<float> (steps - 1);

        FxChorus::Settings c;
        c.enabled = true;
        c.mix = t;
        c.rateHz = juce::jmap (t, 0.01f, 20.0f);
        c.depth = t;
        c.voices = 1 + step % FxChorus::kMaxVoices;
        c.spread = t;
        c.feedback = t;
        chorus.setSettings (c);

        FxFlanger::Settings f;
        f.enabled = true;
        f.mix = t;
        f.rateHz = juce::jmap (t, 0.01f, 20.0f);
        f.depth = t;
        f.feedback = juce::jmap (t, -1.0f, 1.0f);
        f.manual = t;
        f.stereo = t;
        flanger.setSettings (f);

        FxPhaser::Settings p;
        p.enabled = true;
        p.mix = t;
        p.rateHz = juce::jmap (t, 0.01f, 20.0f);
        p.depth = t;
        p.stages = 2 + step % (FxPhaser::kMaxStages - 1);
        p.centreHz = juce::jmap (t, 20.0f, 20000.0f);
        p.feedback = juce::jmap (t, 1.0f, -1.0f);
        p.stereo = t;
        phaser.setSettings (p);

        FxHyper::Settings h;
        h.enabled = true;
        h.mix = t;
        h.amount = t;
        h.detune = t;
        h.voices = 2 + step % (FxHyper::kMaxVoices - 1);
        h.width = t;
        hyper.setSettings (h);

        FxDimension::Settings d;
        d.enabled = true;
        d.mix = t;
        d.amount = t;
        d.width = t;
        d.timeMs = juce::jmap (t, 1.0f, 40.0f);
        dimension.setSettings (d);

        for (int i = 0; i < 512; ++i)
        {
            const auto input = std::sin (static_cast<float> (i) * 0.1f) * 0.8f;

            INFO ("step " << step);

            for (int channel = 0; channel < 2; ++channel)
            {
                REQUIRE (std::isfinite (chorus.processSample (channel, input)));
                REQUIRE (std::isfinite (flanger.processSample (channel, input)));
                REQUIRE (std::isfinite (phaser.processSample (channel, input)));
            }

            auto left = input;
            auto right = -input;
            hyper.processSample (left, right);
            REQUIRE (std::isfinite (left));
            REQUIRE (std::isfinite (right));

            left = input;
            right = -input;
            dimension.processSample (left, right);
            REQUIRE (std::isfinite (left));
            REQUIRE (std::isfinite (right));
        }
    }
}
