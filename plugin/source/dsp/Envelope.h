#pragma once

#include "LfoCurve.h"
#include "../params/ParameterChoices.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace gnarl::dsp
{

/**
    DAHDSR envelope with a curve control per segment.

    Replaces the placeholder amplitude ramp from Phase 1. Env 1 is the amp
    envelope; the other three are free modulation sources.

    PER-SEGMENT CURVES are not a luxury. A linear attack-decay cannot produce
    a convincing pluck: the ear hears the initial transient of a real
    percussive sound decaying fast and then slowly, which is an exponential,
    and a linear decay of the same length sounds like a synth pad with the
    release turned down. The curve control is reused from LfoCurve so the
    shaping is identical everywhere in the plugin.

    Runs at the modulation chunk rate rather than per sample. An envelope is a
    control signal; evaluating it per sample costs 256 times as much for a
    difference nothing can hear, PROVIDED the chunk is short enough that a
    fast attack is not stepped - which is why the engine's chunk is 32 samples
    (0.67 ms at 48 kHz) rather than a whole block.
*/
class Envelope
{
public:
    struct Settings
    {
        choices::EnvelopeMode mode = choices::EnvelopeMode::adsr;

        float delaySeconds = 0.0f;
        float attackSeconds = 0.002f;
        float holdSeconds = 0.0f;
        float decaySeconds = 0.4f;
        float sustain = 1.0f;           // 0..1
        float releaseSeconds = 0.05f;

        /** -1..1 each, shaping their segment. */
        float attackCurve = 0.0f;
        float decayCurve = 0.0f;
        float releaseCurve = 0.0f;

        /** 0..1: how much velocity scales the output. */
        float velocityAmount = 1.0f;
    };

    enum class Stage
    {
        idle = 0,
        delay,
        attack,
        hold,
        decay,
        sustain,
        release
    };

    void prepare (double sampleRate) noexcept
    {
        setSampleRate (sampleRate);
        reset();
    }

    /** AUDIO THREAD SAFE. Changes the working rate WITHOUT resetting.

        Separate from prepare() on purpose. The oversampling factor is a live
        parameter, so the voices' rate changes while notes are sounding - and
        routing that through prepare() would reset the envelope to idle at
        level zero, which kills every held note the instant the user touches
        the oversampling control. Stage and level are timed in seconds and
        stage fractions, not in samples, so nothing here needs rescaling. */
    void setSampleRate (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;
    }

    void reset() noexcept
    {
        stage = Stage::idle;
        stagePosition = 0.0f;
        level = 0.0f;
        releaseStartLevel = 0.0f;
        velocity = 1.0f;
    }

    void setSettings (const Settings& newSettings) noexcept { settings = newSettings; }

    void noteOn (float noteVelocity) noexcept
    {
        velocity = juce::jlimit (0.0f, 1.0f, noteVelocity);

        // DAHDSR starts at the delay; ADSR skips straight to the attack. The
        // mode is not just a label: a delay of zero still costs a stage
        // transition, and an ADSR patch should not pay for one.
        stage = settings.mode == choices::EnvelopeMode::dahdsr && settings.delaySeconds > 0.0f
            ? Stage::delay
            : Stage::attack;

        stagePosition = 0.0f;

        // The attack starts from the CURRENT level, not from zero, so
        // retriggering a voice still in its release does not click.
        attackStartLevel = level;
    }

    void noteOff() noexcept
    {
        if (stage == Stage::idle || stage == Stage::release)
            return;

        releaseStartLevel = level;
        stage = Stage::release;
        stagePosition = 0.0f;
    }

    /** Cuts immediately, for a stolen voice. */
    void kill() noexcept
    {
        stage = Stage::idle;
        level = 0.0f;
    }

    bool isActive() const noexcept { return stage != Stage::idle; }
    bool isReleasing() const noexcept { return stage == Stage::release; }
    Stage getStage() const noexcept { return stage; }

    /** Level scaled by velocity, which is what an amp envelope should output.
        Modulation destinations use this too, so velocity reaches them. */
    float getLevel() const noexcept
    {
        const auto velocityScale = 1.0f - settings.velocityAmount * (1.0f - velocity);
        return level * velocityScale;
    }

    /** Raw level, ignoring velocity. */
    float getRawLevel() const noexcept { return level; }

    /** Advances by `numSamples` and returns the new velocity-scaled level. */
    float process (int numSamples) noexcept
    {
        if (stage == Stage::idle)
            return 0.0f;

        const auto seconds = static_cast<float> (numSamples) / static_cast<float> (sampleRateHz);

        switch (stage)
        {
            case Stage::delay:
                if (advanceStage (seconds, settings.delaySeconds))
                {
                    stage = Stage::attack;
                    attackStartLevel = level;
                }
                break;

            case Stage::attack:
            {
                const auto done = advanceStage (seconds, settings.attackSeconds);
                const auto shaped = LfoCurve::applyTension (stagePosition, -settings.attackCurve);
                level = attackStartLevel + (1.0f - attackStartLevel) * shaped;

                if (done)
                {
                    level = 1.0f;
                    stage = settings.mode == choices::EnvelopeMode::dahdsr
                                && settings.holdSeconds > 0.0f
                        ? Stage::hold
                        : Stage::decay;
                    stagePosition = 0.0f;
                }
                break;
            }

            case Stage::hold:
                level = 1.0f;

                if (advanceStage (seconds, settings.holdSeconds))
                {
                    stage = Stage::decay;
                    stagePosition = 0.0f;
                }
                break;

            case Stage::decay:
            {
                const auto done = advanceStage (seconds, settings.decaySeconds);
                const auto shaped = LfoCurve::applyTension (stagePosition, settings.decayCurve);
                level = 1.0f + (settings.sustain - 1.0f) * shaped;

                if (done)
                {
                    level = settings.sustain;
                    stage = Stage::sustain;
                }
                break;
            }

            case Stage::sustain:
                level = settings.sustain;
                break;

            case Stage::release:
            {
                const auto done = advanceStage (seconds, settings.releaseSeconds);
                const auto shaped = LfoCurve::applyTension (stagePosition, settings.releaseCurve);
                level = releaseStartLevel * (1.0f - shaped);

                if (done)
                {
                    level = 0.0f;
                    stage = Stage::idle;
                }
                break;
            }

            case Stage::idle:
            default:
                break;
        }

        return getLevel();
    }

private:
    /** Advances the current stage; returns true when it has completed.
        A zero-length stage completes immediately rather than dividing by
        zero - and a zero attack must be instant, not silent. */
    bool advanceStage (float seconds, float stageLength) noexcept
    {
        if (stageLength <= 0.0f)
        {
            stagePosition = 1.0f;
            return true;
        }

        stagePosition += seconds / stageLength;

        if (stagePosition >= 1.0f)
        {
            stagePosition = 1.0f;
            return true;
        }

        return false;
    }

    double sampleRateHz = 44100.0;

    Settings settings {};

    Stage stage = Stage::idle;
    float stagePosition = 0.0f;
    float level = 0.0f;
    float attackStartLevel = 0.0f;
    float releaseStartLevel = 0.0f;
    float velocity = 1.0f;
};

} // namespace gnarl::dsp
