#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <utility>

namespace gnarl::dsp
{

/**
    The spectrum the FX tab draws.

    THE SPLIT IS THE WHOLE DESIGN. The audio thread does one thing - copy
    samples into a ring buffer - because everything else an analyser does is
    forbidden there (§3): an FFT allocates its tables, a window costs a
    multiply per sample, and a display that runs at the audio thread's
    convenience would be a display whose frame rate is the block size.

    So the audio thread writes, the MESSAGE thread reads a whole frame out,
    windows it, transforms it and converts to dB. The ring buffer is a
    plain array with an atomic write position: the reader may see a frame
    that is being overwritten as it reads, and that is fine - the worst case
    is one frame with a seam in it, sixty times a second, on a picture. A
    lock to prevent that would put the audio thread behind the UI.

    NOT `juce::dsp::FFT` ON THE AUDIO THREAD. Not anywhere near it.
*/
class SpectrumAnalyser
{
public:
    /** 2048 points at 48 kHz is 23 Hz per bin and 43 ms of signal - fine
        enough to separate a bass note's harmonics, short enough that the
        display still tracks a wobble. */
    static constexpr int kFftOrder = 11;
    static constexpr int kFftSize = 1 << kFftOrder;

    /** What the UI draws. Fewer than kFftSize/2 because a display 700 px
        wide cannot show 1024 bins and the eye cannot read them: the bins are
        grouped logarithmically, which is also how pitch works. */
    static constexpr int kNumBins = 128;

    /** Floor of the drawn scale. Anything quieter is silence as far as a
        picture is concerned. */
    static constexpr float kFloorDb = -90.0f;

    SpectrumAnalyser()
        : fft (kFftOrder),
          window (kFftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        buildBinEdges();
    }

    /** AUDIO THREAD. Copies, and nothing else.

        Deliberately takes the mono sum rather than a channel: a spectrum of
        the left channel alone would show a hole wherever the patch panned
        something right, which reads as a bug in the synth rather than as a
        property of the picture. */
    void pushBlock (const float* left, const float* right, int numSamples) noexcept
    {
        if (left == nullptr || numSamples <= 0)
            return;

        auto position = writePosition.load (std::memory_order_relaxed);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto mono = right != nullptr ? 0.5f * (left[i] + right[i])
                                               : left[i];

            ring[static_cast<std::size_t> (position)] = mono;
            position = (position + 1) % kRingSize;
        }

        // Released last, so a reader that sees this position sees the
        // samples written before it.
        writePosition.store (position, std::memory_order_release);
    }

    /** The FFT bins a drawn bin covers, as [first, last).

        Exposed because the mapping is not a formula anybody should
        reimplement - see `buildBinEdges` for why it is not purely
        logarithmic - and because a test that re-derived it would be two
        copies of the same mistake agreeing with each other. */
    std::pair<int, int> getBinRange (int bin) const noexcept
    {
        const auto index = static_cast<std::size_t> (juce::jlimit (0, kNumBins - 1, bin));
        return { binEdges[index], binEdges[index + 1] };
    }

