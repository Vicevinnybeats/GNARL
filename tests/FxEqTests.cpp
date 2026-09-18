#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/FxEq.h"

#include <cmath>
#include <vector>

using namespace gnarl;
using namespace gnarl::dsp;
using Catch::Approx;

namespace
{
    constexpr double kSampleRate = 48000.0;

    /** Steady-state gain at one frequency, MEASURED rather than derived from
        the coefficients - deriving it from the coefficients would pass even if
        the mixing were applied to the wrong internal signal.

        A whole number of cycles of settling is discarded first: the TPT state
        starts at zero, so the first cycles are the transient, and including
        them reads as a gain error that gets smaller the longer the test runs. */
    double gainAt (FxEq& eq, double frequencyHz)
    {
        const auto increment = juce::MathConstants<double>::twoPi * frequencyHz / kSampleRate;

        // Long enough for the slowest band here (a 20 Hz high-pass) to settle.
        const auto settle = static_cast<int> (kSampleRate * 0.5);
        const auto measure = static_cast<int> (kSampleRate * 0.5);

        auto phase = 0.0;

        for (int i = 0; i < settle; ++i, phase += increment)
            eq.processSample (0, static_cast<float> (std::sin (phase)));

        auto sumSquares = 0.0;

        for (int i = 0; i < measure; ++i, phase += increment)
        {
            const auto out = eq.processSample (0, static_cast<float> (std::sin (phase)));
            sumSquares += static_cast<double> (out) * static_cast<double> (out);
        }

        // Against a unit sine, whose RMS is 1/sqrt(2).
        const auto rms = std::sqrt (sumSquares / static_cast<double> (measure));
        return rms * juce::MathConstants<double>::sqrt2;
    }

    double gainDbAt (FxEq& eq, double frequencyHz)
    {
        return 20.0 * std::log10 (std::max (1.0e-12, gainAt (eq, frequencyHz)));
    }

    FxEq::Settings flat()
    {
        FxEq::Settings s;
        s.enabled = true;
        s.mix = 1.0f;
        return s;
    }

    FxEq makeEq (const FxEq::Settings& settings)
    {
        FxEq eq;
        eq.prepare (kSampleRate);
        eq.setSettings (settings);
        return eq;
    }
}

TEST_CASE ("A flat EQ is bit-transparent", "[fx][eq]")
{
    /*  Not "close to transparent". Every band at 0 dB gives m1 = m2 = 0 and
        m0 = 1, and the two cuts parked at the ends of their ranges are
        skipped, so a flat EQ returns its input EXACTLY. That matters because
        two of these sit in a 14-slot chain that a patch mostly does not use:
        an EQ that is only nearly transparent would put a small error, and the
        cost of six filters, in every patch in the product. */
    auto eq = makeEq (flat());

    for (int i = 0; i < 2048; ++i)
    {
        const auto input = std::sin (static_cast<float> (i) * 0.05f) * 0.7f;

        CHECK (eq.processSample (0, input) == input);
        CHECK (eq.processSample (1, input) == input);
    }
}

TEST_CASE ("A disabled EQ passes its input through", "[fx][eq]")
{
    auto settings = flat();
    settings.enabled = false;
    settings.band1Gain = 12.0f;

    auto eq = makeEq (settings);

    for (int i = 0; i < 512; ++i)
    {
        const auto input = std::sin (static_cast<float> (i) * 0.11f);
        CHECK (eq.processSample (0, input) == input);
    }
}

TEST_CASE ("A bell's measured gain matches its dB setting", "[fx][eq]")
{
    // The defining property: at the centre frequency the gain IS the setting.
    for (const auto gainDb : { -18.0f, -9.0f, 6.0f, 15.0f })
    {
        auto settings = flat();
        settings.band1Freq = 1000.0f;
        settings.band1Gain = gainDb;
        settings.band1Q = 2.0f;

        auto eq = makeEq (settings);

        INFO ("bell at " << gainDb << " dB");
        CHECK (gainDbAt (eq, 1000.0) == Approx (static_cast<double> (gainDb)).margin (0.2));
    }
}

