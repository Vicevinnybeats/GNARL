#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/DelayLine.h"
#include "dsp/FxDelay.h"

#include <cmath>
#include <vector>

using namespace gnarl;
using namespace gnarl::dsp;
using Catch::Approx;

namespace
{
    constexpr double kSampleRate = 48000.0;

    FxDelay::Settings clean()
    {
        FxDelay::Settings s;
        s.enabled = true;
        s.mix = 1.0f;
        s.syncEnabled = false;
        s.timeMs = 100.0f;
        s.feedback = 0.0f;
        s.pingPong = false;
        s.width = 0.0f;
        s.lowCutHz = 20.0f;
        s.highCutHz = 20000.0f;
        s.modRateHz = 0.3f;
        s.modDepth = 0.0f;
        return s;
    }

    /** Where the loudest echo of a single impulse lands, in samples. */
    int firstEchoAt (FxDelay& delay, int searchSamples, int channel = 0)
    {
        auto left = 1.0f;
        auto right = 1.0f;
        delay.processSample (left, right);

        auto bestIndex = -1;
        auto bestValue = 0.0f;

        for (int i = 1; i < searchSamples; ++i)
        {
            auto l = 0.0f;
            auto r = 0.0f;
            delay.processSample (l, r);

            const auto value = std::abs (channel == 0 ? l : r);

            if (value > bestValue)
            {
                bestValue = value;
                bestIndex = i;
            }
        }

        return bestIndex;
    }
}

TEST_CASE ("A fractional delay line reads back what was written", "[fx][delayline]")
{
    DelayLine line;
    line.prepare (512);

    // A whole-number delay must be EXACT: the cubic interpolator's weights at
    // fraction zero are (0, 1, 0, 0), so any error here is an indexing bug
    // rather than an interpolation one.
    for (int i = 0; i < 400; ++i)
        line.write (static_cast<float> (i));

    CHECK (line.read (1.0f) == Approx (399.0f).margin (1.0e-4f));
    CHECK (line.read (10.0f) == Approx (390.0f).margin (1.0e-4f));
    CHECK (line.read (100.0f) == Approx (300.0f).margin (1.0e-4f));
}

TEST_CASE ("A fractional read interpolates between neighbours", "[fx][delayline]")
{
    DelayLine line;
    line.prepare (512);

    // A ramp is exactly representable by any sane interpolator, so a
    // half-sample read of one must land half way.
    for (int i = 0; i < 400; ++i)
        line.write (static_cast<float> (i));

    CHECK (line.read (10.5f) == Approx (389.5f).margin (1.0e-3f));
    CHECK (line.read (10.25f) == Approx (389.75f).margin (1.0e-3f));
}

TEST_CASE ("A delay line clamps rather than reading outside its buffer",
           "[fx][delayline]")
{
    DelayLine line;
    line.prepare (64);

    for (int i = 0; i < 200; ++i)
        line.write (1.0f);

    // A modulated delay WILL be driven past its limits. Reading the closest
    // sample inside the buffer is the only safe answer; returning garbage from
    // outside it is not.
    CHECK (std::isfinite (line.read (-100.0f)));
    CHECK (std::isfinite (line.read (0.0f)));
    CHECK (std::isfinite (line.read (1.0e9f)));
    CHECK (line.read (1.0e9f) == Approx (1.0f).margin (1.0e-4f));
}

TEST_CASE ("An unprepared delay line is safe to use", "[fx][delayline]")
{
    // processBlock can be called before prepareToPlay - broken hosts exist,
    // and CLAUDE.md section 3 requires this not to dereference anything.
    DelayLine line;

    CHECK_FALSE (line.isPrepared());
    line.write (1.0f);
    CHECK (line.read (10.0f) == 0.0f);
}

