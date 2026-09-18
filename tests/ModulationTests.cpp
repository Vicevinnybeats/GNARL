#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/Envelope.h"
#include "dsp/Lfo.h"
#include "dsp/LfoCurve.h"
#include "dsp/ModMatrix.h"
#include "dsp/Modulation.h"
#include "dsp/SyncRates.h"
#include "dsp/VoiceSettings.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace gnarl;
using namespace gnarl::dsp;
using Catch::Approx;

namespace
{
    constexpr double kSampleRate = 48000.0;

    /** The engine's modulation chunk. The tests advance in the same steps the
        voice does, so a bug that only shows at that granularity shows here. */
    constexpr int kChunk = 32;

    Lfo::TransportInfo playingAt (double ppq, double bpm = 140.0)
    {
        Lfo::TransportInfo transport;
        transport.bpm = bpm;
        transport.ppqPosition = ppq;
        transport.isPlaying = true;

        return transport;
    }

    /** A square-ish two-step curve: 1 for the first half, 0 for the second.
        The staircase riddim is built on, and the shape that catches a curve
        evaluator that interpolates through a step. */
    LfoCurve twoStepCurve()
    {
        LfoCurve curve;
        curve.clear();
        curve.addPoint ({ 0.0f, 1.0f, 0.0f, LfoCurve::Shape::step });
        curve.addPoint ({ 0.5f, 0.0f, 0.0f, LfoCurve::Shape::step });
        curve.addPoint ({ 1.0f, 0.0f, 0.0f, LfoCurve::Shape::step });

        return curve;
    }
}

// ============================================================================
// LfoCurve
// ============================================================================

TEST_CASE ("A fresh curve is the falling ramp", "[lfo][curve]")
{
    const LfoCurve curve;

    REQUIRE (curve.isDefaultRamp());
    CHECK (curve.evaluate (0.0f) == Approx (1.0f));
    CHECK (curve.evaluate (0.5f) == Approx (0.5f));
    CHECK (curve.evaluate (1.0f) == Approx (0.0f));
}

TEST_CASE ("A step segment holds rather than interpolating", "[lfo][curve]")
{
    const auto curve = twoStepCurve();

    CHECK (curve.evaluate (0.0f) == Approx (1.0f));
    CHECK (curve.evaluate (0.25f) == Approx (1.0f));
    CHECK (curve.evaluate (0.49f) == Approx (1.0f));

    // The transition is a step, so nothing in between should ever be read.
    CHECK (curve.evaluate (0.51f) == Approx (0.0f));
    CHECK (curve.evaluate (0.9f) == Approx (0.0f));
}

TEST_CASE ("Tension bends a segment without breaking monotonicity", "[lfo][curve]")
{
    // A non-monotonic segment reads as a glitch rather than as a curve, so
    // this is a correctness property, not a nicety.
    for (const auto tension : { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f })
    {
        auto previous = LfoCurve::applyTension (0.0f, tension);

        for (int i = 1; i <= 200; ++i)
        {
            const auto t = static_cast<float> (i) / 200.0f;
            const auto value = LfoCurve::applyTension (t, tension);

            CHECK (value >= previous - 1.0e-6f);
            previous = value;
        }

        // The endpoints are fixed whatever the tension, or a curve's shape
        // would change its own start and end values.
        CHECK (LfoCurve::applyTension (0.0f, tension) == Approx (0.0f));
        CHECK (LfoCurve::applyTension (1.0f, tension) == Approx (1.0f));
    }

    // Positive holds low then rises late; negative rises early.
    CHECK (LfoCurve::applyTension (0.5f, 1.0f) < 0.5f);
    CHECK (LfoCurve::applyTension (0.5f, -1.0f) > 0.5f);
    CHECK (LfoCurve::applyTension (0.5f, 0.0f) == Approx (0.5f));
}

TEST_CASE ("A curve holds its end values outside its own time range", "[lfo][curve]")
{
    LfoCurve curve;
    curve.clear();
    curve.addPoint ({ 0.25f, 0.2f, 0.0f, LfoCurve::Shape::curved });
    curve.addPoint ({ 0.75f, 0.8f, 0.0f, LfoCurve::Shape::curved });

    CHECK (curve.evaluate (0.0f) == Approx (0.2f));
    CHECK (curve.evaluate (0.5f) == Approx (0.5f));
    CHECK (curve.evaluate (1.0f) == Approx (0.8f));
}

