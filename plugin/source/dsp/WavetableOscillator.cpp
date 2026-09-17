#include "WavetableOscillator.h"

#include <cmath>

namespace gnarl::dsp
{

namespace
{
    /** Widest unison detune, in semitones either side of centre, at amount 1.
        A whole tone each way: wider than this stops reading as one sound. */
    constexpr float kMaxUnisonDetuneSemitones = 2.0f;

    /** How much of the detune spread comes from the per-voice random value
        rather than the even spacing. A perfectly even spread beats into a
        recognisable comb; a little irregularity makes it sound like an
        ensemble. */
    constexpr float kUnisonRandomShare = 0.35f;

    /** Phase-modulation depth in cycles at warp amount 1. Above about half a
        cycle the result is closer to noise than to a tone. */
    constexpr float kMaxFmDepthCycles = 0.5f;

    /** Grain pitch jitter range, in semitones, at jitter 1. */
    constexpr float kMaxGrainPitchJitterSemitones = 12.0f;

    constexpr float kMinGrainLengthSamples = 8.0f;

    float semitonesToRatio (float semitones) noexcept
    {
        return std::exp2 (semitones * (1.0f / 12.0f));
    }

    /** Hann window, for grain envelopes. Hann rather than a linear ramp
        because its derivative is zero at both ends, so overlapping grains
        crossfade without a slope discontinuity at the seam. */
    float hann (float normalisedPosition) noexcept
    {
        const auto t = juce::jlimit (0.0f, 1.0f, normalisedPosition);
        return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * t);
    }

    /** Equal-power pan, so sweeping a voice across the field does not dip in
        level at the centre the way a linear pan law does. */
    void panGains (float pan, float& leftGain, float& rightGain) noexcept
    {
        const auto normalised = (juce::jlimit (-1.0f, 1.0f, pan) + 1.0f) * 0.5f;
        const auto angle = normalised * juce::MathConstants<float>::halfPi;

        leftGain = std::cos (angle);
        rightGain = std::sin (angle);
    }
}

void WavetableOscillator::prepare (double sampleRate)
{
    sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;
    reset();
}

void WavetableOscillator::reset() noexcept
{
    phases.fill (0.0);
    unisonRandoms.fill (0.0f);
    grainClocks.fill (0.0f);
    nextGrainSlot.fill (0);

    for (auto& slots : grains)
        for (auto& grain : slots)
            grain = Grain {};

    grainRandomState = 1;
}

float WavetableOscillator::nextGrainRandom() noexcept
{
    // xorshift32: deterministic, cheap, and good enough for jitter. A
    // std::mt19937 here would be both slower and harder to reproduce.
    grainRandomState ^= grainRandomState << 13;
    grainRandomState ^= grainRandomState >> 17;
    grainRandomState ^= grainRandomState << 5;

    return static_cast<float> (grainRandomState & 0xFFFFFFu)
         / static_cast<float> (0xFFFFFF);
}

void WavetableOscillator::noteOn (const float* unisonRandom,
                                  int numUnisonRandom,
                                  float startPhase,
                                  float phaseRandomAmount) noexcept
{
    const auto randomAmount = juce::jlimit (0.0f, 1.0f, phaseRandomAmount);
    const auto basePhase = warp::wrapPhase (startPhase);

    for (int i = 0; i < kMaxUnison; ++i)
    {
        const auto random = (unisonRandom != nullptr && i < numUnisonRandom)
                          ? unisonRandom[i]
                          : 0.0f;

        unisonRandoms[static_cast<std::size_t> (i)] = random;

        // Phase random 0 means every note starts phase-locked, which is what
        // makes a patch's transient punch identically every time. Only the
        // random amount breaks that, and only as far as asked.
        const auto offset = randomAmount * (random * 0.5f + 0.5f);
        phases[static_cast<std::size_t> (i)] = warp::wrapPhase (basePhase + offset);
    }

    // Seeded from the voice's own random, so grain jitter is deterministic per
    // note but differs between voices.
    const auto seedSource = unisonRandoms[0] * 0.5f + 0.5f;
    grainRandomState = 1u + static_cast<std::uint32_t> (seedSource * 4294967040.0f);

    if (grainRandomState == 0u)
        grainRandomState = 1u;

    // Primed so the FIRST rendered sample fires a grain. Starting the clock
    // at zero would leave the oscillator silent for a whole grain period -
    // a full second at 1 Hz density - which reads as a broken patch rather
    // than as a slow grain stream.
    grainClocks.fill (1.0f);
    nextGrainSlot.fill (0);

    for (auto& slots : grains)
        for (auto& grain : slots)
            grain = Grain {};
}