TEST_CASE ("A disabled delay passes its input through", "[fx][delay]")
{
    auto settings = clean();
    settings.enabled = false;
    settings.feedback = 0.9f;

    FxDelay delay;
    delay.prepare (kSampleRate);
    delay.setSettings (settings);

    for (int i = 0; i < 512; ++i)
    {
        auto left = std::sin (static_cast<float> (i) * 0.1f);
        auto right = std::cos (static_cast<float> (i) * 0.1f);
        const auto dryLeft = left;
        const auto dryRight = right;

        delay.processSample (left, right);

        CHECK (left == dryLeft);
        CHECK (right == dryRight);
    }
}

TEST_CASE ("Mix at zero is bit-transparent", "[fx][delay]")
{
    auto settings = clean();
    settings.mix = 0.0f;
    settings.feedback = 0.8f;

    FxDelay delay;
    delay.prepare (kSampleRate);
    delay.setSettings (settings);

    for (int i = 0; i < 512; ++i)
    {
        auto left = std::sin (static_cast<float> (i) * 0.1f);
        const auto dry = left;
        auto right = 0.0f;

        delay.processSample (left, right);

        CHECK (left == dry);
    }
}

TEST_CASE ("The echo lands at the time asked for", "[fx][delay]")
{
    for (const auto milliseconds : { 10.0f, 50.0f, 250.0f, 1000.0f })
    {
        auto settings = clean();
        settings.timeMs = milliseconds;

        FxDelay delay;
        delay.prepare (kSampleRate);
        delay.setSettings (settings);

        const auto expected = static_cast<int> (kSampleRate * milliseconds * 0.001);
        const auto measured = firstEchoAt (delay, expected * 2 + 64);

        INFO (milliseconds << " ms: echo at sample " << measured
              << ", expected " << expected);

        // Within a sample. A delay that is close to the time on its readout is
        // not good enough when it is tempo-synced against a DAW's grid.
        CHECK (std::abs (measured - expected) <= 1);
    }
}

TEST_CASE ("A tempo-synced delay lands on the beat", "[fx][delay]")
{
    /*  The point of sync. An eighth at 140 BPM is 60/140/2 seconds; if this is
        even slightly wrong the delay drifts against the track, which is the
        one thing a synced delay must not do. */
    constexpr auto bpm = 140.0;

    struct Case { sync::Division division; double beats; };

    for (const auto& testCase : { Case { sync::Division::quarter, 1.0 },
                                  Case { sync::Division::eighth, 0.5 },
                                  Case { sync::Division::sixteenth, 0.25 },
                                  Case { sync::Division::eighthTriplet, 1.0 / 3.0 },
                                  Case { sync::Division::quarterDotted, 1.5 } })
    {
        auto settings = clean();
        settings.syncEnabled = true;
        settings.division = testCase.division;

        FxDelay delay;
        delay.prepare (kSampleRate);
        delay.setSettings (settings, bpm);

        const auto expected = static_cast<int> (
            kSampleRate * testCase.beats * 60.0 / bpm);
        const auto measured = firstEchoAt (delay, expected * 2 + 64);

        INFO ("expected " << expected << " samples, measured " << measured);
        CHECK (std::abs (measured - expected) <= 1);
    }
}

TEST_CASE ("Feedback repeats and decays", "[fx][delay]")
{
    auto settings = clean();
    settings.timeMs = 20.0f;
    settings.feedback = 0.5f;

    FxDelay delay;
    delay.prepare (kSampleRate);
    delay.setSettings (settings);

    const auto delaySamples = static_cast<int> (kSampleRate * 0.02);

    auto left = 1.0f;
    auto right = 1.0f;
    delay.processSample (left, right);

    std::vector<float> peaks;

    for (int repeat = 1; repeat <= 4; ++repeat)
    {
        auto peak = 0.0f;

        for (int i = 0; i < delaySamples; ++i)
        {
            auto l = 0.0f;
            auto r = 0.0f;
            delay.processSample (l, r);
            peak = juce::jmax (peak, std::abs (l));
        }

        peaks.push_back (peak);
    }

    // Each repeat quieter than the last, and audible: a decay that stops after
    // one repeat is a delay whose feedback does nothing.
    for (std::size_t i = 1; i < peaks.size(); ++i)
    {
        INFO ("repeat " << i << " peaks at " << peaks[i]
              << ", previous at " << peaks[i - 1]);
        CHECK (peaks[i] < peaks[i - 1]);
        CHECK (peaks[i] > 0.0f);
    }
}

