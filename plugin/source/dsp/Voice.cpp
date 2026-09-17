#include "Voice.h"

#include "SmoothedParameter.h"
#include "../params/ParameterIDs.h"

namespace gnarl::dsp
{

namespace
{
    /** Placeholder amplitude envelope times, until Env 1 lands in Phase 3.
        Short attack so a note is immediate; release long enough that stopping
        a note is a fade rather than a cut. */
    constexpr double kPlaceholderAttackSeconds  = 0.005;
    constexpr double kPlaceholderReleaseSeconds = 0.080;

    /** Below this level a releasing voice is inaudible, so it is freed for
        reuse. -80 dB, chosen because it is below the noise floor of any
        24-bit render. */
    constexpr float kSilenceThreshold = 0.0001f;

    /** Changes a smoother's ramp length WITHOUT losing where it currently is.

        juce::SmoothedValue::reset() snaps the current value to the target as a
        side effect, so the obvious `reset(); setTargetValue(x);` silently
        teleports the value first - which is exactly the discontinuity the
        smoother exists to prevent. This preserves the current value across the
        ramp change. */
    template <typename SmoothedType>
    void setRampPreservingValue (SmoothedType& smoothed, double sampleRate, double rampSeconds)
    {
        const auto current = smoothed.getCurrentValue();
        smoothed.reset (sampleRate, rampSeconds);
        smoothed.setCurrentAndTargetValue (current);
    }
}

void Voice::prepare (double sampleRate, int maximumBlockSize)
{
    juce::ignoreUnused (maximumBlockSize);

    sampleRateHz = sampleRate;

    // All resets happen here, never in render().
    pitch.reset (sampleRate, 0.0);
    amplitude.reset (sampleRate, kPlaceholderAttackSeconds);
    stealFade.reset (sampleRate, ramp::stealFadeSeconds);

    reset();
}

void Voice::setRandomSeed (juce::int64 seed)
{
    // A dedicated Random per voice, seeded deterministically, so the values
    // below are identical on every load of the same patch.
    juce::Random random (seed);

    for (auto& value : unisonRandom)
        value = random.nextFloat() * 2.0f - 1.0f;

    driftAmount = random.nextFloat() * 2.0f - 1.0f;
}

void Voice::reset()
{
    state = State::idle;

    pitch.setCurrentAndTargetValue (static_cast<float> (currentNote));
    amplitude.setCurrentAndTargetValue (0.0f);
    stealFade.setCurrentAndTargetValue (1.0f);

    queuedNote.reset();
    queuedGlideTime = 0.0f;
}

float Voice::getUnisonRandom (int unisonIndex) const noexcept
{
    const auto clamped = juce::jlimit (0, static_cast<int> (unisonRandom.size()) - 1, unisonIndex);
    return unisonRandom[static_cast<std::size_t> (clamped)];
}

float Voice::getCurrentAmplitude() const noexcept
{
    return amplitude.getCurrentValue() * currentVelocity * stealFade.getCurrentValue();
}

void Voice::updatePitchTarget (const NoteRequest& request, float glideTimeSeconds)
{
    const auto target = static_cast<float> (request.noteNumber);

    if (request.glideFromNote.has_value() && glideTimeSeconds > 0.0f)
    {
        pitch.reset (sampleRateHz, glideTimeSeconds);
        pitch.setCurrentAndTargetValue (static_cast<float> (*request.glideFromNote));
        pitch.setTargetValue (target);
    }
    else
    {
        pitch.reset (sampleRateHz, 0.0);
        pitch.setCurrentAndTargetValue (target);
    }
}

void Voice::beginAmplitudeAttack()
{
    // Preserves the current level, so retriggering a voice that is still
    // releasing ramps up from where it actually is rather than jumping to zero.
    setRampPreservingValue (amplitude, sampleRateHz, kPlaceholderAttackSeconds);
    amplitude.setTargetValue (1.0f);
}

void Voice::startNote (const NoteRequest& request, float glideTimeSeconds)
{
    currentNote = request.noteNumber;
    currentChannel = request.channel;
    currentVelocity = juce::jlimit (0.0f, 1.0f, request.velocity);

    updatePitchTarget (request, glideTimeSeconds);

    // Starts from wherever the envelope currently is, not from zero, so
    // retriggering a still-sounding voice does not produce a gap.
    beginAmplitudeAttack();

    stealFade.setCurrentAndTargetValue (1.0f);

    state = State::active;
}

void Voice::changeNote (const NoteRequest& request, float glideTimeSeconds)
{
    // Legato: pitch moves, the envelope is left alone.
    currentNote = request.noteNumber;
    currentChannel = request.channel;

    updatePitchTarget (request, glideTimeSeconds);

    if (state == State::releasing)
    {
        beginAmplitudeAttack();
        state = State::active;
    }
}

void Voice::stopNote (bool allowTailOff)
{
    if (state == State::idle)
        return;

    if (! allowTailOff)
    {
        // The host wants the voice gone now. Still fade rather than hard-zero:
        // an instantaneous cut is an audible click.
        setRampPreservingValue (stealFade, sampleRateHz, ramp::stealFadeSeconds);
        stealFade.setTargetValue (0.0f);
        queuedNote.reset();
        state = State::stealing;
        return;
    }

    setRampPreservingValue (amplitude, sampleRateHz, kPlaceholderReleaseSeconds);
    amplitude.setTargetValue (0.0f);

    state = State::releasing;
}

void Voice::stealWith (const NoteRequest& request, float glideTimeSeconds)
{
    queuedNote = request;
    queuedGlideTime = glideTimeSeconds;

    if (state == State::idle)
    {
        // Nothing to fade out.
        startNote (request, glideTimeSeconds);
        return;
    }

    setRampPreservingValue (stealFade, sampleRateHz, ramp::stealFadeSeconds);
    stealFade.setTargetValue (0.0f);

    state = State::stealing;
}

void Voice::render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    juce::ignoreUnused (buffer, startSample);

    if (state == State::idle || numSamples <= 0)
        return;

    // Phase 1 has no oscillators, so nothing is added to the buffer. The
    // envelope, pitch and fade still advance, which is what makes voice
    // stealing and glide testable before any sound exists.
    amplitude.skip (numSamples);
    pitch.skip (numSamples);

    if (state == State::stealing)
    {
        stealFade.skip (numSamples);

        if (! stealFade.isSmoothing())
        {
            if (queuedNote.has_value())
            {
                const auto request = *queuedNote;
                const auto glide = queuedGlideTime;
                queuedNote.reset();
                queuedGlideTime = 0.0f;

                // The fade is spent, so the voice is silent: it is safe to
                // jump the envelope to zero and restart from there.
                stealFade.setCurrentAndTargetValue (1.0f);
                amplitude.setCurrentAndTargetValue (0.0f);
                startNote (request, glide);
            }
            else
            {
                reset();
            }
        }

        return;
    }

    if (state == State::releasing && amplitude.getCurrentValue() <= kSilenceThreshold)
        reset();
}

} // namespace gnarl::dsp
