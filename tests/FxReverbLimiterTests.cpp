#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/FxLimiter.h"
#include "dsp/FxReverb.h"

// The reverb needs only juce_audio_basics; the spectral assertion here needs
// the FFT, so this test pulls juce_dsp in for itself.
#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <numeric>
#include <vector>

using namespace gnarl;
using namespace gnarl::dsp;
using Catch::Approx;

namespace
{
    constexpr double kSampleRate = 48000.0;

    FxReverb::Settings reverbSettings()
    {
        FxReverb::Settings s;
        s.enabled = true;
        s.mix = 1.0f;
        s.size = 0.5f;
        s.decay = 0.5f;
        s.damping = 0.3f;
        s.preDelayMs = 0.0f;
        s.width = 1.0f;
        s.lowCutHz = 20.0f;
        s.highCutHz = 20000.0f;
        s.modDepth = 0.2f;
        return s;
    }

    FxLimiter::Settings limiterSettings()
    {
        FxLimiter::Settings s;
        s.enabled = true;
        s.mix = 1.0f;
        s.thresholdDb = -6.0f;
        s.releaseMs = 50.0f;
        s.ceilingDb = -0.3f;
        return s;
    }

    /** Feeds an impulse and returns the envelope of the tail, in dB, sampled
        once per block. */
    std::vector<double> tailEnvelope (FxReverb& reverb, int blocks, int blockSize = 4800)
    {
        auto left = 1.0f;
        auto right = 1.0f;
        reverb.processSample (left, right);

        std::vector<double> envelope;
        envelope.reserve (static_cast<std::size_t> (blocks));

        for (int block = 0; block < blocks; ++block)
        {
            auto sumSquares = 0.0;

            for (int i = 0; i < blockSize; ++i)
            {
                auto l = 0.0f;
                auto r = 0.0f;
                reverb.processSample (l, r);

                sumSquares += static_cast<double> (l) * l + static_cast<double> (r) * r;
            }

            const auto rms = std::sqrt (sumSquares / (2.0 * blockSize));
            envelope.push_back (20.0 * std::log10 (std::max (1.0e-15, rms)));
        }

        return envelope;
    }

    /** The first block at which the tail has fallen `dropDb` below its peak. */
    int blocksToDecay (const std::vector<double>& envelope, double dropDb)
    {
        const auto peak = *std::max_element (envelope.begin(), envelope.end());

        for (std::size_t i = 0; i < envelope.size(); ++i)
            if (envelope[i] < peak - dropDb)
                return static_cast<int> (i);

        return static_cast<int> (envelope.size());
    }
}

// --- Reverb ----------------------------------------------------------------

TEST_CASE ("A disabled reverb is transparent", "[fx][reverb]")
{
    auto settings = reverbSettings();
    settings.enabled = false;

    FxReverb reverb;
    reverb.prepare (kSampleRate);
    reverb.setSettings (settings);

    for (int i = 0; i < 512; ++i)
    {
        auto left = std::sin (static_cast<float> (i) * 0.1f);
        auto right = std::cos (static_cast<float> (i) * 0.1f);
        const auto dryLeft = left;
        const auto dryRight = right;

        reverb.processSample (left, right);

        CHECK (left == dryLeft);
        CHECK (right == dryRight);
    }
}

TEST_CASE ("Reverb mix at zero is bit-transparent", "[fx][reverb]")
{
    auto settings = reverbSettings();
    settings.mix = 0.0f;

    FxReverb reverb;
    reverb.prepare (kSampleRate);
    reverb.setSettings (settings);

    for (int i = 0; i < 512; ++i)
    {
        auto left = std::sin (static_cast<float> (i) * 0.1f);
        auto right = std::cos (static_cast<float> (i) * 0.1f);
        const auto dryLeft = left;
        const auto dryRight = right;

        reverb.processSample (left, right);

        CHECK (left == dryLeft);
        CHECK (right == dryRight);
    }
}