TEST_CASE ("Feedback at its maximum does not run away", "[fx][delay]")
{
    /*  THE FAILURE THIS PREVENTS. A delay whose loop gain reaches unity is an
        oscillator, and with the tone filters inside the loop it can exceed
        unity at some frequencies before the nominal setting says it should.
        A delay that can run away will, on somebody's preset, while they are
        recording. */
    auto settings = clean();
    settings.timeMs = 15.0f;
    settings.feedback = 1.0f;
    settings.lowCutHz = 400.0f;
    settings.highCutHz = 3000.0f;

    FxDelay delay;
    delay.prepare (kSampleRate);
    delay.setSettings (settings);

    // A full-scale burst, then thirty seconds of silence.
    for (int i = 0; i < 4800; ++i)
    {
        auto left = (i % 32 < 16 ? 1.0f : -1.0f);
        auto right = -left;
        delay.processSample (left, right);
    }

    auto earlyPeak = 0.0f;
    auto latePeak = 0.0f;

    for (int i = 0; i < static_cast<int> (kSampleRate * 30); ++i)
    {
        auto left = 0.0f;
        auto right = 0.0f;
        delay.processSample (left, right);

        REQUIRE (std::isfinite (left));
        REQUIRE (std::isfinite (right));

        if (i < 48000)
            earlyPeak = juce::jmax (earlyPeak, std::abs (left));
        else if (i > static_cast<int> (kSampleRate * 25))
            latePeak = juce::jmax (latePeak, std::abs (left));
    }

    INFO ("early peak " << earlyPeak << ", late peak " << latePeak);
    CHECK (latePeak < earlyPeak);
}

TEST_CASE ("The feedback path loses top end rather than repeating brightly",
           "[fx][delay]")
{
    /*  What makes repeats sound like echoes. The tone controls are INSIDE the
        loop, so each repeat is filtered again; an unfiltered delay repeats a
        bright signal brightly forever. Measured as the ratio of high-frequency
        to low-frequency energy, first repeat against fourth. */
    auto settings = clean();
    settings.timeMs = 20.0f;
    settings.feedback = 0.8f;
    settings.highCutHz = 2000.0f;

    FxDelay delay;
    delay.prepare (kSampleRate);
    delay.setSettings (settings);

    const auto delaySamples = static_cast<int> (kSampleRate * 0.02);

    // White-ish noise burst so there is energy at both ends to lose.
    juce::Random random (1234);

    for (int i = 0; i < delaySamples; ++i)
    {
        auto left = random.nextFloat() * 2.0f - 1.0f;
        auto right = left;
        delay.processSample (left, right);
    }

    const auto brightnessOfNextRepeat = [&]
    {
        // A one-pole split at 2 kHz, enough to tell bright from dull.
        OnePole splitter;
        splitter.prepare (kSampleRate);
        splitter.setCutoff (2000.0f);

        auto lowEnergy = 0.0;
        auto highEnergy = 0.0;

        for (int i = 0; i < delaySamples; ++i)
        {
            auto left = 0.0f;
            auto right = 0.0f;
            delay.processSample (left, right);

            const auto low = splitter.processLowPass (left);
            const auto high = left - low;

            lowEnergy += static_cast<double> (low) * low;
            highEnergy += static_cast<double> (high) * high;
        }

        return highEnergy / juce::jmax (1.0e-12, lowEnergy);
    };

    const auto first = brightnessOfNextRepeat();
    brightnessOfNextRepeat();
    brightnessOfNextRepeat();
    const auto fourth = brightnessOfNextRepeat();

    INFO ("brightness ratio: first repeat " << first << ", fourth " << fourth);
    CHECK (fourth < first);
}