TEST_CASE ("A curve refuses points beyond its capacity", "[lfo][curve]")
{
    LfoCurve curve;
    curve.clear();

    for (int i = 0; i < LfoCurve::kMaxPoints; ++i)
    {
        const auto t = static_cast<float> (i) / static_cast<float> (LfoCurve::kMaxPoints);
        REQUIRE (curve.addPoint ({ t, 0.5f, 0.0f, LfoCurve::Shape::curved }));
    }

    // Fixed capacity is the point: a vector here would mean an allocation on
    // the message thread and a lock to read it from the audio thread.
    CHECK_FALSE (curve.addPoint ({ 1.0f, 0.0f, 0.0f, LfoCurve::Shape::curved }));
    CHECK (curve.getNumPoints() == LfoCurve::kMaxPoints);
}

// ============================================================================
// Sync rates
// ============================================================================

TEST_CASE ("Dotted and triplet divisions have the right beat lengths", "[lfo][sync]")
{
    using D = choices::LfoRateDivision;

    CHECK (sync::getBeatsPerCycle (D::quarter) == Approx (1.0));
    CHECK (sync::getBeatsPerCycle (D::quarterDotted) == Approx (1.5));
    CHECK (sync::getBeatsPerCycle (D::quarterTriplet) == Approx (2.0 / 3.0));

    // Three triplet eighths fill one beat; two straight eighths do.
    CHECK (3.0 * sync::getBeatsPerCycle (D::eighthTriplet) == Approx (1.0));
    CHECK (2.0 * sync::getBeatsPerCycle (D::eighth) == Approx (1.0));

    CHECK (sync::isTriplet (D::sixteenthTriplet));
    CHECK_FALSE (sync::isTriplet (D::sixteenth));
    CHECK (sync::isDotted (D::sixteenthDotted));
    CHECK_FALSE (sync::isDotted (D::sixteenth));
}

TEST_CASE ("Synced rates convert to the right frequency", "[lfo][sync]")
{
    using D = choices::LfoRateDivision;

    // At 120 BPM a beat is half a second, so 1/4 is 2 Hz and 1/16 is 8 Hz.
    CHECK (sync::getFrequencyHz (D::quarter, 120.0) == Approx (2.0));
    CHECK (sync::getFrequencyHz (D::sixteenth, 120.0) == Approx (8.0));

    // A triplet is faster than its straight rate, a dotted one slower.
    CHECK (sync::getFrequencyHz (D::eighthTriplet, 120.0)
         > sync::getFrequencyHz (D::eighth, 120.0));
    CHECK (sync::getFrequencyHz (D::eighthDotted, 120.0)
         < sync::getFrequencyHz (D::eighth, 120.0));

    // The grid divides the CYCLE, not the bar: "1/16" is sixteen steps across
    // one LFO cycle at any rate. The tempo-relative reading made the default
    // patch's grid two steps wide, so no shape could be drawn at all.
    CHECK (sync::getGridStepsPerCycle (choices::GridDivision::sixteenth) == 16);
    CHECK (sync::getGridStepsPerCycle (choices::GridDivision::twelfth) == 12);
    CHECK (sync::getGridStepsPerCycle (choices::GridDivision::twentyFourth) == 24);
    CHECK (sync::getGridStepsPerCycle (choices::GridDivision::off) == 0);

    CHECK (sync::getGridFraction (choices::GridDivision::sixteenth) == Approx (1.0 / 16.0));
    CHECK (sync::getGridFraction (choices::GridDivision::off) == Approx (0.0));

    CHECK (sync::isTripletGrid (choices::GridDivision::twentyFourth));
    CHECK_FALSE (sync::isTripletGrid (choices::GridDivision::sixteenth));
}

// ============================================================================
// LFO
// ============================================================================

TEST_CASE ("A free-run LFO's phase is decided by the host position", "[lfo]")
{
    Lfo lfo;
    lfo.prepare (kSampleRate);

    Lfo::Settings settings;
    settings.mode = choices::LfoMode::freeRun;
    settings.shape = choices::LfoShape::sawUp;
    settings.syncEnabled = true;
    settings.division = choices::LfoRateDivision::quarter;
    lfo.setSettings (settings);

    // A quarter-note cycle: at PPQ 4.25 the LFO is a quarter of the way
    // through its cycle, and so is it at PPQ 8.25 - four beats later.
    const auto atFirstBar = lfo.process (kChunk, playingAt (4.25));
    const auto atLaterBar = lfo.process (kChunk, playingAt (8.25));

    CHECK (atFirstBar == Approx (0.25f).margin (0.01f));
    CHECK (atLaterBar == Approx (atFirstBar).margin (1.0e-4f));
}

