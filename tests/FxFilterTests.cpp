#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/FxFilter.h"

#include <cmath>
#include <vector>

using namespace gnarl;
using namespace gnarl::dsp;
using Catch::Approx;
using Type = FxFilter::Type;

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
        return choices::fxFilterType[static_cast<int> (type)].toRawUTF8();
    }

    FxFilter::Settings settingsFor (Type type, float cutoffHz, float resonance = 0.0f)
    {
        FxFilter::Settings s;
        s.enabled = true;
        s.mix = 1.0f;
        s.type = type;
        s.cutoffHz = cutoffHz;
        s.resonance = resonance;
        s.drive = 0.0f;
        return s;
    }

    FxFilter make (const FxFilter::Settings& settings)
    {
        FxFilter filter;
        filter.prepare (kSampleRate);
        filter.setSettings (settings);
        return filter;
    }

    /** Steady-state gain, measured rather than derived from coefficients. */
    double gainAt (FxFilter& filter, double frequencyHz)
    {
        const auto increment = juce::MathConstants<double>::twoPi * frequencyHz / kSampleRate;
        const auto settle = static_cast<int> (kSampleRate * 0.5);
        const auto measure = static_cast<int> (kSampleRate * 0.5);

        auto phase = 0.0;

        for (int i = 0; i < settle; ++i, phase += increment)
            filter.processSample (0, static_cast<float> (std::sin (phase)));

        auto sumSquares = 0.0;

        for (int i = 0; i < measure; ++i, phase += increment)
        {
            const auto out = filter.processSample (0, static_cast<float> (std::sin (phase)));
            sumSquares += static_cast<double> (out) * static_cast<double> (out);
        }

        return std::sqrt (sumSquares / static_cast<double> (measure))
             * juce::MathConstants<double>::sqrt2;
    }

    double gainDbAt (FxFilter& filter, double frequencyHz)
    {
        return 20.0 * std::log10 (std::max (1.0e-12, gainAt (filter, frequencyHz)));
    }

    double rmsOf (FxFilter& filter, float amplitude, int channel = 0)
    {
        // A few harmonics rather than a sine, so a filter cannot pass the
        // level test by being transparent at one frequency.
        auto sumSquares = 0.0;
        constexpr auto samples = 48000;

        for (int i = 0; i < samples; ++i)
        {
            const auto t = static_cast<float> (i) / static_cast<float> (kSampleRate);
            const auto input = amplitude
                             * (std::sin (juce::MathConstants<float>::twoPi * 110.0f * t)
                                + 0.5f * std::sin (juce::MathConstants<float>::twoPi * 330.0f * t)
                                + 0.25f * std::sin (juce::MathConstants<float>::twoPi * 770.0f * t))
                             / 1.75f;

            const auto out = filter.processSample (channel, input);

            if (i > samples / 4)
                sumSquares += static_cast<double> (out) * static_cast<double> (out);
        }

        return std::sqrt (sumSquares / static_cast<double> (samples - samples / 4));
    }
}

TEST_CASE ("A disabled FX filter passes its input through", "[fx][fxfilter]")
{
    for (const auto type : allTypes())
    {
        auto settings = settingsFor (type, 500.0f, 0.8f);
        settings.enabled = false;
        settings.drive = 1.0f;

        auto filter = make (settings);

        INFO ("type " << nameOf (type));

        for (int i = 0; i < 256; ++i)
        {
            const auto input = std::sin (static_cast<float> (i) * 0.13f);
            CHECK (filter.processSample (0, input) == input);
        }
    }
}

TEST_CASE ("Mix at zero is bit-transparent", "[fx][fxfilter]")
{
    auto settings = settingsFor (Type::lowPass24, 200.0f, 0.9f);
    settings.mix = 0.0f;
    settings.drive = 0.7f;

    auto filter = make (settings);

    for (int i = 0; i < 512; ++i)
    {
        const auto input = std::sin (static_cast<float> (i) * 0.07f);
        CHECK (filter.processSample (0, input) == input);
    }
}