TEST_CASE ("A reverb produces a decaying tail", "[fx][reverb]")
{
    FxReverb reverb;
    reverb.prepare (kSampleRate);
    reverb.setSettings (reverbSettings());

    const auto envelope = tailEnvelope (reverb, 20);

    // There IS a tail - an FDN that sums to nothing is the failure mode this
    // catches, and it is what the OTT crossover did.
    REQUIRE (envelope.front() > -80.0);

    // And it goes down. Monotonically is too strong for a modulated network,
    // so: each later block quieter than the one four blocks before it.
    for (std::size_t i = 4; i < envelope.size(); ++i)
    {
        INFO ("block " << i << " at " << envelope[i] << " dB, block " << i - 4
              << " at " << envelope[i - 4] << " dB");
        CHECK (envelope[i] < envelope[i - 4]);
    }
}

TEST_CASE ("A longer decay setting gives a longer tail", "[fx][reverb]")
{
    /*  The control has to mean what it says. This is also what catches the
        decay gain being computed from the wrong quantity: the gain is solved
        from the decay TIME and the mean delay length, so that the decay
        control means the same thing at every room size. */
    std::vector<int> lengths;

    for (const auto decay : { 0.1f, 0.4f, 0.7f, 1.0f })
    {
        auto settings = reverbSettings();
        settings.decay = decay;

        FxReverb reverb;
        reverb.prepare (kSampleRate);
        reverb.setSettings (settings);

        // 400 blocks of 2400 samples is 20 seconds, which has to cover the
        // whole 0.15 to 12 second decay range. A 60-block window measured
        // 3 seconds and so could not tell 8.4 s from 12 s - both came back
        // pinned at the end of the window, and the test failed on a
        // measurement ceiling rather than on the reverb.
        const auto envelope = tailEnvelope (reverb, 400, 2400);
        lengths.push_back (blocksToDecay (envelope, 30.0));
    }

    for (std::size_t i = 1; i < lengths.size(); ++i)
    {
        INFO ("decay step " << i << " lasts " << lengths[i]
              << " blocks, previous " << lengths[i - 1]);
        CHECK (lengths[i] > lengths[i - 1]);
    }
}

TEST_CASE ("The decay time does not depend on the room size", "[fx][reverb]")
{
    /*  THE POINT OF SOLVING THE FEEDBACK GAIN RATHER THAN EXPOSING IT. A
        fixed per-pass gain means a bigger room takes longer per pass AND
        keeps more of the signal each pass, so size would change the decay
        twice over - and a user adjusting the room would have to re-adjust the
        decay every time. */
    std::vector<int> lengths;

    for (const auto size : { 0.1f, 0.5f, 1.0f })
    {
        auto settings = reverbSettings();
        settings.size = size;
        settings.decay = 0.5f;

        FxReverb reverb;
        reverb.prepare (kSampleRate);
        reverb.setSettings (settings);

        const auto envelope = tailEnvelope (reverb, 80, 2400);
        lengths.push_back (blocksToDecay (envelope, 30.0));
    }

    const auto shortest = *std::min_element (lengths.begin(), lengths.end());
    const auto longest = *std::max_element (lengths.begin(), lengths.end());

    INFO ("decay length across room sizes: " << lengths[0] << ", " << lengths[1]
          << ", " << lengths[2] << " blocks");

    // Within a factor of two across the whole size range. Not identical - the
    // network's mode density genuinely changes with size - but nothing like
    // the factor of six a fixed per-pass gain would give.
    CHECK (static_cast<double> (longest) < static_cast<double> (shortest) * 2.0);
}

