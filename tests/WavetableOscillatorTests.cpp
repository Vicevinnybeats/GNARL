#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/WavetableOscillator.h"
#include "dsp/WavetableLibrary.h"

#include <cmath>
#include <numeric>
#include <vector>

using namespace gnarl::dsp;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kFftOrder = 14;                 // 16384 samples
    constexpr int kFftSize = 1 << kFftOrder;

    /** A table holding a single harmonic, so any other energy in the output is
        something the oscillator added rather than something the table held.
        That is what makes the aliasing figures below attributable. */
    void sineGenerator (float, Wavetable::Spectrum& spectrum)
    {
        spectrum.setHarmonic (1, 1.0f);
    }

    void sawGenerator (float, Wavetable::Spectrum& spectrum)
    {
        for (int h = 1; h <= Wavetable::kBaseNumHarmonics; ++h)
            spectrum.setHarmonic (h, 1.0f / static_cast<float> (h));
    }

    struct Rendered
    {
        std::vector<float> left;
        std::vector<float> right;
    };

    Rendered renderOscillator (WavetableOscillator& oscillator,
                               int numSamples,
                               float frequencyHz,
                               const WavetableOscillator::Settings& settings,
                               const float* modulator = nullptr)
    {
        Rendered out { std::vector<float> (static_cast<std::size_t> (numSamples), 0.0f),
                       std::vector<float> (static_cast<std::size_t> (numSamples), 0.0f) };

        // Rendered in blocks, the way a host calls it, so any per-block state
        // error shows up rather than being hidden by one giant call.
        constexpr int blockSize = 256;

        for (int offset = 0; offset < numSamples; offset += blockSize)
        {
            const auto chunk = juce::jmin (blockSize, numSamples - offset);

            oscillator.render (out.left.data() + offset,
                               out.right.data() + offset,
                               chunk,
                               frequencyHz,
                               frequencyHz,
                               settings,
                               modulator != nullptr ? modulator + offset : nullptr);
        }

        return out;
    }

    /** Mainlobe half-width of the analysis window, in bins. */
    constexpr int kWindowMainlobeBins = 4;

    /**
        Blackman-Harris 4-term window.

        NOT Hann. A Hann window's first sidelobe is only -31 dB down, so with
        several hundred harmonics present its leakage fills the gaps between
        them at roughly -48 dBc. That is indistinguishable from aliasing in the
        spectrum, and it does not move when the oscillator is improved - which
        is exactly how an earlier version of this test produced a confident,
        constant, entirely fictional -48 dB "aliasing" figure.

        Blackman-Harris sidelobes are about -92 dB, comfortably below the
        -60 dB the tests assert.
    */
    float blackmanHarris (int index, int count)
    {
        constexpr float a0 = 0.35875f;
        constexpr float a1 = 0.48829f;
        constexpr float a2 = 0.14128f;
        constexpr float a3 = 0.01168f;

        const auto t = juce::MathConstants<float>::twoPi
                     * static_cast<float> (index) / static_cast<float> (count);

        return a0 - a1 * std::cos (t) + a2 * std::cos (2.0f * t) - a3 * std::cos (3.0f * t);
    }

    /** Magnitude spectrum of a Blackman-Harris windowed signal. */
    std::vector<float> magnitudeSpectrum (const std::vector<float>& signal)
    {
        juce::dsp::FFT fft (kFftOrder);
        std::vector<float> scratch (static_cast<std::size_t> (kFftSize) * 2, 0.0f);

        const auto count = juce::jmin (static_cast<int> (signal.size()), kFftSize);

        for (int i = 0; i < count; ++i)
            scratch[static_cast<std::size_t> (i)] =
                signal[static_cast<std::size_t> (i)] * blackmanHarris (i, count);

        fft.performRealOnlyForwardTransform (scratch.data(), true);

        std::vector<float> magnitudes (static_cast<std::size_t> (kFftSize / 2), 0.0f);

        for (int bin = 0; bin < kFftSize / 2; ++bin)
        {
            const auto real = scratch[static_cast<std::size_t> (bin) * 2];
            const auto imaginary = scratch[static_cast<std::size_t> (bin) * 2 + 1];
            magnitudes[static_cast<std::size_t> (bin)] =
                std::sqrt (real * real + imaginary * imaginary);
        }

        return magnitudes;
    }

    /**
        Worst inharmonic ("aliased") partial, in dB relative to the fundamental.

        Bins within `skirtBins` of any true harmonic are excluded, because the
        Hann window spreads each real harmonic over a few bins and counting
        that skirt as aliasing would fail every test regardless of quality.
    */
    float worstAliasDb (const std::vector<float>& signal, float fundamentalHz)
    {
        const auto magnitudes = magnitudeSpectrum (signal);

        const auto binHz = static_cast<float> (kSampleRate) / static_cast<float> (kFftSize);
        const auto fundamentalBin = fundamentalHz / binHz;

        // Wide enough to clear the window's mainlobe on both sides, so a real
        // harmonic's own energy is never counted as an alias.
        constexpr int skirtBins = kWindowMainlobeBins * 2;

        auto reference = 0.0f;

        for (int bin = juce::jmax (1, static_cast<int> (fundamentalBin) - skirtBins);
             bin <= static_cast<int> (fundamentalBin) + skirtBins; ++bin)
        {
            if (bin < static_cast<int> (magnitudes.size()))
                reference = juce::jmax (reference, magnitudes[static_cast<std::size_t> (bin)]);
        }

        if (reference <= 0.0f)
            return -200.0f;

        auto worst = 0.0f;

        for (int bin = 2; bin < static_cast<int> (magnitudes.size()); ++bin)
        {
            const auto frequency = static_cast<float> (bin) * binHz;

            // Distance to the nearest true harmonic, in bins.
            const auto harmonicNumber = std::round (frequency / fundamentalHz);
            const auto nearestHarmonicBin = harmonicNumber * fundamentalBin;

            if (std::abs (static_cast<float> (bin) - nearestHarmonicBin)
                <= static_cast<float> (skirtBins))
            {
                continue;
            }

            worst = juce::jmax (worst, magnitudes[static_cast<std::size_t> (bin)]);
        }

        return juce::Decibels::gainToDecibels (worst / reference, -200.0f);
    }

    float peakOf (const std::vector<float>& signal)
    {
        auto peak = 0.0f;

        for (const auto sample : signal)
            peak = juce::jmax (peak, std::abs (sample));

        return peak;
    }

    bool allFinite (const Rendered& rendered)
    {
        for (const auto* channel : { &rendered.left, &rendered.right })
            for (const auto sample : *channel)
                if (! std::isfinite (sample))
                    return false;

        return true;
    }

    WavetableOscillator::Settings defaultSettings()
    {
        WavetableOscillator::Settings settings;
        settings.level = 1.0f;
        settings.unisonVoices = 1;
        settings.unisonDetune = 0.0f;
        return settings;
    }
}