TEST_CASE ("Each type passes and stops the bands it claims", "[fx][fxfilter]")
{
    SECTION ("low-pass")
    {
        for (const auto type : { Type::lowPass12, Type::lowPass24 })
        {
            auto filter = make (settingsFor (type, 1000.0f));

            INFO ("type " << nameOf (type));

            /*  -6 dB at the corner, not the -3 dB a Butterworth would give.
                Resonance 0 maps to Q = 0.5 in StateVariableFilter, which is
                over-damped on purpose - the bottom of the range is meant to be
                the gentlest the filter gets. It is the VOICE filter's mapping,
                shared deliberately so the two controls feel like one, and
                changing it would change the voice filter's sound.

                The cascade is -9 dB rather than twice -6: only its SECOND
                section carries the resonance, so the corner is the flat
                Butterworth section's -3 dB plus the resonant section's -6. */
            const auto atCorner = type == Type::lowPass24 ? -9.0 : -6.0;

            CHECK (gainDbAt (filter, 100.0) == Approx (0.0).margin (0.5));
            CHECK (gainDbAt (filter, 1000.0) == Approx (atCorner).margin (0.8));
            CHECK (gainDbAt (filter, 8000.0) < -30.0);
        }
    }

    SECTION ("high-pass")
    {
        for (const auto type : { Type::highPass12, Type::highPass24 })
        {
            auto filter = make (settingsFor (type, 1000.0f));

            INFO ("type " << nameOf (type));

            // Same corner gains as the low-pass, for the same reasons.
            const auto atCorner = type == Type::highPass24 ? -9.0 : -6.0;

            CHECK (gainDbAt (filter, 8000.0) == Approx (0.0).margin (0.5));
            CHECK (gainDbAt (filter, 1000.0) == Approx (atCorner).margin (0.8));
            CHECK (gainDbAt (filter, 100.0) < -30.0);
        }
    }

    SECTION ("band-pass")
    {
        auto filter = make (settingsFor (Type::bandPass12, 1000.0f));

        /*  A band-pass rejects BOTH ends, and its peak is at the cutoff and is
            UNITY - the raw band-pass output has a peak gain of Q, so without
            the normalisation this measured 6 dB down at resonance 0 and would
            have arrived +26 dB hot at resonance 1. Resonance sets the WIDTH,
            not the level.

            The skirts are 6 dB per octave per side and the normalisation
            lifts them with the peak, so four octaves out is about -18 dB
            rather than the -24 the unnormalised filter gave. That is the
            trade for a level that does not jump. */
        CHECK (gainDbAt (filter, 1000.0) == Approx (0.0).margin (0.5));
        CHECK (gainDbAt (filter, 60.0) < -15.0);
        CHECK (gainDbAt (filter, 16000.0) < -15.0);
    }

    SECTION ("notch")
    {
        auto filter = make (settingsFor (Type::notch12, 1000.0f));

        // A notch rejects only the middle and passes both ends.
        CHECK (gainDbAt (filter, 1000.0) < -20.0);
        CHECK (gainDbAt (filter, 60.0) == Approx (0.0).margin (0.6));
        CHECK (gainDbAt (filter, 16000.0) == Approx (0.0).margin (0.6));
    }
}

TEST_CASE ("The 24 dB types are twice as steep as the 12 dB ones", "[fx][fxfilter]")
{
    /*  The point of having both. Measured an octave apart well inside the stop
        band, and well clear of Nyquist - a low-pass slope measured near
        Nyquist reads steeper than it is, because a bilinear response goes to
        -inf there rather than continuing at a fixed slope. */
    const auto slopeOf = [] (Type type)
    {
        auto filter = make (settingsFor (type, 200.0f));

        const auto near = gainDbAt (filter, 800.0);
        const auto far = gainDbAt (filter, 1600.0);

        return near - far;
    };

    const auto twelve = slopeOf (Type::lowPass12);
    const auto twentyFour = slopeOf (Type::lowPass24);

    INFO ("LP 12 " << twelve << " dB/octave, LP 24 " << twentyFour << " dB/octave");

    CHECK (twelve == Approx (12.0).margin (0.8));
    CHECK (twentyFour == Approx (24.0).margin (1.2));
}

