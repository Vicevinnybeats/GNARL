#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace gnarl::dsp
{

/**
    A band-limited wavetable: 256 frames, mip-mapped.

    WHY MIP-MAPPING. Reading a table containing 1024 harmonics at a high pitch
    folds every harmonic above Nyquist back down into the audible band as
    inharmonic garbage. That aliasing is the single most common reason a
    wavetable synth sounds cheap in the top two octaves. The fix is to store
    several band-limited versions of each frame and read from the one whose
    highest harmonic still fits below Nyquist at the current pitch.

    Level 0 holds 1024 harmonics in 2048 samples. Each level above halves both,
    down to level 10 (1 harmonic, 2 samples). Halving the sample count as well
    as the harmonic count is not an approximation: a signal with N harmonics is
    fully described by 2N samples, so the smaller levels are exact.

    Total cost is about 4 MB per table, because 2048 + 1024 + 512 + ... < 4096
    samples per frame.

    GUARD SAMPLES. Each frame is padded on BOTH sides with samples wrapped
    from the other end, so 4-point cubic interpolation - which reads
    x[i-1] .. x[i+2] - needs no branch, no modulo and no wraparound test
    anywhere in the inner loop. One sample before, three after.

    getFrame() returns a pointer to sample 0, so frame[-1] is legal and holds
    the last sample of the cycle. That is the correct value, not a fudge: a
    single cycle is periodic.

    Generation happens on the message thread. Reads are const and real-time
    safe.
*/
class Wavetable
{
public:
    static constexpr int kNumFrames = 256;

    /** Samples in mip level 0. */
    static constexpr int kBaseFrameSize = 2048;

    /** Harmonics in mip level 0. A 2048-sample frame can hold 1024. */
    static constexpr int kBaseNumHarmonics = kBaseFrameSize / 2;

    /** Levels 0..10: 1024 harmonics down to 1. */
    static constexpr int kNumMipLevels = 11;

    /** Wrapped samples before sample 0, so cubic can read frame[-1]. */
    static constexpr int kGuardSamplesBefore = 1;

    /** Wrapped samples after the last sample, so cubic can read frame[size+1]. */
    static constexpr int kGuardSamplesAfter = 3;

    /** One frame's harmonic content. Index 0 is the fundamental.

        A generator fills this instead of writing samples directly, which is
        what makes band-limiting possible at all: the mip levels are built by
        truncating the harmonic series, and you cannot truncate what you were
        handed as raw samples. */
    struct Spectrum
    {
        std::array<float, kBaseNumHarmonics> magnitude {};
        std::array<float, kBaseNumHarmonics> phase {};

        void clear()
        {
            magnitude.fill (0.0f);
            phase.fill (0.0f);
        }

        /** Sets one harmonic. `harmonic` is 1-based, as musicians count. */
        void setHarmonic (int harmonic, float amplitude, float phaseRadians = 0.0f)
        {
            if (harmonic < 1 || harmonic > kBaseNumHarmonics)
                return;

            magnitude[static_cast<std::size_t> (harmonic - 1)] = amplitude;
            phase[static_cast<std::size_t> (harmonic - 1)] = phaseRadians;
        }
    };

    /** Fills the spectrum for one frame. `framePosition` runs 0..1 across the
        table, which is what a generator should morph over - not the raw index,
        so the frame count can change without rewriting every generator. */
    using FrameGenerator = std::function<void (float framePosition, Spectrum&)>;

    Wavetable();

    /** Builds every frame and every mip level. Message thread only: this
        allocates and runs a few thousand FFTs. */
    void generate (const FrameGenerator& generator);

    /** Loads raw single-cycle frames, re-deriving the harmonic content so the
        mip levels are still band-limited. Used for imported .wav tables. */
    void generateFromSamples (const float* interleavedFrames,
                              int numFrames,
                              int frameSize);

    bool isGenerated() const noexcept { return generated; }

    const juce::String& getName() const noexcept { return name; }
    void setName (juce::String newName) { name = std::move (newName); }

    // --- Real-time safe reads ---------------------------------------------

    static constexpr int getFrameSizeForLevel (int mipLevel) noexcept
    {
        return kBaseFrameSize >> mipLevel;
    }

    static constexpr int getNumHarmonicsForLevel (int mipLevel) noexcept
    {
        return kBaseNumHarmonics >> mipLevel;
    }

    /**
        Picks the coarsest mip level that still has no harmonic above Nyquist.

        `increment` is the phase advance per sample, i.e. frequency / sampleRate.
        A level with H harmonics is safe while H * increment <= 0.5.
    */
    static int getMipLevelForIncrement (float increment) noexcept;

    /** Sample 0 of one frame at one level.

        Readable range is [-kGuardSamplesBefore, frameSize + kGuardSamplesAfter),
        so a 4-point interpolator may read frame[i - 1] through frame[i + 2]
        for any i in [0, frameSize) without a bounds check. */
    const float* getFrame (int mipLevel, int frameIndex) const noexcept;

    /** Peak absolute sample across the whole table, for normalisation. */
    float getPeakMagnitude() const noexcept { return peakMagnitude; }

private:
    void buildMipLevel (int mipLevel, const std::vector<Spectrum>& spectra);

    /** Scales every level so the table peaks at 1.0, and marks it generated. */
    void normaliseAndFinish();

    juce::String name { "Init" };
    bool generated = false;
    float peakMagnitude = 0.0f;

    /** One contiguous block per level: frames laid end to end, each followed
        by its guard samples. One allocation per level rather than 256 keeps
        the frames cache-friendly when table position is being modulated. */
    std::array<std::vector<float>, kNumMipLevels> levels;

    JUCE_LEAK_DETECTOR (Wavetable)
};

} // namespace gnarl::dsp
