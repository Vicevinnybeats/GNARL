#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <vector>

namespace gnarl::dsp
{

/**
    Feedback comb filter with a damping low-pass in the loop.

    The cutoff parameter sets the delay length rather than a corner frequency:
    a comb's "pitch" is the reciprocal of its delay, so sweeping cutoff sweeps
    the resonant series. Fractional delay is interpolated, so the sweep is
    continuous rather than stepping between integer sample lengths - a stepping
    comb sweep sounds broken.

    The damping filter in the feedback path is not optional. Without it a comb
    at high feedback accumulates high-frequency energy until it screams, and no
    amount of feedback clamping fixes that because the problem is spectral, not
    a level.
*/
class CombFilter
{
public:
    /** MESSAGE THREAD. `maximumSampleRate` sizes the delay line, so pass the
        highest rate this filter will ever run at - including any oversampled
        rate. Changing the working rate afterwards must not allocate, because
        the oversampling factor is a live parameter. */
    void prepare (double maximumSampleRate)
    {
        const auto maxRate = maximumSampleRate > 0.0 ? maximumSampleRate : 44100.0;

        const auto maxDelaySamples =
            static_cast<int> (std::ceil (maxRate / kMinFrequencyHz)) + 4;

        buffer.assign (static_cast<std::size_t> (maxDelaySamples), 0.0f);
        writeIndex = 0;

        setSampleRate (maxRate);
        reset();
        setFrequency (440.0f);
        setFeedback (0.5f);
        setDamping (0.3f);
    }

    /** AUDIO THREAD SAFE. Never allocates: the buffer was sized in prepare
        for the highest rate this filter can run at. */
    void setSampleRate (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        dampingState = 0.0f;
        writeIndex = 0;
    }

    /** SAMPLE-RATE safe. The frequency whose period equals the delay. */
    void setFrequency (float frequencyHz) noexcept
    {
        if (buffer.empty())
            return;

        const auto nyquist = static_cast<float> (sampleRateHz) * 0.5f;
        const auto clamped = juce::jlimit (kMinFrequencyHz, nyquist * 0.5f, frequencyHz);

        delaySamples = juce::jlimit (1.0f,
                                     static_cast<float> (buffer.size()) - 3.0f,
                                     static_cast<float> (sampleRateHz) / clamped);
    }

    void setFeedback (float amount) noexcept
    {
        // Capped below 1: at or above unity the loop gain never decays, and
        // with the damping filter's phase shift it can still grow.
        feedback = juce::jlimit (-0.98f, 0.98f, amount);
    }

    /** 0 is no damping, 1 is heavy. */
    void setDamping (float amount) noexcept
    {
        // One-pole coefficient. At damping 1 the loop keeps almost no high
        // frequency, which is what makes extreme feedback survivable.
        dampingCoefficient = juce::jlimit (0.0f, 0.98f, amount);
    }

    /** SAMPLE-RATE. */
    float processSample (float input) noexcept
    {
        if (buffer.empty())
            return input;

        const auto size = static_cast<int> (buffer.size());

        // Linear interpolation on the read tap, so a swept delay length is
        // continuous instead of stepping sample to sample.
        auto readPosition = static_cast<float> (writeIndex) - delaySamples;

        while (readPosition < 0.0f)
            readPosition += static_cast<float> (size);

        const auto readIndex = static_cast<int> (readPosition);
        const auto fraction = readPosition - static_cast<float> (readIndex);

        const auto a = buffer[static_cast<std::size_t> (readIndex % size)];
        const auto b = buffer[static_cast<std::size_t> ((readIndex + 1) % size)];
        const auto delayed = a + (b - a) * fraction;

        // One-pole low-pass in the feedback path.
        dampingState = delayed * (1.0f - dampingCoefficient)
                     + dampingState * dampingCoefficient;

        buffer[static_cast<std::size_t> (writeIndex)] = input + dampingState * feedback;

        writeIndex = (writeIndex + 1) % size;

        return delayed;
    }

    bool hasBlownUp() const noexcept { return ! std::isfinite (dampingState); }

private:
    /** Lowest comb frequency, which sets the delay buffer size. 20 Hz is a
        50 ms delay - past that a comb is a delay effect, not a filter. */
    static constexpr float kMinFrequencyHz = 20.0f;

    double sampleRateHz = 44100.0;

    std::vector<float> buffer;
    int writeIndex = 0;

    float delaySamples = 100.0f;
    float feedback = 0.0f;
    float dampingCoefficient = 0.0f;
    float dampingState = 0.0f;
};

} // namespace gnarl::dsp
