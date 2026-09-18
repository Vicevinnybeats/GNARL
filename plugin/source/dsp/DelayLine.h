#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <vector>

namespace gnarl::dsp
{

/**
    A fractional-delay line, shared by every effect here that needs one.

    CUBIC INTERPOLATION ON THE READ TAP, NOT LINEAR, and the reason is the
    modulated effects rather than the delay. Linear interpolation between two
    samples is a low-pass whose corner depends on the FRACTIONAL part of the
    delay: at a whole number of samples it is transparent and half way between
    two it is at its dullest. A chorus sweeps that fraction continuously, so
    linear interpolation makes the tone brighten and dull in time with the
    sweep - a warble on top of the pitch modulation, and one that gets worse
    the higher the material. Catmull-Rom costs three more multiplies and a
    wider read window, and does not have it.

    Allocation happens ONLY in prepare. Every other member is real-time safe:
    the write index wraps with a comparison rather than a modulo, and the read
    clamps its delay into the buffer rather than trusting the caller.
*/
class DelayLine
{
public:
    /** MESSAGE THREAD. Sizes the buffer for the longest delay this line will
        ever be asked for, INCLUDING any oversampled rate - the buffer must not
        be reallocated when the oversampling factor changes. */
    void prepare (int maximumDelaySamples)
    {
        // Four samples of headroom: the cubic read touches the two samples
        // either side of its position, and one more for the write head so a
        // read at the maximum delay cannot land on the sample being written.
        const auto size = juce::jmax (8, maximumDelaySamples + 4);

        buffer.assign (static_cast<std::size_t> (size), 0.0f);
        writeIndex = 0;
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

    /** SAMPLE-RATE. */
    void write (float input) noexcept
    {
        if (buffer.empty())
            return;

        buffer[static_cast<std::size_t> (writeIndex)] = input;

        if (++writeIndex >= static_cast<int> (buffer.size()))
            writeIndex = 0;
    }

    /** SAMPLE-RATE. `delaySamples` may be fractional and is clamped into the
        buffer: a modulated delay WILL be driven past its own limits, and
        reading outside the buffer is worse than reading the closest sample
        inside it. */
    float read (float delaySamples) const noexcept
    {
        if (buffer.empty())
            return 0.0f;

        const auto size = static_cast<int> (buffer.size());

        // A delay of less than one sample cannot be read: the sample it wants
        // has not been written yet. Two samples of margin at the far end keeps
        // the cubic window inside the buffer.
        const auto clamped = juce::jlimit (1.0f, static_cast<float> (size) - 3.0f,
                                           delaySamples);

        auto position = static_cast<float> (writeIndex) - clamped;

        while (position < 0.0f)
            position += static_cast<float> (size);

        const auto index = static_cast<int> (position);
        const auto fraction = position - static_cast<float> (index);

        const auto at = [this, size] (int offset)
        {
            auto i = offset % size;

            if (i < 0)
                i += size;

            return buffer[static_cast<std::size_t> (i)];
        };

        // Catmull-Rom through the four samples around the read position.
        const auto y0 = at (index - 1);
        const auto y1 = at (index);
        const auto y2 = at (index + 1);
        const auto y3 = at (index + 2);

        const auto a = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        const auto b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const auto c = 0.5f * (y2 - y0);

        return ((a * fraction + b) * fraction + c) * fraction + y1;
    }

    /** The longest delay this line can serve, in samples. */
    float getMaximumDelaySamples() const noexcept
    {
        return buffer.empty() ? 0.0f : static_cast<float> (buffer.size()) - 3.0f;
    }

    bool isPrepared() const noexcept { return ! buffer.empty(); }

private:
    std::vector<float> buffer;
    int writeIndex = 0;
};

/**
    A one-pole low-pass and a one-pole high-pass, for the tone controls inside
    the time-based effects.

    These are NOT the EQ's bands and deliberately not as good. A delay's
    feedback path wants a gentle 6 dB per octave tilt that costs one multiply
    per sample, because it runs inside the feedback loop where a steeper filter
    would add phase the repeats accumulate.
*/
class OnePole
{
public:
    void prepare (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate;
        reset();
    }

    void setSampleRate (double sampleRate) noexcept { sampleRateHz = sampleRate; }

    void reset() noexcept { state = 0.0f; }

    /** BLOCK-RATE. */
    void setCutoff (float cutoffHz) noexcept
    {
        const auto nyquist = static_cast<float> (sampleRateHz) * 0.5f;
        const auto clamped = juce::jlimit (10.0f, nyquist * 0.98f, cutoffHz);

        coefficient = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * clamped
                                       / static_cast<float> (sampleRateHz));
    }

    /** SAMPLE-RATE. */
    float processLowPass (float input) noexcept
    {
        state += coefficient * (input - state);
        return state;
    }

    /** SAMPLE-RATE. */
    float processHighPass (float input) noexcept
    {
        return input - processLowPass (input);
    }

    bool hasBlownUp() const noexcept { return ! std::isfinite (state); }

private:
    double sampleRateHz = 44100.0;
    float coefficient = 1.0f;
    float state = 0.0f;
};

} // namespace gnarl::dsp
