#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"
#include "preset/PresetMorph.h"
#include "preset/PresetRandomiser.h"

#include <cmath>
#include <map>
#include <vector>

using namespace gnarl;
using Catch::Approx;
using Policy = preset::PresetRandomiser::Policy;

namespace
{
    constexpr double kSampleRate = 48000.0;

    std::unique_ptr<GnarlProcessor> preparedProcessor()
    {
        auto processor = std::make_unique<GnarlProcessor>();
        processor->setPlayConfigDetails (0, 2, kSampleRate, 256);
        processor->prepareToPlay (kSampleRate, 256);
        return processor;
    }

    float readNormalised (GnarlProcessor& processor, const char* id)
    {
        auto* parameter = processor.getValueTreeState().getParameter (id);
        REQUIRE (parameter != nullptr);
        return parameter->getValue();
    }

    void setNormalised (GnarlProcessor& processor, const char* id, float value)
    {
        auto* parameter = processor.getValueTreeState().getParameter (id);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (value);
    }
}

// ---------------------------------------------------------------------------
// Randomise: the classification
// ---------------------------------------------------------------------------

TEST_CASE ("The player's setup is never randomised", "[preset][random]")
{
    /*  A randomise that moves any of these breaks the instrument rather than
        varying it. A quiet patch because the dice chose -48 dB of master gain
        reads as a bug; so does a patch that silently switched to mono. */
    for (const auto* id : { pid::masterGain, pid::bypass, pid::maxVoices,
                            pid::polyMode, pid::pitchBendRange,
                            pid::oversampling, pid::velocityCurve })
    {
        INFO ("parameter " << id);
        CHECK (preset::PresetRandomiser::classify (id) == Policy::frozen);
    }
}

TEST_CASE ("Every parameter is classified, and enables are gated",
           "[preset][random]")
{
    /*  Asserted on the CLASSIFICATION rather than inferred from outcomes: a
        miscategorised parameter is the interesting failure here, and a
        statistical test for one needs thousands of draws to see what a single
        lookup answers directly. */
    auto processor = preparedProcessor();

    auto gated = 0;
    auto ranged = 0;
    auto frozen = 0;
    auto free = 0;

    for (auto* parameter : processor->getParameters())
    {
        auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter);
        REQUIRE (withID != nullptr);

        const auto policy = preset::PresetRandomiser::classify (withID->paramID);

        switch (policy)
        {
            case Policy::gated:  ++gated;  break;
            case Policy::ranged: ++ranged; break;
            case Policy::frozen: ++frozen; break;
            case Policy::free:   ++free;   break;
        }

        // Every enable is gated, with no exceptions - an enable drawn
        // uniformly is a coin flip, and fourteen of those is mush.
        if (withID->paramID.endsWith ("_enabled"))
        {
            INFO ("parameter " << withID->paramID);
            CHECK (policy == Policy::gated);
        }
    }

    INFO ("frozen " << frozen << ", gated " << gated << ", ranged " << ranged
          << ", free " << free);

    // Sanity on the shape of the split rather than exact counts, which would
    // just be a second copy of the classifier.
    CHECK (frozen >= 7);
    CHECK (gated >= 25);
    CHECK (ranged >= 25);
    CHECK (free > 100);
}

TEST_CASE ("A ranged parameter's dice stay inside a musical sub-range",
           "[preset][random]")
{
    // The point of the RANGED policy: a cutoff may go to 20 Hz because a user
    // may want that, but a roll that lands there is a wasted roll.
    const auto cutoff = preset::PresetRandomiser::getRange (pid::filter[0].cutoff);

    CHECK (cutoff.getStart() > 0.0f);
    CHECK (cutoff.getEnd() <= 1.0f);
    CHECK (cutoff.getLength() > 0.3f);   // still a real spread

    // An eight-second attack is not a patch, it is a mistake.
    const auto attack = preset::PresetRandomiser::getRange (pid::envelope[0].attack);
    CHECK (attack.getEnd() < 0.6f);
}

TEST_CASE ("The first oscillator is always on and the limiter usually is",
           "[preset][random]")
{
    // A patch with no oscillator is silence, which is never a useful roll;
    // one with no limiter can clip.
    CHECK (preset::PresetRandomiser::getProbability (pid::osc[0].enabled) == 1.0f);
    CHECK (preset::PresetRandomiser::getProbability (pid::fxLimiter.enabled) > 0.7f);

    // And the exotic effects are rare, so a patch is not a wash.
    CHECK (preset::PresetRandomiser::getProbability (pid::fxFlanger.enabled) < 0.25f);
    CHECK (preset::PresetRandomiser::getProbability (pid::fxPhaser.enabled) < 0.25f);
}

