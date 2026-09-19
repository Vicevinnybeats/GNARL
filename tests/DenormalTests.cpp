#include <catch2/catch_test_macros.hpp>

#include "dsp/CombFilter.h"
#include "dsp/FilterSlot.h"
#include "dsp/FormantFilter.h"
#include "dsp/FxChorus.h"
#include "dsp/FxDelay.h"
#include "dsp/FxDimension.h"
#include "dsp/FxEq.h"
#include "dsp/FxFilter.h"
#include "dsp/FxFlanger.h"
#include "dsp/FxLimiter.h"
#include "dsp/FxPhaser.h"
#include "dsp/FxReverb.h"
#include "dsp/LadderFilter.h"
#include "dsp/OttCompressor.h"
#include "dsp/StateVariableFilter.h"

#include <cmath>
#include <string>

using namespace gnarl;
using namespace gnarl::dsp;

/*
    Denormal checks for every DSP unit that can idle.

    SECTION 8 OF CLAUDE.md SAID EVERY DSP UNIT GOT ONE OF THESE, and exactly
    one did - the FX EQ, written while chasing something else. The claim was
    recorded as false in the same file rather than quietly corrected, and this
    is the file that makes it true.

    WHY IT MATTERS HERE MORE THAN IN MOST CODE. A synth is mostly silence: a
    riddim patch plays a note and then a filter, a delay and a reverb tail all
    decay towards zero without arriving. On x86 a subnormal operand can cost
    around 100x a normal one, and there are fourteen effects and thirty-two
    filter instances in the signal path. A stage that idles subnormal is a
    stage that costs its worst-case CPU when the instrument is doing nothing -
    which is exactly when a host is least tolerant of an overrun.

    WHAT IS ASSERTED, and it is not "the output reaches zero". A TPT filter's
    state converges on a rounding fixed point near 1e-37 and stays there
    forever - that is a NORMAL float, so it costs nothing, and asserting exact
    silence would fail on correct code (CLAUDE.md section 3). The property
    that matters is: under ScopedNoDenormals - which is how processBlock always
    runs - no output sample sits in the subnormal range, and the tail is
    inaudible.
*/

namespace
{
    constexpr double kSampleRate = 48000.0;

    /** Long enough for any tail here to be well past audible. The reverb at
        maximum decay is the slowest thing in the plugin. */
    constexpr int kSilenceSamples = 240000;   // 5 seconds

    /** Ignore the first stretch: a tail is legitimately audible on its way
        down, and the question is what it does once it has arrived. */
    constexpr int kSettleSamples = 48000;

    /*  THE TAIL IS ASSERTED TO BE DECAYING, NOT TO BE BELOW A FLOOR.

        An absolute threshold is the wrong property for anything with a
        feedback path, and the first version of this file used one: a delay at
        0.85 feedback loses 15% per repeat, so after five seconds it is at
        about 1e-7 - decaying perfectly and nowhere near any floor worth
        calling negligible. It would take about thirty seconds of silence to
        reach 1e-20, and asserting that would be asserting the feedback
        setting rather than the denormal behaviour.

        So: the peak in the LAST tenth of the window against the peak in the
        first tenth after settling. That is the question - is it still going
        down - and it holds for two state variables and for eight coupled
        delay lines alike. */
    struct Result
    {
        bool sawSubnormal = false;
        float earlyPeak = 0.0f;
        float latePeak = 0.0f;
        bool finite = true;
    };

    /** Excites a unit, then watches its idle tail.

        `excite` and `tick` are separate so a unit can be driven hard and then
        asked for silence, which is the case that matters: a unit fed silence
        from the start never had any state to decay. */
    template <typename Excite, typename Tick>
    Result measureIdle (Excite&& excite, Tick&& tick)
    {
        // ALWAYS under the protection processBlock's first line provides.
        // Measuring without it would be measuring a configuration that does
        // not ship.
        const juce::ScopedNoDenormals noDenormals;

        excite();

        Result result;

        for (int i = 0; i < kSilenceSamples; ++i)
        {
            const auto out = tick();

            if (! std::isfinite (out))
            {
                result.finite = false;
                break;
            }

            if (i < kSettleSamples)
                continue;

            if (std::fpclassify (out) == FP_SUBNORMAL)
                result.sawSubnormal = true;

            const auto magnitude = std::abs (out);

            if (i < kSettleSamples + (kSilenceSamples - kSettleSamples) / 10)
                result.earlyPeak = juce::jmax (result.earlyPeak, magnitude);

            if (i > kSilenceSamples - (kSilenceSamples - kSettleSamples) / 10)
                result.latePeak = juce::jmax (result.latePeak, magnitude);
        }

        return result;
    }