TEST_CASE ("A free-run LFO survives a loop jump with no resynchronisation", "[lfo]")
{
    // The property the whole design is for: because the phase is COMPUTED
    // from PPQ rather than accumulated, seeking backwards cannot leave stale
    // state behind. A producer loops a bar and the wobble has to land in the
    // same place every time round.
    Lfo lfo;
    lfo.prepare (kSampleRate);

    Lfo::Settings settings;
    settings.mode = choices::LfoMode::freeRun;
    settings.shape = choices::LfoShape::sawUp;
    settings.division = choices::LfoRateDivision::eighth;
    lfo.setSettings (settings);

    const auto firstPass = lfo.process (kChunk, playingAt (2.375));

    // Play on, then jump back to the top of the loop and return.
    for (double ppq = 2.5; ppq < 6.0; ppq += 0.125)
        lfo.process (kChunk, playingAt (ppq));

    lfo.process (kChunk, playingAt (0.0));

    const auto secondPass = lfo.process (kChunk, playingAt (2.375));

    CHECK (secondPass == Approx (firstPass).margin (1.0e-4f));
}

TEST_CASE ("A trigger LFO restarts on a note and a free-run one does not", "[lfo]")
{
    Lfo::Settings settings;
    settings.shape = choices::LfoShape::sawUp;
    settings.syncEnabled = false;
    settings.rateHz = 4.0f;

    SECTION ("trigger restarts")
    {
        Lfo lfo;
        lfo.prepare (kSampleRate);
        settings.mode = choices::LfoMode::trigger;
        lfo.setSettings (settings);

        // Advance a good way into the cycle, then retrigger.
        for (int i = 0; i < 100; ++i)
            lfo.process (kChunk, playingAt (0.0));

        REQUIRE (lfo.getCurrentValue() > 0.1f);

        lfo.noteOn();

        CHECK (lfo.process (kChunk, playingAt (0.0)) == Approx (0.0f).margin (0.01f));
    }

    SECTION ("free run ignores the note")
    {
        Lfo lfo;
        lfo.prepare (kSampleRate);
        settings.mode = choices::LfoMode::freeRun;
        settings.syncEnabled = true;
        settings.division = choices::LfoRateDivision::quarter;
        lfo.setSettings (settings);

        lfo.process (kChunk, playingAt (0.5));
        lfo.noteOn();

        CHECK (lfo.process (kChunk, playingAt (0.5)) == Approx (0.5f).margin (0.02f));
    }
}

TEST_CASE ("An envelope-mode LFO is a one-shot that holds its last value", "[lfo]")
{
    Lfo lfo;
    lfo.prepare (kSampleRate);

    Lfo::Settings settings;
    settings.mode = choices::LfoMode::envelope;
    settings.shape = choices::LfoShape::sawUp;
    settings.syncEnabled = false;
    settings.rateHz = 8.0f;
    lfo.setSettings (settings);

    lfo.noteOn();

    // One cycle at 8 Hz is 125 ms: 6000 samples, so 188 chunks. Run well past
    // it.
    for (int i = 0; i < 600; ++i)
        lfo.process (kChunk, playingAt (0.0));

    const auto held = lfo.getCurrentValue();

    CHECK (held == Approx (1.0f).margin (0.01f));

    // And it stays there rather than wrapping back to the start.
    for (int i = 0; i < 100; ++i)
        CHECK (lfo.process (kChunk, playingAt (0.0)) == Approx (held).margin (1.0e-4f));
}

TEST_CASE ("Sample and hold turns a smooth shape into a staircase", "[lfo]")
{
    Lfo lfo;
    lfo.prepare (kSampleRate);

    Lfo::Settings settings;
    settings.mode = choices::LfoMode::sampleAndHold;
    settings.shape = choices::LfoShape::sawUp;
    settings.syncEnabled = true;
    settings.division = choices::LfoRateDivision::quarter;
    settings.grid = choices::GridDivision::quarter;
    lfo.setSettings (settings);
    lfo.noteOn();

    // The grid divides the CYCLE, so a 1/4 grid is four steps per cycle
    // whatever the rate.
    std::vector<float> values;

    for (int i = 0; i < 400; ++i)
        values.push_back (lfo.process (kChunk, playingAt (0.0)));

    std::vector<float> distinct;

    for (const auto value : values)
    {
        const auto isNew = std::none_of (distinct.begin(), distinct.end(),
            [value] (float existing) { return std::abs (existing - value) < 1.0e-5f; });

        if (isNew)
            distinct.push_back (value);
    }

    // The staircase is the feature; a smooth output here would mean the hold
    // is not working at all. A few more than four is fine - the run covers a
    // little over one cycle - but a saw sampled continuously would give
    // hundreds.
    CHECK (distinct.size() <= 8);
    CHECK (distinct.size() >= 3);
}