// --- Table reading ---------------------------------------------------------

float WavetableOscillator::interpolateFrame (const float* frame,
                                             int frameSize,
                                             float phase) noexcept
{
    const auto position = phase * static_cast<float> (frameSize);
    const auto index = static_cast<int> (position);
    const auto t = position - static_cast<float> (index);

    // No bounds test: the frame is padded on both sides, so index-1 and
    // index+2 are always readable. See Wavetable's guard samples.
    const auto xm1 = frame[index - 1];
    const auto x0  = frame[index];
    const auto x1  = frame[index + 1];
    const auto x2  = frame[index + 2];

    // 4-point Hermite (Catmull-Rom), in Horner form.
    const auto c1 = 0.5f * (x1 - xm1);
    const auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);

    return ((c3 * t + c2) * t + c1) * t + x0;
}

float WavetableOscillator::readTable (const Wavetable& source,
                                      int mipLevel,
                                      int frameIndex,
                                      float frameBlend,
                                      float phase) noexcept
{
    const auto frameSize = Wavetable::getFrameSizeForLevel (mipLevel);
    const auto* frameA = source.getFrame (mipLevel, frameIndex);

    if (frameA == nullptr)
        return 0.0f;

    const auto sampleA = interpolateFrame (frameA, frameSize, phase);

    // Linear between frames, not cubic: the frames are already a smooth
    // morph, and a cubic there would overshoot into frames the generator
    // never wrote.
    if (frameBlend <= 0.0f)
        return sampleA;

    const auto* frameB = source.getFrame (mipLevel, frameIndex + 1);

    if (frameB == nullptr)
        return sampleA;

    const auto sampleB = interpolateFrame (frameB, frameSize, phase);

    return sampleA + (sampleB - sampleA) * frameBlend;
}

// --- Unison ----------------------------------------------------------------

float WavetableOscillator::getUnisonDetuneSemitones (int index,
                                                     int count,
                                                     float amount) const noexcept
{
    if (count <= 1 || amount <= 0.0f)
        return 0.0f;

    // Even spread across -1..1, plus a little per-voice randomness.
    const auto spread = static_cast<float> (index) / static_cast<float> (count - 1) * 2.0f - 1.0f;
    const auto random = unisonRandoms[static_cast<std::size_t> (index)];

    const auto shaped = spread * (1.0f - kUnisonRandomShare)
                      + random * kUnisonRandomShare;

    return shaped * amount * kMaxUnisonDetuneSemitones;
}

void WavetableOscillator::getUnisonGains (int index,
                                          int count,
                                          const Settings& settings,
                                          float& leftGain,
                                          float& rightGain) const noexcept
{
    auto level = settings.level;

    if (count > 1)
    {
        // Blend sets how loud the side voices are relative to the centre. At
        // blend 0 only the centre sounds, so turning unison up cannot quietly
        // change the patch's level.
        const auto distanceFromCentre =
            std::abs (static_cast<float> (index) / static_cast<float> (count - 1) * 2.0f - 1.0f);

        const auto blend = juce::jlimit (0.0f, 1.0f, settings.unisonBlend);
        level *= juce::jmap (distanceFromCentre, 1.0f, blend);

        // Normalise by voice count so unison changes width, not loudness.
        level /= std::sqrt (static_cast<float> (count));
    }

    // Stereo spread, then the oscillator's own pan on top.
    auto voicePan = settings.pan;

    if (count > 1)
    {
        const auto spreadPosition =
            static_cast<float> (index) / static_cast<float> (count - 1) * 2.0f - 1.0f;

        voicePan = juce::jlimit (-1.0f, 1.0f,
                                 settings.pan + spreadPosition * settings.unisonSpread);
    }

    panGains (voicePan, leftGain, rightGain);

    leftGain *= level;
    rightGain *= level;
}

// --- Rendering -------------------------------------------------------------