TEST_CASE ("Ping-pong alternates sides rather than widening", "[fx][delay]")
{
    /*  THE DIFFERENCE BETWEEN A PING-PONG AND A WIDE DELAY. Two independent
        lines panned apart give repeats on both sides at once. A ping-pong
        cross-couples the feedback, so a repeat physically alternates - which
        means feeding ONE side must produce a repeat on the OTHER. */
    auto settings = clean();
    settings.timeMs = 20.0f;
    settings.feedback = 0.6f;
    settings.pingPong = true;

    FxDelay delay;
    delay.prepare (kSampleRate);
    delay.setSettings (settings);

    const auto delaySamples = static_cast<int> (kSampleRate * 0.02);

    // Left only.
    auto left = 1.0f;
    auto right = 0.0f;
    delay.processSample (left, right);

    const auto peakOverOneRepeat = [&] (float& target, float& other)
    {
        auto targetPeak = 0.0f;
        auto otherPeak = 0.0f;

        for (int i = 0; i < delaySamples; ++i)
        {
            auto l = 0.0f;
            auto r = 0.0f;
            delay.processSample (l, r);
            targetPeak = juce::jmax (targetPeak, std::abs (l));
            otherPeak = juce::jmax (otherPeak, std::abs (r));
        }

        target = targetPeak;
        other = otherPeak;
    };

    auto firstLeft = 0.0f;
    auto firstRight = 0.0f;
    peakOverOneRepeat (firstLeft, firstRight);

    auto secondLeft = 0.0f;
    auto secondRight = 0.0f;
    peakOverOneRepeat (secondLeft, secondRight);

    INFO ("repeat 1: L " << firstLeft << " R " << firstRight
          << ";  repeat 2: L " << secondLeft << " R " << secondRight);

    // The first repeat is on the left, where the input was. The SECOND is on
    // the right, because the feedback crossed over.
    CHECK (firstLeft > firstRight * 4.0f);
    CHECK (secondRight > secondLeft * 4.0f);
}

TEST_CASE ("Width offsets the two lines without coupling them", "[fx][delay]")
{
    auto settings = clean();
    settings.timeMs = 100.0f;
    settings.width = 1.0f;

    FxDelay delay;
    delay.prepare (kSampleRate);
    delay.setSettings (settings);

    const auto expected = static_cast<int> (kSampleRate * 0.1);

    auto left = 1.0f;
    auto right = 1.0f;
    delay.processSample (left, right);

    auto leftAt = -1;
    auto rightAt = -1;
    auto leftPeak = 0.0f;
    auto rightPeak = 0.0f;

    for (int i = 1; i < expected * 2; ++i)
    {
        auto l = 0.0f;
        auto r = 0.0f;
        delay.processSample (l, r);

        if (std::abs (l) > leftPeak) { leftPeak = std::abs (l); leftAt = i; }
        if (std::abs (r) > rightPeak) { rightPeak = std::abs (r); rightAt = i; }
    }

    INFO ("left echo at " << leftAt << ", right at " << rightAt);

    // The right side is LATER by the width offset, which is a fraction of the
    // delay rather than a fixed number of milliseconds.
    CHECK (rightAt > leftAt);
    CHECK (leftAt == Approx (expected).margin (2));
}