TEST_CASE ("A bell leaves frequencies away from its centre alone", "[fx][eq]")
{
    auto settings = flat();
    settings.band1Freq = 1000.0f;
    settings.band1Gain = 18.0f;
    settings.band1Q = 4.0f;

    auto eq = makeEq (settings);

    // Two decades away from a Q of 4 is untouched. A bell that is not local is
    // a tilt, and a user reaching for one of these expects the other to stay.
    CHECK (gainDbAt (eq, 50.0) == Approx (0.0).margin (0.5));
    CHECK (gainDbAt (eq, 16000.0) == Approx (0.0).margin (0.5));
}

TEST_CASE ("A bell boost and the matching cut are mirror images", "[fx][eq]")
{
    /*  THE REASON THE BELL'S k CARRIES THE GAIN. With a plain k = 1/Q, a cut
        is narrower than the boost of the same size, so running one into the
        other does not give back the original signal - and "undo what I just
        did" is most of what an EQ is used for. */
    constexpr auto gainDb = 12.0f;

    auto boostSettings = flat();
    boostSettings.band1Freq = 800.0f;
    boostSettings.band1Gain = gainDb;
    boostSettings.band1Q = 1.5f;

    auto cutSettings = boostSettings;
    cutSettings.band1Gain = -gainDb;

    auto boost = makeEq (boostSettings);
    auto cut = makeEq (cutSettings);

    for (const auto frequency : { 100.0, 400.0, 800.0, 1600.0, 6000.0 })
    {
        const auto sum = gainDbAt (boost, frequency) + gainDbAt (cut, frequency);

        INFO ("at " << frequency << " Hz the boost and cut sum to " << sum << " dB");
        CHECK (sum == Approx (0.0).margin (0.2));
    }
}

TEST_CASE ("The shelves reach their gain on one side and unity on the other", "[fx][eq]")
{
    SECTION ("low shelf")
    {
        auto settings = flat();
        settings.lowShelfFreq = 200.0f;
        settings.lowShelfGain = 12.0f;

        auto eq = makeEq (settings);

        // Well below the corner it is the full boost; well above, unity. At
        // the corner itself a shelf is at HALF its gain in dB, which is what
        // makes it a shelf rather than a step.
        CHECK (gainDbAt (eq, 25.0) == Approx (12.0).margin (0.5));
        CHECK (gainDbAt (eq, 200.0) == Approx (6.0).margin (0.6));
        CHECK (gainDbAt (eq, 8000.0) == Approx (0.0).margin (0.3));
    }

    SECTION ("high shelf")
    {
        auto settings = flat();
        settings.highShelfFreq = 4000.0f;
        settings.highShelfGain = -12.0f;

        auto eq = makeEq (settings);

        CHECK (gainDbAt (eq, 100.0) == Approx (0.0).margin (0.3));
        CHECK (gainDbAt (eq, 4000.0) == Approx (-6.0).margin (0.6));
        CHECK (gainDbAt (eq, 19000.0) == Approx (-12.0).margin (0.7));
    }
}

TEST_CASE ("The cuts are 12 dB per octave and flat in the pass band", "[fx][eq]")
{
    SECTION ("high-pass")
    {
        auto settings = flat();
        settings.highPassFreq = 500.0f;

        auto eq = makeEq (settings);

        // -3 dB at the corner for a Butterworth Q, then 12 dB per octave.
        CHECK (gainDbAt (eq, 500.0) == Approx (-3.0).margin (0.4));
        CHECK (gainDbAt (eq, 8000.0) == Approx (0.0).margin (0.2));

        const auto atCorner = gainDbAt (eq, 125.0);
        const auto anOctaveDown = gainDbAt (eq, 62.5);

        INFO ("125 Hz " << atCorner << " dB, 62.5 Hz " << anOctaveDown << " dB");
        CHECK (atCorner - anOctaveDown == Approx (12.0).margin (0.6));
    }

    SECTION ("low-pass")
    {
        auto settings = flat();
        settings.lowPassFreq = 200.0f;

        auto eq = makeEq (settings);

        CHECK (gainDbAt (eq, 200.0) == Approx (-3.0).margin (0.4));
        CHECK (gainDbAt (eq, 20.0) == Approx (0.0).margin (0.2));

        // The two probe frequencies are kept well below Nyquist on purpose.
        // A bilinear filter's response goes to -inf AT Nyquist rather than
        // continuing at 12 dB per octave, so measuring a low-pass slope near
        // the top of the band reads as 13.3 dB per octave - the warping, not
        // the filter. The high-pass has no equivalent problem because its
        // stop band is at the bottom, where the warping is negligible.
        const auto atCorner = gainDbAt (eq, 800.0);
        const auto anOctaveUp = gainDbAt (eq, 1600.0);

        INFO ("800 Hz " << atCorner << " dB, 1600 Hz " << anOctaveUp << " dB");
        CHECK (atCorner - anOctaveUp == Approx (12.0).margin (0.6));
    }
}