    void check (const std::string& name, const Result& result)
    {
        INFO (name << ": tail goes from " << result.earlyPeak
              << " to " << result.latePeak);

        CHECK (result.finite);

        // The point of the file.
        CHECK_FALSE (result.sawSubnormal);

        /*  NOT-INCREASING rather than strictly decreasing, which the first
            version asserted and which fails on correct code: the FX filter
            reaches its rounding fixed point at 7.2e-37 before this window
            even opens, so its early and late peaks are the same number, and
            the phaser arrives at exactly zero. "It has already arrived" and
            "it is still going down" are both fine; "it is going up" is not. */
        CHECK (result.latePeak <= result.earlyPeak);

        /*  And inaudible. -80 dB rather than an absolute floor near the
            denormal range, because a delay at 0.85 feedback is at about 1e-7
            after five seconds - decaying perfectly, and nowhere near 1e-20.
            Asserting that would be asserting the feedback setting. */
        CHECK (result.latePeak < 1.0e-4f);
    }

    float noise (int i)
    {
        // Deterministic, and broadband enough to put energy into every state
        // a filter or delay has.
        return std::sin (static_cast<float> (i) * 0.37f)
             + 0.5f * std::sin (static_cast<float> (i) * 1.13f);
    }
}

TEST_CASE ("The TPT state variable filter idles without denormals",
           "[dsp][denormal]")
{
    StateVariableFilter filter;
    filter.prepare (kSampleRate);
    filter.setCutoff (400.0f);
    filter.setResonance (0.9f);

    check ("StateVariableFilter", measureIdle (
        [&filter] { for (int i = 0; i < 4800; ++i) filter.processSample (noise (i)); },
        [&filter] { return filter.processSample (0.0f).lowPass; }));
}

TEST_CASE ("The ladder filter idles without denormals", "[dsp][denormal]")
{
    LadderFilter filter;
    filter.prepare (kSampleRate);
    filter.setCutoff (500.0f);
    filter.setResonance (0.8f);

    check ("LadderFilter", measureIdle (
        [&filter] { for (int i = 0; i < 4800; ++i) filter.processSample (noise (i)); },
        [&filter] { return filter.processSample (0.0f); }));
}

TEST_CASE ("The comb filter idles without denormals", "[dsp][denormal]")
{
    /*  The one most at risk: a comb is a delay line with feedback, so its
        decay is geometric over a whole buffer rather than over two state
        variables, and every sample in that buffer goes subnormal on the way
        down. */
    CombFilter filter;
    filter.prepare (kSampleRate);
    filter.setFrequency (220.0f);
    filter.setFeedback (0.9f);
    filter.setDamping (0.2f);

    check ("CombFilter", measureIdle (
        [&filter] { for (int i = 0; i < 4800; ++i) filter.processSample (noise (i)); },
        [&filter] { return filter.processSample (0.0f); }));
}

TEST_CASE ("The formant filter idles without denormals", "[dsp][denormal]")
{
    FormantFilter filter;
    filter.prepare (kSampleRate);
    filter.setPosition (0.4f, 0.6f);
    filter.setThroat (0.3f);

    check ("FormantFilter", measureIdle (
        [&filter] { for (int i = 0; i < 4800; ++i) filter.processSample (noise (i)); },
        [&filter] { return filter.processSample (0.0f); }));
}

TEST_CASE ("Every voice filter type idles without denormals", "[dsp][denormal]")
{
    for (int type = 0; type < static_cast<int> (choices::FilterType::count); ++type)
    {
        FilterSlot slot;
        slot.prepare (kSampleRate);

        FilterSlot::Settings settings;
        settings.type = static_cast<choices::FilterType> (type);
        settings.cutoffHz = 700.0f;
        settings.resonance = 0.85f;
        settings.mix = 1.0f;
        slot.setSettings (settings);

        check (std::string ("FilterSlot ")
                   + choices::filterType[type].toStdString(),
               measureIdle (
                   [&slot] { for (int i = 0; i < 4800; ++i) slot.processSample (noise (i)); },
                   [&slot] { return slot.processSample (0.0f); }));
    }
}

