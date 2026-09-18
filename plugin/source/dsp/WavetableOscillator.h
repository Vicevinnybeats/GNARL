#pragma once

#include "Wavetable.h"
#include "WarpProcessor.h"
#include "../params/ParameterChoices.h"
#include "../params/ParameterIDs.h"

#include <array>

namespace gnarl::dsp
{

/**
    One oscillator of one voice: wavetable or graintable, with unison and warp.

    INTERPOLATION. Cubic (4-point Hermite) within a frame, linear between
    frames. Cubic within the frame because that is where the waveform's detail
    lives; linear between frames because a table's frames are already a smooth
    morph, and cubic there would overshoot into frames the generator never
    wrote.

    ALIASING. Two defences, and both are needed:
      - the mip level is chosen from the phase increment, so the table itself
        never contains a harmonic above Nyquist;
      - a warp reintroduces harmonics the mip choice did not account for, so
        the level is additionally biased by the warp's bandwidth expansion.
    Warp modes that are genuinely nonlinear still want the oversampled path on
    top of that, which the engine provides.

    FREQUENCY IS INTERPOLATED ACROSS THE BLOCK, from a start value to an end
    value, rather than being held constant. Block-rate pitch would step a fast
    glide or vibrato into audible zipper noise at 256 samples a block.

    Real-time contract: render() allocates nothing, takes no locks, and holds
    only a raw pointer to a Wavetable owned elsewhere.
*/
class WavetableOscillator
{
public:
    /** Everything the oscillator reads per block. Block-rate: the engine
        smooths these before filling the struct. */
    struct Settings
    {
        choices::OscMode mode = choices::OscMode::wavetable;

        float tablePosition = 0.0f;     // 0..1
        warp::Mode warpMode = warp::Mode::off;
        float warpAmount = 0.0f;        // -1..1

        int unisonVoices = 1;           // 1..kMaxUnisonVoices
        float unisonDetune = 0.0f;      // 0..1
        float unisonBlend = 0.5f;       // 0..1, side-voice level
        float unisonSpread = 0.5f;      // 0..1, stereo width

        float pan = 0.0f;               // -1..1
        float level = 1.0f;             // 0..1

        // Graintable mode only.
        float grainSizeMs = 40.0f;
        float grainDensityHz = 20.0f;
        float grainPosJitter = 0.0f;    // 0..1
        float grainPitchJitter = 0.0f;  // 0..1
    };

    WavetableOscillator() = default;

    // --- Setup -------------------------------------------------------------

    void prepare (double sampleRate);

    /** AUDIO THREAD SAFE. Phase increments are derived per render call, so
        changing the rate needs no recomputation and no allocation. */
    void setSampleRate (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;
    }

    /**
        Band-limits to the BASE rate's Nyquist while running at an oversampled
        rate. Pass the oversampling ratio (1, 2 or 4).

        Without this, oversampling makes the oscillator brighter as well as
        giving the nonlinear stages headroom - at 4x it would emit content up
        to 96 kHz. None of that is audible after downsampling, but all of it
        intermodulates in the drive stage and folds back into the audible band.
        Measured: 4x oversampling came out WORSE than 2x on a hard-clipped
        sine until the oscillator's band limit was pinned to the base rate.
    */
    void setOversamplingRatio (float ratio) noexcept
    {
        bandLimitRatio = juce::jlimit (1.0f, 16.0f, ratio);
    }

    void reset() noexcept;

    /** The table to read. May be nullptr, in which case render() outputs
        silence rather than dereferencing it - a table can legitimately still
        be generating on a background thread. */
    void setTable (const Wavetable* newTable) noexcept { table = newTable; }
    const Wavetable* getTable() const noexcept { return table; }

    /**
        Starts a note: seeds the unison phases.

        @param unisonRandom      per-unison random values in -1..1, from the Voice
        @param startPhase        0..1, the patch's phase setting
        @param phaseRandomAmount 0..1; 0 means every note starts phase-locked,
                                 which is what makes a patch punch consistently
    */
    void noteOn (const float* unisonRandom,
                 int numUnisonRandom,
                 float startPhase,
                 float phaseRandomAmount) noexcept;

    // --- Rendering ---------------------------------------------------------

    /**
        Adds this oscillator's output into `left` and `right`.

        @param startFrequencyHz  frequency at the first sample
        @param endFrequencyHz    frequency at the last sample; interpolated between
        @param modulator         optional per-sample signal for FM / ring mod,
                                 or nullptr. Required by warp modes
                                 fmFromOther and ringMod, ignored otherwise.
    */
    void render (float* left,
                 float* right,
                 int numSamples,
                 float startFrequencyHz,
                 float endFrequencyHz,
                 const Settings& settings,
                 const float* modulator = nullptr) noexcept;

private:
    /** 4-point Hermite within the frame, linear between frames. Relies on the
        guard samples on both sides of each frame, so there is no bounds test
        here at all. */
    static float readTable (const Wavetable& table,
                            int mipLevel,
                            int frameIndex,
                            float frameBlend,
                            float phase) noexcept;

    static float interpolateFrame (const float* frame, int frameSize, float phase) noexcept;

    void renderWavetable (float* left, float* right, int numSamples,
                          float startIncrement, float endIncrement,
                          const Settings& settings, const float* modulator) noexcept;

    void renderGraintable (float* left, float* right, int numSamples,
                           float startIncrement, float endIncrement,
                           const Settings& settings) noexcept;

    /** Detune offset in semitones for one unison voice, symmetric about the
        centre. */
    float getUnisonDetuneSemitones (int index, int count, float amount) const noexcept;

    /** Level and pan for one unison voice. */
    void getUnisonGains (int index, int count, const Settings& settings,
                         float& leftGain, float& rightGain) const noexcept;

    static constexpr int kMaxUnison = pid::kMaxUnisonVoices;

    /** Overlapping grain readers per unison voice. Two is enough for a
        crossfade between consecutive grains, which is what keeps the stream
        continuous; more overlap would multiply the read cost by the same
        factor for a subtler result. */
    static constexpr int kGrainSlots = 2;

    struct Grain
    {
        double phase = 0.0;
        float framePosition = 0.0f;  // 0..1 into the table
        float pitchRatio = 1.0f;
        float age = 0.0f;            // samples since the grain started
        float length = 1.0f;         // samples
        bool active = false;
    };

    const Wavetable* table = nullptr;

    double sampleRateHz = 44100.0;

    /** Scales the increment used for MIP LEVEL SELECTION ONLY, never the
        increment used to advance phase. */
    float bandLimitRatio = 1.0f;

    /** Phase accumulators, one per unison voice. DOUBLE, not float: a float
        accumulator drifts audibly out of tune over a long held note, because
        the increment is small relative to the accumulated value. */
    std::array<double, kMaxUnison> phases {};

    /** Per-unison random, captured at note start so unison detune does not
        shimmer while the note is held. */
    std::array<float, kMaxUnison> unisonRandoms {};

    std::array<std::array<Grain, kGrainSlots>, kMaxUnison> grains {};
    std::array<float, kMaxUnison> grainClocks {};
    std::array<int, kMaxUnison> nextGrainSlot {};

    /** Deterministic jitter stream for grains, reseeded per note. */
    std::uint32_t grainRandomState = 1;

    float nextGrainRandom() noexcept;

    JUCE_LEAK_DETECTOR (WavetableOscillator)
};

} // namespace gnarl::dsp