TEST_CASE ("An oscillator with no table renders silence", "[osc]")
{
    // A table can legitimately still be generating on a background thread, so
    // this must be silence rather than a crash.
    WavetableOscillator oscillator;
    oscillator.prepare (kSampleRate);

    const auto rendered = renderOscillator (oscillator, 512, 440.0f, defaultSettings());

    CHECK (peakOf (rendered.left) == 0.0f);
    CHECK (peakOf (rendered.right) == 0.0f);
}

TEST_CASE ("A sine table produces a sine at the requested pitch", "[osc]")
{
    Wavetable table;
    table.generate (sineGenerator);

    WavetableOscillator oscillator;
    oscillator.prepare (kSampleRate);
    oscillator.setTable (&table);
    oscillator.noteOn (nullptr, 0, 0.0f, 0.0f);

    constexpr float frequency = 440.0f;
    const auto rendered = renderOscillator (oscillator, kFftSize, frequency, defaultSettings());

    REQUIRE (allFinite (rendered));
    CHECK (peakOf (rendered.left) > 0.1f);

    const auto magnitudes = magnitudeSpectrum (rendered.left);
    const auto binHz = static_cast<float> (kSampleRate) / static_cast<float> (kFftSize);

    // The loudest bin must be the fundamental.
    auto loudestBin = 0;

    for (int bin = 1; bin < static_cast<int> (magnitudes.size()); ++bin)
        if (magnitudes[static_cast<std::size_t> (bin)] > magnitudes[static_cast<std::size_t> (loudestBin)])
            loudestBin = bin;

    CHECK (static_cast<float> (loudestBin) * binHz
           == Catch::Approx (frequency).margin (binHz * 2.0f));
}