TEST_CASE ("Slew smooths in the same time whatever the chunk size", "[lfo]")
{
    // The coefficient is raised to the chunk length precisely so the smoothing
    // time does not depend on a number the engine picked for other reasons.
    const auto runFor = [] (int chunk, int totalSamples)
    {
        Lfo lfo;
        lfo.prepare (kSampleRate);

        Lfo::Settings settings;
        settings.mode = choices::LfoMode::trigger;
        settings.shape = choices::LfoShape::square;
        settings.syncEnabled = false;
        settings.rateHz = 0.001f;   // effectively a step held at 1
        settings.smoothing = 1.0f;
        lfo.setSettings (settings);
        lfo.noteOn();

        auto value = 0.0f;

        for (int i = 0; i < totalSamples / chunk; ++i)
            value = lfo.process (chunk, playingAt (0.0));

        return value;
    };

    const auto viaSmallChunks = runFor (16, 4800);   // 100 ms
    const auto viaLargeChunks = runFor (256, 4800);

    CHECK (viaSmallChunks == Approx (viaLargeChunks).margin (0.02f));
}

TEST_CASE ("A bipolar LFO is centred on zero", "[lfo]")
{
    Lfo lfo;
    lfo.prepare (kSampleRate);

    Lfo::Settings settings;
    settings.mode = choices::LfoMode::freeRun;
    settings.shape = choices::LfoShape::sawUp;
    settings.division = choices::LfoRateDivision::quarter;
    settings.bipolar = true;
    lfo.setSettings (settings);

    CHECK (lfo.process (kChunk, playingAt (0.0)) == Approx (-1.0f).margin (0.02f));
    CHECK (lfo.process (kChunk, playingAt (0.5)) == Approx (0.0f).margin (0.02f));
}

TEST_CASE ("A random-shape LFO repeats the same pattern every cycle", "[lfo]")
{
    // A wobble that never repeats cannot be played to, so the randomness is
    // hashed from the step index rather than drawn from a running generator.
    Lfo lfo;
    lfo.prepare (kSampleRate);

    Lfo::Settings settings;
    settings.mode = choices::LfoMode::freeRun;
    settings.shape = choices::LfoShape::randomStep;
    settings.division = choices::LfoRateDivision::quarter;
    settings.grid = choices::GridDivision::sixteenth;
    lfo.setSettings (settings);

    for (const auto offset : { 0.0, 0.25, 0.5, 0.75 })
    {
        const auto first = lfo.process (kChunk, playingAt (offset));
        const auto later = lfo.process (kChunk, playingAt (offset + 8.0));

        CHECK (later == Approx (first).margin (1.0e-4f));
    }
}

TEST_CASE ("A drawn curve is what the LFO outputs", "[lfo][curve]")
{
    Lfo lfo;
    lfo.prepare (kSampleRate);
    lfo.setCurve (twoStepCurve());

    Lfo::Settings settings;
    settings.mode = choices::LfoMode::freeRun;
    settings.shape = choices::LfoShape::custom;
    settings.division = choices::LfoRateDivision::quarter;
    lfo.setSettings (settings);

    CHECK (lfo.process (kChunk, playingAt (0.1)) == Approx (1.0f).margin (0.01f));
    CHECK (lfo.process (kChunk, playingAt (0.6)) == Approx (0.0f).margin (0.01f));
}

TEST_CASE ("A phase offset shifts the whole shape", "[lfo]")
{
    Lfo lfo;
    lfo.prepare (kSampleRate);

    Lfo::Settings settings;
    settings.mode = choices::LfoMode::freeRun;
    settings.shape = choices::LfoShape::sawUp;
    settings.division = choices::LfoRateDivision::quarter;
    settings.phaseOffset = 0.25f;
    lfo.setSettings (settings);

    CHECK (lfo.process (kChunk, playingAt (0.0)) == Approx (0.25f).margin (0.02f));

    // And it wraps rather than clipping at the top of the cycle.
    CHECK (lfo.process (kChunk, playingAt (0.8)) == Approx (0.05f).margin (0.02f));
}

// ============================================================================
// Envelope
// ============================================================================