// ---------------------------------------------------------------------------
// Randomise: the behaviour
// ---------------------------------------------------------------------------

TEST_CASE ("Randomise leaves the frozen parameters exactly alone",
           "[preset][random]")
{
    auto processor = preparedProcessor();

    setNormalised (*processor, pid::masterGain, 0.8f);
    const auto before = readNormalised (*processor, pid::masterGain);

    juce::Random random (1234);
    preset::PresetRandomiser::apply (processor->getValueTreeState(), {}, random);

    CHECK (readNormalised (*processor, pid::masterGain) == before);
}

TEST_CASE ("Randomise does not turn everything on at once", "[preset][random]")
{
    /*  THE FAILURE THIS EXISTS FOR. Fourteen effects at even odds gives seven
        effects on, which is mush - a randomise button people press once and
        never again. Measured over many draws, because the question is about
        the distribution rather than about any one patch. */
    constexpr auto draws = 200;

    auto total = 0;
    auto worst = 0;

    const std::vector<const char*> fxEnables {
        pid::fxDistortion[0].enabled, pid::fxDistortion[1].enabled,
        pid::fxEq[0].enabled, pid::fxEq[1].enabled,
        pid::fxFilter[0].enabled, pid::fxFilter[1].enabled,
        pid::fxChorus.enabled, pid::fxFlanger.enabled, pid::fxPhaser.enabled,
        pid::fxHyper.enabled, pid::fxDimension.enabled, pid::fxDelay.enabled,
        pid::fxReverb.enabled,
    };

    for (int draw = 0; draw < draws; ++draw)
    {
        auto processor = preparedProcessor();

        juce::Random random (draw * 7919 + 13);
        preset::PresetRandomiser::apply (processor->getValueTreeState(), {}, random);

        auto on = 0;

        for (const auto* id : fxEnables)
            if (readNormalised (*processor, id) > 0.5f)
                ++on;

        total += on;
        worst = juce::jmax (worst, on);
    }

    const auto average = static_cast<double> (total) / draws;

    INFO ("average effects enabled: " << average << " of " << fxEnables.size()
          << ", worst case " << worst);

    // A coin flip over thirteen effects would average 6.5. The weighted
    // probabilities should land well under a third of them.
    CHECK (average < 4.0);
    CHECK (average > 0.5);   // and not so sparse that it does nothing
}

TEST_CASE ("Randomise always leaves an oscillator sounding", "[preset][random]")
{
    // Silence is never a useful roll.
    for (int draw = 0; draw < 50; ++draw)
    {
        auto processor = preparedProcessor();

        juce::Random random (draw * 104729 + 7);
        preset::PresetRandomiser::apply (processor->getValueTreeState(), {}, random);

        INFO ("draw " << draw);
        CHECK (readNormalised (*processor, pid::osc[0].enabled) > 0.5f);
    }
}

TEST_CASE ("A randomised patch makes finite sound", "[preset][random][audio]")
{
    /*  The end-to-end question: a randomise that produces a patch which
        crashes, NaNs or sits silent is worse than no randomise. Fifty patches
        through the real processor. */
    for (int draw = 0; draw < 50; ++draw)
    {
        auto processor = preparedProcessor();

        juce::Random random (draw * 31337 + 11);
        preset::PresetRandomiser::apply (processor->getValueTreeState(), {}, random);

        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 36, 1.0f), 0);

        auto peak = 0.0f;

        for (int block = 0; block < 24; ++block)
        {
            buffer.clear();
            processor->processBlock (buffer, midi);
            midi.clear();

            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < 256; ++i)
                {
                    const auto sample = buffer.getReadPointer (channel)[i];
                    INFO ("draw " << draw << ", block " << block);
                    REQUIRE (std::isfinite (sample));
                    peak = juce::jmax (peak, std::abs (sample));
                }
        }

        INFO ("draw " << draw << " peaks at " << peak);
        CHECK (peak > 1.0e-5f);
    }
}