TEST_CASE ("Oscillator output is band-limited across the keyboard",
           "[osc][aliasing]")
{
    // The measurement that matters, and the one that drove the wavetable
    // geometry. A saw at every harmonic is the hardest case for the mip map.
    //
    // Measured worst case with the shipped geometry: -65.6 dBc, at MIDI 45-54.
    // Note that the worst case is at LOW notes, not high ones: at high pitch
    // the mip level is coarse and its frame size is floored generously, so the
    // interpolator has plenty of samples per cycle to work with.
    //
    // If this starts failing, suspect the table geometry constants in
    // Wavetable.h before suspecting the oscillator.
    Wavetable table;
    table.generate (sawGenerator);

    // Dense sweep: the worst case sits wherever a mip boundary lands, so
    // sampling only every octave can walk straight past it.
    // Stepped finely enough to land on mip boundaries: sampling every octave
    // walks straight past the worst case.
    for (int midiNote = 12; midiNote <= 120; midiNote += 3)
    {
        const auto frequency = 440.0f
            * std::pow (2.0f, (static_cast<float> (midiNote) - 69.0f) / 12.0f);

        WavetableOscillator oscillator;
        oscillator.prepare (kSampleRate);
        oscillator.setTable (&table);
        oscillator.noteOn (nullptr, 0, 0.0f, 0.0f);

        const auto rendered = renderOscillator (oscillator, kFftSize, frequency,
                                                defaultSettings());
        REQUIRE (allFinite (rendered));

        const auto alias = worstAliasDb (rendered.left, frequency);

        INFO ("note " << midiNote << " (" << frequency << " Hz), worst alias "
              << alias << " dBc");

        CHECK (alias < -60.0f);
    }
}

TEST_CASE ("Every warp mode stays finite and bounded", "[osc][warp]")
{
    Wavetable table;
    table.generate (sawGenerator);

    std::vector<float> modulator (4096, 0.0f);

    for (std::size_t i = 0; i < modulator.size(); ++i)
        modulator[i] = std::sin (juce::MathConstants<float>::twoPi
                                 * static_cast<float> (i) * 0.01f);

    for (int modeIndex = 0; modeIndex < static_cast<int> (warp::Mode::count); ++modeIndex)
    {
        for (const auto amount : { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f })
        {
            auto settings = defaultSettings();
            settings.warpMode = static_cast<warp::Mode> (modeIndex);
            settings.warpAmount = amount;

            WavetableOscillator oscillator;
            oscillator.prepare (kSampleRate);
            oscillator.setTable (&table);
            oscillator.noteOn (nullptr, 0, 0.0f, 0.0f);

            const auto rendered = renderOscillator (oscillator, 4096, 220.0f, settings,
                                                    modulator.data());

            INFO ("warp mode " << modeIndex << " amount " << amount);
            REQUIRE (allFinite (rendered));

            // A warp must not blow the level up: the table is normalised to
            // 1.0, and a warp only reshapes it.
            CHECK (peakOf (rendered.left) <= 2.0f);
        }
    }
}

TEST_CASE ("Warp off is bit-identical to no warp at all", "[osc][warp]")
{
    Wavetable table;
    table.generate (sawGenerator);

    const auto renderWith = [&table] (warp::Mode mode, float amount)
    {
        auto settings = defaultSettings();
        settings.warpMode = mode;
        settings.warpAmount = amount;

        WavetableOscillator oscillator;
        oscillator.prepare (kSampleRate);
        oscillator.setTable (&table);
        oscillator.noteOn (nullptr, 0, 0.0f, 0.0f);

        return renderOscillator (oscillator, 2048, 330.0f, settings);
    };

    const auto plain = renderWith (warp::Mode::off, 0.0f);

    // Amount 0 on any mode must cost nothing and change nothing.
    for (int modeIndex = 0; modeIndex < static_cast<int> (warp::Mode::count); ++modeIndex)
    {
        const auto zeroAmount = renderWith (static_cast<warp::Mode> (modeIndex), 0.0f);

        INFO ("mode " << modeIndex);

        for (std::size_t i = 0; i < plain.left.size(); i += 17)
            REQUIRE (zeroAmount.left[i] == plain.left[i]);
    }
}

