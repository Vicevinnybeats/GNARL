#pragma once

#include "../params/ParameterChoices.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <cstdint>

namespace gnarl::dsp
{

/**
    Per-voice noise source: white, pink, brown, blue and vinyl.

    Uses a seeded xorshift rather than juce::Random so a patch renders
    identically every time - a bounce whose noise differs from the preview is
    a bug report waiting to happen, and noise is exactly where that is hardest
    to notice before release.

    No band-limiting: noise is broadband by definition, so there is nothing to
    alias. The coloured variants are ordinary filters over the white source.
*/
class NoiseGenerator
{
public:
    using Type = choices::NoiseType;

    void prepare (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;
        reset();
    }

    /** AUDIO THREAD SAFE. Only the vinyl crackle interval depends on it. */
    void setSampleRate (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;
    }

    void reset() noexcept
    {
        pinkState.fill (0.0f);
        brownState = 0.0f;
        lastWhite = 0.0f;
        vinylLowPass = 0.0f;
        crackleCountdown = 0;
    }

    /** Seeded per voice, so stacked notes are not correlated. */
    void setSeed (std::uint32_t seed) noexcept
    {
        state = seed != 0u ? seed : 1u;
    }

    void setType (Type newType) noexcept { type = newType; }

    /** SAMPLE-RATE. Output is roughly +/-1 for every type, so switching type
        is not also a level change. */
    float processSample() noexcept
    {
        const auto white = nextWhite();

        switch (type)
        {
            case Type::white:
                return white;

            case Type::pink:
                return processPink (white);

            case Type::brown:
            {
                // Leaky integrator: a pure integrator random-walks away from
                // zero and eventually saturates the mix bus.
                brownState = brownState * 0.997f + white * 0.035f;
                return juce::jlimit (-1.0f, 1.0f, brownState * 3.0f);
            }

            case Type::blue:
            {
                // Differentiated white: +3 dB/octave.
                const auto blue = (white - lastWhite) * 0.5f;
                lastWhite = white;
                return blue;
            }

            case Type::vinyl:
                return processVinyl (white);

            case Type::count:
            default:
                return white;
        }
    }

private:
    float nextWhite() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;

        // 24 bits is more than enough entropy for noise and keeps the
        // conversion exact.
        return static_cast<float> (state & 0xFFFFFFu)
             / static_cast<float> (0x7FFFFF) - 1.0f;
    }

    /** Paul Kellet's filter bank: -3 dB/octave to well within a dB across the
        audio band, for a handful of multiply-adds. Much cheaper than an FFT
        approach and indistinguishable in use. */
    float processPink (float white) noexcept
    {
        pinkState[0] = 0.99886f * pinkState[0] + white * 0.0555179f;
        pinkState[1] = 0.99332f * pinkState[1] + white * 0.0750759f;
        pinkState[2] = 0.96900f * pinkState[2] + white * 0.1538520f;
        pinkState[3] = 0.86650f * pinkState[3] + white * 0.3104856f;
        pinkState[4] = 0.55000f * pinkState[4] + white * 0.5329522f;
        pinkState[5] = -0.7616f * pinkState[5] - white * 0.0168980f;

        const auto pink = pinkState[0] + pinkState[1] + pinkState[2] + pinkState[3]
                        + pinkState[4] + pinkState[5] + pinkState[6] + white * 0.5362f;

        pinkState[6] = white * 0.115926f;

        return pink * 0.11f;
    }

    /** Filtered noise plus sparse crackle: surface noise rather than hiss.
        Useful under a growl as grit that does not add brightness. */
    float processVinyl (float white) noexcept
    {
        // Dull the hiss.
        vinylLowPass += 0.08f * (white - vinylLowPass);

        auto crackle = 0.0f;

        if (crackleCountdown <= 0)
        {
            // Next click somewhere in the following ~50 ms.
            const auto interval = static_cast<int> (sampleRateHz * 0.05);
            crackleCountdown = 1 + static_cast<int> (
                (nextWhite() * 0.5f + 0.5f) * static_cast<float> (interval));

            crackle = nextWhite() * 0.8f;
        }
        else
        {
            --crackleCountdown;
        }

        return vinylLowPass * 0.6f + crackle;
    }

    double sampleRateHz = 44100.0;
    Type type = Type::white;

    std::uint32_t state = 1u;

    std::array<float, 7> pinkState {};
    float brownState = 0.0f;
    float lastWhite = 0.0f;
    float vinylLowPass = 0.0f;
    int crackleCountdown = 0;
};

} // namespace gnarl::dsp