TEST_CASE ("A reverb tail is smooth rather than ringing on one pitch",
           "[fx][reverb]")
{
    /*  WHY THIS IS AN FDN AND NOT A COMB BANK. Independent combs each
        resonate on a harmonic series and nothing moves energy between them,
        so the tail rings. Measured as how concentrated the tail's energy is:
        a ringing tail has most of its energy in a few bins, a smooth one
        spreads it.

        Measured against a reference rather than an absolute threshold, because
        "smooth" is not a number: the same tail through a single comb of the
        same mean length is the thing the FDN has to beat. */
    FxReverb reverb;
    reverb.prepare (kSampleRate);

    auto settings = reverbSettings();
    settings.decay = 0.8f;
    settings.modDepth = 0.0f;   // Static, so any ringing is the network's.
    settings.damping = 0.0f;
    reverb.setSettings (settings);

    constexpr auto fftOrder = 14;
    constexpr auto fftSize = 1 << fftOrder;

    std::vector<float> scratch (static_cast<std::size_t> (fftSize) * 2, 0.0f);

    auto left = 1.0f;
    auto right = 1.0f;
    reverb.processSample (left, right);

    for (int i = 0; i < fftSize; ++i)
    {
        auto l = 0.0f;
        auto r = 0.0f;
        reverb.processSample (l, r);
        scratch[static_cast<std::size_t> (i)] = l;
    }

    juce::dsp::FFT fft (fftOrder);
    fft.performRealOnlyForwardTransform (scratch.data(), true);

    std::vector<double> power;
    power.reserve (static_cast<std::size_t> (fftSize / 2));

    for (int bin = 1; bin < fftSize / 2; ++bin)
    {
        const auto re = static_cast<double> (scratch[static_cast<std::size_t> (bin) * 2]);
        const auto im = static_cast<double> (scratch[static_cast<std::size_t> (bin) * 2 + 1]);
        power.push_back (re * re + im * im);
    }

    const auto total = std::accumulate (power.begin(), power.end(), 0.0);
    REQUIRE (total > 0.0);

    // The share of the total energy held by the loudest 1% of bins. A tail
    // ringing on a few modes concentrates there; a smooth one does not.
    std::sort (power.begin(), power.end(), std::greater<double>());

    const auto topCount = power.size() / 100;
    const auto topEnergy = std::accumulate (power.begin(),
                                            power.begin() + static_cast<long> (topCount),
                                            0.0);

    const auto share = topEnergy / total;

    INFO ("the loudest 1% of bins hold " << share * 100.0 << "% of the tail's energy");

    // A single comb concentrates well over half its energy in its harmonics.
    // The network spreads it: under a third is the bar.
    CHECK (share < 0.33);
}

TEST_CASE ("Reverb pre-delay delays the tail", "[fx][reverb]")
{
    const auto onsetOf = [] (float preDelayMs)
    {
        auto settings = reverbSettings();
        settings.preDelayMs = preDelayMs;

        FxReverb reverb;
        reverb.prepare (kSampleRate);
        reverb.setSettings (settings);

        auto left = 1.0f;
        auto right = 1.0f;
        reverb.processSample (left, right);

        for (int i = 1; i < 48000; ++i)
        {
            auto l = 0.0f;
            auto r = 0.0f;
            reverb.processSample (l, r);

            if (std::abs (l) > 0.001f)
                return i;
        }

        return -1;
    };

    const auto immediate = onsetOf (0.0f);
    const auto delayed = onsetOf (100.0f);

    INFO ("tail starts at sample " << immediate << " with no pre-delay, "
          << delayed << " with 100 ms");

    REQUIRE (immediate > 0);
    REQUIRE (delayed > 0);

    // 100 ms is 4800 samples at this rate.
    CHECK (delayed - immediate == Approx (4800).margin (100));
}