TEST_CASE ("The OTT compressor idles without denormals", "[dsp][denormal]")
{
    OttCompressor ott;
    ott.prepare (kSampleRate, 2);

    OttCompressor::Settings settings;
    settings.enabled = true;
    settings.depth = 0.8f;
    ott.setSettings (settings);

    check ("OttCompressor", measureIdle (
        [&ott] { for (int i = 0; i < 4800; ++i) ott.processSample (0, noise (i)); },
        [&ott] { return ott.processSample (0, 0.0f); }));
}

TEST_CASE ("The FX EQ idles without denormals", "[dsp][denormal]")
{
    FxEq eq;
    eq.prepare (kSampleRate);

    FxEq::Settings settings;
    settings.enabled = true;
    settings.band1Gain = 12.0f;
    settings.highPassFreq = 400.0f;
    eq.setSettings (settings);

    check ("FxEq", measureIdle (
        [&eq] { for (int i = 0; i < 4800; ++i) eq.processSample (0, noise (i)); },
        [&eq] { return eq.processSample (0, 0.0f); }));
}

TEST_CASE ("The FX filter idles without denormals", "[dsp][denormal]")
{
    FxFilter filter;
    filter.prepare (kSampleRate);

    FxFilter::Settings settings;
    settings.enabled = true;
    settings.type = choices::FxFilterType::lowPass24;
    settings.cutoffHz = 600.0f;
    settings.resonance = 0.9f;
    filter.setSettings (settings);

    check ("FxFilter", measureIdle (
        [&filter] { for (int i = 0; i < 4800; ++i) filter.processSample (0, noise (i)); },
        [&filter] { return filter.processSample (0, 0.0f); }));
}

TEST_CASE ("The modulated effects idle without denormals", "[dsp][denormal]")
{
    SECTION ("chorus")
    {
        FxChorus effect;
        effect.prepare (kSampleRate);

        FxChorus::Settings settings;
        settings.enabled = true;
        settings.feedback = 0.7f;
        settings.voices = FxChorus::kMaxVoices;
        effect.setSettings (settings);

        check ("FxChorus", measureIdle (
            [&effect] { for (int i = 0; i < 4800; ++i) effect.processSample (0, noise (i)); },
            [&effect] { return effect.processSample (0, 0.0f); }));
    }

    SECTION ("flanger")
    {
        FxFlanger effect;
        effect.prepare (kSampleRate);

        FxFlanger::Settings settings;
        settings.enabled = true;
        settings.feedback = 0.9f;
        effect.setSettings (settings);

        check ("FxFlanger", measureIdle (
            [&effect] { for (int i = 0; i < 4800; ++i) effect.processSample (0, noise (i)); },
            [&effect] { return effect.processSample (0, 0.0f); }));
    }

    SECTION ("phaser")
    {
        FxPhaser effect;
        effect.prepare (kSampleRate);

        FxPhaser::Settings settings;
        settings.enabled = true;
        settings.feedback = 0.85f;
        settings.stages = FxPhaser::kMaxStages;
        effect.setSettings (settings);

        check ("FxPhaser", measureIdle (
            [&effect] { for (int i = 0; i < 4800; ++i) effect.processSample (0, noise (i)); },
            [&effect] { return effect.processSample (0, 0.0f); }));
    }
}

TEST_CASE ("The delay idles without denormals", "[dsp][denormal]")
{
    /*  A long feedback delay is the worst case in the plugin for this: the
        line holds tens of thousands of samples, all decaying geometrically,
        and every one of them passes through the subnormal range on its way
        down. */
    FxDelay delay;
    delay.prepare (kSampleRate);

    FxDelay::Settings settings;
    settings.enabled = true;
    settings.mix = 1.0f;
    settings.syncEnabled = false;
    settings.timeMs = 50.0f;
    settings.feedback = 0.85f;
    delay.setSettings (settings);

    const juce::ScopedNoDenormals noDenormals;

    for (int i = 0; i < 4800; ++i)
    {
        auto left = noise (i);
        auto right = left;
        delay.processSample (left, right);
    }

    auto sawSubnormal = false;
    auto peak = 0.0f;

    for (int i = 0; i < kSilenceSamples; ++i)
    {
        auto left = 0.0f;
        auto right = 0.0f;
        delay.processSample (left, right);

        REQUIRE (std::isfinite (left));

        // Only the final window, for the reason in the Result comment above.
        if (i < kSilenceSamples - kSettleSamples)
            continue;

        if (std::fpclassify (left) == FP_SUBNORMAL) sawSubnormal = true;

        peak = juce::jmax (peak, std::abs (left));
    }

    INFO ("FxDelay: idle tail peaks at " << peak << " over the final window");
    CHECK_FALSE (sawSubnormal);

    // A tail this far down is inaudible; the exact figure depends on the
    // feedback setting, which is not what this file is about.
    CHECK (peak < 1.0e-4f);
}

