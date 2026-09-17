#include "Wavetable.h"

#include <cmath>

namespace gnarl::dsp
{

namespace
{
    /** Stride of one frame at a level, including guard samples on both sides. */
    constexpr int frameStride (int mipLevel) noexcept
    {
        return Wavetable::getFrameSizeForLevel (mipLevel)
             + Wavetable::kGuardSamplesBefore
             + Wavetable::kGuardSamplesAfter;
    }

}

Wavetable::Wavetable() = default;

int Wavetable::getMipLevelForIncrement (float increment) noexcept
{
    // Guard the degenerate case: a stopped or reversed oscillator needs the
    // full-bandwidth level, and feeding 0 into a log would produce -inf.
    const auto magnitude = std::abs (increment);

    if (! (magnitude > 0.0f))
        return 0;

    // Safe while numHarmonics * increment <= 0.5, so the coarsest safe level
    // satisfies (kBaseNumHarmonics >> level) <= 0.5 / increment.
    const auto maxHarmonics = 0.5f / magnitude;

    if (maxHarmonics >= static_cast<float> (kBaseNumHarmonics))
        return 0;

    const auto level = static_cast<int> (
        std::ceil (std::log2 (static_cast<float> (kBaseNumHarmonics) / maxHarmonics)));

    return juce::jlimit (0, kNumMipLevels - 1, level);
}

const float* Wavetable::getFrame (int mipLevel, int frameIndex) const noexcept
{
    const auto level = juce::jlimit (0, kNumMipLevels - 1, mipLevel);
    const auto frame = juce::jlimit (0, kNumFrames - 1, frameIndex);

    const auto& data = levels[static_cast<std::size_t> (level)];

    if (data.empty())
        return nullptr;

    // Offset past the leading guard, so the returned pointer is sample 0 and
    // frame[-1] is the legal wrapped sample before it.
    return data.data()
         + static_cast<std::size_t> (frame) * static_cast<std::size_t> (frameStride (level))
         + static_cast<std::size_t> (kGuardSamplesBefore);
}

void Wavetable::buildMipLevel (int mipLevel, const std::vector<Spectrum>& spectra)
{
    const auto frameSize = getFrameSizeForLevel (mipLevel);
    const auto numHarmonics = getNumHarmonicsForLevel (mipLevel);
    const auto stride = frameStride (mipLevel);

    auto& destination = levels[static_cast<std::size_t> (mipLevel)];
    destination.assign (static_cast<std::size_t> (kNumFrames) * static_cast<std::size_t> (stride), 0.0f);

    const auto order = static_cast<int> (std::log2 (static_cast<double> (frameSize)));

    juce::dsp::FFT fft { order };
    std::vector<float> fftBuffer (static_cast<std::size_t> (frameSize) * 2, 0.0f);

    // juce::dsp::FFT's inverse transform divides by the transform size, so
    // the SAME harmonic amplitudes come out 2048/frameSize times quieter at
    // each finer level. Undoing it here is what keeps every mip level on one
    // amplitude scale - without it, switching level mid-glide is a jump in
    // volume, and the table's normalisation only makes level 0 correct.
    const auto inverseFftScale = static_cast<float> (frameSize);

    for (int frameIndex = 0; frameIndex < kNumFrames; ++frameIndex)
    {
        const auto& spectrum = spectra[static_cast<std::size_t> (frameIndex)];
        auto* frame = destination.data()
                    + static_cast<std::size_t> (frameIndex) * static_cast<std::size_t> (stride)
                    + static_cast<std::size_t> (kGuardSamplesBefore);

        {
            std::fill (fftBuffer.begin(), fftBuffer.end(), 0.0f);

            // Pack the harmonic series as a half-complex spectrum and invert
            // it. Only harmonics this level can represent are written; that
            // truncation IS the band-limiting.
            const auto usable = juce::jmin (numHarmonics, frameSize / 2 - 1);

            for (int h = 1; h <= usable; ++h)
            {
                const auto index = static_cast<std::size_t> (h);
                const auto magnitude = spectrum.magnitude[index - 1];

                // Exact zero, because the spectrum was filled with exact
                // zeroes: this skips the harmonics the generator never set.
                if (juce::exactlyEqual (magnitude, 0.0f))
                    continue;

                const auto phase = spectrum.phase[index - 1];

                fftBuffer[index * 2]     = magnitude * std::cos (phase);
                fftBuffer[index * 2 + 1] = magnitude * std::sin (phase);

                // Conjugate half, so the inverse transform is real.
                const auto mirror = static_cast<std::size_t> (frameSize) - index;
                fftBuffer[mirror * 2]     =  magnitude * std::cos (phase);
                fftBuffer[mirror * 2 + 1] = -magnitude * std::sin (phase);
            }

            fft.performRealOnlyInverseTransform (fftBuffer.data());

            for (int i = 0; i < frameSize; ++i)
                frame[i] = fftBuffer[static_cast<std::size_t> (i)] * inverseFftScale;
        }

        // Guard samples wrap around the SAME frame: a single cycle is
        // periodic, so this is the correct continuation, not a fudge.
        for (int g = 0; g < kGuardSamplesAfter; ++g)
            frame[frameSize + g] = frame[g % frameSize];

        for (int g = 1; g <= kGuardSamplesBefore; ++g)
            frame[-g] = frame[frameSize - g];
    }
}

void Wavetable::normaliseAndFinish()
{
    // Normalise the WHOLE table by its loudest frame, not each frame by its
    // own peak: per-frame normalisation would turn a table that deliberately
    // fades out into one that does not, destroying the morph the generator
    // described.
    peakMagnitude = 0.0f;

    if (const auto& base = levels[0]; ! base.empty())
        for (const auto sample : base)
            peakMagnitude = juce::jmax (peakMagnitude, std::abs (sample));

    if (peakMagnitude > 0.0f)
    {
        const auto scale = 1.0f / peakMagnitude;

        for (auto& level : levels)
            for (auto& sample : level)
                sample *= scale;
    }

    generated = true;
}

void Wavetable::generate (const FrameGenerator& generator)
{
    jassert (generator != nullptr);

    std::vector<Spectrum> spectra (static_cast<std::size_t> (kNumFrames));

    for (int frameIndex = 0; frameIndex < kNumFrames; ++frameIndex)
    {
        auto& spectrum = spectra[static_cast<std::size_t> (frameIndex)];
        spectrum.clear();

        // Position runs 0..1 inclusive, so a generator can morph between two
        // endpoints without special-casing the last frame.
        const auto position = static_cast<float> (frameIndex)
                            / static_cast<float> (kNumFrames - 1);

        generator (position, spectrum);
    }

    for (int level = 0; level < kNumMipLevels; ++level)
        buildMipLevel (level, spectra);

    normaliseAndFinish();
}

void Wavetable::generateFromSamples (const float* interleavedFrames,
                                     int numFrames,
                                     int frameSize)
{
    if (interleavedFrames == nullptr || numFrames <= 0 || frameSize <= 1)
        return;

    // Analyse each source frame back into harmonics, so the mip levels are
    // still genuinely band-limited. Copying samples straight in would give
    // level 0 the right content and every level above it the wrong content.
    const auto order = static_cast<int> (std::floor (std::log2 (static_cast<double> (frameSize))));
    const auto analysisSize = 1 << order;

    juce::dsp::FFT fft (order);
    std::vector<float> scratch (static_cast<std::size_t> (analysisSize) * 2, 0.0f);
    std::vector<Spectrum> spectra (static_cast<std::size_t> (kNumFrames));

    for (int frameIndex = 0; frameIndex < kNumFrames; ++frameIndex)
    {
        auto& spectrum = spectra[static_cast<std::size_t> (frameIndex)];
        spectrum.clear();

        // Map our fixed 256 frames onto however many the source had.
        const auto sourcePosition = static_cast<float> (frameIndex)
                                  / static_cast<float> (kNumFrames - 1)
                                  * static_cast<float> (numFrames - 1);
        const auto sourceFrame = juce::jlimit (0, numFrames - 1,
                                               juce::roundToInt (sourcePosition));

        std::fill (scratch.begin(), scratch.end(), 0.0f);

        const auto* source = interleavedFrames
                           + static_cast<std::size_t> (sourceFrame) * static_cast<std::size_t> (frameSize);

        for (int i = 0; i < analysisSize; ++i)
            scratch[static_cast<std::size_t> (i)] = source[i];

        fft.performRealOnlyForwardTransform (scratch.data(), true);

        const auto usable = juce::jmin (kBaseNumHarmonics, analysisSize / 2 - 1);
        const auto normalisation = 2.0f / static_cast<float> (analysisSize);

        for (int h = 1; h <= usable; ++h)
        {
            const auto real = scratch[static_cast<std::size_t> (h) * 2];
            const auto imaginary = scratch[static_cast<std::size_t> (h) * 2 + 1];

            spectrum.setHarmonic (h,
                                  std::sqrt (real * real + imaginary * imaginary) * normalisation,
                                  std::atan2 (imaginary, real));
        }
    }

    for (int level = 0; level < kNumMipLevels; ++level)
        buildMipLevel (level, spectra);

    normaliseAndFinish();
}

} // namespace gnarl::dsp
