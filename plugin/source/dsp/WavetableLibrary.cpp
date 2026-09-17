#include "WavetableLibrary.h"

#include <cmath>

namespace gnarl::dsp
{

namespace
{
    using Spectrum = Wavetable::Spectrum;

    constexpr int kMaxHarmonics = Wavetable::kBaseNumHarmonics;

    /** Deterministic pseudo-random in 0..1 from an integer pair.

        A hash rather than a Random object: the generators must produce the
        identical table on every load and on every machine, and a table that
        differs between runs would make a bounce differ from the preview. */
    float hashToUnit (int a, int b) noexcept
    {
        auto h = static_cast<std::uint32_t> (a) * 0x9E3779B1u
               ^ static_cast<std::uint32_t> (b) * 0x85EBCA77u;
        h ^= h >> 15;
        h *= 0x2545F491u;
        h ^= h >> 13;

        return static_cast<float> (h & 0xFFFFFFu) / static_cast<float> (0xFFFFFF);
    }

    float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

    // --- Building blocks ---------------------------------------------------

    /** Saw: every harmonic at 1/n. */
    void addSaw (Spectrum& s, float amount, int maxHarmonic = kMaxHarmonics)
    {
        for (int h = 1; h <= maxHarmonic; ++h)
            s.magnitude[static_cast<std::size_t> (h - 1)] +=
                amount / static_cast<float> (h);
    }

    /** Square: odd harmonics at 1/n. */
    void addSquare (Spectrum& s, float amount, int maxHarmonic = kMaxHarmonics)
    {
        for (int h = 1; h <= maxHarmonic; h += 2)
            s.magnitude[static_cast<std::size_t> (h - 1)] +=
                amount / static_cast<float> (h);
    }

    /** Triangle: odd harmonics at 1/n^2, alternating phase. */
    void addTriangle (Spectrum& s, float amount, int maxHarmonic = kMaxHarmonics)
    {
        for (int h = 1; h <= maxHarmonic; h += 2)
        {
            const auto index = static_cast<std::size_t> (h - 1);
            const auto n = static_cast<float> (h);
            s.magnitude[index] += amount / (n * n);
            s.phase[index] += ((h / 2) % 2 == 0) ? 0.0f : juce::MathConstants<float>::pi;
        }
    }

    /** A resonant peak in the harmonic series - the basis of every vowel-like
        table here. `centreHarmonic` need not be an integer. */
    void addFormant (Spectrum& s,
                     float centreHarmonic,
                     float bandwidthHarmonics,
                     float amount)
    {
        if (bandwidthHarmonics <= 0.0f)
            return;

        for (int h = 1; h <= kMaxHarmonics; ++h)
        {
            const auto distance = (static_cast<float> (h) - centreHarmonic) / bandwidthHarmonics;
            const auto gain = std::exp (-distance * distance);

            if (gain > 1.0e-4f)
                s.magnitude[static_cast<std::size_t> (h - 1)] += amount * gain;
        }
    }

    /** Harmonics of a classic two-operator FM tone, via Bessel-like rolloff.

        Uses the series magnitude approximation rather than real Bessel
        functions: the audible result is the characteristic FM brightness
        sweep, and it is cheap and monotonic in the index. */
    void addFmSeries (Spectrum& s, float ratio, float index, float amount)
    {
        const auto ratioHarmonic = juce::jmax (1.0f, ratio);

        for (int h = 1; h <= kMaxHarmonics; ++h)
        {
            const auto sidebandOrder = std::abs (static_cast<float> (h) - ratioHarmonic)
                                     / ratioHarmonic;
            const auto rolloff = std::exp (-sidebandOrder * sidebandOrder / juce::jmax (0.01f, index));

            if (rolloff > 1.0e-4f)
                s.magnitude[static_cast<std::size_t> (h - 1)] +=
                    amount * rolloff / std::sqrt (static_cast<float> (h));
        }
    }

    /** Only harmonics that are multiples of `n`, which thins a dense spectrum
        into something hollow and metallic. */
    void addHarmonicComb (Spectrum& s, int n, float amount, float rolloff = 1.0f)
    {
        if (n < 1)
            return;

        for (int h = n; h <= kMaxHarmonics; h += n)
            s.magnitude[static_cast<std::size_t> (h - 1)] +=
                amount / std::pow (static_cast<float> (h / n), rolloff);
    }

    /** Randomises phases. Changes the waveform's shape without touching its
        spectrum, which is how two tables can measure identically and still
        sound different under a filter. */
    void randomisePhases (Spectrum& s, int seed, float amount)
    {
        for (int h = 1; h <= kMaxHarmonics; ++h)
        {
            const auto index = static_cast<std::size_t> (h - 1);

            // Leaves untouched harmonics untouched: rotating the phase of a
            // harmonic with no amplitude would do nothing but cost time.
            if (juce::exactlyEqual (s.magnitude[index], 0.0f))
                continue;

            s.phase[index] += amount * juce::MathConstants<float>::twoPi
                            * hashToUnit (seed, h);
        }
    }