TEST_CASE ("Unison widens the stereo image without changing loudness much",
           "[osc][unison]")
{
    Wavetable table;
    table.generate (sawGenerator);

    const auto measure = [&table] (int voices)
    {
        auto settings = defaultSettings();
        settings.unisonVoices = voices;
        settings.unisonDetune = 0.5f;
        settings.unisonBlend = 1.0f;
        settings.unisonSpread = 1.0f;

        WavetableOscillator oscillator;
        oscillator.prepare (kSampleRate);
        oscillator.setTable (&table);

        std::vector<float> randoms (16);

        for (std::size_t i = 0; i < randoms.size(); ++i)
            randoms[i] = std::sin (static_cast<float> (i) * 1.7f);

        oscillator.noteOn (randoms.data(), static_cast<int> (randoms.size()), 0.0f, 0.5f);

        return renderOscillator (oscillator, 8192, 110.0f, settings);
    };

    const auto single = measure (1);
    const auto wide = measure (16);

    REQUIRE (allFinite (single));
    REQUIRE (allFinite (wide));

    const auto rms = [] (const std::vector<float>& signal)
    {
        auto sum = 0.0;

        for (const auto sample : signal)
            sum += static_cast<double> (sample) * sample;

        return std::sqrt (sum / static_cast<double> (signal.size()));
    };

    // Turning unison up must change the width, not the loudness: a producer
    // reaching for unison is not asking for 12 dB.
    const auto singleRms = rms (single.left);
    const auto wideRms = rms (wide.left);

    INFO ("1 voice RMS " << singleRms << ", 16 voice RMS " << wideRms);
    CHECK (wideRms < singleRms * 3.0);
    CHECK (wideRms > singleRms * 0.2);

    // And the channels must genuinely differ, or "spread" did nothing.
    auto channelDifference = 0.0;

    for (std::size_t i = 0; i < wide.left.size(); ++i)
        channelDifference += std::abs (wide.left[i] - wide.right[i]);

    CHECK (channelDifference > 1.0);
}

TEST_CASE ("A single unison voice is centred", "[osc][unison]")
{
    Wavetable table;
    table.generate (sawGenerator);

    WavetableOscillator oscillator;
    oscillator.prepare (kSampleRate);
    oscillator.setTable (&table);
    oscillator.noteOn (nullptr, 0, 0.0f, 0.0f);

    const auto rendered = renderOscillator (oscillator, 1024, 220.0f, defaultSettings());

    for (std::size_t i = 0; i < rendered.left.size(); i += 13)
        REQUIRE (rendered.left[i] == Catch::Approx (rendered.right[i]).margin (1.0e-6));
}

TEST_CASE ("Pan moves energy between channels", "[osc]")
{
    Wavetable table;
    table.generate (sawGenerator);

    const auto renderPanned = [&table] (float pan)
    {
        auto settings = defaultSettings();
        settings.pan = pan;

        WavetableOscillator oscillator;
        oscillator.prepare (kSampleRate);
        oscillator.setTable (&table);
        oscillator.noteOn (nullptr, 0, 0.0f, 0.0f);

        return renderOscillator (oscillator, 2048, 220.0f, settings);
    };

    const auto hardLeft = renderPanned (-1.0f);
    const auto hardRight = renderPanned (1.0f);

    CHECK (peakOf (hardLeft.left) > 0.1f);
    CHECK (peakOf (hardLeft.right) == Catch::Approx (0.0f).margin (1.0e-6));
    CHECK (peakOf (hardRight.right) > 0.1f);
    CHECK (peakOf (hardRight.left) == Catch::Approx (0.0f).margin (1.0e-6));
}