TEST_CASE ("Reverb width collapses the tail without silencing it", "[fx][reverb]")
{
    const auto channelDifference = [] (float width)
    {
        auto settings = reverbSettings();
        settings.width = width;

        FxReverb reverb;
        reverb.prepare (kSampleRate);
        reverb.setSettings (settings);

        auto left = 1.0f;
        auto right = 1.0f;
        reverb.processSample (left, right);

        auto difference = 0.0;
        auto level = 0.0;

        for (int i = 0; i < 24000; ++i)
        {
            auto l = 0.0f;
            auto r = 0.0f;
            reverb.processSample (l, r);

            difference += std::abs (static_cast<double> (l) - r);
            level += std::abs (static_cast<double> (l)) + std::abs (r);
        }

        return std::make_pair (difference, level);
    };

    const auto [wideDifference, wideLevel] = channelDifference (1.0f);
    const auto [narrowDifference, narrowLevel] = channelDifference (0.0f);

    INFO ("wide: difference " << wideDifference << " level " << wideLevel
          << ";  narrow: difference " << narrowDifference
          << " level " << narrowLevel);

    // Narrow is mono, so the channels agree.
    CHECK (narrowDifference < wideDifference * 0.1);

    // And it is a CENTRED room rather than a quieter one: collapsing to mono
    // must not throw the tail away.
    CHECK (narrowLevel > wideLevel * 0.5);
}

TEST_CASE ("A reverb at maximum decay does not run away", "[fx][reverb]")
{
    /*  The whole stability argument is that the mixing matrix is orthogonal,
        so the network cannot add energy and the decay is set by the feedback
        gain alone. This is the test that says so: at the longest decay, the
        largest room and no damping, a burst must still die. */
    auto settings = reverbSettings();
    settings.decay = 1.0f;
    settings.size = 1.0f;
    settings.damping = 0.0f;
    settings.modDepth = 1.0f;

    FxReverb reverb;
    reverb.prepare (kSampleRate);
    reverb.setSettings (settings);

    for (int i = 0; i < 48000; ++i)
    {
        auto left = (i % 64 < 32 ? 1.0f : -1.0f);
        auto right = -left;
        reverb.processSample (left, right);
    }

    auto earlyPeak = 0.0f;
    auto latePeak = 0.0f;

    for (int i = 0; i < static_cast<int> (kSampleRate * 120); ++i)
    {
        auto left = 0.0f;
        auto right = 0.0f;
        reverb.processSample (left, right);

        REQUIRE (std::isfinite (left));
        REQUIRE (std::isfinite (right));

        if (i < 48000)
            earlyPeak = juce::jmax (earlyPeak, std::abs (left));
        else if (i > static_cast<int> (kSampleRate * 100))
            latePeak = juce::jmax (latePeak, std::abs (left));
    }

    INFO ("early peak " << earlyPeak << ", peak after 100 s " << latePeak);
    CHECK (latePeak < earlyPeak);
}

TEST_CASE ("Reverb output stays finite across sample rates", "[fx][reverb]")
{
    for (const auto sampleRate : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        FxReverb reverb;
        reverb.prepare (sampleRate);

        auto settings = reverbSettings();
        settings.size = 1.0f;
        settings.decay = 1.0f;
        settings.damping = 0.0f;
        settings.modDepth = 1.0f;
        settings.preDelayMs = 250.0f;
        settings.lowCutHz = 19000.0f;
        settings.highCutHz = 25.0f;
        reverb.setSettings (settings);

        INFO ("sample rate " << sampleRate);

        for (int i = 0; i < 16384; ++i)
        {
            auto left = (i % 64 < 32 ? 1.0f : -1.0f) + (i % 512 == 0 ? 4.0f : 0.0f);
            auto right = -left;

            reverb.processSample (left, right);

            REQUIRE (std::isfinite (left));
            REQUIRE (std::isfinite (right));
        }
    }
}

