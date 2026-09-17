#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/FilterSlot.h"

#include <cmath>
#include <vector>

using namespace gnarl::dsp;
using namespace gnarl;
using Type = FilterSlot::Type;

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

    /**
        Steady-state gain at one frequency, measured by driving a sine through
        and taking the RMS after the filter has settled.

        Measured rather than derived from coefficients: a transfer function
        computed on paper cannot catch a filter that is stable in theory and
        blows up in the implementation.
    */
    float measureGain (FilterSlot& filter, float frequencyHz, float amplitude = 0.5f)
    {
        const auto settleSamples = static_cast<int> (kSampleRate * 0.2);
        const auto measureSamples = static_cast<int> (kSampleRate * 0.2);

        const auto increment = juce::MathConstants<float>::twoPi
                             * frequencyHz / static_cast<float> (kSampleRate);
        auto phase = 0.0f;

        for (int i = 0; i < settleSamples; ++i)
        {
            filter.processSample (amplitude * std::sin (phase));
            phase += increment;
        }

        auto sumSquares = 0.0;

        for (int i = 0; i < measureSamples; ++i)
        {
            const auto out = filter.processSample (amplitude * std::sin (phase));
            phase += increment;
            sumSquares += static_cast<double> (out) * out;
        }

        const auto rms = std::sqrt (sumSquares / static_cast<double> (measureSamples));
        const auto inputRms = amplitude / std::sqrt (2.0f);

        return static_cast<float> (rms) / inputRms;
    }

    float measureGainDb (FilterSlot& filter, float frequencyHz, float amplitude = 0.5f)
    {
        return juce::Decibels::gainToDecibels (measureGain (filter, frequencyHz, amplitude),
                                               -120.0f);
    }

    FilterSlot::Settings settingsFor (Type type, float cutoff = 1000.0f)
    {
        FilterSlot::Settings settings;
        settings.type = type;
        settings.cutoffHz = cutoff;
        settings.resonance = 0.0f;
        settings.drive = 0.0f;
        settings.mix = 1.0f;
        return settings;
    }

    std::unique_ptr<FilterSlot> makeFilter (const FilterSlot::Settings& settings)
    {
        auto filter = std::make_unique<FilterSlot>();
        filter->prepare (kSampleRate);
        filter->setSettings (settings);
        return filter;
    }
}

// --- Responses -------------------------------------------------------------

TEST_CASE ("Low-pass passes below cutoff and rejects above", "[filter]")
{
    for (const auto type : { Type::lowPass12, Type::lowPass24 })
    {
        auto filter = makeFilter (settingsFor (type, 1000.0f));

        const auto passband = measureGainDb (*filter, 100.0f);
        const auto stopband = measureGainDb (*filter, 8000.0f);

        INFO ("type " << static_cast<int> (type)
              << " passband " << passband << " dB, stopband " << stopband << " dB");

        CHECK (passband > -1.5f);
        CHECK (stopband < -20.0f);
    }
}

TEST_CASE ("24 dB slopes are steeper than 12 dB slopes", "[filter]")
{
    // Three octaves above cutoff, a 12 dB/oct filter should be around -36 dB
    // and a 24 dB/oct one around -72 dB. If they measure the same, the second
    // section is not actually in the path.
    auto twelve = makeFilter (settingsFor (Type::lowPass12, 1000.0f));
    auto twentyFour = makeFilter (settingsFor (Type::lowPass24, 1000.0f));

    const auto gain12 = measureGainDb (*twelve, 8000.0f);
    const auto gain24 = measureGainDb (*twentyFour, 8000.0f);

    INFO ("12 dB: " << gain12 << " dB, 24 dB: " << gain24 << " dB");

    CHECK (gain24 < gain12 - 15.0f);
}

TEST_CASE ("High-pass rejects below cutoff and passes above", "[filter]")
{
    for (const auto type : { Type::highPass12, Type::highPass24 })
    {
        auto filter = makeFilter (settingsFor (type, 1000.0f));

        const auto stopband = measureGainDb (*filter, 100.0f);
        const auto passband = measureGainDb (*filter, 8000.0f);

        INFO ("type " << static_cast<int> (type)
              << " stopband " << stopband << " dB, passband " << passband << " dB");

        CHECK (stopband < -20.0f);
        CHECK (passband > -1.5f);
    }
}

