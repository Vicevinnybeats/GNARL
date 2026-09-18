#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"
#include "dsp/FxRack.h"
#include "params/FxOrderBridge.h"

#include <cmath>
#include <vector>

using namespace gnarl;
using Slot = choices::FxSlot;

namespace
{
    constexpr double kSampleRate = 48000.0;

    /** Sets a parameter by ID to a normalised value. */
    void setNormalised (GnarlProcessor& processor, const char* id, float normalised)
    {
        auto* parameter = processor.getValueTreeState().getParameter (id);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (normalised);
    }

    /** Sets a parameter by ID to a real value, converting through its range. */
    void setValue (GnarlProcessor& processor, const char* id, float value)
    {
        auto* parameter = processor.getValueTreeState().getParameter (id);
        REQUIRE (parameter != nullptr);

        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
        else
            parameter->setValueNotifyingHost (value);
    }

    std::unique_ptr<GnarlProcessor> preparedProcessor (int blockSize = 256)
    {
        auto processor = std::make_unique<GnarlProcessor>();
        processor->setPlayConfigDetails (0, 2, kSampleRate, blockSize);
        processor->prepareToPlay (kSampleRate, blockSize);
        return processor;
    }

    /** Renders a note and returns the output, so two chains can be compared. */
    std::vector<float> render (GnarlProcessor& processor, int blocks, int blockSize = 256)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 36, 1.0f), 0);

        std::vector<float> output;
        output.reserve (static_cast<std::size_t> (blocks * blockSize));

        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
            midi.clear();

            for (int i = 0; i < blockSize; ++i)
                output.push_back (buffer.getReadPointer (0)[i]);
        }

        return output;
    }

    double rmsOf (const std::vector<float>& samples)
    {
        auto sumSquares = 0.0;

        for (const auto value : samples)
            sumSquares += static_cast<double> (value) * value;

        return std::sqrt (sumSquares / std::max<std::size_t> (1, samples.size()));
    }
}

TEST_CASE ("Every FX instance defaults to disabled", "[fx][rack]")
{
    /*  Fourteen effects in the chain, and a patch that has not touched them
        must sound exactly as it did before the rack existed. This is also what
        keeps preset compatibility: the FX parameters were appended to the
        layout, so an old preset carries none of them and they all default off.
    */
    auto processor = preparedProcessor();
    auto& apvts = processor->getValueTreeState();

    const auto checkOff = [&apvts] (const char* id)
    {
        auto* parameter = apvts.getParameter (id);
        REQUIRE (parameter != nullptr);
        INFO ("parameter " << id);
        CHECK (parameter->getValue() < 0.5f);
    };

    for (std::size_t i = 0; i < pid::kNumFxDistortions; ++i)
        checkOff (pid::fxDistortion[i].enabled);

    for (std::size_t i = 0; i < pid::kNumFxEqs; ++i)
        checkOff (pid::fxEq[i].enabled);

    for (std::size_t i = 0; i < pid::kNumFxFilters; ++i)
        checkOff (pid::fxFilter[i].enabled);

    for (const auto* id : { pid::fxChorus.enabled, pid::fxFlanger.enabled,
                            pid::fxPhaser.enabled, pid::fxHyper.enabled,
                            pid::fxDimension.enabled, pid::fxDelay.enabled,
                            pid::fxReverb.enabled, pid::fxLimiter.enabled })
        checkOff (id);
}

TEST_CASE ("A rack with everything disabled changes nothing", "[fx][rack][audio]")
{
    // The whole chain runs every block. With nothing enabled the effects are
    // each transparent, so the total must be too - bit-identically, since a
    // chain of fourteen nearly-transparent effects would not be.
    auto processor = preparedProcessor();

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;

    // No note: just the silence a synth outputs at rest.
    for (int block = 0; block < 8; ++block)
    {
        buffer.clear();
        processor->processBlock (buffer, midi);

        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 256; ++i)
                CHECK (buffer.getReadPointer (channel)[i] == 0.0f);
    }
}

