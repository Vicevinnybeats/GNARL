#pragma once

#include "StateVariableFilter.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>

namespace gnarl::dsp
{

/**
    Three-band upward AND downward compressor - the "OTT" sound.

    Built in because riddim producers put OTT on everything, so shipping it
    inside the synth removes a plugin from the chain and lets the compressor
    see the voices before the master stage.

    WHAT MAKES IT "OTT" RATHER THAN A MULTIBAND COMPRESSOR. Two things:

    1. UPWARD compression as well as downward. Downward compression pulls loud
       material down; upward compression pushes QUIET material up. Running both
       hard in each band is what produces the dense, always-present, slightly
       breathing texture - it is closer to aggressive levelling than to peak
       control, and it is why the effect brings up detail a normal compressor
       would leave buried.

    2. Everything is per band and set hard. A gentle version of this is a
       different effect.

    CROSSOVERS ARE LINKWITZ-RILEY 4th ORDER, which is two cascaded Butterworth
    sections. LR4 matters here rather than being a detail: its low- and
    high-pass outputs sum to an allpass, so the three bands reconstruct with a
    flat magnitude response. A naive crossover leaves a dip or a bump at the
    split frequency that no amount of band gain can fix.

    Note the low band is ALSO passed through the second crossover and summed.
    That applies the same allpass to it that the mid and high bands received;
    without it the bands sum to something that is not flat, and the effect
    colours the sound even at zero depth.
*/
class OttCompressor
{
public:
    static constexpr int kNumBands = 3;

    struct Settings
    {
        bool enabled = false;

        /** The macro everyone actually uses: scales up- and downward
            compression together. 0 is transparent, 1 is the full effect. */
        float depth = 0.0f;

        /** Scales attack and release together. 0.5 is the reference timing. */
        float time = 0.5f;

        float mix = 1.0f;           // 0..1 dry/wet
        float inputGainDb = 0.0f;
        float outputGainDb = 0.0f;

        float crossoverLowHz = 88.0f;
        float crossoverHighHz = 2500.0f;

        /** Per-band trim, in dB. */
        std::array<float, kNumBands> bandGainDb { 0.0f, 0.0f, 0.0f };

        /** Per-band upward and downward amounts, 0..1, scaled by depth. */
        std::array<float, kNumBands> upwardAmount { 1.0f, 1.0f, 1.0f };
        std::array<float, kNumBands> downwardAmount { 1.0f, 1.0f, 1.0f };
    };

    void prepare (double maximumSampleRate, int numChannels)
    {
        channels = juce::jlimit (1, 2, numChannels);

        for (auto& channel : state)
        {
            channel.lowCrossover.prepare (maximumSampleRate);
            channel.highCrossover.prepare (maximumSampleRate);
            channel.lowAllpass.prepare (maximumSampleRate);
        }

        setSampleRate (maximumSampleRate);
        reset();
    }

    /** AUDIO THREAD SAFE. */
    void setSampleRate (double sampleRate) noexcept
    {
        sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;

        for (auto& channel : state)
        {
            channel.lowCrossover.setSampleRate (sampleRateHz);
            channel.highCrossover.setSampleRate (sampleRateHz);
            channel.lowAllpass.setSampleRate (sampleRateHz);
        }

        coefficientsDirty = true;
    }

    void reset() noexcept
    {
        for (auto& channel : state)
        {
            channel.lowCrossover.reset();
            channel.highCrossover.reset();
            channel.lowAllpass.reset();

            channel.envelope.fill (0.0f);
            channel.gain.fill (1.0f);
        }
    }

    /** BLOCK-RATE. */
    void setSettings (const Settings& newSettings) noexcept
    {
        const auto crossoversChanged =
            ! juce::approximatelyEqual (newSettings.crossoverLowHz, settings.crossoverLowHz)
            || ! juce::approximatelyEqual (newSettings.crossoverHighHz, settings.crossoverHighHz);

        settings = newSettings;

        if (crossoversChanged || coefficientsDirty)
            updateCrossovers();

        const auto depth = juce::jlimit (0.0f, 1.0f, settings.depth);

        // Reference timings, scaled by the time control. Attack is short
        // because the effect is meant to grab transients, not ride them.
        const auto timeScale = std::exp2 ((juce::jlimit (0.0f, 1.0f, settings.time) - 0.5f) * 4.0f);

        for (int band = 0; band < kNumBands; ++band)
        {
            const auto index = static_cast<std::size_t> (band);

            // Higher bands are faster: a 5 kHz transient is over before a
            // 60 Hz one has finished its first cycle, so one time constant
            // for all three would either smear the top or pump the bottom.
            const auto attackMs = kBandAttackMs[index] * timeScale;
            const auto releaseMs = kBandReleaseMs[index] * timeScale;

            attackCoefficient[index] = coefficientFor (attackMs);
            releaseCoefficient[index] = coefficientFor (releaseMs);

            downwardStrength[index] = depth * juce::jlimit (0.0f, 1.0f, settings.downwardAmount[index]);
            upwardStrength[index] = depth * juce::jlimit (0.0f, 1.0f, settings.upwardAmount[index]);

            bandGain[index] = juce::Decibels::decibelsToGain (settings.bandGainDb[index]);
        }

        inputGain = juce::Decibels::decibelsToGain (settings.inputGainDb);
        outputGain = juce::Decibels::decibelsToGain (settings.outputGainDb);
        wetMix = juce::jlimit (0.0f, 1.0f, settings.mix);
    }