TEST_CASE ("Randomise amount is a dial, not a switch", "[preset][random]")
{
    /*  A nudge is what a producer actually reaches for - "this is close, give
        me a variation" - and it is the setting that makes this worth having
        rather than a novelty. Measured as how far the patch moved. */
    const auto distanceAt = [] (float amount)
    {
        auto processor = preparedProcessor();

        // A known starting point in the middle, so movement in either
        // direction counts.
        for (auto* parameter : processor->getParameters())
            parameter->setValueNotifyingHost (0.5f);

        std::vector<float> before;

        for (auto* parameter : processor->getParameters())
            before.push_back (parameter->getValue());

        preset::PresetRandomiser::Options options;
        options.amount = amount;

        juce::Random random (98765);
        preset::PresetRandomiser::apply (processor->getValueTreeState(), options, random);

        auto sum = 0.0;
        auto index = std::size_t { 0 };

        for (auto* parameter : processor->getParameters())
        {
            sum += std::abs (static_cast<double> (parameter->getValue()) - before[index]);
            ++index;
        }

        return sum / static_cast<double> (before.size());
    };

    const auto none = distanceAt (0.0f);
    const auto nudge = distanceAt (0.15f);
    const auto full = distanceAt (1.0f);

    INFO ("mean movement: 0% -> " << none << ", 15% -> " << nudge
          << ", 100% -> " << full);

    CHECK (none == 0.0);          // nothing at all at zero
    CHECK (nudge > 0.0);
    CHECK (nudge < full * 0.5);   // and a nudge is much less than a reroll
}

TEST_CASE ("Randomise can be limited to one section", "[preset][random]")
{
    // Most of how this gets used once someone has a patch they like.
    auto processor = preparedProcessor();

    for (auto* parameter : processor->getParameters())
        parameter->setValueNotifyingHost (0.5f);

    preset::PresetRandomiser::Options options;
    options.oscillators = false;
    options.filters = false;
    options.envelopes = false;
    options.lfos = false;
    options.modMatrix = false;
    options.fx = true;

    juce::Random random (424242);
    preset::PresetRandomiser::apply (processor->getValueTreeState(), options, random);

    // The oscillator section is untouched...
    CHECK (readNormalised (*processor, pid::osc[0].tablePos) == 0.5f);
    CHECK (readNormalised (*processor, pid::filter[0].cutoff) == 0.5f);
    CHECK (readNormalised (*processor, pid::envelope[0].decay) == 0.5f);

    // ...and something in the FX did move. Checked across several rather than
    // one, because any single parameter could be drawn back to where it was.
    auto moved = 0;

    for (const auto* id : { pid::fxReverb.size, pid::fxDelay.feedback,
                            pid::fxDistortion[0].drive, pid::fxChorus.depth,
                            pid::fxPhaser.centre, pid::fxEq[0].band1Gain })
    {
        if (std::abs (readNormalised (*processor, id) - 0.5f) > 1.0e-6f)
            ++moved;
    }

    CHECK (moved > 0);
}

// ---------------------------------------------------------------------------
// Morph
// ---------------------------------------------------------------------------

TEST_CASE ("A choice parameter has no midpoint and is treated as discrete",
           "[preset][morph]")
{
    /*  THE ONLY INTERESTING THING ABOUT MORPHING. There is nothing half way
        between tanh and bitcrush, and interpolating the choice index lands on
        `fold` on the way past - which is neither of the two things the user
        asked to morph between. */
    for (const auto* id : { pid::fxDistortion[0].type, pid::filter[0].type,
                            pid::lfo[0].shape, pid::osc[0].mode,
                            pid::fxReverb.enabled, pid::sub.waveform,
                            pid::fxDelay.division, pid::polyMode })
    {
        INFO ("parameter " << id);
        CHECK (preset::PresetMorph::isDiscrete (id));
    }

    // And the things that genuinely do have a midpoint do not step.
    for (const auto* id : { pid::filter[0].cutoff, pid::fxReverb.mix,
                            pid::envelope[0].decay, pid::osc[0].tablePos,
                            pid::fxDelay.feedback })
    {
        INFO ("parameter " << id);
        CHECK_FALSE (preset::PresetMorph::isDiscrete (id));
    }
}

TEST_CASE ("Morphing at the ends gives the endpoints", "[preset][morph]")
{
    auto source = preparedProcessor();

    setNormalised (*source, pid::filter[0].cutoff, 0.2f);
    setNormalised (*source, pid::fxReverb.mix, 0.1f);
    const auto a = source->getPresets().capture ({});

    setNormalised (*source, pid::filter[0].cutoff, 0.8f);
    setNormalised (*source, pid::fxReverb.mix, 0.9f);
    const auto b = source->getPresets().capture ({});

    auto target = preparedProcessor();
    auto& state = target->getValueTreeState();

    REQUIRE (preset::PresetMorph::apply (state, a, b, 0.0f));
    CHECK (readNormalised (*target, pid::filter[0].cutoff) == Approx (0.2f).margin (0.002f));

    REQUIRE (preset::PresetMorph::apply (state, a, b, 1.0f));
    CHECK (readNormalised (*target, pid::filter[0].cutoff) == Approx (0.8f).margin (0.002f));
}