TEST_CASE ("Phase random zero starts every note identically", "[osc]")
{
    // A patch's transient has to punch the same way every time. If phase
    // randomisation were always on, the attack would vary note to note.
    Wavetable table;
    table.generate (sawGenerator);

    const auto renderNote = [&table] (float phaseRandom, float seed)
    {
        WavetableOscillator oscillator;
        oscillator.prepare (kSampleRate);
        oscillator.setTable (&table);

        std::vector<float> randoms (16, seed);
        oscillator.noteOn (randoms.data(), 16, 0.0f, phaseRandom);

        return renderOscillator (oscillator, 512, 220.0f, defaultSettings());
    };

    const auto a = renderNote (0.0f, 0.3f);
    const auto b = renderNote (0.0f, -0.8f);

    for (std::size_t i = 0; i < a.left.size(); i += 7)
        REQUIRE (a.left[i] == b.left[i]);

    // With randomisation on, the two must differ.
    const auto c = renderNote (1.0f, 0.3f);
    const auto d = renderNote (1.0f, -0.8f);

    auto difference = 0.0f;

    for (std::size_t i = 0; i < c.left.size(); ++i)
        difference += std::abs (c.left[i] - d.left[i]);

    CHECK (difference > 0.1f);
}

TEST_CASE ("Graintable mode produces sound and stays finite", "[osc][grain]")
{
    Wavetable table;
    table.generate (sawGenerator);

    for (const auto density : { 1.0f, 20.0f, 200.0f })
    {
        for (const auto sizeMs : { 1.0f, 40.0f, 500.0f })
        {
            auto settings = defaultSettings();
            settings.mode = gnarl::choices::OscMode::graintable;
            settings.grainDensityHz = density;
            settings.grainSizeMs = sizeMs;
            settings.grainPosJitter = 0.5f;
            settings.grainPitchJitter = 0.5f;

            WavetableOscillator oscillator;
            oscillator.prepare (kSampleRate);
            oscillator.setTable (&table);

            std::vector<float> randoms (16, 0.4f);
            oscillator.noteOn (randoms.data(), 16, 0.0f, 0.0f);

            const auto rendered = renderOscillator (oscillator, 16384, 110.0f, settings);

            INFO ("density " << density << " Hz, grain " << sizeMs << " ms");
            REQUIRE (allFinite (rendered));
            CHECK (peakOf (rendered.left) > 0.0f);
            CHECK (peakOf (rendered.left) < 4.0f);
        }
    }
}

TEST_CASE ("Graintable is deterministic for the same note", "[osc][grain]")
{
    Wavetable table;
    table.generate (sawGenerator);

    const auto renderGrains = [&table]
    {
        auto settings = defaultSettings();
        settings.mode = gnarl::choices::OscMode::graintable;
        settings.grainPosJitter = 1.0f;
        settings.grainPitchJitter = 1.0f;

        WavetableOscillator oscillator;
        oscillator.prepare (kSampleRate);
        oscillator.setTable (&table);

        std::vector<float> randoms (16, 0.25f);
        oscillator.noteOn (randoms.data(), 16, 0.0f, 0.0f);

        return renderOscillator (oscillator, 4096, 110.0f, settings);
    };

    const auto first = renderGrains();
    const auto second = renderGrains();

    // Jitter must come from a seeded stream, not from wall-clock state: a
    // bounce that differs from the preview is the worst kind of bug to chase.
    for (std::size_t i = 0; i < first.left.size(); i += 11)
        REQUIRE (first.left[i] == second.left[i]);
}

TEST_CASE ("Degenerate settings do not crash or produce garbage", "[osc]")
{
    Wavetable table;
    table.generate (sawGenerator);

    WavetableOscillator oscillator;
    oscillator.prepare (kSampleRate);
    oscillator.setTable (&table);
    oscillator.noteOn (nullptr, 0, 0.0f, 0.0f);

    auto settings = defaultSettings();
    settings.unisonVoices = 0;          // below the legal minimum
    settings.grainDensityHz = 0.0f;     // would divide by zero if unclamped
    settings.grainSizeMs = 0.0f;

    std::vector<float> left (64, 0.0f);
    std::vector<float> right (64, 0.0f);

    oscillator.render (left.data(), right.data(), 64, 440.0f, 440.0f, settings);
    oscillator.render (left.data(), right.data(), 0, 440.0f, 440.0f, settings);
    oscillator.render (nullptr, right.data(), 64, 440.0f, 440.0f, settings);

    settings.mode = gnarl::choices::OscMode::graintable;
    oscillator.render (left.data(), right.data(), 64, 440.0f, 440.0f, settings);

    for (const auto sample : left)
        REQUIRE (std::isfinite (sample));

    SUCCEED ("degenerate settings handled");
}