void WavetableOscillator::render (float* left,
                                  float* right,
                                  int numSamples,
                                  float startFrequencyHz,
                                  float endFrequencyHz,
                                  const Settings& settings,
                                  const float* modulator) noexcept
{
    if (table == nullptr || ! table->isGenerated() || numSamples <= 0)
        return;

    if (left == nullptr || right == nullptr)
        return;

    const auto startIncrement = static_cast<float> (startFrequencyHz / sampleRateHz);
    const auto endIncrement = static_cast<float> (endFrequencyHz / sampleRateHz);

    if (settings.mode == choices::OscMode::graintable)
        renderGraintable (left, right, numSamples, startIncrement, endIncrement, settings);
    else
        renderWavetable (left, right, numSamples, startIncrement, endIncrement,
                         settings, modulator);
}

void WavetableOscillator::renderWavetable (float* left,
                                           float* right,
                                           int numSamples,
                                           float startIncrement,
                                           float endIncrement,
                                           const Settings& settings,
                                           const float* modulator) noexcept
{
    const auto count = juce::jlimit (1, kMaxUnison, settings.unisonVoices);

    const auto warpMode = settings.warpMode;
    const auto warpAmount = juce::jlimit (-1.0f, 1.0f, settings.warpAmount);
    const auto isPhaseWarp = warp::isPhaseWarp (warpMode);

    // FM and ring mod need the other oscillator; without it they are no-ops
    // rather than silence, so a patch does not vanish if the modulator source
    // is disabled.
    const auto useFm = warpMode == warp::Mode::fmFromOther && modulator != nullptr;
    const auto useRingMod = warpMode == warp::Mode::ringMod && modulator != nullptr;
    const auto fmDepth = std::abs (warpAmount) * kMaxFmDepthCycles;
    const auto ringModAmount = std::abs (warpAmount);

    // A warp adds harmonics the plain increment does not predict, so bias the
    // mip choice by the mode's bandwidth expansion or the warp will alias.
    const auto expansion = warp::getBandwidthExpansion (warpMode, warpAmount);

    const auto tablePosition = juce::jlimit (0.0f, 1.0f, settings.tablePosition);
    const auto framePosition = tablePosition * static_cast<float> (Wavetable::kNumFrames - 1);
    const auto frameIndex = static_cast<int> (framePosition);
    const auto frameBlend = framePosition - static_cast<float> (frameIndex);

    const auto lastSample = juce::jmax (1, numSamples - 1);

    for (int voice = 0; voice < count; ++voice)
    {
        const auto ratio = semitonesToRatio (
            getUnisonDetuneSemitones (voice, count, settings.unisonDetune));

        const auto voiceStartIncrement = startIncrement * ratio;
        const auto voiceEndIncrement = endIncrement * ratio;

        float leftGain = 0.0f;
        float rightGain = 0.0f;
        getUnisonGains (voice, count, settings, leftGain, rightGain);

        // Chosen from the faster end of the block, so a rising glide cannot
        // outrun the level picked for it.
        const auto mipLevel = Wavetable::getMipLevelForIncrement (
            juce::jmax (std::abs (voiceStartIncrement), std::abs (voiceEndIncrement))
            * expansion);

        auto phase = phases[static_cast<std::size_t> (voice)];

        for (int i = 0; i < numSamples; ++i)
        {
            const auto t = static_cast<float> (i) / static_cast<float> (lastSample);
            const auto increment = voiceStartIncrement
                                 + (voiceEndIncrement - voiceStartIncrement) * t;

            auto readPhase = static_cast<float> (phase);

            // Phase modulation rather than true frequency modulation: PM does
            // not drift the note's pitch as depth rises, which FM does.
            if (useFm)
                readPhase = warp::wrapPhase (readPhase + modulator[i] * fmDepth);
            else if (isPhaseWarp)
                readPhase = warp::applyPhase (readPhase, warpMode, warpAmount);

            auto sample = readTable (*table, mipLevel, frameIndex, frameBlend, readPhase);

            // Blended by amount, so ring mod at 0 is identity like every
            // other warp rather than a permanent 100% wet effect.
            if (useRingMod)
                sample = sample * (1.0f - ringModAmount)
                       + sample * modulator[i] * ringModAmount;

            left[i] += sample * leftGain;
            right[i] += sample * rightGain;

            phase += increment;

            // Cheaper than fmod and exact for the one-cycle overshoot an
            // increment below Nyquist can produce.
            while (phase >= 1.0)
                phase -= 1.0;
            while (phase < 0.0)
                phase += 1.0;
        }

        phases[static_cast<std::size_t> (voice)] = phase;
    }
}

