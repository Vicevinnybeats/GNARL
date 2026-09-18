#pragma once

#include "../params/ParameterChoices.h"

#include <juce_dsp/juce_dsp.h>

#include <memory>

namespace gnarl::dsp
{

/**
    Oversampling around the whole voice section.

    WHY AROUND THE VOICES, not around each nonlinearity. Aliasing is created
    the moment a nonlinear stage runs, and no filter applied afterwards can
    remove it - the aliased partials are already inside the audible band and
    indistinguishable from real ones. So the nonlinear stages have to RUN at
    the higher rate, which means the oscillators and filters feeding them do
    too. Wrapping each drive stage individually would mean up/down conversion
    per filter per voice: thirty-two conversions a block instead of one, for a
    worse result, because the filters between them would still be aliasing
    their own resonance.

    HOW IT WORKS WITH A SYNTH. juce::dsp::Oversampling is built for effects: up,
    process, down. A synth has no input, so the up-conversion is fed silence
    and the voices ADD into the oversampled block. Feeding silence through the
    polyphase filters costs a little arithmetic and gives back exactly the
    right buffer to render into, with the correct anti-imaging filters on the
    way back down.

    The latency the filters introduce is reported so the host can compensate.
*/
class VoiceOversampler
{
public:
    using Factor = choices::Oversampling;

    /** The highest ratio this class can be set to. Callers size their buffers
        and delay lines from it, so switching factor never allocates. */
    static constexpr int kMaxRatio = 4;

    /** MESSAGE THREAD. Allocates for the worst case, so changing factor later
        never allocates. */
    void prepare (int numChannels, int maximumBlockSize)
    {
        channels = juce::jmax (1, numChannels);
        maxBlockSize = juce::jmax (1, maximumBlockSize);

        // One Oversampling object per factor, all prepared up front. Building
        // one on demand would allocate on the audio thread the first time a
        // user touched the control.
        for (int stages = 1; stages <= kMaxStages; ++stages)
        {
            auto oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
                static_cast<std::size_t> (channels),
                static_cast<std::size_t> (stages),
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                true,   // maximum quality
                true);  // integer latency, so host compensation is exact

            oversampler->initProcessing (static_cast<std::size_t> (maxBlockSize));
            oversampler->reset();

            oversamplers[static_cast<std::size_t> (stages - 1)] = std::move (oversampler);
        }

        silence.setSize (channels, maxBlockSize, false, true, true);
        silence.clear();
    }

    void reset() noexcept
    {
        for (auto& oversampler : oversamplers)
            if (oversampler != nullptr)
                oversampler->reset();
    }

    void setFactor (Factor newFactor) noexcept { factor = newFactor; }
    Factor getFactor() const noexcept { return factor; }

    static int getStagesForFactor (Factor value) noexcept
    {
        switch (value)
        {
            case Factor::twoTimes:  return 1;
            case Factor::fourTimes: return 2;

            case Factor::off:
            case Factor::count:
            default:                return 0;
        }
    }

    /** Latency in samples at the base rate, for setLatencySamples(). */
    float getLatencySamples() const noexcept
    {
        const auto stages = getStagesForFactor (factor);

        if (stages <= 0)
            return 0.0f;

        const auto& oversampler = oversamplers[static_cast<std::size_t> (stages - 1)];

        return oversampler != nullptr ? oversampler->getLatencyInSamples() : 0.0f;
    }

    /**
        Runs `renderVoices` at the oversampled rate and sums the result into
        `output`.

        `renderVoices` is handed a block at the higher rate and the rate
        itself, and must ADD into it.
    */
    template <typename RenderFunction>
    void process (juce::AudioBuffer<float>& output,
                  int numSamples,
                  double baseSampleRate,
                  RenderFunction&& renderVoices)
    {
        const auto stages = getStagesForFactor (factor);

        if (stages <= 0 || numSamples <= 0 || numSamples > maxBlockSize)
        {
            // Off, or a block larger than we prepared for: render at the base
            // rate rather than refusing to make a sound.
            juce::dsp::AudioBlock<float> block (output);
            renderVoices (block.getSubBlock (0, static_cast<std::size_t> (numSamples)),
                          baseSampleRate);
            return;
        }

        auto& oversampler = oversamplers[static_cast<std::size_t> (stages - 1)];

        if (oversampler == nullptr)
            return;

        // Up-convert silence, purely to obtain a correctly sized oversampled
        // block with the filters' state advanced.
        juce::dsp::AudioBlock<float> silenceBlock (silence);
        auto inputBlock = silenceBlock.getSubBlock (0, static_cast<std::size_t> (numSamples));
        inputBlock.clear();

        auto oversampledBlock = oversampler->processSamplesUp (inputBlock);

        const auto ratio = 1 << stages;
        renderVoices (oversampledBlock, baseSampleRate * ratio);

        juce::dsp::AudioBlock<float> outputBlock (output);
        auto destination = outputBlock.getSubBlock (0, static_cast<std::size_t> (numSamples));

        oversampler->processSamplesDown (destination);
    }

private:
    /** Two stages is 4x. More is not offered: the polyphase filters are not
        free, and beyond 4x the remaining aliasing is below the noise floor of
        anything these curves produce. */
    static constexpr int kMaxStages = 2;

    Factor factor = Factor::twoTimes;

    int channels = 2;
    int maxBlockSize = 512;

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, kMaxStages> oversamplers {};

    /** Fed to processSamplesUp, since a synth has no input. */
    juce::AudioBuffer<float> silence;
};

} // namespace gnarl::dsp