    /** Hard limit on harmonic count, for tables that should stay dull at one
        end of their morph. */
    void lowPassHarmonics (Spectrum& s, float maxHarmonic, float softness)
    {
        for (int h = 1; h <= kMaxHarmonics; ++h)
        {
            const auto index = static_cast<std::size_t> (h - 1);

            if (juce::exactlyEqual (s.magnitude[index], 0.0f))
                continue;

            const auto over = (static_cast<float> (h) - maxHarmonic)
                            / juce::jmax (1.0f, softness);

            if (over > 0.0f)
                s.magnitude[index] *= std::exp (-over * over);
        }
    }

    // --- The 20 tables -----------------------------------------------------
    // Each morphs across framePosition 0..1. The morph is the product: a table
    // whose frames all sound the same is a waveform, not a wavetable.

    void basicShapes (float p, Spectrum& s)
    {
        // Sine -> triangle -> square -> saw. The reference table; every synth
        // needs one, and it is what a new patch starts on.
        if (p < 0.333f)
        {
            const auto t = p / 0.333f;
            s.setHarmonic (1, 1.0f - t * 0.2f);
            addTriangle (s, t * 0.8f);
        }
        else if (p < 0.666f)
        {
            const auto t = (p - 0.333f) / 0.333f;
            addTriangle (s, (1.0f - t) * 0.8f);
            addSquare (s, t * 0.6f);
        }
        else
        {
            const auto t = (p - 0.666f) / 0.334f;
            addSquare (s, (1.0f - t) * 0.6f);
            addSaw (s, t * 0.5f);
        }
    }

    void growlVowels (float p, Spectrum& s)
    {
        // A -> E -> I -> O -> U as harmonic formant peaks over a saw. This is
        // the table the whole plugin is aimed at: a table position wobble
        // through it IS a talking growl, before the formant filter is even
        // engaged.
        static constexpr float vowelF1[] = { 8.0f,  5.0f,  3.0f,  5.0f,  3.0f };
        static constexpr float vowelF2[] = { 13.0f, 22.0f, 26.0f, 9.0f,  7.0f };
        static constexpr float vowelF3[] = { 28.0f, 30.0f, 32.0f, 27.0f, 25.0f };

        const auto scaled = p * 4.0f;
        const auto lower = juce::jlimit (0, 4, static_cast<int> (scaled));
        const auto upper = juce::jlimit (0, 4, lower + 1);
        const auto t = scaled - static_cast<float> (lower);

        addSaw (s, 0.25f, 64);

        addFormant (s, lerp (vowelF1[lower], vowelF1[upper], t), 2.5f, 1.0f);
        addFormant (s, lerp (vowelF2[lower], vowelF2[upper], t), 4.0f, 0.55f);
        addFormant (s, lerp (vowelF3[lower], vowelF3[upper], t), 6.0f, 0.3f);
    }

    void metallicFm (float p, Spectrum& s)
    {
        // Rising FM index: clean at 0, clangorous at 1.
        addFmSeries (s, 1.0f, 0.2f + p * 12.0f, 0.7f);
        addFmSeries (s, 3.0f, 0.1f + p * 6.0f, 0.4f * p);
        randomisePhases (s, 11, p * 0.5f);
    }

    void hollowComb (float p, Spectrum& s)
    {
        // Every harmonic -> every 2nd -> every 3rd... The spectrum thins as
        // the table position rises, which reads as the sound getting smaller
        // and more nasal without getting quieter.
        const auto n = 1 + static_cast<int> (p * 7.0f);
        addHarmonicComb (s, n, 0.8f, 0.8f);
        addSaw (s, 0.15f * (1.0f - p), 32);
    }

    void oddScreech (float p, Spectrum& s)
    {
        // Odd harmonics with an upper emphasis that climbs. Screech leads.
        addSquare (s, 0.4f);

        const auto peak = 6.0f + p * 60.0f;
        addFormant (s, peak, 3.0f + p * 8.0f, 0.9f);
        addFormant (s, peak * 2.0f, 6.0f, 0.4f * p);
    }

    void subSine (float p, Spectrum& s)
    {
        // Fundamental plus a controlled second and third. For the sub layer,
        // where anything above the third harmonic is mud.
        s.setHarmonic (1, 1.0f);
        s.setHarmonic (2, p * 0.35f);
        s.setHarmonic (3, p * p * 0.2f);
    }