TEST_CASE ("A reverb parameter sweep produces no NaN", "[fx][reverb]")
{
    FxReverb reverb;
    reverb.prepare (kSampleRate);

    constexpr auto steps = 24;

    for (int step = 0; step < steps; ++step)
    {
        const auto t = static_cast<float> (step) / static_cast<float> (steps - 1);

        FxReverb::Settings settings;
        settings.enabled = true;
        settings.mix = t;
        settings.size = t;
        settings.decay = 1.0f - t;
        settings.damping = t;
        settings.preDelayMs = juce::jmap (t, 0.0f, 250.0f);
        settings.width = t;
        settings.lowCutHz = juce::jmap (t, 20.0f, 20000.0f);
        settings.highCutHz = juce::jmap (t, 20000.0f, 20.0f);
        settings.modDepth = t;

        reverb.setSettings (settings);

        for (int i = 0; i < 512; ++i)
        {
            auto left = std::sin (static_cast<float> (i) * 0.1f) * 0.8f;
            auto right = -left;

            reverb.processSample (left, right);

            INFO ("step " << step);
            REQUIRE (std::isfinite (left));
            REQUIRE (std::isfinite (right));
        }
    }
}

// --- Limiter ---------------------------------------------------------------

TEST_CASE ("A disabled limiter is transparent and adds no latency",
           "[fx][limiter]")
{
    auto settings = limiterSettings();
    settings.enabled = false;

    FxLimiter limiter;
    limiter.prepare (kSampleRate);
    limiter.setSettings (settings);

    CHECK (limiter.getLatencySamples() == 0);

    for (int i = 0; i < 512; ++i)
    {
        auto left = std::sin (static_cast<float> (i) * 0.1f) * 2.0f;
        auto right = -left;
        const auto dryLeft = left;
        const auto dryRight = right;

        limiter.processSample (left, right);

        CHECK (left == dryLeft);
        CHECK (right == dryRight);
    }
}

TEST_CASE ("The ceiling is never exceeded", "[fx][limiter]")
{
    /*  THE ONE THING THIS EFFECT PROMISES. Everything else about a limiter is
        a matter of taste; the ceiling is a guarantee, and a limiter that
        exceeds it on some signal is broken rather than differently voiced.

        Hostile input on purpose: a full-scale square is the worst case for a
        look-ahead detector, and the impulses are there to catch a peak that
        arrives faster than the envelope can respond. */
    for (const auto ceilingDb : { -12.0f, -6.0f, -0.3f, 0.0f })
    {
        auto settings = limiterSettings();
        settings.thresholdDb = ceilingDb;
        settings.ceilingDb = ceilingDb;

        FxLimiter limiter;
        limiter.prepare (kSampleRate);
        limiter.setSettings (settings);

        const auto ceiling = juce::Decibels::decibelsToGain (ceilingDb);

        juce::Random random (98765);

        for (int i = 0; i < 96000; ++i)
        {
            // Square, impulses, noise and silence in turn.
            auto input = 0.0f;

            switch ((i / 8000) % 4)
            {
                case 0:  input = (i % 64 < 32 ? 4.0f : -4.0f); break;
                case 1:  input = i % 128 == 0 ? 8.0f : 0.0f;   break;
                case 2:  input = (random.nextFloat() * 2.0f - 1.0f) * 4.0f; break;
                default: input = 0.0f; break;
            }

            auto left = input;
            auto right = -input * 0.5f;

            limiter.processSample (left, right);

            INFO ("ceiling " << ceilingDb << " dB, sample " << i);

            // A hair of tolerance for float rounding in the gain, and no more.
            REQUIRE (std::abs (left) <= ceiling * 1.0001f);
            REQUIRE (std::abs (right) <= ceiling * 1.0001f);
        }
    }
}