TEST_CASE ("An enabled effect changes the output", "[fx][rack][audio]")
{
    // The end-to-end connection test: a parameter in the tree has to reach the
    // effect in the rack. A silently unconnected relay is the failure mode
    // that passes every unit test.
    auto clean = preparedProcessor();
    const auto dry = render (*clean, 12);

    auto wet = preparedProcessor();
    setNormalised (*wet, pid::fxReverb.enabled, 1.0f);
    setValue (*wet, pid::fxReverb.mix, 1.0f);
    const auto reverbed = render (*wet, 12);

    REQUIRE (rmsOf (dry) > 1.0e-5);

    auto largestDifference = 0.0f;

    for (std::size_t i = 0; i < dry.size(); ++i)
        largestDifference = juce::jmax (largestDifference,
                                        std::abs (dry[i] - reverbed[i]));

    INFO ("largest difference from enabling the reverb: " << largestDifference);
    CHECK (largestDifference > 1.0e-4f);
}

TEST_CASE ("The chain order changes the sound", "[fx][rack][audio]")
{
    /*  THE POINT OF THE WHOLE ARCHITECTURE. If the order did not change the
        result, the reordering UI would be decoration. Distortion into a filter
        and a filter into distortion are genuinely different: the first
        saturates the full-bandwidth signal and then removes harmonics, the
        second removes them first and then generates new ones from what is
        left.

        This is also the test that catches the order being published but never
        read - the failure that leaves a reorderable rack running in a fixed
        order, which nothing else here would notice. */
    const auto renderWith = [] (bool distortionFirst)
    {
        auto processor = preparedProcessor();

        setNormalised (*processor, pid::fxDistortion[0].enabled, 1.0f);
        setValue (*processor, pid::fxDistortion[0].drive, 30.0f);
        setValue (*processor, pid::fxDistortion[0].mix, 1.0f);

        setNormalised (*processor, pid::fxFilter[0].enabled, 1.0f);
        setValue (*processor, pid::fxFilter[0].cutoff, 400.0f);
        setValue (*processor, pid::fxFilter[0].mix, 1.0f);

        // Everything else stays in its default position; only the two active
        // effects are swapped, so nothing else can account for a difference.
        std::array<Slot, dsp::FxOrder::kNumSlots> order {};

        for (std::size_t i = 0; i < order.size(); ++i)
            order[i] = static_cast<Slot> (i);

        if (! distortionFirst)
        {
            const auto distortionPosition = static_cast<std::size_t> (Slot::distortion1);
            const auto filterPosition = static_cast<std::size_t> (Slot::filter1);

            std::swap (order[distortionPosition], order[filterPosition]);
        }

        REQUIRE (processor->getFxOrder().setOrder (order));

        return render (*processor, 12);
    };

    const auto distortionFirst = renderWith (true);
    const auto filterFirst = renderWith (false);

    REQUIRE (rmsOf (distortionFirst) > 1.0e-5);

    auto largestDifference = 0.0f;

    for (std::size_t i = 0; i < distortionFirst.size(); ++i)
        largestDifference = juce::jmax (largestDifference,
                                        std::abs (distortionFirst[i] - filterFirst[i]));

    INFO ("largest difference from swapping the distortion and the filter: "
          << largestDifference);
    CHECK (largestDifference > 1.0e-3f);
}

TEST_CASE ("The chain order survives a state round trip", "[fx][rack]")
{
    /*  The order is ValueTree state rather than a parameter, so nothing else
        saves it. A reorder that is lost on reload is a preset that does not
        recall - which for a chain of fourteen effects is most of the patch. */
    auto processor = preparedProcessor();

    std::array<Slot, dsp::FxOrder::kNumSlots> order {};

    for (std::size_t i = 0; i < order.size(); ++i)
        order[i] = static_cast<Slot> (order.size() - 1 - i);

    REQUIRE (processor->getFxOrder().setOrder (order));

    juce::MemoryBlock state;
    processor->getStateInformation (state);

    auto reloaded = preparedProcessor();
    reloaded->setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    const auto& recalled = reloaded->getFxOrder().getOrder();

    for (std::size_t i = 0; i < order.size(); ++i)
    {
        INFO ("position " << i);
        CHECK (recalled.getSlot (i) == order[i]);
    }
}

TEST_CASE ("A malformed order in the tree falls back to the default",
           "[fx][rack]")
{
    /*  A hand-edited or truncated preset must not be able to produce a chain
        that runs one effect twice and another never. Rejecting it outright is
        the only safe answer: patching it up would give a chain the user never
        asked for and cannot see. */
    auto processor = preparedProcessor();
    auto& bridge = processor->getFxOrder();

    // Every slot the same: not a permutation.
    std::array<Slot, dsp::FxOrder::kNumSlots> duplicated {};
    duplicated.fill (Slot::reverb);

    CHECK_FALSE (bridge.setOrder (duplicated));
    CHECK (bridge.getOrder().isDefault());

    // And a branch written straight into the tree with nonsense in it.
    auto branch = processor->getValueTreeState().state.getOrCreateChildWithName (
        params::FxOrderBridge::kOrderType, nullptr);
    branch.setProperty (params::FxOrderBridge::kSlotsProperty,
                        "Reverb|Not An Effect|Limiter", nullptr);

    CHECK (bridge.getOrder().isDefault());
}

