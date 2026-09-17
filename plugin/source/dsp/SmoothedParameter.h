#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

namespace gnarl::dsp
{

/**
    Ramp times for smoothed parameters.

    These are a policy, not arbitrary numbers. Too short and the parameter
    clicks; too long and the control feels disconnected from the hand moving it.
    The values below are the shortest ramp that is inaudible for each kind of
    parameter.
*/
namespace ramp
{
    /** Gain and level. 20 ms is long enough that a full-scale jump is a fade
        rather than a step, and short enough to feel immediate. */
    inline constexpr double gainSeconds = 0.02;

    /** Filter cutoff and resonance. Shorter than gain: a filter sweep should
        track the hand closely, and coefficient changes are less click-prone
        than a gain step. */
    inline constexpr double filterSeconds = 0.01;

    /** Pan and stereo width. */
    inline constexpr double panSeconds = 0.02;

    /** Pitch, excluding glide (which has its own user-facing time). 5 ms:
        any longer and a detune tweak audibly lags. */
    inline constexpr double pitchSeconds = 0.005;

    /** Wavetable position, warp amount, grain parameters. Deliberately short
        so a fast modulated wobble is not smeared by the smoother itself. */
    inline constexpr double morphSeconds = 0.005;

    /** Ramp used when a voice is stolen, to fade it out before the new note
        starts. 4 ms is below the threshold of a perceptible click but short
        enough that the stolen note's tail is not audible as a separate event. */
    inline constexpr double stealFadeSeconds = 0.004;
}

/**
    A host parameter plus its smoother.

    Holds the raw `std::atomic<float>*` rather than looking the parameter up by
    ID, because a string lookup per block is a hash per block for no reason.

    Update rate vocabulary (see CLAUDE.md section 5):
      - `updateTarget()` is BLOCK-RATE: call it once per processBlock.
      - `getNextValue()` is SAMPLE-RATE: call it per sample inside the block.
      - `getCurrentValue()` reads the smoother without advancing it, for the
        block-rate case where per-sample accuracy is not needed.

    Real-time safe: no allocation, no locks, no virtual calls.
*/
class SmoothedParameter
{
public:
    SmoothedParameter() = default;

    /** Binds to a parameter. Call from the constructor, never from the audio
        thread: getRawParameterValue does a string lookup. */
    void bind (juce::AudioProcessorValueTreeState& apvts,
               const char* parameterID,
               double rampSeconds)
    {
        source = apvts.getRawParameterValue (parameterID);
        rampTime = rampSeconds;
        jassert (source != nullptr);
    }

    /** Allocation and reset belong here, in prepareToPlay. */
    void prepare (double sampleRate)
    {
        smoothed.reset (sampleRate, rampTime);
        smoothed.setCurrentAndTargetValue (readSource());
    }

    /** BLOCK-RATE. Pulls the parameter's current value in as the new target. */
    void updateTarget() noexcept
    {
        smoothed.setTargetValue (readSource());
    }

    /** SAMPLE-RATE. Advances the smoother by one sample. */
    float getNextValue() noexcept { return smoothed.getNextValue(); }

    /** Reads without advancing. */
    float getCurrentValue() const noexcept { return smoothed.getCurrentValue(); }

    bool isSmoothing() const noexcept { return smoothed.isSmoothing(); }

    /** Skips the ramp. For a preset load, where ramping from the old patch's
        value to the new one would be an audible glide rather than a change. */
    void snapToTarget() noexcept
    {
        smoothed.setCurrentAndTargetValue (readSource());
    }

    /** SAMPLE-RATE. Advances by `numSamples` and returns the final value, for
        the block-rate case where the intermediate values are not needed. */
    float skip (int numSamples) noexcept { return smoothed.skip (numSamples); }

private:
    float readSource() const noexcept
    {
        return source != nullptr ? source->load() : 0.0f;
    }

    std::atomic<float>* source = nullptr;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothed { 0.0f };
    double rampTime = ramp::gainSeconds;
};

} // namespace gnarl::dsp