    void reeseDetune (float p, Spectrum& s)
    {
        // Two saws a fixed interval apart in the harmonic domain, the beating
        // between them widening with position. The classic bass timbre.
        addSaw (s, 0.5f, 128);

        for (int h = 1; h <= 128; ++h)
        {
            const auto index = static_cast<std::size_t> (h - 1);
            s.phase[index] += p * juce::MathConstants<float>::pi
                            * static_cast<float> (h) * 0.05f;
        }
    }

    void formantSweep (float p, Spectrum& s)
    {
        // One tall formant travelling up the series. Deliberately simpler than
        // growlVowels: a single peak is easier to hear moving.
        addSaw (s, 0.2f, 96);
        addFormant (s, 2.0f + p * 48.0f, 1.5f, 1.0f);
    }

    void bitcrushSteps (float p, Spectrum& s)
    {
        // Quantised staircase, approximated as a saw plus increasingly strong
        // high harmonics at integer multiples of the step count.
        addSaw (s, 0.4f, 16);

        const auto steps = 2 + static_cast<int> ((1.0f - p) * 14.0f);

        for (int k = 1; k * steps <= kMaxHarmonics; ++k)
            s.magnitude[static_cast<std::size_t> (k * steps - 1)] +=
                0.5f * p / static_cast<float> (k);
    }

    void pulseWidth (float p, Spectrum& s)
    {
        // Pulse width from 50% down towards a narrow spike. Implemented in the
        // harmonic domain so it stays band-limited at any width.
        const auto width = lerp (0.5f, 0.04f, p);

        for (int h = 1; h <= kMaxHarmonics; ++h)
        {
            const auto n = static_cast<float> (h);
            const auto magnitude = std::abs (std::sin (juce::MathConstants<float>::pi * n * width))
                                 / n;
            s.magnitude[static_cast<std::size_t> (h - 1)] += magnitude;
        }
    }

    void additiveStack (float p, Spectrum& s)
    {
        // A harmonic series whose tilt sweeps from steeply falling to almost
        // flat: dull to piercing with no change in which harmonics exist.
        const auto tilt = lerp (2.0f, 0.25f, p);

        for (int h = 1; h <= 256; ++h)
            s.magnitude[static_cast<std::size_t> (h - 1)] +=
                1.0f / std::pow (static_cast<float> (h), tilt);
    }

    void ringModBell (float p, Spectrum& s)
    {
        // Inharmonic-feeling clusters from two combs whose spacing is coprime,
        // so the partials never line up into a simple series.
        addHarmonicComb (s, 5, 0.6f, 0.7f);
        addHarmonicComb (s, 7, 0.5f * p, 0.7f);
        addHarmonicComb (s, 11, 0.35f * p * p, 0.7f);
        randomisePhases (s, 23, 0.8f);
    }

    void dirtySaw (float p, Spectrum& s)
    {
        // Saw with harmonic-level noise. The noise is hashed, so the "dirt" is
        // identical on every load.
        addSaw (s, 0.5f);

        for (int h = 2; h <= 512; ++h)
        {
            const auto index = static_cast<std::size_t> (h - 1);
            const auto jitter = (hashToUnit (37, h) - 0.5f) * 2.0f;
            s.magnitude[index] *= 1.0f + jitter * p * 0.9f;
        }
    }

    void talkBox (float p, Spectrum& s)
    {
        // Three formants sliding in opposite directions, which produces the
        // diphthong-like movement a single sweeping peak cannot.
        addSaw (s, 0.2f, 80);
        addFormant (s, 4.0f + p * 10.0f, 2.0f, 1.0f);
        addFormant (s, 26.0f - p * 14.0f, 3.5f, 0.7f);
        addFormant (s, 34.0f + p * 20.0f, 7.0f, 0.35f);
    }

    void wobbleBass (float p, Spectrum& s)
    {
        // Strong fundamental with a mid-range shelf that rises with position.
        // Built for table-position modulation: the fundamental never moves, so
        // the wobble does not sound like a volume change.
        s.setHarmonic (1, 1.0f);
        addSaw (s, 0.12f, 24);
        addFormant (s, 5.0f + p * 25.0f, 6.0f + p * 10.0f, 0.8f * p + 0.15f);
    }

    void glassHarmonics (float p, Spectrum& s)
    {
        // Sparse high partials over a quiet fundamental.
        s.setHarmonic (1, 0.5f * (1.0f - p * 0.5f));

        for (int k = 1; k <= 12; ++k)
        {
            const auto h = static_cast<int> (std::pow (2.0f, static_cast<float> (k) * 0.6f)) + k;

            if (h <= kMaxHarmonics)
                s.magnitude[static_cast<std::size_t> (h - 1)] +=
                    0.6f * p / static_cast<float> (k);
        }

        randomisePhases (s, 53, 1.0f);
    }