    /** SAMPLE-RATE. Processes one sample of one channel in place. */
    float processSample (int channel, float input) noexcept
    {
        if (! settings.enabled || wetMix <= 0.0f)
            return input;

        auto& c = state[static_cast<std::size_t> (juce::jlimit (0, 1, channel))];

        const auto dry = input;
        const auto driven = input * inputGain;

        // --- Band split (LR4) ----------------------------------------------
        float lowRaw = 0.0f;
        float rest = 0.0f;
        c.lowCrossover.process (driven, lowRaw, rest);

        float midBand = 0.0f;
        float highBand = 0.0f;
        c.highCrossover.process (rest, midBand, highBand);

        // The low band is run through an LR4 split at the HIGH crossover and
        // both halves summed, which is an allpass. That applies to the low
        // band the same phase shift the mid and high bands picked up from the
        // second split - without it the three bands do not sum flat, and the
        // effect colours the sound even at zero depth.
        float lowA = 0.0f;
        float lowB = 0.0f;
        c.lowAllpass.process (lowRaw, lowA, lowB);
        const auto lowBand = lowA + lowB;

        const std::array<float, kNumBands> bands { lowBand, midBand, highBand };

        // --- Per-band compression ------------------------------------------
        auto wet = 0.0f;

        for (int band = 0; band < kNumBands; ++band)
        {
            const auto index = static_cast<std::size_t> (band);
            const auto sample = bands[index];
            const auto rectified = std::abs (sample);

            // Peak follower with separate attack and release.
            auto& envelope = c.envelope[index];
            const auto coefficient = rectified > envelope
                                   ? attackCoefficient[index]
                                   : releaseCoefficient[index];

            envelope = coefficient * envelope + (1.0f - coefficient) * rectified;

            wet += sample * computeGain (index, envelope) * bandGain[index];
        }

        wet *= outputGain;

        return dry + (wet - dry) * wetMix;
    }

    /** Gain reduction currently applied to each band, in dB, for the UI's
        meters. Negative is downward, positive is upward. */
    float getBandGainDb (int band) const noexcept
    {
        const auto index = static_cast<std::size_t> (juce::jlimit (0, kNumBands - 1, band));
        return juce::Decibels::gainToDecibels (state[0].gain[index], -60.0f);
    }

private:
    /** Thresholds are fixed rather than exposed. OTT's character comes from
        being set hard in a particular place; a threshold control turns it into
        a generic multiband compressor, which is a different product. */
    static constexpr float kDownwardThresholdDb = -28.0f;
    static constexpr float kUpwardThresholdDb = -38.0f;

    /** Ratios at full depth. */
    static constexpr float kDownwardRatio = 4.0f;
    static constexpr float kUpwardRatio = 4.0f;

    /** Ceiling on upward gain, so near-silence is lifted but not amplified
        into the noise floor. */
    static constexpr float kMaxUpwardDb = 24.0f;

    static constexpr std::array<float, kNumBands> kBandAttackMs { 8.0f, 4.0f, 1.5f };
    static constexpr std::array<float, kNumBands> kBandReleaseMs { 180.0f, 120.0f, 80.0f };

    float coefficientFor (float milliseconds) const noexcept
    {
        const auto seconds = juce::jmax (0.0001f, milliseconds * 0.001f);
        return std::exp (-1.0f / (seconds * static_cast<float> (sampleRateHz)));
    }