TEST_CASE ("An envelope runs attack, decay, sustain and release", "[envelope]")
{
    Envelope envelope;
    envelope.prepare (kSampleRate);

    Envelope::Settings settings;
    settings.mode = choices::EnvelopeMode::adsr;
    settings.attackSeconds = 0.01f;
    settings.decaySeconds = 0.02f;
    settings.sustain = 0.5f;
    settings.releaseSeconds = 0.02f;
    envelope.setSettings (settings);

    REQUIRE_FALSE (envelope.isActive());

    envelope.noteOn (1.0f);
    REQUIRE (envelope.isActive());

    const auto advance = [&envelope] (double seconds)
    {
        const auto chunks = static_cast<int> (seconds * kSampleRate) / kChunk;

        for (int i = 0; i < chunks; ++i)
            envelope.process (kChunk);
    };

    // Halfway through a 10 ms attack.
    advance (0.005);
    CHECK (envelope.getLevel() > 0.2f);
    CHECK (envelope.getLevel() < 0.8f);

    // Past the attack and the decay: parked on the sustain.
    advance (0.05);
    CHECK (envelope.getStage() == Envelope::Stage::sustain);
    CHECK (envelope.getLevel() == Approx (0.5f).margin (0.01f));

    envelope.noteOff();
    CHECK (envelope.isReleasing());

    advance (0.05);
    CHECK_FALSE (envelope.isActive());
    CHECK (envelope.getLevel() == Approx (0.0f).margin (1.0e-4f));
}

TEST_CASE ("A zero-length attack is instant, not silent", "[envelope]")
{
    // The trap in a stage-length divisor: a zero-length stage must complete
    // immediately rather than dividing by zero or never finishing.
    Envelope envelope;
    envelope.prepare (kSampleRate);

    Envelope::Settings settings;
    settings.attackSeconds = 0.0f;
    settings.decaySeconds = 1.0f;
    settings.sustain = 1.0f;
    envelope.setSettings (settings);

    envelope.noteOn (1.0f);
    envelope.process (kChunk);

    CHECK (envelope.getLevel() == Approx (1.0f).margin (1.0e-3f));
}

TEST_CASE ("A DAHDSR envelope waits out its delay and hold", "[envelope]")
{
    Envelope envelope;
    envelope.prepare (kSampleRate);

    Envelope::Settings settings;
    settings.mode = choices::EnvelopeMode::dahdsr;
    settings.delaySeconds = 0.02f;
    settings.attackSeconds = 0.001f;
    settings.holdSeconds = 0.02f;
    settings.decaySeconds = 0.5f;
    settings.sustain = 0.0f;
    envelope.setSettings (settings);

    envelope.noteOn (1.0f);

    // Still inside the delay, so still silent.
    for (int i = 0; i < 10; ++i)
        envelope.process (kChunk);

    CHECK (envelope.getStage() == Envelope::Stage::delay);
    CHECK (envelope.getLevel() == Approx (0.0f).margin (1.0e-4f));

    // Through the delay and the attack, into the hold: pinned at the top.
    for (int i = 0; i < 40; ++i)
        envelope.process (kChunk);

    CHECK (envelope.getStage() == Envelope::Stage::hold);
    CHECK (envelope.getLevel() == Approx (1.0f).margin (1.0e-3f));
}

TEST_CASE ("Retriggering during a release starts from the current level", "[envelope]")
{
    // Starting the attack from zero instead would click on every retrigger,
    // which in this genre means on every note of a fast pattern.
    Envelope envelope;
    envelope.prepare (kSampleRate);

    Envelope::Settings settings;
    settings.attackSeconds = 0.05f;
    settings.decaySeconds = 0.01f;
    settings.sustain = 1.0f;
    settings.releaseSeconds = 0.2f;
    envelope.setSettings (settings);

    envelope.noteOn (1.0f);

    for (int i = 0; i < 200; ++i)
        envelope.process (kChunk);

    envelope.noteOff();

    for (int i = 0; i < 50; ++i)
        envelope.process (kChunk);

    const auto levelBeforeRetrigger = envelope.getLevel();
    REQUIRE (levelBeforeRetrigger > 0.2f);

    envelope.noteOn (1.0f);
    const auto levelAfterRetrigger = envelope.process (kChunk);

    // No discontinuity: the level after one chunk is close to the level
    // before, and rising.
    CHECK (levelAfterRetrigger == Approx (levelBeforeRetrigger).margin (0.05f));
    CHECK (levelAfterRetrigger >= levelBeforeRetrigger);
}