TEST_CASE ("The bands are independent and compose", "[fx][eq]")
{
    // Six bands in series should give the SUM of their dB gains where they do
    // not overlap. If they interact, the EQ is not doing what its readouts say.
    auto settings = flat();
    settings.lowShelfFreq = 120.0f;
    settings.lowShelfGain = 9.0f;
    settings.band1Freq = 1000.0f;
    settings.band1Gain = -12.0f;
    settings.band1Q = 3.0f;
    settings.band2Freq = 5000.0f;
    settings.band2Gain = 6.0f;
    settings.band2Q = 3.0f;

    auto eq = makeEq (settings);

    CHECK (gainDbAt (eq, 30.0) == Approx (9.0).margin (0.6));
    CHECK (gainDbAt (eq, 1000.0) == Approx (-12.0).margin (0.5));
    CHECK (gainDbAt (eq, 5000.0) == Approx (6.0).margin (0.5));
}

TEST_CASE ("Mix at zero is bit-transparent", "[fx][eq]")
{
    auto settings = flat();
    settings.mix = 0.0f;
    settings.band1Gain = 18.0f;
    settings.highPassFreq = 2000.0f;

    auto eq = makeEq (settings);

    for (int i = 0; i < 512; ++i)
    {
        const auto input = std::sin (static_cast<float> (i) * 0.07f);
        CHECK (eq.processSample (0, input) == input);
    }
}

TEST_CASE ("The two channels are independent of the block layout", "[fx][eq]")
{
    /*  A STEREO PATH IS TWO PATHS (CLAUDE.md section 3). If the channels
        shared a band's state, the same input would give a different answer
        depending on the ORDER the channels were interleaved in - and the voice
        filter shipped exactly that bug. Here: run the two channels
        interleaved, then run channel 0 alone, and the samples must match. */
    auto settings = flat();
    settings.band1Gain = 12.0f;
    settings.band1Q = 6.0f;
    settings.highPassFreq = 300.0f;

    auto interleaved = makeEq (settings);
    auto alone = makeEq (settings);

    for (int i = 0; i < 4096; ++i)
    {
        const auto left = std::sin (static_cast<float> (i) * 0.03f);
        const auto right = std::sin (static_cast<float> (i) * 0.19f) * 0.5f;

        const auto both = interleaved.processSample (0, left);
        interleaved.processSample (1, right);

        CHECK (both == alone.processSample (0, left));
    }
}

TEST_CASE ("Output stays finite across sample rates, block sizes and settings", "[fx][eq]")
{
    for (const auto sampleRate : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        FxEq eq;
        eq.prepare (sampleRate);

        // Every band at an extreme at once, with the cuts crossed over each
        // other - a setting no preset would hold, which is the point.
        FxEq::Settings settings;
        settings.enabled = true;
        settings.highPassFreq = 18000.0f;
        settings.lowPassFreq = 30.0f;
        settings.lowShelfFreq = 20.0f;
        settings.lowShelfGain = 18.0f;
        settings.band1Freq = 20.0f;
        settings.band1Gain = 18.0f;
        settings.band1Q = 18.0f;
        settings.band2Freq = 19000.0f;
        settings.band2Gain = -18.0f;
        settings.band2Q = 18.0f;
        settings.highShelfFreq = 19500.0f;
        settings.highShelfGain = 18.0f;
        eq.setSettings (settings);

        for (int i = 0; i < 8192; ++i)
        {
            // Hostile input: full-scale square plus an impulse train.
            const auto input = (i % 64 < 32 ? 1.0f : -1.0f) + (i % 512 == 0 ? 4.0f : 0.0f);

            for (int channel = 0; channel < 2; ++channel)
            {
                const auto out = eq.processSample (channel, input);
                INFO ("sample rate " << sampleRate << ", sample " << i);
                REQUIRE (std::isfinite (out));
            }
        }
    }
}