TEST_CASE ("The reverb idles without denormals", "[dsp][denormal]")
{
    // Eight delay lines feeding each other, at the longest decay: the largest
    // amount of decaying state anywhere in the plugin.
    FxReverb reverb;
    reverb.prepare (kSampleRate);

    FxReverb::Settings settings;
    settings.enabled = true;
    settings.mix = 1.0f;
    settings.decay = 1.0f;
    settings.size = 1.0f;
    settings.damping = 0.0f;
    reverb.setSettings (settings);

    const juce::ScopedNoDenormals noDenormals;

    for (int i = 0; i < 4800; ++i)
    {
        auto left = noise (i);
        auto right = left;
        reverb.processSample (left, right);
    }

    auto sawSubnormal = false;
    auto peak = 0.0f;

    // The reverb at maximum decay genuinely rings for a long time, so it gets
    // a longer window than the rest before anyone asks it to be quiet.
    constexpr auto reverbSettle = 48000 * 20;

    for (int i = 0; i < reverbSettle + kSilenceSamples; ++i)
    {
        auto left = 0.0f;
        auto right = 0.0f;
        reverb.processSample (left, right);

        REQUIRE (std::isfinite (left));

        if (i < reverbSettle)
            continue;

        if (std::fpclassify (left) == FP_SUBNORMAL) sawSubnormal = true;

        peak = juce::jmax (peak, std::abs (left));
    }

    INFO ("FxReverb: idle tail peaks at " << peak);
    CHECK_FALSE (sawSubnormal);
}

TEST_CASE ("The stereo effects idle without denormals", "[dsp][denormal]")
{
    SECTION ("dimension")
    {
        FxDimension effect;
        effect.prepare (kSampleRate);

        FxDimension::Settings settings;
        settings.enabled = true;
        settings.width = 1.0f;
        settings.amount = 1.0f;
        effect.setSettings (settings);

        const juce::ScopedNoDenormals noDenormals;

        for (int i = 0; i < 4800; ++i)
        {
            auto left = noise (i);
            auto right = -left;
            effect.processSample (left, right);
        }

        auto sawSubnormal = false;

        for (int i = 0; i < kSilenceSamples; ++i)
        {
            auto left = 0.0f;
            auto right = 0.0f;
            effect.processSample (left, right);

            if (i > kSettleSamples && std::fpclassify (left) == FP_SUBNORMAL)
                sawSubnormal = true;
        }

        CHECK_FALSE (sawSubnormal);
    }

    SECTION ("limiter")
    {
        FxLimiter effect;
        effect.prepare (kSampleRate);

        FxLimiter::Settings settings;
        settings.enabled = true;
        settings.thresholdDb = -12.0f;
        settings.releaseMs = 500.0f;
        effect.setSettings (settings);

        const juce::ScopedNoDenormals noDenormals;

        for (int i = 0; i < 4800; ++i)
        {
            auto left = noise (i) * 4.0f;
            auto right = left;
            effect.processSample (left, right);
        }

        auto sawSubnormal = false;

        for (int i = 0; i < kSilenceSamples; ++i)
        {
            auto left = 0.0f;
            auto right = 0.0f;
            effect.processSample (left, right);

            // The limiter's ENVELOPE decays exponentially too, and an
            // envelope follower idling subnormal costs the same as a filter
            // doing it.
            if (i > kSettleSamples && std::fpclassify (left) == FP_SUBNORMAL)
                sawSubnormal = true;
        }

        CHECK_FALSE (sawSubnormal);
    }
}