TEST_CASE ("Velocity scales the envelope by its velocity amount", "[envelope]")
{
    const auto peakAtVelocity = [] (float velocity, float amount)
    {
        Envelope envelope;
        envelope.prepare (kSampleRate);

        Envelope::Settings settings;
        settings.attackSeconds = 0.001f;
        settings.sustain = 1.0f;
        settings.velocityAmount = amount;
        envelope.setSettings (settings);

        envelope.noteOn (velocity);

        for (int i = 0; i < 20; ++i)
            envelope.process (kChunk);

        return envelope.getLevel();
    };

    CHECK (peakAtVelocity (1.0f, 1.0f) == Approx (1.0f).margin (1.0e-3f));
    CHECK (peakAtVelocity (0.5f, 1.0f) == Approx (0.5f).margin (1.0e-3f));

    // Amount zero means velocity does nothing, which is what a modulation
    // envelope usually wants.
    CHECK (peakAtVelocity (0.5f, 0.0f) == Approx (1.0f).margin (1.0e-3f));
}

TEST_CASE ("A killed envelope stops immediately", "[envelope]")
{
    Envelope envelope;
    envelope.prepare (kSampleRate);

    Envelope::Settings settings;
    settings.attackSeconds = 0.001f;
    settings.sustain = 1.0f;
    settings.releaseSeconds = 5.0f;
    envelope.setSettings (settings);

    envelope.noteOn (1.0f);

    for (int i = 0; i < 20; ++i)
        envelope.process (kChunk);

    envelope.kill();

    CHECK_FALSE (envelope.isActive());
    CHECK (envelope.getLevel() == Approx (0.0f).margin (1.0e-6f));
}

// ============================================================================
// Mod matrix
// ============================================================================

TEST_CASE ("A mod slot routes its source to its destination", "[modmatrix]")
{
    ModMatrix matrix;

    ModMatrix::Slot slot;
    slot.enabled = true;
    slot.source = choices::ModSource::lfo1;
    slot.destination = ModDestination::filter1Cutoff;
    slot.depth = 0.5f;
    slot.bipolar = false;
    matrix.setSlot (0, slot);

    ModMatrix::SourceValues sources;
    sources.set (choices::ModSource::lfo1, 1.0f);

    ModulationOffsets offsets;
    matrix.apply (sources, offsets);

    CHECK (offsets.get (ModDestination::filter1Cutoff) == Approx (0.5f));

    // And nothing else moved.
    CHECK (offsets.get (ModDestination::filter2Cutoff) == Approx (0.0f));
    CHECK (offsets.get (ModDestination::osc1TablePosition) == Approx (0.0f));
}

TEST_CASE ("A disabled slot, or one with no destination, does nothing", "[modmatrix]")
{
    ModMatrix matrix;

    ModMatrix::Slot disabled;
    disabled.enabled = false;
    disabled.source = choices::ModSource::lfo1;
    disabled.destination = ModDestination::filter1Cutoff;
    disabled.depth = 1.0f;
    matrix.setSlot (0, disabled);

    // The state a slot is in the moment it is created: on, but pointed
    // nowhere. It must not modulate anything.
    ModMatrix::Slot unrouted;
    unrouted.enabled = true;
    unrouted.source = choices::ModSource::lfo1;
    unrouted.destination = ModDestination::none;
    unrouted.depth = 1.0f;
    matrix.setSlot (1, unrouted);

    ModMatrix::SourceValues sources;
    sources.set (choices::ModSource::lfo1, 1.0f);

    ModulationOffsets offsets;
    matrix.apply (sources, offsets);

    CHECK (offsets.get (ModDestination::filter1Cutoff) == Approx (0.0f));
}

TEST_CASE ("A bipolar slot modulates either side of the set value", "[modmatrix]")
{
    ModMatrix matrix;

    ModMatrix::Slot slot;
    slot.enabled = true;
    slot.source = choices::ModSource::lfo1;
    slot.destination = ModDestination::filter1Cutoff;
    slot.depth = 1.0f;
    slot.bipolar = true;
    matrix.setSlot (0, slot);

    const auto offsetForSource = [&matrix] (float sourceValue)
    {
        ModMatrix::SourceValues sources;
        sources.set (choices::ModSource::lfo1, sourceValue);

        ModulationOffsets offsets;
        matrix.apply (sources, offsets);

        return offsets.get (ModDestination::filter1Cutoff);
    };

    // A source at its midpoint leaves the destination alone; the extremes
    // push it both ways. Unipolar-only routing would mean a wobble that never
    // dips below the knob position.
    CHECK (offsetForSource (0.5f) == Approx (0.0f).margin (1.0e-6f));
    CHECK (offsetForSource (1.0f) == Approx (1.0f));
    CHECK (offsetForSource (0.0f) == Approx (-1.0f));
}

