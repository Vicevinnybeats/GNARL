#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"
#include "params/ParameterIDs.h"

#include <cmath>
#include <utility>

using namespace gnarl;

/*
    The output and gain-reduction meters, tested through processBlock.

    The interesting property is not "the number is roughly right" - it is that
    the number does NOT depend on how the host chops the audio up. The UI
    samples at 60 Hz while a host at 48 kHz / 256 calls back at 187 Hz, so a
    meter holding the last block's peak shows whichever two blocks in three
    the timer happened to land on. That is the same family as the
    filter-per-channel and shared-LFO bugs in CLAUDE.md section 3: a quantity
    that is a function of the BLOCK LAYOUT rather than of the signal.

    Writing this test found exactly that, in the first version of the code it
    was written for: the decay coefficient came from the block size
    prepareToPlay was HANDED, so a host sending shorter blocks than it
    declared decayed the needle faster per second than one that did not.
*/

namespace
{
    constexpr double kSampleRate = 48000.0;

    std::unique_ptr<GnarlProcessor> makeProcessor (int blockSize)
    {
        auto processor = std::make_unique<GnarlProcessor>();
        processor->prepareToPlay (kSampleRate, blockSize);
        return processor;
    }

    /** Renders a note, then silence, in blocks of the given size. */
    void render (GnarlProcessor& processor, int blockSize, int totalSamples,
                 int midiNote = -1)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);

        for (int rendered = 0; rendered < totalSamples; rendered += blockSize)
        {
            juce::MidiBuffer midi;

            if (rendered == 0 && midiNote >= 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, midiNote, 1.0f), 0);

            buffer.setSize (2, blockSize, false, false, true);
            processor.processBlock (buffer, midi);
        }
    }
}

TEST_CASE ("The output meter reads silence as exactly the floor", "[meters]")
{
    auto processor = makeProcessor (256);

    render (*processor, 256, 48000);

    const auto meters = processor->getMeterSnapshot();

    /*  EXACTLY the floor, not merely near it. The decaying peak snaps to zero
        below -80 dBFS rather than approaching it forever, and that is not
        cosmetic: the editor drops a frame identical to the last one, so an
        ever-smaller number would have the plugin pushing sixty messages a
        second at an empty room for as long as its window is open. A TPT
        filter's idle state does not reach zero on its own (section 3) and
        neither does this one. */
    CHECK (meters.outputDb[0] == GnarlProcessor::kMeterFloorDb);
    CHECK (meters.outputDb[1] == GnarlProcessor::kMeterFloorDb);
}

TEST_CASE ("The output meter reads the level that leaves the plugin", "[meters]")
{
    auto processor = makeProcessor (256);

    render (*processor, 256, 24000, 36);

    const auto meters = processor->getMeterSnapshot();

    // Something is playing, and it is not pinned at full scale either.
    CHECK (meters.outputDb[0] > -40.0f);
    CHECK (meters.outputDb[0] <= 0.0f);
}