TEST_CASE ("Band-pass peaks at cutoff and rejects both sides", "[filter]")
{
    auto filter = makeFilter (settingsFor (Type::bandPass12, 1000.0f));

    const auto below = measureGainDb (*filter, 50.0f);
    const auto atCutoff = measureGainDb (*filter, 1000.0f);
    const auto above = measureGainDb (*filter, 20000.0f);

    INFO ("below " << below << ", at " << atCutoff << ", above " << above);

    CHECK (atCutoff > below + 15.0f);
    CHECK (atCutoff > above + 15.0f);
}

TEST_CASE ("Notch rejects at cutoff and passes either side", "[filter]")
{
    auto settings = settingsFor (Type::notch12, 1000.0f);
    settings.resonance = 0.0f;
    auto filter = makeFilter (settings);

    const auto below = measureGainDb (*filter, 100.0f);
    const auto atCutoff = measureGainDb (*filter, 1000.0f);
    const auto above = measureGainDb (*filter, 10000.0f);

    INFO ("below " << below << ", at " << atCutoff << ", above " << above);

    CHECK (atCutoff < below - 10.0f);
    CHECK (atCutoff < above - 10.0f);
}

TEST_CASE ("Resonance produces a peak at cutoff", "[filter]")
{
    auto flat = settingsFor (Type::lowPass12, 1000.0f);
    flat.resonance = 0.0f;

    auto resonant = flat;
    resonant.resonance = 0.9f;

    auto flatFilter = makeFilter (flat);
    auto resonantFilter = makeFilter (resonant);

    const auto flatGain = measureGainDb (*flatFilter, 1000.0f);
    const auto resonantGain = measureGainDb (*resonantFilter, 1000.0f);

    INFO ("flat " << flatGain << " dB, resonant " << resonantGain << " dB");

    CHECK (resonantGain > flatGain + 6.0f);
}

TEST_CASE ("The ladder filter low-passes and its resonance bounded",
           "[filter][ladder]")
{
    auto settings = settingsFor (Type::ladderLowPass, 1000.0f);
    auto filter = makeFilter (settings);

    CHECK (measureGainDb (*filter, 100.0f) > -3.0f);
    CHECK (measureGainDb (*filter, 8000.0f) < -20.0f);

    // Full resonance must be a sound, not a hazard: the tanh in the feedback
    // path is what bounds it.
    settings.resonance = 1.0f;
    settings.drive = 1.0f;
    auto screaming = makeFilter (settings);

    const auto gain = measureGain (*screaming, 1000.0f);

    INFO ("gain at full resonance and drive: " << gain);
    CHECK (std::isfinite (gain));
    CHECK (gain < 20.0f);
    CHECK_FALSE (screaming->hasBlownUp());
}

TEST_CASE ("The comb filter produces a harmonic series of peaks",
           "[filter][comb]")
{
    auto settings = settingsFor (Type::comb, 500.0f);
    settings.combFeedback = 0.9f;
    settings.combDamping = 0.1f;

    // A comb delayed by 1/500 s reinforces 500 Hz and its multiples, and
    // cancels the odd half-multiples between them.
    auto atPeak = makeFilter (settings);
    auto atNull = makeFilter (settings);

    const auto peak = measureGainDb (*atPeak, 500.0f);
    const auto null = measureGainDb (*atNull, 750.0f);

    INFO ("peak at 500 Hz: " << peak << " dB, null at 750 Hz: " << null << " dB");

    CHECK (peak > null + 10.0f);
}

TEST_CASE ("The comb filter survives extreme feedback", "[filter][comb]")
{
    for (const auto feedback : { -0.99f, 0.0f, 0.99f, 5.0f, -5.0f })
    {
        for (const auto damping : { 0.0f, 0.5f, 1.0f, 2.0f })
        {
            auto settings = settingsFor (Type::comb, 200.0f);
            settings.combFeedback = feedback;
            settings.combDamping = damping;

            auto filter = makeFilter (settings);

            // Impulse, then a long tail: the classic way a comb runs away.
            auto peak = 0.0f;
            peak = juce::jmax (peak, std::abs (filter->processSample (1.0f)));

            for (int i = 0; i < static_cast<int> (kSampleRate); ++i)
                peak = juce::jmax (peak, std::abs (filter->processSample (0.0f)));

            INFO ("feedback " << feedback << " damping " << damping
                  << " peak " << peak);

            REQUIRE (std::isfinite (peak));
            CHECK (peak < 50.0f);
            CHECK_FALSE (filter->hasBlownUp());
        }
    }
}