void WavetableOscillator::renderGraintable (float* left,
                                            float* right,
                                            int numSamples,
                                            float startIncrement,
                                            float endIncrement,
                                            const Settings& settings) noexcept
{
    const auto count = juce::jlimit (1, kMaxUnison, settings.unisonVoices);

    const auto grainLength = juce::jmax (
        kMinGrainLengthSamples,
        settings.grainSizeMs * 0.001f * static_cast<float> (sampleRateHz));

    // Grains per sample. Clamped away from zero so the scheduler always
    // eventually fires; a density of zero would leave the oscillator silent
    // with no indication why.
    const auto grainRate = juce::jmax (0.01f, settings.grainDensityHz)
                         / static_cast<float> (sampleRateHz);

    const auto positionJitter = juce::jlimit (0.0f, 1.0f, settings.grainPosJitter);
    const auto pitchJitter = juce::jlimit (0.0f, 1.0f, settings.grainPitchJitter);

    const auto basePosition = juce::jlimit (0.0f, 1.0f, settings.tablePosition);
    const auto lastSample = juce::jmax (1, numSamples - 1);

    for (int voice = 0; voice < count; ++voice)
    {
        const auto ratio = semitonesToRatio (
            getUnisonDetuneSemitones (voice, count, settings.unisonDetune));

        const auto voiceStartIncrement = startIncrement * ratio;
        const auto voiceEndIncrement = endIncrement * ratio;

        float leftGain = 0.0f;
        float rightGain = 0.0f;
        getUnisonGains (voice, count, settings, leftGain, rightGain);

        auto& slots = grains[static_cast<std::size_t> (voice)];
        auto& clock = grainClocks[static_cast<std::size_t> (voice)];
        auto& slotCursor = nextGrainSlot[static_cast<std::size_t> (voice)];

        for (int i = 0; i < numSamples; ++i)
        {
            const auto t = static_cast<float> (i) / static_cast<float> (lastSample);
            const auto increment = voiceStartIncrement
                                 + (voiceEndIncrement - voiceStartIncrement) * t;

            clock += grainRate;

            if (clock >= 1.0f)
            {
                clock -= 1.0f;

                // Round-robin over the slots. With two slots a new grain can
                // replace one that is still sounding; that is the documented
                // limit of the overlap, and the Hann window means the
                // replacement fades in rather than clicking.
                auto& grain = slots[static_cast<std::size_t> (slotCursor)];
                slotCursor = (slotCursor + 1) % kGrainSlots;

                const auto jitterPosition = (nextGrainRandom() * 2.0f - 1.0f) * positionJitter;
                const auto jitterPitch = (nextGrainRandom() * 2.0f - 1.0f) * pitchJitter;

                grain.phase = nextGrainRandom();
                grain.framePosition = juce::jlimit (0.0f, 1.0f, basePosition + jitterPosition);
                grain.pitchRatio = semitonesToRatio (jitterPitch * kMaxGrainPitchJitterSemitones);
                grain.age = 0.0f;
                grain.length = grainLength;
                grain.active = true;
            }

            auto sum = 0.0f;

            for (auto& grain : slots)
            {
                if (! grain.active)
                    continue;

                const auto grainIncrement = increment * grain.pitchRatio;
                const auto mipLevel = Wavetable::getMipLevelForIncrement (grainIncrement);

                const auto framePosition = grain.framePosition
                                         * static_cast<float> (Wavetable::kNumFrames - 1);
                const auto frameIndex = static_cast<int> (framePosition);
                const auto frameBlend = framePosition - static_cast<float> (frameIndex);

                const auto windowed = readTable (*table, mipLevel, frameIndex, frameBlend,
                                                 static_cast<float> (grain.phase))
                                    * hann (grain.age / grain.length);

                sum += windowed;

                grain.phase += grainIncrement;

                while (grain.phase >= 1.0)
                    grain.phase -= 1.0;
                while (grain.phase < 0.0)
                    grain.phase += 1.0;

                grain.age += 1.0f;

                if (grain.age >= grain.length)
                    grain.active = false;
            }

            left[i] += sum * leftGain;
            right[i] += sum * rightGain;
        }
    }
}

} // namespace gnarl::dsp