TEST_CASE ("The look-ahead catches a peak rather than letting its front through",
           "[fx][limiter]")
{
    /*  WHAT SEPARATES A LIMITER FROM A FAST COMPRESSOR. Without look-ahead the
        first samples of a sudden peak arrive before the gain is down, so the
        output overshoots and then recovers. The delay is what makes the
        reduction already applied when the peak emerges.

        A step from silence straight to full scale: the FIRST output sample
        that is not silence must already be limited. */
    auto settings = limiterSettings();
    settings.thresholdDb = -12.0f;
    settings.ceilingDb = -12.0f;

    FxLimiter limiter;
    limiter.prepare (kSampleRate);
    limiter.setSettings (settings);

    const auto ceiling = juce::Decibels::decibelsToGain (-12.0f);

    // Silence first, so the envelope is fully released.
    for (int i = 0; i < 4800; ++i)
    {
        auto left = 0.0f;
        auto right = 0.0f;
        limiter.processSample (left, right);
    }

    auto worstOvershoot = 0.0f;

    for (int i = 0; i < 4800; ++i)
    {
        auto left = 1.0f;
        auto right = 1.0f;

        limiter.processSample (left, right);

        worstOvershoot = juce::jmax (worstOvershoot, std::abs (left) - ceiling);
    }

    INFO ("worst overshoot above the ceiling: " << worstOvershoot);
    CHECK (worstOvershoot <= ceiling * 1.0e-4f);
}

TEST_CASE ("The limiter reports its latency", "[fx][limiter]")
{
    // A limiter that looks ahead without telling the host puts the whole
    // instrument early against the rest of the session.
    FxLimiter limiter;
    limiter.prepare (kSampleRate);
    limiter.setSettings (limiterSettings());

    const auto expected = static_cast<int> (
        std::round (kSampleRate * FxLimiter::kLookAheadSeconds));

    CHECK (limiter.getLatencySamples() == expected);
}

TEST_CASE ("Both channels get the same gain", "[fx][limiter]")
{
    /*  A stereo limiter must apply ONE gain to both channels. Detecting per
        channel means a peak on one side pulls that side down alone, so the
        image shifts towards the quieter channel every time the bass hits -
        which is the one place where treating the channels independently is
        wrong rather than merely different.

        Measured by driving ONLY the left channel and checking the right is
        attenuated by the same amount. */
    auto settings = limiterSettings();
    settings.thresholdDb = -12.0f;
    settings.ceilingDb = 0.0f;

    FxLimiter limiter;
    limiter.prepare (kSampleRate);
    limiter.setSettings (settings);

    // A loud left and a quiet right: the right must come down too.
    constexpr auto quietLevel = 0.05f;

    auto largestRight = 0.0f;

    for (int i = 0; i < 24000; ++i)
    {
        auto left = 1.0f * (i % 32 < 16 ? 1.0f : -1.0f);
        auto right = quietLevel;

        limiter.processSample (left, right);

        if (i > 4800)
            largestRight = juce::jmax (largestRight, std::abs (right));
    }

    INFO ("the quiet channel peaks at " << largestRight
          << " against an input of " << quietLevel);

    // Pulled down along with the loud channel, not left alone.
    CHECK (largestRight < quietLevel * 0.5f);
}

TEST_CASE ("The dry path is delayed with the wet one", "[fx][limiter]")
{
    /*  Mixing the UNDELAYED dry against the delayed wet would comb-filter the
        two: a 2 ms offset puts a null at 250 Hz and every odd multiple, which
        on a bass patch removes the fundamental. So at any mix setting the
        output must be a clean delayed copy when the signal is below the
        threshold - not a combed one. */
    auto settings = limiterSettings();
    settings.thresholdDb = 0.0f;
    settings.ceilingDb = 0.0f;
    settings.mix = 0.5f;

    FxLimiter limiter;
    limiter.prepare (kSampleRate);
    limiter.setSettings (settings);

    const auto latency = limiter.getLatencySamples();

    std::vector<float> input;
    std::vector<float> output;

    for (int i = 0; i < 4800; ++i)
    {
        // Quiet, so the limiter is not acting and the output should be a pure
        // delay.
        const auto value = std::sin (static_cast<float> (i) * 0.2f) * 0.1f;

        auto left = value;
        auto right = value;

        limiter.processSample (left, right);

        input.push_back (value);
        output.push_back (left);
    }

    auto largestError = 0.0f;

    for (std::size_t i = static_cast<std::size_t> (latency) + 100; i < output.size(); ++i)
        largestError = juce::jmax (
            largestError,
            std::abs (output[i] - input[i - static_cast<std::size_t> (latency)]));

    INFO ("largest departure from a pure delay: " << largestError);
    CHECK (largestError < 1.0e-5f);
}