TEST_CASE ("A mod curve shapes the value but keeps its sign", "[modmatrix]")
{
    // An exponential curve that flipped the negative half would be a different
    // modulation, not a shaped one.
    ModMatrix matrix;

    ModMatrix::Slot slot;
    slot.enabled = true;
    slot.source = choices::ModSource::lfo1;
    slot.destination = ModDestination::filter1Cutoff;
    slot.depth = 1.0f;
    slot.bipolar = true;
    slot.curve = choices::ModCurve::exponential;
    matrix.setSlot (0, slot);

    const auto offsetForSource = [&matrix] (float sourceValue)
    {
        ModMatrix::SourceValues sources;
        sources.set (choices::ModSource::lfo1, sourceValue);

        ModulationOffsets offsets;
        matrix.apply (sources, offsets);

        return offsets.get (ModDestination::filter1Cutoff);
    };

    // Source 0.75 is +0.5 bipolar; squared that is +0.25.
    CHECK (offsetForSource (0.75f) == Approx (0.25f));

    // Source 0.25 is -0.5; the magnitude is squared and the sign kept.
    CHECK (offsetForSource (0.25f) == Approx (-0.25f));
}

TEST_CASE ("An aux source scales a slot's own depth", "[modmatrix]")
{
    // The pattern this exists for: an LFO faded in by an envelope, without
    // burning a second of the sixteen slots.
    ModMatrix matrix;

    ModMatrix::Slot slot;
    slot.enabled = true;
    slot.source = choices::ModSource::lfo1;
    slot.destination = ModDestination::filter1Cutoff;
    slot.depth = 1.0f;
    slot.bipolar = false;
    slot.auxSource = choices::ModSource::env2;
    slot.auxAmount = 1.0f;
    matrix.setSlot (0, slot);

    const auto offsetForAux = [&matrix] (float auxValue)
    {
        ModMatrix::SourceValues sources;
        sources.set (choices::ModSource::lfo1, 1.0f);
        sources.set (choices::ModSource::env2, auxValue);

        ModulationOffsets offsets;
        matrix.apply (sources, offsets);

        return offsets.get (ModDestination::filter1Cutoff);
    };

    CHECK (offsetForAux (0.0f) == Approx (0.0f).margin (1.0e-6f));
    CHECK (offsetForAux (0.5f) == Approx (0.5f));
    CHECK (offsetForAux (1.0f) == Approx (1.0f));
}

TEST_CASE ("Several slots on one destination sum", "[modmatrix]")
{
    ModMatrix matrix;

    for (std::size_t i = 0; i < 3; ++i)
    {
        ModMatrix::Slot slot;
        slot.enabled = true;
        slot.source = choices::ModSource::lfo1;
        slot.destination = ModDestination::osc1TablePosition;
        slot.depth = 0.2f;
        slot.bipolar = false;
        matrix.setSlot (i, slot);
    }

    ModMatrix::SourceValues sources;
    sources.set (choices::ModSource::lfo1, 1.0f);

    ModulationOffsets offsets;
    matrix.apply (sources, offsets);

    CHECK (offsets.get (ModDestination::osc1TablePosition) == Approx (0.6f));
}

// ============================================================================
// Destination table
// ============================================================================

TEST_CASE ("Every destination round-trips through its parameter ID", "[modulation]")
{
    // Destinations are STORED as parameter-ID strings so presets survive the
    // list growing. A destination that did not round-trip would silently
    // repoint a preset's slot.
    for (int i = 1; i < static_cast<int> (ModDestination::count); ++i)
    {
        const auto destination = static_cast<ModDestination> (i);
        const auto parameterID = getParameterIDForDestination (destination);

        REQUIRE_FALSE (parameterID.isEmpty());
        CHECK (getDestinationForParameterID (parameterID) == destination);
        CHECK_FALSE (getDestinationDisplayName (destination).isEmpty());
    }

    // The picker's list leads with a "no destination" entry, because
    // unrouting a slot has to be as reachable as routing it.
    CHECK (getAllDestinationDisplayNames().size()
        == static_cast<int> (ModDestination::count));
    CHECK (getAllDestinationDisplayNames()[0] == "-");
}

TEST_CASE ("An unknown destination ID resolves to none", "[modulation]")
{
    // This is what makes a preset from a later build, or one naming a
    // since-removed parameter, load harmlessly rather than failing.
    CHECK (getDestinationForParameterID ("not_a_parameter") == ModDestination::none);
    CHECK (getDestinationForParameterID ("") == ModDestination::none);
    CHECK (getParameterIDForDestination (ModDestination::none).isEmpty());
}

// ============================================================================
// Applying modulation to the settings
// ============================================================================