    void phaseDistortion (float p, Spectrum& s)
    {
        // Fixed 1/n spectrum, phases progressively warped. The spectrum is
        // constant across the whole table, so any change you hear is purely
        // the waveform's shape - the clearest demonstration of why phase
        // matters once a filter is in the path.
        addSaw (s, 0.5f, 256);

        for (int h = 1; h <= 256; ++h)
        {
            const auto index = static_cast<std::size_t> (h - 1);
            const auto n = static_cast<float> (h);
            s.phase[index] += p * juce::MathConstants<float>::twoPi
                            * (n * n) * 0.002f;
        }
    }

    void noiseBed (float p, Spectrum& s)
    {
        // Dense hashed harmonics, band-limited harder at low positions. Not
        // true noise - it is periodic - but it fills the same role in a patch
        // and is cheap to modulate.
        for (int h = 1; h <= kMaxHarmonics; ++h)
            s.magnitude[static_cast<std::size_t> (h - 1)] =
                hashToUnit (71, h) / std::sqrt (static_cast<float> (h));

        randomisePhases (s, 89, 1.0f);
        lowPassHarmonics (s, 8.0f + p * 400.0f, 16.0f);
    }

    void growlMorph (float p, Spectrum& s)
    {
        // The flagship table: vowel formants, FM grit and a comb, all moving
        // at once. Deliberately the most complex morph in the set, because a
        // single table-position LFO on this one should already sound finished.
        const auto vowel = std::sin (p * juce::MathConstants<float>::pi);

        addSaw (s, 0.18f, 48);
        addFormant (s, 4.0f + vowel * 14.0f, 2.5f, 1.0f);
        addFormant (s, 20.0f - vowel * 10.0f, 4.0f, 0.6f);
        addFmSeries (s, 2.0f, 0.5f + p * 5.0f, 0.3f);
        addHarmonicComb (s, 3 + static_cast<int> (p * 4.0f), 0.25f, 1.2f);
        randomisePhases (s, 101, p * 0.4f);
    }

    using Generator = void (*) (float, Spectrum&);

    constexpr Generator kGenerators[WavetableLibrary::kNumFactoryTables] = {
        basicShapes, growlVowels, metallicFm, hollowComb, oddScreech,
        subSine, reeseDetune, formantSweep, bitcrushSteps, pulseWidth,
        additiveStack, ringModBell, dirtySaw, talkBox, wobbleBass,
        glassHarmonics, phaseDistortion, noiseBed, growlMorph, basicShapes
    };

    const char* const kTableNames[WavetableLibrary::kNumFactoryTables] = {
        "Basic Shapes", "Growl Vowels", "Metallic FM", "Hollow Comb", "Odd Screech",
        "Sub Sine", "Reese Detune", "Formant Sweep", "Bitcrush Steps", "Pulse Width",
        "Additive Stack", "Ring Mod Bell", "Dirty Saw", "Talk Box", "Wobble Bass",
        "Glass Harmonics", "Phase Distortion", "Noise Bed", "Growl Morph", "Init"
    };
}

WavetableLibrary::WavetableLibrary() = default;
WavetableLibrary::~WavetableLibrary() = default;

void WavetableLibrary::fillTable (int index, Wavetable& table)
{
    const auto clamped = juce::jlimit (0, kNumFactoryTables - 1, index);

    table.setName (kTableNames[clamped]);
    table.generate ([clamped] (float position, Spectrum& spectrum)
    {
        kGenerators[clamped] (position, spectrum);
    });
}

const Wavetable& WavetableLibrary::getTable (int index)
{
    const auto clamped = static_cast<std::size_t> (
        juce::jlimit (0, kNumFactoryTables - 1, index));

    if (tables[clamped] == nullptr)
    {
        auto table = std::make_unique<Wavetable>();
        fillTable (static_cast<int> (clamped), *table);
        tables[clamped] = std::move (table);
    }

    return *tables[clamped];
}

const Wavetable* WavetableLibrary::getTableIfLoaded (int index) const noexcept
{
    if (index < 0 || index >= kNumFactoryTables)
        return nullptr;

    return tables[static_cast<std::size_t> (index)].get();
}

void WavetableLibrary::generateAll()
{
    for (int i = 0; i < kNumFactoryTables; ++i)
        getTable (i);
}

juce::StringArray WavetableLibrary::getTableNames()
{
    juce::StringArray names;

    for (const auto* name : kTableNames)
        names.add (name);

    return names;
}

juce::String WavetableLibrary::getTableName (int index)
{
    return kTableNames[juce::jlimit (0, kNumFactoryTables - 1, index)];
}

} // namespace gnarl::dsp