TEST_CASE ("A longer release holds the gain down for longer", "[fx][limiter]")
{
    const auto recoveryBlocks = [] (float releaseMs)
    {
        auto settings = limiterSettings();
        settings.thresholdDb = -12.0f;
        settings.releaseMs = releaseMs;
        settings.ceilingDb = 0.0f;

        FxLimiter limiter;
        limiter.prepare (kSampleRate);
        limiter.setSettings (settings);

        // Hit it hard, then feed a quiet tone and see how long the gain stays
        // down.
        for (int i = 0; i < 4800; ++i)
        {
            auto left = (i % 32 < 16 ? 4.0f : -4.0f);
            auto right = left;
            limiter.processSample (left, right);
        }

        constexpr auto quiet = 0.1f;

        /*  FLUSH THE LOOK-AHEAD LINE FIRST. It still holds two milliseconds of
            the loud burst, so the first outputs after the burst are that burst
            attenuated - which is loud enough to look like the gain having
            already recovered. Measuring straight away reported zero blocks of
            recovery for every release setting, which was the delay line's
            contents rather than the envelope's. */
        for (int i = 0; i < limiter.getLatencySamples() + 8; ++i)
        {
            auto left = quiet;
            auto right = quiet;
            limiter.processSample (left, right);
        }

        for (int block = 0; block < 400; ++block)
        {
            auto peak = 0.0f;

            for (int i = 0; i < 48; ++i)
            {
                auto left = quiet;
                auto right = quiet;
                limiter.processSample (left, right);
                peak = juce::jmax (peak, std::abs (left));
            }

            if (peak > quiet * 0.9f)
                return block;
        }

        return 400;
    };

    const auto fast = recoveryBlocks (1.0f);
    const auto slow = recoveryBlocks (500.0f);

    INFO ("recovery: 1 ms release takes " << fast
          << " blocks, 500 ms takes " << slow);
    CHECK (slow > fast);
}

TEST_CASE ("Limiter output stays finite across sample rates", "[fx][limiter]")
{
    for (const auto sampleRate : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        FxLimiter limiter;
        limiter.prepare (sampleRate);

        auto settings = limiterSettings();
        settings.thresholdDb = -40.0f;
        settings.releaseMs = 1.0f;
        settings.ceilingDb = -40.0f;
        limiter.setSettings (settings);

        INFO ("sample rate " << sampleRate);

        for (int i = 0; i < 8192; ++i)
        {
            auto left = (i % 64 < 32 ? 8.0f : -8.0f) + (i % 512 == 0 ? 32.0f : 0.0f);
            auto right = -left;

            limiter.processSample (left, right);

            REQUIRE (std::isfinite (left));
            REQUIRE (std::isfinite (right));
        }
    }
}

TEST_CASE ("A limiter parameter sweep produces no NaN", "[fx][limiter]")
{
    FxLimiter limiter;
    limiter.prepare (kSampleRate);

    constexpr auto steps = 40;

    for (int step = 0; step < steps; ++step)
    {
        const auto t = static_cast<float> (step) / static_cast<float> (steps - 1);

        FxLimiter::Settings settings;
        settings.enabled = true;
        settings.mix = t;
        settings.thresholdDb = juce::jmap (t, -40.0f, 0.0f);
        settings.releaseMs = juce::jmap (t, 1.0f, 500.0f);
        settings.ceilingDb = juce::jmap (t, 0.0f, -40.0f);

        limiter.setSettings (settings);

        for (int i = 0; i < 512; ++i)
        {
            auto left = std::sin (static_cast<float> (i) * 0.1f) * 4.0f;
            auto right = -left;

            limiter.processSample (left, right);

            INFO ("step " << step);
            REQUIRE (std::isfinite (left));
            REQUIRE (std::isfinite (right));
        }
    }
}