TEST_CASE ("Moving a slot reorders the chain", "[fx][rack]")
{
    auto processor = preparedProcessor();
    auto& bridge = processor->getFxOrder();

    REQUIRE (bridge.getOrder().isDefault());

    // The limiter is last by default; drag it to the front.
    const auto last = dsp::FxOrder::kNumSlots - 1;
    REQUIRE (bridge.move (last, 0));

    const auto& order = bridge.getOrder();

    CHECK (order.getSlot (0) == Slot::limiter);
    CHECK (order.getSlot (1) == Slot::distortion1);

    // And every slot is still present exactly once - a move must not be able
    // to lose one.
    std::array<bool, dsp::FxOrder::kNumSlots> seen {};

    for (std::size_t i = 0; i < dsp::FxOrder::kNumSlots; ++i)
        seen[static_cast<std::size_t> (order.getSlot (i))] = true;

    for (const auto present : seen)
        CHECK (present);
}

TEST_CASE ("Enabling a distortion switches the rack to oversampling",
           "[fx][rack][audio]")
{
    /*  A memoryless waveshaper generates harmonics above Nyquist by
        construction, and no filter after it can remove the ones that fold
        down. So a distortion in the chain has to run oversampled - and the
        check is per block, so a patch with no distortion pays nothing. */
    auto processor = preparedProcessor();

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;

    buffer.clear();
    processor->processBlock (buffer, midi);

    // Reported latency with nothing nonlinear enabled.
    const auto cleanLatency = processor->getLatencySamples();

    setNormalised (*processor, pid::fxDistortion[0].enabled, 1.0f);

    buffer.clear();
    processor->processBlock (buffer, midi);

    const auto drivenLatency = processor->getLatencySamples();

    INFO ("latency: " << cleanLatency << " clean, " << drivenLatency << " driven");

    // The oversampler's filters add latency, which is how the switch is
    // visible from outside - and reporting it is what stops the instrument
    // sitting early against the session.
    CHECK (drivenLatency > cleanLatency);
}

TEST_CASE ("A clean FX filter does not pay for oversampling", "[fx][rack][audio]")
{
    // The FILTER is linear; only its drive is not. A patch using it clean must
    // not be charged for oversampling it does not need.
    auto processor = preparedProcessor();

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;

    buffer.clear();
    processor->processBlock (buffer, midi);
    const auto before = processor->getLatencySamples();

    setNormalised (*processor, pid::fxFilter[0].enabled, 1.0f);
    setValue (*processor, pid::fxFilter[0].drive, 0.0f);

    buffer.clear();
    processor->processBlock (buffer, midi);

    CHECK (processor->getLatencySamples() == before);

    // And with the drive up it does.
    setValue (*processor, pid::fxFilter[0].drive, 1.0f);

    buffer.clear();
    processor->processBlock (buffer, midi);

    CHECK (processor->getLatencySamples() > before);
}