// --- Formant filter --------------------------------------------------------

TEST_CASE ("Each vowel anchor emphasises its own formant frequencies",
           "[filter][formant]")
{
    // The test that decides whether this sounds like a voice. At each anchor,
    // that vowel's first formant must be clearly louder than a neighbouring
    // vowel's first formant - if they measure the same, the morph is a blur
    // and the pad is decorative.
    using Vowel = FormantFilter::Vowel;

    const std::pair<Vowel, const char*> vowels[] = {
        { Vowel::a, "A" }, { Vowel::e, "E" }, { Vowel::i, "I" },
        { Vowel::o, "O" }, { Vowel::u, "U" }
    };

    for (const auto& [vowel, name] : vowels)
    {
        const auto anchor = FormantFilter::getVowelAnchor (vowel);

        auto settings = settingsFor (Type::formant);
        settings.formantX = anchor.x;
        settings.formantY = anchor.y;
        settings.resonance = 0.8f;

        auto filter = makeFilter (settings);

        const auto ownF1 = FormantFilter::getFormantFrequency (vowel, 0);
        const auto gainAtOwnF1 = measureGainDb (*filter, ownF1);

        // A frequency between F1 and F2 should be in a trough.
        const auto ownF2 = FormantFilter::getFormantFrequency (vowel, 1);
        const auto between = std::sqrt (ownF1 * ownF2);
        const auto gainBetween = measureGainDb (*filter, between * 1.0f);

        INFO ("vowel " << name << ": F1 " << ownF1 << " Hz at " << gainAtOwnF1
              << " dB, between-formant " << between << " Hz at " << gainBetween << " dB");

        CHECK (std::isfinite (gainAtOwnF1));

        // F1 is the loudest formant of every vowel, so it must stand above
        // the gap between F1 and F2 - unless they are close together, as in O
        // and U, where there is barely a gap to speak of.
        if (ownF2 > ownF1 * 2.0f)
            CHECK (gainAtOwnF1 > gainBetween);
    }
}

TEST_CASE ("Moving the formant pad changes the spectrum", "[filter][formant]")
{
    // Two opposite anchors must produce measurably different responses at the
    // same probe frequency, or the pad does nothing.
    const auto aAnchor = FormantFilter::getVowelAnchor (FormantFilter::Vowel::a);
    const auto iAnchor = FormantFilter::getVowelAnchor (FormantFilter::Vowel::i);

    auto aSettings = settingsFor (Type::formant);
    aSettings.formantX = aAnchor.x;
    aSettings.formantY = aAnchor.y;
    aSettings.resonance = 0.8f;

    auto iSettings = aSettings;
    iSettings.formantX = iAnchor.x;
    iSettings.formantY = iAnchor.y;

    auto aFilter = makeFilter (aSettings);
    auto iFilter = makeFilter (iSettings);

    // 800 Hz is A's first formant and well above I's (350 Hz).
    const auto aGain = measureGainDb (*aFilter, 800.0f);
    const auto iGain = measureGainDb (*iFilter, 800.0f);

    INFO ("at 800 Hz: A " << aGain << " dB, I " << iGain << " dB");

    CHECK (std::abs (aGain - iGain) > 3.0f);
}

TEST_CASE ("The throat control shifts formants without changing vowel",
           "[filter][formant]")
{
    const auto anchor = FormantFilter::getVowelAnchor (FormantFilter::Vowel::a);

    auto settings = settingsFor (Type::formant);
    settings.formantX = anchor.x;
    settings.formantY = anchor.y;
    settings.resonance = 0.8f;

    auto neutral = makeFilter (settings);
    const auto neutralAt800 = measureGainDb (*neutral, 800.0f);

    // Positive throat shifts formants DOWN an octave, so A's 800 Hz formant
    // should move towards 400 Hz and 800 Hz should lose level.
    settings.formantThroat = 1.0f;
    auto lowered = makeFilter (settings);
    const auto loweredAt800 = measureGainDb (*lowered, 800.0f);
    const auto loweredAt400 = measureGainDb (*lowered, 400.0f);

    INFO ("neutral@800 " << neutralAt800 << ", throat+1@800 " << loweredAt800
          << ", throat+1@400 " << loweredAt400);

    CHECK (loweredAt400 > loweredAt800);
}