TEST_CASE ("The meter's fall time does not depend on the block size", "[meters]")
{
    /*  THE POINT OF THE WHOLE DESIGN. Same elapsed time, different chunking -
        the needle must land in the same place.

        THE DECAY IS MEASURED IN ISOLATION, under bypass, and that is not
        incidental. The first version of this case rendered a sustained note
        in two block sizes and compared the readings - and passed with the
        bug deliberately put back, because a note that is still sounding
        makes the block's own peak larger than anything the decay did to the
        held value. It was a test of the note, not of the ballistics. Getting
        the level up and then measuring purely how it falls is the only shape
        that can see this.

        Both processors are prepared for the LARGEST block they will see, so
        this covers the case the implementation got wrong: a host that
        declares 512 and then sends 64. */
    constexpr int kPreparedBlock = 512;
    /*  A whole number of BOTH block sizes: the render loop below advances by
        whole blocks, so 48000 samples is 94 blocks of 512 (48128) and 750 of
        64 (48000), and the runs would differ by 128 samples of decay - about
        0.05 dB - for reasons that have nothing to do with the property being
        tested. That near-miss is what this constant exists to remove. */
    constexpr int kDecaySamples = 512 * 93;   // 47616, ~0.99 s

    const auto measure = [] (int decayBlockSize)
    {
        auto processor = makeProcessor (kPreparedBlock);

        // Get the needle up, identically in both runs.
        render (*processor, kPreparedBlock, kPreparedBlock * 16, 36);

        const auto before = processor->getMeterSnapshot().outputDb[0];

        auto* bypass = processor->getValueTreeState().getParameter (pid::bypass);
        REQUIRE (bypass != nullptr);
        bypass->setValueNotifyingHost (1.0f);

        render (*processor, decayBlockSize, kDecaySamples);

        return std::make_pair (before, processor->getMeterSnapshot().outputDb[0]);
    };

    const auto [coarseBefore, coarseAfter] = measure (512);
    const auto [fineBefore, fineAfter] = measure (64);

    // Same starting point, or the comparison below means nothing.
    REQUIRE (coarseBefore == fineBefore);
    REQUIRE (coarseBefore > GnarlProcessor::kMeterFloorDb + 20.0f);

    /*  A thousandth of a decibel, and NOT bit-exact - which this case
        asserted first and was wrong about. An exponential composes exactly
        in real arithmetic; in float, 93 multiplications and 744 of them
        round differently, and the two runs land 8e-6 dB apart. That residual
        is the arithmetic, not the ballistics.

        A thousandth of a dB is still five orders of magnitude tighter than
        the bug being excluded, which decayed the 64-sample run eight times
        too fast - about 160 dB over this window. The threshold is set by
        what the property actually guarantees rather than by what would pass,
        which is the lesson the drive stage's +15 dB left (CLAUDE.md §3). */
    CHECK (std::abs (coarseAfter - fineAfter) < 0.001f);
}

TEST_CASE ("The meter keeps falling while the plugin is bypassed", "[meters]")
{
    auto processor = makeProcessor (256);

    render (*processor, 256, 24000, 36);

    const auto playing = processor->getMeterSnapshot();
    REQUIRE (playing.outputDb[0] > GnarlProcessor::kMeterFloorDb);

    auto* bypass = processor->getValueTreeState().getParameter (pid::bypass);
    REQUIRE (bypass != nullptr);
    bypass->setValueNotifyingHost (1.0f);

    // Exactly one second of bypassed silence.
    render (*processor, 256, static_cast<int> (kSampleRate));

    const auto afterOneSecond = processor->getMeterSnapshot();

    /*  Not frozen at whatever it read the instant bypass was pressed. The
        bypass path returns early, so the meter has to be updated BEFORE that
        return or the needle stays up - showing signal on a silent plugin,
        for as long as the window is open.

        And it falls at the rate it says it does: one second, 20 dB. Checking
        the RATE rather than just "it went down" is what makes this a test of
        the ballistics instead of a test that something happened - the first
        version of this case rendered one second, expected the floor, and
        failed at -36 dB, which was the correct answer to a question it was
        not asking. */
    CHECK (afterOneSecond.outputDb[0]
             == Catch::Approx (playing.outputDb[0] - 20.0f).margin (1.0));

    // And given long enough it reaches the floor exactly, rather than
    // approaching it forever - see the silence case above for why that
    // matters to the editor.
    render (*processor, 256, static_cast<int> (kSampleRate) * 5);

    CHECK (processor->getMeterSnapshot().outputDb[0] == GnarlProcessor::kMeterFloorDb);
}

TEST_CASE ("Gain reduction is reported per band and signed", "[meters]")
{
    auto processor = makeProcessor (256);

    auto set = [&processor] (const char* id, float value)
    {
        auto* parameter = processor->getValueTreeState().getParameter (id);
        REQUIRE (parameter != nullptr);

        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
    };

    set (pid::ott.enabled, 1.0f);
    set (pid::ott.depth, 1.0f);

    render (*processor, 256, 48000, 36);

    const auto meters = processor->getMeterSnapshot();

    /*  At full depth on a sustained note SOMETHING has to be moving. Reported
        per band rather than combined, because an OTT lifts the quiet top
        while holding the loud low down - the bands routinely pull in
        opposite directions, and any average or sum of them reads "nothing is
        happening" at exactly the moment the most is. */
    auto moved = false;

    for (const auto gainDb : meters.ottGainDb)
    {
        CHECK (std::isfinite (gainDb));

        if (std::abs (gainDb) > 0.1f)
            moved = true;
    }

    CHECK (moved);
}
