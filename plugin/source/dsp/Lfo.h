#pragma once

#include "LfoCurve.h"
#include "SyncRates.h"
#include "../params/ParameterChoices.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace gnarl::dsp
{

/**
    One LFO.

    HOST TRANSPORT LOCK is the property that matters most here, and it is not
    optional. A wobble that drifts against the beat is useless whatever its
    shape: the producer draws a 1/6 pattern, expects it to land on the triplet
    grid, and expects it to land there identically every time the loop comes
    round. So in free-run mode the phase is COMPUTED FROM the host's PPQ
    position rather than accumulated, which means it is correct after a loop
    jump, a seek, a tempo change or a pause with no resynchronisation logic at
    all - there is no accumulated state to be wrong.

    Trigger and envelope modes accumulate phase from the note instead, because
    those are meant to be relative to the note rather than to the bar.

    The four modes:
      - trigger:   restarts on each note and loops. The default.
      - envelope:  one-shot; holds the final value. A drawable envelope.
      - freeRun:   locked to the host timeline, ignores note starts.
      - sampleAndHold: samples the drawn curve at grid steps, so a smooth
                   shape becomes a staircase.

    Real-time safe. The curve is copied in by value from the message thread.
*/
class Lfo
{
public:
    struct Settings
    {
        choices::LfoShape shape = choices::LfoShape::custom;
        choices::LfoMode mode = choices::LfoMode::trigger;

        bool syncEnabled = true;
        choices::LfoRateDivision division = choices::LfoRateDivision::eighth;
        float rateHz = 2.0f;

        /** 0..1, phase offset applied on top of whatever the mode produces. */
        float phaseOffset = 0.0f;

        /** 0..1 slew, to stop a steep drawn step clicking when it modulates
            something audible. */
        float smoothing = 0.0f;

        /** Grid used by sample-and-hold, and by the editor's snapping. */
        choices::GridDivision grid = choices::GridDivision::sixteenth;

        /** Output -1..1 instead of 0..1. */
        bool bipolar = false;
    };

    /** Everything the LFO needs to know about the host, per block. */
    struct TransportInfo
    {
        double bpm = 120.0;
        double ppqPosition = 0.0;
        bool isPlaying = false;
    };

    void prepare (double sampleRate) noexcept
    {
        setSampleRate (sampleRate);
        reset();
    }

    /** AUDIO THREAD SAFE. Changes the working rate WITHOUT resetting.

        The oversampling factor is a live parameter, so this happens mid-note.
        Going through prepare() would reset the phase, which restarts every
        wobble in the patch the moment the user changes the oversampling
        setting. The smoothing coefficient depends on the rate, but it is
        recomputed in setSettings() every block, so there is nothing to fix up
        here. */
    void setSampleRate (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;
    }

    void reset() noexcept
    {
        notePhase = 0.0;
        smoothed = 0.0f;
        heldValue = 0.0f;
        lastHoldStep = -1;
        finished = false;
        hasOutput = false;
    }

    /** Called when a note starts. Trigger and envelope modes restart here;
        free-run deliberately does not, which is the whole point of it. */
    void noteOn() noexcept
    {
        if (settings.mode == choices::LfoMode::freeRun)
            return;

        notePhase = 0.0;
        lastHoldStep = -1;
        finished = false;

        // The smoothed output is NOT reset: a note starting must not jump the
        // modulation to zero, or every note would click on whatever the LFO
        // is driving.
    }

    /** BLOCK-RATE. */
    void setSettings (const Settings& newSettings) noexcept
    {
        settings = newSettings;

        // A one-pole coefficient from the slew control. At smoothing 1 the
        // time constant is 100 ms, which is enough to turn the steepest drawn
        // step into a glide.
        const auto timeConstant = juce::jmap (juce::jlimit (0.0f, 1.0f, settings.smoothing),
                                              0.0f, 0.1f);

        smoothingCoefficient = timeConstant > 0.0f
            ? std::exp (-1.0f / (timeConstant * static_cast<float> (sampleRateHz)))
            : 0.0f;
    }

    /** MESSAGE THREAD publishes; copied by value so the audio thread never
        shares the editor's curve. */
    void setCurve (const LfoCurve& newCurve) noexcept { curve = newCurve; }
    const LfoCurve& getCurve() const noexcept { return curve; }

    /**
        Advances by `numSamples` and returns the value for that chunk.

        Called once per MODULATION CHUNK, not once per block. A block of 256
        samples is 5.3 ms, and a 1/16 wobble at 140 BPM completes in 107 ms -
        so once per block is only about 20 steps per cycle, which is audibly
        stepped. The engine subdivides the block instead.
    */
    float process (int numSamples, const TransportInfo& transport) noexcept
    {
        const auto cyclesPerSecond = getFrequencyHz (transport.bpm);

        auto phase = 0.0;

        if (settings.mode == choices::LfoMode::freeRun && settings.syncEnabled
            && transport.isPlaying)
        {
            // COMPUTED, not accumulated: correct across loops and seeks with
            // no resync logic, because there is no state to be stale.
            const auto beatsPerCycle = sync::getBeatsPerCycle (settings.division);

            phase = beatsPerCycle > 0.0
                ? std::fmod (transport.ppqPosition / beatsPerCycle, 1.0)
                : 0.0;

            if (phase < 0.0)
                phase += 1.0;
        }
        else
        {
            phase = notePhase;

            // Advanced after reading, so the value returned is the value at
            // the START of the chunk it applies to.
            notePhase += cyclesPerSecond * static_cast<double> (numSamples)
                       / sampleRateHz;

            if (settings.mode == choices::LfoMode::envelope)
            {
                // One-shot: clamp rather than wrap, and remember that it is
                // done so the held value is stable.
                if (notePhase >= 1.0)
                {
                    notePhase = 1.0;
                    finished = true;
                }
            }
            else
            {
                while (notePhase >= 1.0)
                    notePhase -= 1.0;
            }
        }

        if (settings.mode == choices::LfoMode::envelope && finished)
            phase = 1.0;

        auto offsetPhase = phase + static_cast<double> (settings.phaseOffset);

        if (settings.mode == choices::LfoMode::envelope)
        {
            // A one-shot has no wrap-around, and wrapping here would be worse
            // than wrong: a completed envelope sits at phase 1, so the wrap
            // would send it straight back to phase 0 and the held value would
            // snap to the START of the shape the instant the shape finished.
            offsetPhase = juce::jlimit (0.0, 1.0, offsetPhase);
        }
        else
        {
            while (offsetPhase >= 1.0)
                offsetPhase -= 1.0;
            while (offsetPhase < 0.0)
                offsetPhase += 1.0;
        }

        auto raw = evaluateShape (static_cast<float> (offsetPhase));

        if (settings.mode == choices::LfoMode::sampleAndHold)
            raw = applySampleAndHold (static_cast<float> (offsetPhase), raw);

        // Slew. Applied per chunk with the coefficient raised to the chunk
        // length, so the smoothing time is independent of the chunk size the
        // engine happens to use.
        if (smoothingCoefficient > 0.0f && hasOutput)
        {
            const auto coefficient = std::pow (smoothingCoefficient,
                                               static_cast<float> (numSamples));
            smoothed = coefficient * smoothed + (1.0f - coefficient) * raw;
        }
        else
        {
            smoothed = raw;
        }

        hasOutput = true;

        return settings.bipolar ? smoothed * 2.0f - 1.0f : smoothed;
    }

    /** The last value produced, for the UI's playhead and for metering. */
    float getCurrentValue() const noexcept
    {
        return settings.bipolar ? smoothed * 2.0f - 1.0f : smoothed;
    }

    /** Normalised phase of the last chunk, for the UI's playhead. */
    float getCurrentPhase() const noexcept { return lastPhase; }

    double getFrequencyHz (double bpm) const noexcept
    {
        if (settings.syncEnabled)
            return sync::getFrequencyHz (settings.division, bpm);

        return static_cast<double> (juce::jmax (0.001f, settings.rateHz));
    }

private:
    /** Built-in shapes, plus the drawn curve. The built-ins exist because a
        producer reaching for "square" should not have to draw one. */
    float evaluateShape (float phase) noexcept
    {
        lastPhase = phase;

        switch (settings.shape)
        {
            case choices::LfoShape::custom:
                return curve.evaluate (phase);

            case choices::LfoShape::sine:
                return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase);

            case choices::LfoShape::triangle:
                return phase < 0.5f ? phase * 2.0f : 2.0f - phase * 2.0f;

            case choices::LfoShape::sawUp:
                return phase;

            case choices::LfoShape::sawDown:
                return 1.0f - phase;

            case choices::LfoShape::square:
                return phase < 0.5f ? 1.0f : 0.0f;

            case choices::LfoShape::randomStep:
            case choices::LfoShape::randomSmooth:
                return evaluateRandom (phase,
                                       settings.shape == choices::LfoShape::randomSmooth);

            case choices::LfoShape::count:
            default:
                return 0.0f;
        }
    }

    /** Deterministic per-step random: hashed from the step index, so the same
        pattern repeats every cycle rather than wandering. A wobble that never
        repeats cannot be played to. */
    float evaluateRandom (float phase, bool smooth) noexcept
    {
        const auto steps = juce::jmax (1.0, 1.0 / juce::jmax (1.0e-6, gridFraction()));
        const auto position = static_cast<double> (phase) * steps;
        const auto step = static_cast<int> (position);

        const auto value = hashToUnit (step);

        if (! smooth)
            return value;

        const auto next = hashToUnit (step + 1);
        const auto local = static_cast<float> (position - step);

        return value + (next - value) * local;
    }

    float applySampleAndHold (float phase, float raw) noexcept
    {
        const auto fraction = gridFraction();

        if (fraction <= 0.0)
            return raw;

        const auto step = static_cast<int> (static_cast<double> (phase) / fraction);

        if (step != lastHoldStep)
        {
            lastHoldStep = step;
            heldValue = raw;
        }

        return heldValue;
    }

    /** The grid step as a fraction of one LFO cycle.

        The grid divides the CYCLE, not the bar - see SyncRates.h. So a 1/16
        grid is sixteen steps per cycle at every rate, rather than however many
        sixteenth notes happen to fit inside the cycle. */
    double gridFraction() const noexcept
    {
        return sync::getGridFraction (settings.grid);
    }

    static float hashToUnit (int step) noexcept
    {
        auto h = static_cast<std::uint32_t> (step) * 0x9E3779B1u;
        h ^= h >> 15;
        h *= 0x2545F491u;
        h ^= h >> 13;

        return static_cast<float> (h & 0xFFFFFFu) / static_cast<float> (0xFFFFFF);
    }

    double sampleRateHz = 44100.0;

    Settings settings {};
    LfoCurve curve;

    double notePhase = 0.0;
    float lastPhase = 0.0f;

    float smoothed = 0.0f;
    float smoothingCoefficient = 0.0f;
    bool hasOutput = false;

    float heldValue = 0.0f;
    int lastHoldStep = -1;

    bool finished = false;
};

} // namespace gnarl::dsp