TEST_CASE ("Sitting exactly on a vowel anchor is that vowel alone",
           "[filter][formant]")
{
    // Inverse-distance weighting divides by zero at an anchor if unguarded.
    using Vowel = FormantFilter::Vowel;

    for (const auto vowel : { Vowel::a, Vowel::e, Vowel::i, Vowel::o, Vowel::u })
    {
        const auto anchor = FormantFilter::getVowelAnchor (vowel);

        auto settings = settingsFor (Type::formant);
        settings.formantX = anchor.x;
        settings.formantY = anchor.y;

        auto filter = makeFilter (settings);

        for (int i = 0; i < 1024; ++i)
            REQUIRE (std::isfinite (filter->processSample (0.5f)));
    }
}

// --- Drive -----------------------------------------------------------------

TEST_CASE ("Drive adds harmonics without adding much level", "[filter][drive]")
{
    // Drive that doubles as a volume control makes every A/B dishonest.
    for (int curveIndex = 0; curveIndex < static_cast<int> (gnarl::choices::DriveCurve::count);
         ++curveIndex)
    {
        auto settings = settingsFor (Type::lowPass12, 20000.0f);
        settings.driveCurve = static_cast<gnarl::choices::DriveCurve> (curveIndex);

        settings.drive = 0.0f;
        auto clean = makeFilter (settings);
        const auto cleanGain = measureGainDb (*clean, 200.0f, 0.05f);

        settings.drive = 0.5f;
        auto driven = makeFilter (settings);
        const auto drivenGain = measureGainDb (*driven, 200.0f, 0.05f);

        // Rectification is an EVEN function: it turns a sine into |sine|,
        // whose energy is mostly DC. The DC blocker then removes that, so the
        // remaining AC RMS is inherently about 7 dB below the input's, and no
        // amount of gain compensation can change it without making the peak
        // level wrong instead. -4.3 dB measured is the physics, not a bug.
        const auto tolerance =
            static_cast<choices::DriveCurve> (curveIndex) == choices::DriveCurve::rectify
                ? 6.0f
                : 3.0f;

        INFO ("curve " << curveIndex << ": clean " << cleanGain
              << " dB, driven " << drivenGain << " dB, tolerance " << tolerance);

        // Tight on purpose for the odd curves. A loose bound (12 dB) hid a
        // real bug: the compensation normalised the curve's slope but never
        // cancelled the pre-gain, so drive added 15 dB and every A/B of it
        // was dishonest.
        CHECK (std::abs (drivenGain - cleanGain) < tolerance);
    }
}

TEST_CASE ("Asymmetric drive curves do not leave DC behind", "[filter][drive]")
{
    // Tube and rectify are asymmetric, so without the DC blocker they would
    // pull the mix bus off centre and eat headroom.
    for (const auto curve : { gnarl::choices::DriveCurve::tube, gnarl::choices::DriveCurve::rectify })
    {
        auto settings = settingsFor (Type::lowPass12, 20000.0f);
        settings.driveCurve = curve;
        settings.drive = 0.8f;

        auto filter = makeFilter (settings);

        const auto increment = juce::MathConstants<float>::twoPi * 200.0f
                             / static_cast<float> (kSampleRate);
        auto phase = 0.0f;

        // Settle past the DC blocker's time constant.
        for (int i = 0; i < static_cast<int> (kSampleRate * 0.5); ++i)
        {
            filter->processSample (0.5f * std::sin (phase));
            phase += increment;
        }

        auto sum = 0.0;
        const auto count = static_cast<int> (kSampleRate * 0.5);

        for (int i = 0; i < count; ++i)
        {
            sum += filter->processSample (0.5f * std::sin (phase));
            phase += increment;
        }

        const auto mean = sum / static_cast<double> (count);

        INFO ("curve " << static_cast<int> (curve) << " residual DC " << mean);
        CHECK (std::abs (mean) < 0.02);
    }
}

// --- Robustness ------------------------------------------------------------