namespace
{
    struct ModulatedCopy
    {
        VoiceSettings::OscillatorState oscillators[pid::kNumOscillators] {};
        VoiceSettings::SubState sub {};
        VoiceSettings::NoiseState noise {};
        FilterSlot::Settings filters[pid::kNumFilters] {};
    };

    ModulatedCopy applyOne (const VoiceSettings& base,
                            ModDestination destination,
                            float amount)
    {
        ModulationOffsets offsets;
        offsets.add (destination, amount);

        ModulatedCopy copy;
        applyModulation (base, offsets, copy.oscillators, copy.sub, copy.noise, copy.filters);

        return copy;
    }
}

TEST_CASE ("Cutoff is modulated in octaves, not in hertz", "[modulation]")
{
    // A full-depth LFO on a cutoff in Hz spends almost all its travel above
    // 10 kHz. In octaves the same LFO sweeps evenly across the keyboard,
    // which is what a filter wobble has to do.
    VoiceSettings settings;
    settings.filters[0].cutoffHz = 500.0f;

    const auto upOneSixth = applyOne (settings, ModDestination::filter1Cutoff, 1.0f / 6.0f);
    const auto downOneSixth = applyOne (settings, ModDestination::filter1Cutoff, -1.0f / 6.0f);

    // A sixth of full depth is one octave, either way.
    CHECK (upOneSixth.filters[0].cutoffHz == Approx (1000.0f).margin (1.0f));
    CHECK (downOneSixth.filters[0].cutoffHz == Approx (250.0f).margin (1.0f));

    // And it is clamped to the audible range rather than running off.
    const auto wideOpen = applyOne (settings, ModDestination::filter1Cutoff, 1.0f);
    const auto slammedShut = applyOne (settings, ModDestination::filter1Cutoff, -1.0f);

    CHECK (wideOpen.filters[0].cutoffHz <= 20000.0f);
    CHECK (slammedShut.filters[0].cutoffHz >= 20.0f);
}

TEST_CASE ("Modulation is clamped to each destination's legal range", "[modulation]")
{
    // Modulation overshoots by design - that is what lets a small depth reach
    // the end of a range - so every destination has to survive being pushed
    // past its limits.
    VoiceSettings settings;
    settings.oscillators[0].settings.tablePosition = 0.8f;
    settings.oscillators[0].settings.level = 0.8f;
    settings.oscillators[0].settings.pan = 0.5f;
    settings.filters[0].resonance = 0.9f;

    const auto pushed = applyOne (settings, ModDestination::osc1TablePosition, 5.0f);
    CHECK (pushed.oscillators[0].settings.tablePosition == Approx (1.0f));

    const auto pulled = applyOne (settings, ModDestination::osc1Level, -5.0f);
    CHECK (pulled.oscillators[0].settings.level == Approx (0.0f));

    const auto panned = applyOne (settings, ModDestination::osc1Pan, 5.0f);
    CHECK (panned.oscillators[0].settings.pan == Approx (1.0f));

    const auto resonant = applyOne (settings, ModDestination::filter1Resonance, 5.0f);
    CHECK (resonant.filters[0].resonance == Approx (1.0f));
}

TEST_CASE ("An empty offset set changes nothing", "[modulation]")
{
    VoiceSettings settings;
    settings.oscillators[0].settings.tablePosition = 0.42f;
    settings.oscillators[1].settings.level = 0.37f;
    settings.filters[0].cutoffHz = 1234.0f;
    settings.noise.level = 0.21f;

    ModulationOffsets offsets;
    ModulatedCopy copy;
    applyModulation (settings, offsets, copy.oscillators, copy.sub, copy.noise, copy.filters);

    CHECK (copy.oscillators[0].settings.tablePosition == Approx (0.42f));
    CHECK (copy.oscillators[1].settings.level == Approx (0.37f));
    CHECK (copy.filters[0].cutoffHz == Approx (1234.0f));
    CHECK (copy.noise.level == Approx (0.21f));
}

TEST_CASE ("Pitch modulation reaches two octaves at full depth", "[modulation]")
{
    VoiceSettings settings;
    settings.oscillators[0].pitchOffsetSemitones = 0.0f;

    const auto up = applyOne (settings, ModDestination::osc1PitchSemi, 1.0f);
    CHECK (up.oscillators[0].pitchOffsetSemitones == Approx (24.0f));

    const auto down = applyOne (settings, ModDestination::osc1PitchSemi, -0.5f);
    CHECK (down.oscillators[0].pitchOffsetSemitones == Approx (-12.0f));
}