TEST_CASE ("Modulation moves the delay length", "[fx][delay]")
{
    // Without this a long feedback setting rings on one pitch.
    auto settings = clean();
    settings.timeMs = 100.0f;
    settings.modDepth = 1.0f;
    settings.modRateHz = 5.0f;

    FxDelay modulated;
    modulated.prepare (kSampleRate);
    modulated.setSettings (settings);

    settings.modDepth = 0.0f;

    FxDelay still;
    still.prepare (kSampleRate);
    still.setSettings (settings);

    auto difference = 0.0f;

    for (int i = 0; i < 48000; ++i)
    {
        const auto input = std::sin (static_cast<float> (i) * 0.3f);

        auto ml = input;
        auto mr = input;
        modulated.processSample (ml, mr);

        auto sl = input;
        auto sr = input;
        still.processSample (sl, sr);

        difference = juce::jmax (difference, std::abs (ml - sl));
    }

    INFO ("largest difference from modulating the delay: " << difference);
    CHECK (difference > 0.01f);
}

TEST_CASE ("Output stays finite across sample rates and hostile settings",
           "[fx][delay]")
{
    for (const auto sampleRate : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        FxDelay delay;
        delay.prepare (sampleRate);

        auto settings = clean();
        settings.timeMs = 1.0f;
        settings.feedback = 1.0f;
        settings.pingPong = true;
        settings.width = 1.0f;
        settings.modDepth = 1.0f;
        settings.modRateHz = 20.0f;
        settings.lowCutHz = 19000.0f;
        settings.highCutHz = 25.0f;
        delay.setSettings (settings);

        INFO ("sample rate " << sampleRate);

        for (int i = 0; i < 16384; ++i)
        {
            auto left = (i % 64 < 32 ? 1.0f : -1.0f) + (i % 512 == 0 ? 4.0f : 0.0f);
            auto right = -left;

            delay.processSample (left, right);

            REQUIRE (std::isfinite (left));
            REQUIRE (std::isfinite (right));
        }
    }
}

TEST_CASE ("A very slow tempo cannot ask for more delay than the buffer holds",
           "[fx][delay]")
{
    /*  Eight bars at 20 BPM is 96 seconds, and the buffer holds four. Without
        the clamp the read would wrap and give a delay that is not the one
        asked for, silently - a wrong answer rather than a limited one. */
    auto settings = clean();
    settings.syncEnabled = true;
    settings.division = sync::Division::bars8;

    FxDelay delay;
    delay.prepare (kSampleRate);
    delay.setSettings (settings, 20.0);

    const auto maximum = static_cast<int> (kSampleRate * FxDelay::kMaximumDelaySeconds);
    const auto measured = firstEchoAt (delay, maximum + 128);

    INFO ("echo at " << measured << ", buffer holds " << maximum);
    CHECK (measured <= maximum);
    CHECK (measured > 0);
}

TEST_CASE ("A parameter sweep produces no NaN", "[fx][delay]")
{
    FxDelay delay;
    delay.prepare (kSampleRate);

    constexpr auto steps = 40;

    for (int step = 0; step < steps; ++step)
    {
        const auto t = static_cast<float> (step) / static_cast<float> (steps - 1);

        FxDelay::Settings settings;
        settings.enabled = true;
        settings.mix = t;
        settings.syncEnabled = step % 2 == 0;
        settings.division = static_cast<sync::Division> (
            step % static_cast<int> (sync::Division::count));
        settings.timeMs = juce::jmap (t, 1.0f, 4000.0f);
        settings.feedback = t;
        settings.pingPong = step % 3 == 0;
        settings.width = t;
        settings.lowCutHz = juce::jmap (t, 20.0f, 20000.0f);
        settings.highCutHz = juce::jmap (t, 20000.0f, 20.0f);
        settings.modRateHz = juce::jmap (t, 0.01f, 20.0f);
        settings.modDepth = t;

        delay.setSettings (settings, juce::jmap (static_cast<double> (t), 20.0, 300.0));

        for (int i = 0; i < 512; ++i)
        {
            auto left = std::sin (static_cast<float> (i) * 0.1f) * 0.8f;
            auto right = std::cos (static_cast<float> (i) * 0.1f) * 0.8f;

            delay.processSample (left, right);

            INFO ("step " << step);
            REQUIRE (std::isfinite (left));
            REQUIRE (std::isfinite (right));
        }
    }
}