TEST_CASE ("A continuous parameter morphs smoothly", "[preset][morph]")
{
    auto source = preparedProcessor();

    setNormalised (*source, pid::filter[0].cutoff, 0.0f);
    const auto a = source->getPresets().capture ({});

    setNormalised (*source, pid::filter[0].cutoff, 1.0f);
    const auto b = source->getPresets().capture ({});

    auto target = preparedProcessor();
    auto& state = target->getValueTreeState();

    // Monotonic across the sweep, which is what "smoothly" means here.
    auto previous = -1.0f;

    for (int step = 0; step <= 10; ++step)
    {
        const auto position = static_cast<float> (step) / 10.0f;

        REQUIRE (preset::PresetMorph::apply (state, a, b, position));

        const auto value = readNormalised (*target, pid::filter[0].cutoff);

        INFO ("at " << position << " the cutoff is " << value);
        CHECK (value >= previous);
        previous = value;
    }

    CHECK (previous == Approx (1.0f).margin (0.002f));
}

TEST_CASE ("A discrete parameter steps once, in the middle", "[preset][morph]")
{
    /*  So a morph control sweeps smoothly and changes character ONCE rather
        than walking through every entry in the list on the way. */
    auto source = preparedProcessor();

    auto* type = source->getValueTreeState().getParameter (pid::fxDistortion[0].type);
    REQUIRE (type != nullptr);

    type->setValueNotifyingHost (0.0f);       // the first curve
    const auto a = source->getPresets().capture ({});

    type->setValueNotifyingHost (1.0f);       // the last curve
    const auto b = source->getPresets().capture ({});

    auto target = preparedProcessor();
    auto& state = target->getValueTreeState();

    std::vector<float> seen;

    for (int step = 0; step <= 20; ++step)
    {
        const auto position = static_cast<float> (step) / 20.0f;
        REQUIRE (preset::PresetMorph::apply (state, a, b, position));
        seen.push_back (readNormalised (*target, pid::fxDistortion[0].type));
    }

    // Exactly two distinct values across the whole sweep: A's and B's.
    std::vector<float> distinct;

    for (const auto value : seen)
        if (std::none_of (distinct.begin(), distinct.end(),
                          [value] (float other) { return std::abs (other - value) < 1.0e-6f; }))
            distinct.push_back (value);

    INFO ("distinct curve values across the morph: " << distinct.size());
    CHECK (distinct.size() == 2);

    // And the change happens at the half-way point.
    CHECK (seen.front() == Approx (seen[4]).margin (1.0e-6f));
    CHECK (seen.back() == Approx (seen[16]).margin (1.0e-6f));
    CHECK (std::abs (seen.front() - seen.back()) > 1.0e-6f);
}

TEST_CASE ("Morphing refuses anything that is not two presets",
           "[preset][morph]")
{
    auto processor = preparedProcessor();
    auto& state = processor->getValueTreeState();

    const auto valid = processor->getPresets().capture ({});

    setNormalised (*processor, pid::filter[0].cutoff, 0.42f);
    const auto before = readNormalised (*processor, pid::filter[0].cutoff);

    CHECK_FALSE (preset::PresetMorph::apply (state, {}, valid, 0.5f));
    CHECK_FALSE (preset::PresetMorph::apply (state, valid, {}, 0.5f));
    CHECK_FALSE (preset::PresetMorph::apply (state, juce::ValueTree { "OTHER" },
                                             valid, 0.5f));

    // A refused morph changes nothing, rather than half-applying.
    CHECK (readNormalised (*processor, pid::filter[0].cutoff) == before);
}

TEST_CASE ("A morph makes finite sound all the way across",
           "[preset][morph][audio]")
{
    auto source = preparedProcessor();

    juce::Random random (5150);
    preset::PresetRandomiser::apply (source->getValueTreeState(), {}, random);
    const auto a = source->getPresets().capture ({});

    juce::Random other (8675309);
    preset::PresetRandomiser::apply (source->getValueTreeState(), {}, other);
    const auto b = source->getPresets().capture ({});

    auto target = preparedProcessor();
    auto& state = target->getValueTreeState();

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, 1.0f), 0);

    for (int step = 0; step <= 20; ++step)
    {
        REQUIRE (preset::PresetMorph::apply (state, a, b,
                                             static_cast<float> (step) / 20.0f));

        for (int block = 0; block < 4; ++block)
        {
            buffer.clear();
            target->processBlock (buffer, midi);
            midi.clear();

            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < 256; ++i)
                {
                    INFO ("morph step " << step);
                    REQUIRE (std::isfinite (buffer.getReadPointer (channel)[i]));
                }
        }
    }
}