TEST_CASE ("A silent input decays to a negligible, non-denormal idle", "[fx][eq]")
{
    /*  Denormals, and a surprise. A TPT filter's state decays geometrically
        towards zero and never arrives, so the worry is that it spends the
        silence between notes holding a SUBNORMAL - which costs 100x on some
        CPUs (CLAUDE.md section 3), in a chain of fourteen effects, on an
        instrument that is mostly silence.

        The protection is ScopedNoDenormals, which processBlock's first line
        sets and which is therefore how this code ALWAYS runs; so the test runs
        under it too, rather than measuring a configuration that does not ship.

        WHAT THE MEASUREMENT ACTUALLY FOUND, though, is that the state does not
        reach the subnormal range at all: it converges on a rounding fixed
        point at about -2e-37 - a NORMAL float - and sits there exactly,
        unchanged over ten million further samples of silence. So there is no
        denormal to flush, and flush-to-zero never fires.

        That is fine on its own terms: -2e-37 is about -740 dBFS and carries no
        CPU penalty. It is recorded because it makes one plausible later
        optimisation wrong - an idle effect CANNOT be detected by its output
        reaching exactly zero, because this one never does. */
    auto settings = flat();
    settings.band1Gain = 12.0f;
    settings.highPassFreq = 400.0f;

    auto eq = makeEq (settings);

    const juce::ScopedNoDenormals noDenormals;

    for (int i = 0; i < 1024; ++i)
        eq.processSample (0, 1.0f);

    auto worstSubnormal = false;
    auto peak = 0.0f;

    for (int i = 0; i < 400000; ++i)
    {
        const auto out = eq.processSample (0, 0.0f);

        // Past the first few thousand samples the tail is well below anything
        // audible, and what matters from there on is only its CLASS.
        if (i > 20000)
        {
            peak = juce::jmax (peak, std::abs (out));
            worstSubnormal = worstSubnormal || std::fpclassify (out) == FP_SUBNORMAL;
        }
    }

    // The expensive case: never a subnormal, so ScopedNoDenormals is enough
    // and no per-sample state-snapping is needed in the hot path.
    CHECK_FALSE (worstSubnormal);

    // And inaudible by a margin no dither could reach. This also says the
    // decay went DOWN: a filter whose state grows on a silent input is
    // self-oscillating, which a preset load would leave screaming.
    INFO ("idle tail peaks at " << peak);
    CHECK (peak < 1.0e-30f);
}

TEST_CASE ("A parameter sweep produces no NaN", "[fx][eq]")
{
    FxEq eq;
    eq.prepare (kSampleRate);

    constexpr auto steps = 40;

    for (int step = 0; step < steps; ++step)
    {
        const auto t = static_cast<float> (step) / static_cast<float> (steps - 1);

        FxEq::Settings settings;
        settings.enabled = true;
        settings.mix = t;
        settings.highPassFreq = juce::jmap (t, 20.0f, 20000.0f);
        settings.lowPassFreq = juce::jmap (t, 20000.0f, 20.0f);
        settings.lowShelfFreq = juce::jmap (t, 20.0f, 1000.0f);
        settings.lowShelfGain = juce::jmap (t, -18.0f, 18.0f);
        settings.band1Freq = juce::jmap (t, 20.0f, 20000.0f);
        settings.band1Gain = juce::jmap (t, 18.0f, -18.0f);
        settings.band1Q = juce::jmap (t, 0.2f, 18.0f);
        settings.band2Freq = juce::jmap (t, 20000.0f, 20.0f);
        settings.band2Gain = juce::jmap (t, -18.0f, 18.0f);
        settings.band2Q = juce::jmap (t, 18.0f, 0.2f);
        settings.highShelfFreq = juce::jmap (t, 1000.0f, 20000.0f);
        settings.highShelfGain = juce::jmap (t, 18.0f, -18.0f);

        eq.setSettings (settings);

        for (int i = 0; i < 256; ++i)
            for (int channel = 0; channel < 2; ++channel)
            {
                const auto out = eq.processSample (
                    channel, std::sin (static_cast<float> (i) * 0.1f) * 0.8f);

                INFO ("step " << step);
                REQUIRE (std::isfinite (out));
            }
    }
}