TEST_CASE ("A 24 dB type is not louder than a 12 dB one at the same resonance",
           "[fx][fxfilter]")
{
    /*  THE BUG THIS EXISTS FOR, inherited from the voice filter. Giving both
        cascaded sections the user's Q multiplies their peaks into Q squared:
        the 24 dB modes measured SIX TIMES louder than the 12 dB ones at the
        same setting, so closing the filter made the patch louder instead of
        darker. Only the second section carries the resonance. */
    for (const auto resonance : { 0.5f, 0.9f, 1.0f })
    {
        auto twelve = make (settingsFor (Type::lowPass12, 800.0f, resonance));
        auto twentyFour = make (settingsFor (Type::lowPass24, 800.0f, resonance));

        const auto peakTwelve = gainDbAt (twelve, 800.0);
        const auto peakTwentyFour = gainDbAt (twentyFour, 800.0);

        INFO ("resonance " << resonance << ": LP 12 peaks at " << peakTwelve
              << " dB, LP 24 at " << peakTwentyFour << " dB");

        // The cascade's peak is allowed to differ - it is a different filter -
        // but not to be a Q-squared multiple of it. Q squared at resonance 0.9
        // would be about +15 dB of difference.
        CHECK (peakTwentyFour - peakTwelve < 4.0);
    }
}

TEST_CASE ("Drive does not double as a volume control", "[fx][fxfilter]")
{
    // The same property FxDistortion has, and the same reason: a drive that
    // changes the level is a level control the user will use as one, and then
    // the patch is loud rather than driven.
    auto quietSettings = settingsFor (Type::lowPass12, 20000.0f);
    quietSettings.drive = 0.0f;

    auto loudSettings = quietSettings;
    loudSettings.drive = 1.0f;

    auto quiet = make (quietSettings);
    auto loud = make (loudSettings);

    const auto undriven = rmsOf (quiet, 0.25f);
    const auto driven = rmsOf (loud, 0.25f);

    REQUIRE (undriven > 1.0e-4);
    REQUIRE (driven > 1.0e-4);

    const auto changeDb = 20.0 * std::log10 (driven / undriven);

    INFO ("drive 0 to 1 changes the level by " << changeDb << " dB");
    CHECK (std::abs (changeDb) < 6.0);
}

TEST_CASE ("Drive adds harmonics rather than just level", "[fx][fxfilter]")
{
    // The other half of the previous test: the compensation must not have been
    // achieved by doing nothing. A saturated sine has a lower crest factor
    // than a clean one, because its peaks are what got flattened.
    const auto crestFactorOf = [] (float drive)
    {
        auto settings = settingsFor (Type::lowPass12, 20000.0f);
        settings.drive = drive;

        auto filter = make (settings);

        auto peak = 0.0f;
        auto sumSquares = 0.0;
        constexpr auto samples = 8192;

        for (int i = 0; i < samples; ++i)
        {
            const auto out = filter.processSample (
                0, 0.5f * std::sin (static_cast<float> (i) * 0.05f));

            peak = juce::jmax (peak, std::abs (out));
            sumSquares += static_cast<double> (out) * static_cast<double> (out);
        }

        return static_cast<double> (peak)
             / std::sqrt (sumSquares / static_cast<double> (samples));
    };

    const auto clean = crestFactorOf (0.0f);
    const auto driven = crestFactorOf (1.0f);

    INFO ("crest factor " << clean << " clean, " << driven << " driven");

    // A sine's crest factor is root two.
    CHECK (clean == Approx (1.41).margin (0.05));
    CHECK (driven < 1.25);
}

TEST_CASE ("The two channels are independent of the block layout", "[fx][fxfilter]")
{
    /*  A STEREO PATH IS TWO PATHS (CLAUDE.md section 3). The voice filter
        shipped one FilterSlot run over the whole left channel and then over
        the whole right, which made the output a function of the BLOCK LAYOUT
        and gave a centred patch different left and right channels. This is the
        assertion that can see it. */
    for (const auto type : allTypes())
    {
        auto settings = settingsFor (type, 600.0f, 0.9f);
        settings.drive = 0.5f;

        auto interleaved = make (settings);
        auto alone = make (settings);

        INFO ("type " << nameOf (type));

        for (int i = 0; i < 2048; ++i)
        {
            const auto left = std::sin (static_cast<float> (i) * 0.03f);
            const auto right = std::sin (static_cast<float> (i) * 0.21f) * 0.4f;

            const auto both = interleaved.processSample (0, left);
            interleaved.processSample (1, right);

            REQUIRE (both == alone.processSample (0, left));
        }
    }
}