TEST_CASE ("The whole rack enabled at once stays finite", "[fx][rack][audio]")
{
    /*  Nobody would use this patch, which is the point: fourteen effects all
        on, several with feedback, feeding each other. If any pair of them can
        run away together this is where it shows.

        Run through the real processor rather than the rack alone, because what
        reaches the rack is the voice section's output - which is where the
        levels actually come from. */
    auto processor = preparedProcessor();

    for (std::size_t i = 0; i < pid::kNumFxDistortions; ++i)
    {
        setNormalised (*processor, pid::fxDistortion[i].enabled, 1.0f);
        setValue (*processor, pid::fxDistortion[i].drive, 48.0f);
    }

    for (std::size_t i = 0; i < pid::kNumFxEqs; ++i)
    {
        setNormalised (*processor, pid::fxEq[i].enabled, 1.0f);
        setValue (*processor, pid::fxEq[i].band1Gain, 18.0f);
        setValue (*processor, pid::fxEq[i].lowShelfGain, 18.0f);
    }

    for (std::size_t i = 0; i < pid::kNumFxFilters; ++i)
    {
        setNormalised (*processor, pid::fxFilter[i].enabled, 1.0f);
        setValue (*processor, pid::fxFilter[i].resonance, 1.0f);
        setValue (*processor, pid::fxFilter[i].drive, 1.0f);
    }

    for (const auto* id : { pid::fxChorus.enabled, pid::fxFlanger.enabled,
                            pid::fxPhaser.enabled, pid::fxHyper.enabled,
                            pid::fxDimension.enabled, pid::fxDelay.enabled,
                            pid::fxReverb.enabled, pid::fxLimiter.enabled })
        setNormalised (*processor, id, 1.0f);

    setValue (*processor, pid::fxDelay.feedback, 1.0f);
    setValue (*processor, pid::fxChorus.feedback, 1.0f);
    setValue (*processor, pid::fxFlanger.feedback, 1.0f);
    setValue (*processor, pid::fxReverb.decay, 1.0f);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, 1.0f), 0);

    for (int block = 0; block < 400; ++block)
    {
        buffer.clear();
        processor->processBlock (buffer, midi);
        midi.clear();

        if (block == 100)
        {
            midi.addEvent (juce::MidiMessage::noteOff (1, 36), 0);
        }

        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 256; ++i)
            {
                INFO ("block " << block);
                REQUIRE (std::isfinite (buffer.getReadPointer (channel)[i]));
            }
    }
}

TEST_CASE ("The rack is block-size invariant", "[fx][rack][audio]")
{
    /*  The assertion shape that found the voice filter's shared stereo state
        three layers below where anyone was looking (CLAUDE.md section 3). If
        rendering the same note in 64-sample blocks and in 512-sample blocks
        does not give the same samples, something in the chain is carrying
        state across a boundary it should not.

        The limiter is left OUT: its look-ahead makes the output a function of
        samples the short-block render has not reached yet at the same index,
        which is a real and intended dependence on the future rather than on
        the block layout. */
    const auto renderAt = [] (int blockSize, int totalSamples)
    {
        auto processor = preparedProcessor (512);

        setNormalised (*processor, pid::fxDistortion[0].enabled, 1.0f);
        setValue (*processor, pid::fxDistortion[0].drive, 18.0f);
        setNormalised (*processor, pid::fxEq[0].enabled, 1.0f);
        setValue (*processor, pid::fxEq[0].band1Gain, 12.0f);
        setNormalised (*processor, pid::fxFilter[0].enabled, 1.0f);
        setNormalised (*processor, pid::fxChorus.enabled, 1.0f);
        setNormalised (*processor, pid::fxDelay.enabled, 1.0f);
        setNormalised (*processor, pid::fxDimension.enabled, 1.0f);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 36, 1.0f), 0);

        std::vector<float> output;
        output.reserve (static_cast<std::size_t> (totalSamples));

        while (static_cast<int> (output.size()) < totalSamples)
        {
            buffer.clear();
            processor->processBlock (buffer, midi);
            midi.clear();

            for (int i = 0; i < blockSize; ++i)
                output.push_back (buffer.getReadPointer (0)[i]);
        }

        output.resize (static_cast<std::size_t> (totalSamples));
        return output;
    };

    constexpr auto totalSamples = 512 * 8;

    const auto small = renderAt (64, totalSamples);
    const auto large = renderAt (512, totalSamples);

    auto largestDifference = 0.0f;

    for (std::size_t i = 0; i < small.size(); ++i)
        largestDifference = juce::jmax (largestDifference,
                                        std::abs (small[i] - large[i]));

    INFO ("largest difference between 64- and 512-sample blocks: "
          << largestDifference);

    /*  BIT-IDENTICAL, and asserted as such rather than with a tolerance. It
        measured 0.002 when written, which was a real bug: the chorus advances
        its one shared LFO phase on channel 0 only, and the rack processed the
        whole left channel and then the whole right, so the right channel's
        modulation froze at whatever phase the left pass had reached - a value
        set entirely by the block size. FxRack::runPerChannel interleaves the
        channels now.

        A tolerance here would have let that through. It is the same trap
        CLAUDE.md records for the drive stage, where a loose threshold hid
        +15 dB of level: if this ever stops being exact, that is a finding,
        not a number to raise. */
    CHECK (largestDifference == 0.0f);
}