TEST_CASE ("A zero or negative frequency does not misbehave", "[osc]")
{
    Wavetable table;
    table.generate (sawGenerator);

    WavetableOscillator oscillator;
    oscillator.prepare (kSampleRate);
    oscillator.setTable (&table);
    oscillator.noteOn (nullptr, 0, 0.0f, 0.0f);

    // A pitch-modulated oscillator can be driven to or past zero.
    const auto stopped = renderOscillator (oscillator, 512, 0.0f, defaultSettings());
    REQUIRE (allFinite (stopped));

    const auto reversed = renderOscillator (oscillator, 512, -220.0f, defaultSettings());
    REQUIRE (allFinite (reversed));
}

TEST_CASE ("Frequency is interpolated across the block", "[osc]")
{
    // Block-rate pitch would step a fast glide into zipper noise. Rendering a
    // glide in one call and comparing against a held pitch proves the ramp is
    // actually applied.
    Wavetable table;
    table.generate (sineGenerator);

    WavetableOscillator oscillator;
    oscillator.prepare (kSampleRate);
    oscillator.setTable (&table);
    oscillator.noteOn (nullptr, 0, 0.0f, 0.0f);

    std::vector<float> glidingLeft (2048, 0.0f);
    std::vector<float> glidingRight (2048, 0.0f);

    oscillator.render (glidingLeft.data(), glidingRight.data(), 2048,
                       110.0f, 880.0f, defaultSettings());

    WavetableOscillator held;
    held.prepare (kSampleRate);
    held.setTable (&table);
    held.noteOn (nullptr, 0, 0.0f, 0.0f);

    std::vector<float> heldLeft (2048, 0.0f);
    std::vector<float> heldRight (2048, 0.0f);

    held.render (heldLeft.data(), heldRight.data(), 2048,
                 110.0f, 110.0f, defaultSettings());

    auto difference = 0.0f;

    for (std::size_t i = 0; i < glidingLeft.size(); ++i)
        difference += std::abs (glidingLeft[i] - heldLeft[i]);

    CHECK (difference > 10.0f);

    // Both start at the same pitch, so the first few samples should agree.
    for (std::size_t i = 0; i < 8; ++i)
        CHECK (glidingLeft[i] == Catch::Approx (heldLeft[i]).margin (0.01));
}

TEST_CASE ("Render accumulates into the buffer rather than overwriting it",
           "[osc]")
{
    // Two oscillators and a sub have to be able to sum into the same buffer.
    Wavetable table;
    table.generate (sineGenerator);

    WavetableOscillator oscillator;
    oscillator.prepare (kSampleRate);
    oscillator.setTable (&table);
    oscillator.noteOn (nullptr, 0, 0.0f, 0.0f);

    std::vector<float> left (512, 1.0f);
    std::vector<float> right (512, 1.0f);

    oscillator.render (left.data(), right.data(), 512, 220.0f, 220.0f, defaultSettings());

    // The pre-existing 1.0 must still be in there.
    auto mean = 0.0;

    for (const auto sample : left)
        mean += sample;

    mean /= static_cast<double> (left.size());

    CHECK (mean == Catch::Approx (1.0).margin (0.2));
}

TEST_CASE ("Reset clears phase and grain state", "[osc]")
{
    Wavetable table;
    table.generate (sawGenerator);

    WavetableOscillator oscillator;
    oscillator.prepare (kSampleRate);
    oscillator.setTable (&table);
    oscillator.noteOn (nullptr, 0, 0.3f, 0.0f);

    renderOscillator (oscillator, 1024, 220.0f, defaultSettings());
    oscillator.reset();

    // After a reset the oscillator must behave as if freshly prepared, or a
    // recycled voice would inherit the previous note's phase.
    WavetableOscillator fresh;
    fresh.prepare (kSampleRate);
    fresh.setTable (&table);

    const auto afterReset = renderOscillator (oscillator, 512, 220.0f, defaultSettings());
    const auto freshRender = renderOscillator (fresh, 512, 220.0f, defaultSettings());

    for (std::size_t i = 0; i < afterReset.left.size(); i += 9)
        REQUIRE (afterReset.left[i] == freshRender.left[i]);
}