TEST_CASE ("Stable at extreme resonance", "[fx][fxfilter]")
{
    // The project's standing filter test: resonance at the top of its range
    // must not self-oscillate, because a preset load would leave it screaming.
    for (const auto type : allTypes())
    {
        auto filter = make (settingsFor (type, 1000.0f, 1.0f));

        INFO ("type " << nameOf (type));

        // An impulse, then silence: a stable filter rings down.
        filter.processSample (0, 1.0f);

        auto early = 0.0f;
        auto late = 0.0f;

        for (int i = 0; i < 48000; ++i)
        {
            const auto out = std::abs (filter.processSample (0, 0.0f));

            if (i < 2400)
                early = juce::jmax (early, out);
            else if (i > 24000)
                late = juce::jmax (late, out);
        }

        INFO ("ring-down: early peak " << early << ", late peak " << late);
        CHECK (late < early);
    }
}

TEST_CASE ("Cutoff swept every sample stays stable", "[fx][fxfilter]")
{
    /*  Every filter here is zero-delay-feedback precisely so that this is
        legal: a bilinear biquad recalculated per sample is not stable, and
        audio-rate filter modulation is most of what makes a growl. The FX
        filter is a mod destination, so it gets the same test the voice filter
        has. */
    for (const auto type : allTypes())
    {
        auto settings = settingsFor (type, 1000.0f, 0.9f);
        auto filter = make (settings);

        INFO ("type " << nameOf (type));

        for (int i = 0; i < 48000; ++i)
        {
            // Full range, every single sample.
            const auto t = 0.5f + 0.5f * std::sin (static_cast<float> (i) * 0.01f);
            settings.cutoffHz = juce::jmap (t, 20.0f, 20000.0f);
            filter.setSettings (settings);

            const auto out = filter.processSample (
                0, std::sin (static_cast<float> (i) * 0.07f) * 0.8f);

            REQUIRE (std::isfinite (out));
        }
    }
}

TEST_CASE ("Output stays finite across sample rates and hostile settings",
           "[fx][fxfilter]")
{
    for (const auto sampleRate : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        for (const auto type : allTypes())
        {
            FxFilter filter;
            filter.prepare (sampleRate);

            auto settings = settingsFor (type, 20.0f, 1.0f);
            settings.drive = 1.0f;
            filter.setSettings (settings);

            INFO ("sample rate " << sampleRate << ", type " << nameOf (type));

            for (int i = 0; i < 4096; ++i)
            {
                // Full-scale square plus an impulse train.
                const auto input = (i % 64 < 32 ? 1.0f : -1.0f)
                                 + (i % 512 == 0 ? 4.0f : 0.0f);

                for (int channel = 0; channel < 2; ++channel)
                    REQUIRE (std::isfinite (filter.processSample (channel, input)));
            }
        }
    }
}

TEST_CASE ("A parameter sweep produces no NaN", "[fx][fxfilter]")
{
    FxFilter filter;
    filter.prepare (kSampleRate);

    constexpr auto steps = 40;

    for (const auto type : allTypes())
    {
        for (int step = 0; step < steps; ++step)
        {
            const auto t = static_cast<float> (step) / static_cast<float> (steps - 1);

            FxFilter::Settings settings;
            settings.enabled = true;
            settings.type = type;
            settings.mix = t;
            settings.cutoffHz = juce::jmap (t, 20.0f, 20000.0f);
            settings.resonance = t;
            settings.drive = 1.0f - t;

            filter.setSettings (settings);

            for (int i = 0; i < 256; ++i)
                for (int channel = 0; channel < 2; ++channel)
                {
                    const auto out = filter.processSample (
                        channel, std::sin (static_cast<float> (i) * 0.1f) * 0.9f);

                    INFO ("type " << nameOf (type) << " step " << step);
                    REQUIRE (std::isfinite (out));
                }
        }
    }
}