TEST_CASE ("The rack handles a mono output bus", "[fx][rack][audio]")
{
    /*  A mono host must get ONE channel processed once, not the same
        per-channel effect advanced twice over one buffer: advancing a filter
        twice in a sample with different inputs corrupts its state, which is
        the bug the OTT crossover shipped. */
    auto processor = std::make_unique<GnarlProcessor>();
    processor->setPlayConfigDetails (0, 1, kSampleRate, 256);
    processor->prepareToPlay (kSampleRate, 256);

    setNormalised (*processor, pid::fxFilter[0].enabled, 1.0f);
    setNormalised (*processor, pid::fxEq[0].enabled, 1.0f);
    setValue (*processor, pid::fxEq[0].band1Gain, 12.0f);
    setNormalised (*processor, pid::fxDelay.enabled, 1.0f);

    juce::AudioBuffer<float> buffer (1, 256);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, 1.0f), 0);

    for (int block = 0; block < 32; ++block)
    {
        buffer.clear();
        processor->processBlock (buffer, midi);
        midi.clear();

        for (int i = 0; i < 256; ++i)
            REQUIRE (std::isfinite (buffer.getReadPointer (0)[i]));
    }
}

TEST_CASE ("The rack survives a double-precision host", "[fx][rack][audio]")
{
    // The rack is float-only, so a double buffer goes through a scratch. The
    // chunking matters: a host handing over more samples than it declared must
    // not overrun it.
    auto processor = preparedProcessor (256);

    setNormalised (*processor, pid::fxDistortion[0].enabled, 1.0f);
    setNormalised (*processor, pid::fxDelay.enabled, 1.0f);
    setNormalised (*processor, pid::fxReverb.enabled, 1.0f);

    juce::AudioBuffer<double> buffer (2, 1024);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, 1.0f), 0);

    for (int block = 0; block < 16; ++block)
    {
        buffer.clear();
        processor->processBlock (buffer, midi);
        midi.clear();

        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                REQUIRE (std::isfinite (buffer.getReadPointer (channel)[i]));
    }
}

TEST_CASE ("Switching the oversampling factor does not silence the rack",
           "[fx][rack][audio]")
{
    /*  Enabling a distortion changes the rack's internal sample rate, which
        re-pushes every effect's settings. If that went through prepare()
        instead of setSampleRate() it would reset every delay line and the
        reverb tail - so a user adding a distortion would hear the delay and
        reverb cut out. That is the same family of bug as the oversampling
        control silencing held voices (CLAUDE.md section 3). */
    auto processor = preparedProcessor();

    setNormalised (*processor, pid::fxDelay.enabled, 1.0f);
    setValue (*processor, pid::fxDelay.mix, 1.0f);
    setValue (*processor, pid::fxDelay.feedback, 0.8f);

    /*  SYNC OFF, with an explicit short time. The delay's default division is
        a dotted eighth, which at the default 120 BPM is 375 ms - about 70
        blocks of 256 samples, so the first version of this test measured the
        silence BEFORE the first repeat had arrived and read it as the tail
        having been wiped. Nothing was wrong with the delay. */
    setNormalised (*processor, pid::fxDelay.syncEnabled, 0.0f);
    setValue (*processor, pid::fxDelay.timeMs, 20.0f);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, 1.0f), 0);

    // Hold the note long enough to fill the line with repeats, then release it
    // so that what is left sounding is only the delay.
    for (int block = 0; block < 60; ++block)
    {
        buffer.clear();
        processor->processBlock (buffer, midi);
        midi.clear();

        if (block == 50)
            midi.addEvent (juce::MidiMessage::noteOff (1, 36), 0);
    }

    const auto peakOfNextBlocks = [&] (int blocks)
    {
        auto peak = 0.0f;

        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            processor->processBlock (buffer, midi);
            midi.clear();

            for (int i = 0; i < 256; ++i)
                peak = juce::jmax (peak, std::abs (buffer.getReadPointer (0)[i]));
        }

        return peak;
    };

    const auto before = peakOfNextBlocks (4);
    REQUIRE (before > 1.0e-5f);

    // Now switch the factor mid-tail.
    setNormalised (*processor, pid::fxDistortion[0].enabled, 1.0f);

    const auto after = peakOfNextBlocks (4);

    INFO ("delay tail peaks at " << before << " before the switch, "
          << after << " after");

    // The tail keeps going. Not identical - the chain genuinely changed - but
    // nowhere near silence.
    CHECK (after > before * 0.1f);
}