    float computeGain (std::size_t band, float envelope) noexcept
    {
        const auto levelDb = juce::Decibels::gainToDecibels (envelope, -100.0f);
        auto gainDb = 0.0f;

        // Downward: above the threshold, reduce the excess.
        if (levelDb > kDownwardThresholdDb && downwardStrength[band] > 0.0f)
        {
            const auto excess = levelDb - kDownwardThresholdDb;
            gainDb -= excess * (1.0f - 1.0f / kDownwardRatio) * downwardStrength[band];
        }

        // Upward: below the threshold, lift the deficit. This is the half that
        // makes it OTT rather than a compressor.
        if (levelDb < kUpwardThresholdDb && upwardStrength[band] > 0.0f)
        {
            const auto deficit = kUpwardThresholdDb - levelDb;
            const auto lift = juce::jmin (deficit * (1.0f - 1.0f / kUpwardRatio),
                                          kMaxUpwardDb);
            gainDb += lift * upwardStrength[band];
        }

        const auto target = juce::Decibels::decibelsToGain (gainDb);

        // Stored for the meters.
        state[0].gain[band] = target;

        return target;
    }

    void updateCrossovers() noexcept
    {
        coefficientsDirty = false;

        const auto nyquist = static_cast<float> (sampleRateHz) * 0.49f;
        const auto low = juce::jlimit (20.0f, nyquist, settings.crossoverLowHz);
        const auto high = juce::jlimit (juce::jmin (low * 1.5f, nyquist), nyquist,
                                        settings.crossoverHighHz);

        for (auto& channel : state)
        {
            channel.lowCrossover.setCutoff (low);
            channel.highCrossover.setCutoff (high);

            // At the HIGH crossover, matching the split the other bands got.
            channel.lowAllpass.setCutoff (high);
        }
    }

    /**
        A Linkwitz-Riley 4th-order crossover: two cascaded Butterworth
        sections on each output.

        EACH PATH HAS ITS OWN FILTERS. Sharing one second-stage filter between
        the low and high paths, by calling it twice per sample with different
        inputs, corrupts its state and the bands then cancel rather than sum -
        measured at 24 dB down before this was split out. A filter instance
        can be advanced once per sample and no more.
    */
    struct LinkwitzRileyCrossover
    {
        void prepare (double maximumSampleRate)
        {
            for (auto* filter : { &lowPass1, &lowPass2, &highPass1, &highPass2 })
                filter->prepare (maximumSampleRate);
        }

        void setSampleRate (double sampleRate) noexcept
        {
            for (auto* filter : { &lowPass1, &lowPass2, &highPass1, &highPass2 })
                filter->setSampleRate (sampleRate);
        }

        void reset() noexcept
        {
            for (auto* filter : { &lowPass1, &lowPass2, &highPass1, &highPass2 })
                filter->reset();
        }

        void setCutoff (float frequencyHz) noexcept
        {
            for (auto* filter : { &lowPass1, &lowPass2, &highPass1, &highPass2 })
            {
                filter->setCutoff (frequencyHz);
                filter->setQ (StateVariableFilter::getFlatQ());
            }
        }

        void process (float input, float& low, float& high) noexcept
        {
            low = lowPass2.processSample (lowPass1.processSample (input).lowPass).lowPass;
            high = highPass2.processSample (highPass1.processSample (input).highPass).highPass;
        }

        StateVariableFilter lowPass1;
        StateVariableFilter lowPass2;
        StateVariableFilter highPass1;
        StateVariableFilter highPass2;
    };

    struct ChannelState
    {
        LinkwitzRileyCrossover lowCrossover;
        LinkwitzRileyCrossover highCrossover;

        /** Sits at the HIGH crossover; both halves are summed, giving an
            allpass that matches the low band's phase to the other two. */
        LinkwitzRileyCrossover lowAllpass;

        std::array<float, kNumBands> envelope {};
        std::array<float, kNumBands> gain { 1.0f, 1.0f, 1.0f };
    };

    double sampleRateHz = 44100.0;
    int channels = 2;
    bool coefficientsDirty = true;

    Settings settings {};

    std::array<ChannelState, 2> state {};

    std::array<float, kNumBands> attackCoefficient { 0.0f, 0.0f, 0.0f };
    std::array<float, kNumBands> releaseCoefficient { 0.0f, 0.0f, 0.0f };
    std::array<float, kNumBands> downwardStrength { 0.0f, 0.0f, 0.0f };
    std::array<float, kNumBands> upwardStrength { 0.0f, 0.0f, 0.0f };
    std::array<float, kNumBands> bandGain { 1.0f, 1.0f, 1.0f };

    float inputGain = 1.0f;
    float outputGain = 1.0f;
    float wetMix = 1.0f;
};

} // namespace gnarl::dsp