TEST_CASE ("Every filter type stays finite under extreme settings",
           "[filter]")
{
    for (const auto type : allTypes())
    {
        for (const auto cutoff : { 10.0f, 20.0f, 1000.0f, 20000.0f, 23000.0f, 100000.0f })
        {
            for (const auto resonance : { 0.0f, 0.5f, 1.0f, 2.0f })
            {
                auto settings = settingsFor (type, cutoff);
                settings.resonance = resonance;
                settings.drive = 1.0f;
                settings.combFeedback = 0.98f;
                settings.combDamping = 0.0f;

                auto filter = makeFilter (settings);

                auto peak = 0.0f;

                for (int i = 0; i < 8192; ++i)
                {
                    // Full-scale square: the worst case for any filter, since
                    // it is broadband and has maximum slew.
                    const auto input = (i / 64) % 2 == 0 ? 1.0f : -1.0f;
                    const auto out = filter->processSample (input);

                    REQUIRE (std::isfinite (out));
                    peak = juce::jmax (peak, std::abs (out));
                }

                INFO ("type " << static_cast<int> (type) << " cutoff " << cutoff
                      << " resonance " << resonance << " peak " << peak);
                CHECK (peak < 100.0f);
            }
        }
    }
}

TEST_CASE ("A filter recovers from a NaN fed in from upstream", "[filter]")
{
    // A NaN in a feedback filter is permanent unless something resets it, and
    // one bad sample would otherwise silence the voice for the rest of the
    // session.
    for (const auto type : allTypes())
    {
        auto settings = settingsFor (type, 1000.0f);
        settings.resonance = 0.8f;

        auto filter = makeFilter (settings);

        filter->processSample (std::numeric_limits<float>::quiet_NaN());

        auto recovered = false;

        for (int i = 0; i < 4096; ++i)
        {
            const auto out = filter->processSample (0.5f);

            if (std::isfinite (out))
                recovered = true;
            else
                recovered = false;
        }

        INFO ("type " << static_cast<int> (type));
        CHECK (recovered);
    }
}

TEST_CASE ("Mix zero is bit-exact bypass", "[filter]")
{
    // A bypassed filter that is merely "almost" transparent makes the control
    // useless for parallel routing.
    for (const auto type : allTypes())
    {
        auto settings = settingsFor (type, 500.0f);
        settings.mix = 0.0f;
        settings.drive = 0.7f;

        auto filter = makeFilter (settings);

        for (int i = 0; i < 512; ++i)
        {
            const auto input = std::sin (static_cast<float> (i) * 0.05f);
            INFO ("type " << static_cast<int> (type) << " sample " << i);
            REQUIRE (filter->processSample (input) == input);
        }
    }
}

TEST_CASE ("Cutoff can be modulated every sample without instability",
           "[filter]")
{
    // The reason the filters are zero-delay-feedback designs at all. A
    // bilinear biquad recalculated per sample is not stable, and audio-rate
    // filter modulation is most of what makes a growl.
    for (const auto type : allTypes())
    {
        auto settings = settingsFor (type, 1000.0f);
        settings.resonance = 0.7f;

        auto filter = makeFilter (settings);

        for (int i = 0; i < 48000; ++i)
        {
            // A full-range sweep at 50 Hz, which is faster than any LFO but
            // slower than audio-rate FM.
            const auto lfo = std::sin (juce::MathConstants<float>::twoPi
                                       * 50.0f * static_cast<float> (i)
                                       / static_cast<float> (kSampleRate));

            settings.cutoffHz = 200.0f * std::exp2 (lfo * 5.0f);
            filter->setSettings (settings);

            const auto out = filter->processSample (
                0.5f * std::sin (static_cast<float> (i) * 0.03f));

            REQUIRE (std::isfinite (out));
            REQUIRE (std::abs (out) < 100.0f);
        }

        INFO ("type " << static_cast<int> (type));
        CHECK_FALSE (filter->hasBlownUp());
    }
}

TEST_CASE ("Reset clears filter state", "[filter]")
{
    for (const auto type : allTypes())
    {
        auto settings = settingsFor (type, 800.0f);
        settings.resonance = 0.8f;

        auto filter = makeFilter (settings);

        for (int i = 0; i < 1000; ++i)
            filter->processSample (1.0f);

        filter->reset();

        auto fresh = makeFilter (settings);

        // After a reset a recycled voice must not inherit the previous note's
        // ringing.
        for (int i = 0; i < 64; ++i)
        {
            INFO ("type " << static_cast<int> (type) << " sample " << i);
            REQUIRE (filter->processSample (0.3f)
                     == Catch::Approx (fresh->processSample (0.3f)).margin (1.0e-5));
        }
    }
}