    /** MESSAGE THREAD. Fills `bins` with dB values, kFloorDb at the bottom.

        Returns false when nothing has been pushed yet, so the UI can draw a
        flat line rather than a frame of garbage. */
    bool read (std::array<float, kNumBins>& bins) noexcept
    {
        const auto end = writePosition.load (std::memory_order_acquire);

        if (! hasData)
        {
            hasData = end != 0;

            if (! hasData)
                return false;
        }

        // The most recent kFftSize samples, unwrapped.
        for (int i = 0; i < kFftSize; ++i)
        {
            const auto index = (end - kFftSize + i + kRingSize) % kRingSize;
            scratch[static_cast<std::size_t> (i)] = ring[static_cast<std::size_t> (index)];
        }

        /*  A HANN WINDOW, and §8's Blackman-Harris rule does not apply here.
            That rule is for spectral ASSERTIONS, where a sidelobe at -31 dB
            is indistinguishable from aliasing and produces a confident
            fictional number. This is a picture: Hann's narrower mainlobe
            separates adjacent harmonics better, and its sidelobes are far
            below anything visible once the bins are grouped and smoothed. */
        window.multiplyWithWindowingTable (scratch.data(), kFftSize);

        std::fill (transform.begin(), transform.end(), 0.0f);
        std::copy (scratch.begin(), scratch.end(), transform.begin());

        fft.performFrequencyOnlyForwardTransform (transform.data());

        for (int bin = 0; bin < kNumBins; ++bin)
        {
            const auto first = binEdges[static_cast<std::size_t> (bin)];
            const auto last = binEdges[static_cast<std::size_t> (bin) + 1];

            /*  The PEAK across the group, not the mean. Averaging a
                logarithmic group that spans hundreds of FFT bins at the top
                end buries every harmonic in the noise between them - the
                display would show a bass patch as almost flat. The peak is
                what the eye is looking for anyway. */
            auto magnitude = 0.0f;

            for (auto i = first; i < last; ++i)
                magnitude = juce::jmax (magnitude, transform[static_cast<std::size_t> (i)]);

            // Undo the window's coherent gain and the transform size, so the
            // reading is dBFS rather than an arbitrary scale.
            const auto normalised = magnitude * (2.0f / (kFftSize * 0.5f));

            bins[static_cast<std::size_t> (bin)] =
                juce::jmax (kFloorDb, juce::Decibels::gainToDecibels (normalised, kFloorDb));
        }

        return true;
    }

private:
    /** Four frames' worth, so a reader is unlikely to catch the writer. */
    static constexpr int kRingSize = kFftSize * 4;

    void buildBinEdges()
    {
        /*  Logarithmic WHERE THE DATA ALLOWS IT, and that qualifier is the
            whole story of this function.

            Pitch is logarithmic, so a linear grouping would give the bottom
            octave - where a riddim patch lives - two bins out of 128, and
            spend half the display on 10-20 kHz where there is nothing to
            see. So the edges follow a log curve.

            BUT A LOG CURVE ASKS FOR RESOLUTION THE FFT DOES NOT HAVE at the
            bottom. At 2048 points and 48 kHz each FFT bin is 23 Hz wide, so
            the first 128th of the curve wants fractions of a bin - a dozen
            drawn bins all land on FFT bin 1. The monotonic pass below then
            forces them apart, one FFT bin each, which is as fine as the data
            gets.

            The consequence, stated rather than hidden: the bottom of the
            display is ONE FFT BIN PER DRAWN BIN (linear, 23 Hz steps) and
            only becomes logarithmic above the crossover, around 2 kHz. That
            is not a compromise to fix by choosing different numbers - it is
            what a 2048-point transform can tell you - and `getBinRange`
            exists so nothing has to guess where the crossover fell.

            From FFT bin 1: bin 0 is DC, which the DC blocker has already
            removed and which would otherwise pin the left edge. */
        constexpr auto firstBin = 1.0;
        constexpr auto lastBin = static_cast<double> (kFftSize / 2);

        for (int i = 0; i <= kNumBins; ++i)
        {
            const auto t = static_cast<double> (i) / kNumBins;
            const auto value = firstBin * std::pow (lastBin / firstBin, t);

            binEdges[static_cast<std::size_t> (i)] =
                juce::jlimit (1, kFftSize / 2, static_cast<int> (value));
        }

        /*  Every group needs at least one FFT bin, or it reads as a
            permanent notch at whatever frequency the rounding collapsed - a
            hole in the display no signal could fill. This is also what makes
            the bottom of the scale linear; see above. */
        for (int i = 1; i <= kNumBins; ++i)
            if (binEdges[static_cast<std::size_t> (i)]
                  <= binEdges[static_cast<std::size_t> (i) - 1])
                binEdges[static_cast<std::size_t> (i)] =
                    binEdges[static_cast<std::size_t> (i) - 1] + 1;
    }

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    std::array<float, kRingSize> ring {};
    std::atomic<int> writePosition { 0 };
    bool hasData = false;

    std::array<float, kFftSize> scratch {};
    /** Twice the size: performFrequencyOnlyForwardTransform needs the room. */
    std::array<float, kFftSize * 2> transform {};

    std::array<int, kNumBins + 1> binEdges {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyser)
};

} // namespace gnarl::dsp
