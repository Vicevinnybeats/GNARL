#pragma once

#include "FxChain.h"
#include "FxChorus.h"
#include "FxDelay.h"
#include "FxDimension.h"
#include "FxDistortion.h"
#include "FxEq.h"
#include "FxFilter.h"
#include "FxFlanger.h"
#include "FxHyper.h"
#include "FxLimiter.h"
#include "FxPhaser.h"
#include "FxReverb.h"
#include "../params/ParameterIDs.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>

namespace gnarl::dsp
{

/**
    The FX rack: fourteen effect instances, run in an order the user sets.

    A FIXED ROSTER WITH A REORDERABLE ORDER, not a set of generic slots - see
    docs/fx-architecture.md for the argument. The short version is that a
    generic slot's type would have to be a parameter, and then changing it
    silently changes the meaning of every automation lane pointing into that
    slot.

    THE WHOLE RACK RUNS OVERSAMPLED WHEN A NONLINEAR EFFECT IS ON, and that
    follows from a constraint rather than a preference. `FxDistortion` and the
    FX filter's drive are memoryless waveshapers: they generate harmonics above
    Nyquist by construction, and no filter after them can remove the partials
    that fold down (CLAUDE.md section 3). Both headers say their aliasing is
    the caller's problem, and this is the caller.

    It is the whole rack rather than just those effects because the order is
    arbitrary. An effect can only be oversampled in isolation if the signal can
    be buffered around it, and a chain whose order the user chooses at runtime
    cannot be split into contiguous oversampled groups. Running everything at
    2x when anything nonlinear is enabled is the honest way to honour the
    constraint; it costs nothing on a patch with no distortion, which is why
    the check is per block.

    2x AND NOT 4x, because that is what the measurement supports: CLAUDE.md
    section 7 records the drive stage at -28.6 dBc with oversampling off,
    -43.1 dBc at 2x and -42.0 dBc at 4x. Past 2x the folded partials are below
    the oscillator's own interpolation floor, so 4x buys nothing and costs
    twice as much.

    EVERY EFFECT'S BUFFERS ARE SIZED FOR THE OVERSAMPLED RATE in prepare, so
    switching the factor mid-session cannot allocate. Every effect's
    `setSampleRate` is non-resetting for the same reason the voice's is: the
    factor changes while notes are sounding, and resetting would silence the
    delay lines and reverb tail the moment a user enabled a distortion.

    Real-time safe.
*/
class FxRack
{
public:
    using Slot = choices::FxSlot;

    /** MESSAGE THREAD. Everything allocates here and nowhere else. */
    void prepare (double sampleRate, int maximumBlockSize)
    {
        baseSampleRate = sampleRate;
        oversampledRate = sampleRate * static_cast<double> (kOversamplingFactor);

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            2,
            kOversamplingStages,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true,
            false);

        oversampler->initProcessing (static_cast<std::size_t> (maximumBlockSize));
        oversampler->reset();

        // EVERY effect is prepared at the OVERSAMPLED rate, so no buffer has
        // to grow when the factor changes on the audio thread.
        for (auto& instance : distortions)
            instance.prepare (oversampledRate);

        for (auto& instance : eqs)
            instance.prepare (oversampledRate);

        for (auto& instance : filters)
            instance.prepare (oversampledRate);

        chorus.prepare (oversampledRate);
        flanger.prepare (oversampledRate);
        phaser.prepare (oversampledRate);
        hyper.prepare (oversampledRate);
        dimension.prepare (oversampledRate);
        delay.prepare (oversampledRate);
        reverb.prepare (oversampledRate);
        limiter.prepare (oversampledRate);

        // Then set to the rate actually in use, which is the base rate until
        // something nonlinear is switched on.
        applySampleRate (baseSampleRate);

        reset();
    }

    void reset() noexcept
    {
        if (oversampler != nullptr)
            oversampler->reset();

        for (auto& instance : distortions)
            instance.reset();

        for (auto& instance : eqs)
            instance.reset();

        for (auto& instance : filters)
            instance.reset();

        chorus.reset();
        flanger.reset();
        phaser.reset();
        hyper.reset();
        dimension.reset();
        delay.reset();
        reverb.reset();
        limiter.reset();
    }

    /** MESSAGE THREAD. Caches the raw atomics so the audio thread never looks
        a parameter up by string. */
    void bind (juce::AudioProcessorValueTreeState& apvts)
    {
        const auto at = [&apvts] (const char* id)
        {
            auto* pointer = apvts.getRawParameterValue (id);
            jassert (pointer != nullptr);
            return pointer;
        };

        for (std::size_t i = 0; i < pid::kNumFxDistortions; ++i)
        {
            const auto& ids = pid::fxDistortion[i];
            auto& bound = distortionParams[i];

            bound.enabled = at (ids.enabled);
            bound.mix = at (ids.mix);
            bound.type = at (ids.type);
            bound.drive = at (ids.drive);
            bound.tone = at (ids.tone);
            bound.bias = at (ids.bias);
            bound.output = at (ids.output);
        }

        for (std::size_t i = 0; i < pid::kNumFxEqs; ++i)
        {
            const auto& ids = pid::fxEq[i];
            auto& bound = eqParams[i];

            bound.enabled = at (ids.enabled);
            bound.mix = at (ids.mix);
            bound.highPassFreq = at (ids.highPassFreq);
            bound.lowShelfFreq = at (ids.lowShelfFreq);
            bound.lowShelfGain = at (ids.lowShelfGain);
            bound.band1Freq = at (ids.band1Freq);
            bound.band1Gain = at (ids.band1Gain);
            bound.band1Q = at (ids.band1Q);
            bound.band2Freq = at (ids.band2Freq);
            bound.band2Gain = at (ids.band2Gain);
            bound.band2Q = at (ids.band2Q);
            bound.highShelfFreq = at (ids.highShelfFreq);
            bound.highShelfGain = at (ids.highShelfGain);
            bound.lowPassFreq = at (ids.lowPassFreq);
        }

        for (std::size_t i = 0; i < pid::kNumFxFilters; ++i)
        {
            const auto& ids = pid::fxFilter[i];
            auto& bound = filterParams[i];

            bound.enabled = at (ids.enabled);
            bound.mix = at (ids.mix);
            bound.type = at (ids.type);
            bound.cutoff = at (ids.cutoff);
            bound.resonance = at (ids.resonance);
            bound.drive = at (ids.drive);
        }

        chorusParams.enabled = at (pid::fxChorus.enabled);
        chorusParams.mix = at (pid::fxChorus.mix);
        chorusParams.rate = at (pid::fxChorus.rate);
        chorusParams.depth = at (pid::fxChorus.depth);
        chorusParams.voices = at (pid::fxChorus.voices);
        chorusParams.spread = at (pid::fxChorus.spread);
        chorusParams.feedback = at (pid::fxChorus.feedback);

        flangerParams.enabled = at (pid::fxFlanger.enabled);
        flangerParams.mix = at (pid::fxFlanger.mix);
        flangerParams.rate = at (pid::fxFlanger.rate);
        flangerParams.depth = at (pid::fxFlanger.depth);
        flangerParams.feedback = at (pid::fxFlanger.feedback);
        flangerParams.manual = at (pid::fxFlanger.manual);
        flangerParams.stereo = at (pid::fxFlanger.stereo);

        phaserParams.enabled = at (pid::fxPhaser.enabled);
        phaserParams.mix = at (pid::fxPhaser.mix);
        phaserParams.rate = at (pid::fxPhaser.rate);
        phaserParams.depth = at (pid::fxPhaser.depth);
        phaserParams.stages = at (pid::fxPhaser.stages);
        phaserParams.centre = at (pid::fxPhaser.centre);
        phaserParams.feedback = at (pid::fxPhaser.feedback);
        phaserParams.stereo = at (pid::fxPhaser.stereo);

        hyperParams.enabled = at (pid::fxHyper.enabled);
        hyperParams.mix = at (pid::fxHyper.mix);
        hyperParams.amount = at (pid::fxHyper.amount);
        hyperParams.detune = at (pid::fxHyper.detune);
        hyperParams.voices = at (pid::fxHyper.voices);
        hyperParams.width = at (pid::fxHyper.width);

        dimensionParams.enabled = at (pid::fxDimension.enabled);
        dimensionParams.mix = at (pid::fxDimension.mix);
        dimensionParams.amount = at (pid::fxDimension.amount);
        dimensionParams.width = at (pid::fxDimension.width);
        dimensionParams.timeMs = at (pid::fxDimension.timeMs);

        delayParams.enabled = at (pid::fxDelay.enabled);
        delayParams.mix = at (pid::fxDelay.mix);
        delayParams.syncEnabled = at (pid::fxDelay.syncEnabled);
        delayParams.division = at (pid::fxDelay.division);
        delayParams.timeMs = at (pid::fxDelay.timeMs);
        delayParams.feedback = at (pid::fxDelay.feedback);
        delayParams.pingPong = at (pid::fxDelay.pingPong);
        delayParams.width = at (pid::fxDelay.width);
        delayParams.lowCut = at (pid::fxDelay.lowCut);
        delayParams.highCut = at (pid::fxDelay.highCut);
        delayParams.modRate = at (pid::fxDelay.modRate);
        delayParams.modDepth = at (pid::fxDelay.modDepth);

        reverbParams.enabled = at (pid::fxReverb.enabled);
        reverbParams.mix = at (pid::fxReverb.mix);
        reverbParams.size = at (pid::fxReverb.size);
        reverbParams.decay = at (pid::fxReverb.decay);
        reverbParams.damping = at (pid::fxReverb.damping);
        reverbParams.preDelay = at (pid::fxReverb.preDelay);
        reverbParams.width = at (pid::fxReverb.width);
        reverbParams.lowCut = at (pid::fxReverb.lowCut);
        reverbParams.highCut = at (pid::fxReverb.highCut);
        reverbParams.modDepth = at (pid::fxReverb.modDepth);

        limiterParams.enabled = at (pid::fxLimiter.enabled);
        limiterParams.mix = at (pid::fxLimiter.mix);
        limiterParams.threshold = at (pid::fxLimiter.threshold);
        limiterParams.release = at (pid::fxLimiter.release);
        limiterParams.ceiling = at (pid::fxLimiter.ceiling);
    }

    /** BLOCK-RATE. Reads every FX parameter once and pushes it into the
        effects.

        Unlike the voices, there is only one rack, so this does not need a
        shared settings struct to stop sixteen readers disagreeing - but it is
        still read ONCE per block rather than per sample, because a parameter
        that changed mid-block would mean two halves of one block running
        different settings. */
    void updateFromParameters (double bpm) noexcept
    {
        const auto read = [] (const std::atomic<float>* pointer)
        {
            return pointer != nullptr ? pointer->load() : 0.0f;
        };

        const auto readBool = [&read] (const std::atomic<float>* pointer)
        {
            return read (pointer) > 0.5f;
        };

        const auto readInt = [&read] (const std::atomic<float>* pointer)
        {
            return juce::roundToInt (read (pointer));
        };

        auto nonlinear = false;

        for (std::size_t i = 0; i < pid::kNumFxDistortions; ++i)
        {
            const auto& bound = distortionParams[i];

            FxDistortion::Settings settings;
            settings.enabled = readBool (bound.enabled);
            settings.mix = read (bound.mix);
            settings.type = static_cast<choices::FxDistortionType> (
                juce::jlimit (0, static_cast<int> (choices::FxDistortionType::count) - 1,
                              readInt (bound.type)));
            settings.driveDb = read (bound.drive);
            settings.tone = read (bound.tone);
            settings.bias = read (bound.bias);
            settings.outputDb = read (bound.output);

            distortions[i].setSettings (settings);

            // A distortion is nonlinear whatever its drive: the curve itself
            // generates harmonics at unity gain.
            nonlinear = nonlinear || settings.enabled;
        }

        for (std::size_t i = 0; i < pid::kNumFxEqs; ++i)
        {
            const auto& bound = eqParams[i];

            FxEq::Settings settings;
            settings.enabled = readBool (bound.enabled);
            settings.mix = read (bound.mix);
            settings.highPassFreq = read (bound.highPassFreq);
            settings.lowShelfFreq = read (bound.lowShelfFreq);
            settings.lowShelfGain = read (bound.lowShelfGain);
            settings.band1Freq = read (bound.band1Freq);
            settings.band1Gain = read (bound.band1Gain);
            settings.band1Q = read (bound.band1Q);
            settings.band2Freq = read (bound.band2Freq);
            settings.band2Gain = read (bound.band2Gain);
            settings.band2Q = read (bound.band2Q);
            settings.highShelfFreq = read (bound.highShelfFreq);
            settings.highShelfGain = read (bound.highShelfGain);
            settings.lowPassFreq = read (bound.lowPassFreq);

            eqs[i].setSettings (settings);
        }

        for (std::size_t i = 0; i < pid::kNumFxFilters; ++i)
        {
            const auto& bound = filterParams[i];

            FxFilter::Settings settings;
            settings.enabled = readBool (bound.enabled);
            settings.mix = read (bound.mix);
            settings.type = static_cast<choices::FxFilterType> (
                juce::jlimit (0, static_cast<int> (choices::FxFilterType::count) - 1,
                              readInt (bound.type)));
            settings.cutoffHz = read (bound.cutoff);
            settings.resonance = read (bound.resonance);
            settings.drive = read (bound.drive);

            filters[i].setSettings (settings);

            // The FX filter is only nonlinear when its DRIVE is up - the
            // filter itself is linear, so a patch using it clean does not pay
            // for oversampling.
            nonlinear = nonlinear || (settings.enabled && settings.drive > 0.0f);
        }

        {
            FxChorus::Settings settings;
            settings.enabled = readBool (chorusParams.enabled);
            settings.mix = read (chorusParams.mix);
            settings.rateHz = read (chorusParams.rate);
            settings.depth = read (chorusParams.depth);
            settings.voices = readInt (chorusParams.voices);
            settings.spread = read (chorusParams.spread);
            settings.feedback = read (chorusParams.feedback);
            chorus.setSettings (settings);
        }

        {
            FxFlanger::Settings settings;
            settings.enabled = readBool (flangerParams.enabled);
            settings.mix = read (flangerParams.mix);
            settings.rateHz = read (flangerParams.rate);
            settings.depth = read (flangerParams.depth);
            settings.feedback = read (flangerParams.feedback);
            settings.manual = read (flangerParams.manual);
            settings.stereo = read (flangerParams.stereo);
            flanger.setSettings (settings);
        }

        {
            FxPhaser::Settings settings;
            settings.enabled = readBool (phaserParams.enabled);
            settings.mix = read (phaserParams.mix);
            settings.rateHz = read (phaserParams.rate);
            settings.depth = read (phaserParams.depth);
            settings.stages = readInt (phaserParams.stages);
            settings.centreHz = read (phaserParams.centre);
            settings.feedback = read (phaserParams.feedback);
            settings.stereo = read (phaserParams.stereo);
            phaser.setSettings (settings);
        }

        {
            FxHyper::Settings settings;
            settings.enabled = readBool (hyperParams.enabled);
            settings.mix = read (hyperParams.mix);
            settings.amount = read (hyperParams.amount);
            settings.detune = read (hyperParams.detune);
            settings.voices = readInt (hyperParams.voices);
            settings.width = read (hyperParams.width);
            hyper.setSettings (settings);
        }

        {
            FxDimension::Settings settings;
            settings.enabled = readBool (dimensionParams.enabled);
            settings.mix = read (dimensionParams.mix);
            settings.amount = read (dimensionParams.amount);
            settings.width = read (dimensionParams.width);
            settings.timeMs = read (dimensionParams.timeMs);
            dimension.setSettings (settings);
        }

        {
            FxDelay::Settings settings;
            settings.enabled = readBool (delayParams.enabled);
            settings.mix = read (delayParams.mix);
            settings.syncEnabled = readBool (delayParams.syncEnabled);
            settings.division = static_cast<sync::Division> (
                juce::jlimit (0, static_cast<int> (sync::Division::count) - 1,
                              readInt (delayParams.division)));
            settings.timeMs = read (delayParams.timeMs);
            settings.feedback = read (delayParams.feedback);
            settings.pingPong = readBool (delayParams.pingPong);
            settings.width = read (delayParams.width);
            settings.lowCutHz = read (delayParams.lowCut);
            settings.highCutHz = read (delayParams.highCut);
            settings.modRateHz = read (delayParams.modRate);
            settings.modDepth = read (delayParams.modDepth);
            delay.setSettings (settings, bpm);
        }

        {
            FxReverb::Settings settings;
            settings.enabled = readBool (reverbParams.enabled);
            settings.mix = read (reverbParams.mix);
            settings.size = read (reverbParams.size);
            settings.decay = read (reverbParams.decay);
            settings.damping = read (reverbParams.damping);
            settings.preDelayMs = read (reverbParams.preDelay);
            settings.width = read (reverbParams.width);
            settings.lowCutHz = read (reverbParams.lowCut);
            settings.highCutHz = read (reverbParams.highCut);
            settings.modDepth = read (reverbParams.modDepth);
            reverb.setSettings (settings);
        }

        {
            FxLimiter::Settings settings;
            settings.enabled = readBool (limiterParams.enabled);
            settings.mix = read (limiterParams.mix);
            settings.thresholdDb = read (limiterParams.threshold);
            settings.releaseMs = read (limiterParams.release);
            settings.ceilingDb = read (limiterParams.ceiling);
            limiter.setSettings (settings);
        }

        anyNonlinearEnabled.store (nonlinear);

        /*  Then the factor, AFTER the settings rather than before them. Every
            effect's `setSampleRate` re-applies its own stored settings at the
            new rate - that is what makes a delay keep its time and an LFO its
            rate when the factor changes - so the settings written above are
            recomputed by this call rather than being left stale for the new
            rate. Doing it in the other order would leave them one block
            behind, which on a delay is an audible jump in its time the moment
            a distortion is switched on. */
        const auto wantedRate = anyNonlinearEnabled.load() ? oversampledRate
                                                           : baseSampleRate;

        if (wantedRate != currentSampleRate)
            applySampleRate (wantedRate);
    }

    /** AUDIO THREAD. Processes the block in place.

        Takes the buffer rather than a sample, because the oversampling is a
        block operation - and because this is the level at which the rack is
        one thing rather than fourteen. */
    void process (juce::dsp::AudioBlock<float> block) noexcept
    {
        if (oversampler == nullptr || block.getNumSamples() == 0)
            return;

        if (! anyNonlinearEnabled.load())
        {
            processChain (block);
            return;
        }

        auto upsampled = oversampler->processSamplesUp (block);
        processChain (upsampled);
        oversampler->processSamplesDown (block);
    }

    /** AUDIO THREAD SAFE. FxOrder is fourteen bytes and trivially copyable,
        so this is a memcpy - which is why the processor can take the published
        order and hand it straight over every block rather than needing a
        second snapshot of its own. */
    void setOrder (const FxOrder& newOrder) noexcept { order = newOrder; }

    const FxOrder& getOrder() const noexcept { return order; }

    /** The latency the rack adds, in samples at the BASE rate.

        Both contributors are conditional, so this changes as the user works -
        the processor re-reports it, and a host that cannot follow a changing
        latency is better off with a value that is right than with one that is
        constant and wrong. */
    int getLatencySamples() const noexcept
    {
        auto samples = limiter.getLatencySamples();

        if (anyNonlinearEnabled.load())
        {
            // The limiter's own look-ahead was counted at the oversampled
            // rate, so it converts back down; the oversampler's filters add
            // their own, already reported at the base rate.
            samples /= kOversamplingFactor;

            if (oversampler != nullptr)
                samples += static_cast<int> (std::round (oversampler->getLatencyInSamples()));
        }

        return samples;
    }

    bool isOversampling() const noexcept { return anyNonlinearEnabled.load(); }

    // Access for the tests and for the UI's meters.
    FxDistortion& getDistortion (std::size_t index) noexcept { return distortions[index]; }
    FxEq& getEq (std::size_t index) noexcept { return eqs[index]; }
    FxFilter& getFilter (std::size_t index) noexcept { return filters[index]; }

private:
    /** One 2x stage. See the class comment for why not two. */
    static constexpr int kOversamplingStages = 1;
    static constexpr int kOversamplingFactor = 2;

    void applySampleRate (double sampleRate) noexcept
    {
        currentSampleRate = sampleRate;

        for (auto& instance : distortions)
            instance.setSampleRate (sampleRate);

        for (auto& instance : eqs)
            instance.setSampleRate (sampleRate);

        for (auto& instance : filters)
            instance.setSampleRate (sampleRate);

        chorus.setSampleRate (sampleRate);
        flanger.setSampleRate (sampleRate);
        phaser.setSampleRate (sampleRate);
        hyper.setSampleRate (sampleRate);
        dimension.setSampleRate (sampleRate);
        delay.setSampleRate (sampleRate);
        reverb.setSampleRate (sampleRate);
        limiter.setSampleRate (sampleRate);
    }

    /** Runs the chain over a block, in the user's order.

        The loop is per SAMPLE inside per SLOT rather than the other way round,
        which is the opposite of what a cache would want - but the alternative
        is a scratch buffer per slot and fourteen passes over the block, and
        the effects' own state makes them sequential anyway. */
    void processChain (juce::dsp::AudioBlock<float>& block) noexcept
    {
        const auto numChannels = block.getNumChannels();
        const auto numSamples = block.getNumSamples();

        if (numChannels == 0)
            return;

        auto* left = block.getChannelPointer (0);
        auto* right = numChannels > 1 ? block.getChannelPointer (1) : left;

        for (std::size_t position = 0; position < FxOrder::kNumSlots; ++position)
        {
            switch (order.getSlot (position))
            {
                case Slot::distortion1: runPerChannel (distortions[0], left, right, numSamples, numChannels); break;
                case Slot::distortion2: runPerChannel (distortions[1], left, right, numSamples, numChannels); break;
                case Slot::eq1:         runPerChannel (eqs[0], left, right, numSamples, numChannels); break;
                case Slot::eq2:         runPerChannel (eqs[1], left, right, numSamples, numChannels); break;
                case Slot::filter1:     runPerChannel (filters[0], left, right, numSamples, numChannels); break;
                case Slot::filter2:     runPerChannel (filters[1], left, right, numSamples, numChannels); break;
                case Slot::chorus:      runPerChannel (chorus, left, right, numSamples, numChannels); break;
                case Slot::flanger:     runPerChannel (flanger, left, right, numSamples, numChannels); break;
                case Slot::phaser:      runPerChannel (phaser, left, right, numSamples, numChannels); break;

                case Slot::hyper:       runStereo (hyper, left, right, numSamples); break;
                case Slot::dimension:   runStereo (dimension, left, right, numSamples); break;
                case Slot::delay:       runStereo (delay, left, right, numSamples); break;
                case Slot::reverb:      runStereo (reverb, left, right, numSamples); break;
                case Slot::limiter:     runStereo (limiter, left, right, numSamples); break;

                case Slot::count:
                default:                break;
            }
        }
    }

    /** For the effects whose per-channel STATE is independent.

        THE CHANNELS ARE INTERLEAVED, SAMPLE BY SAMPLE, and that is not a
        stylistic choice - it is a correctness requirement that cost a
        measurement to find. These effects have independent per-channel state
        (one filter, one set of EQ bands per channel) but the modulated ones
        also have ONE SHARED LFO, whose phase they advance on channel 0 only,
        because advancing it per channel would make its rate depend on the
        channel count.

        Running all of the left channel and then all of the right - the
        cache-friendly order, and what this did first - advances that phase
        `numSamples` times during the left pass and then hands the right
        channel the phase left over at the END of it. So the right channel's
        modulation was frozen at a value determined entirely by the block size.

        Every component test still passed. The chorus's own tests measure
        channel 0, or measure that the two channels DIFFER - which they
        emphatically did. It took the FxDimension in the chain to make it
        visible at all, because only a mid/side effect folds the broken right
        channel back into the left, and then the rack failed block-size
        invariance by 0.002 with neither effect failing alone. That is the same
        shape as the voice filter's shared stereo state, and the same lesson:
        the only test that can see it is one that asserts a property of the
        whole (CLAUDE.md section 3).

        A MONO HOST GETS ONE CHANNEL PROCESSED ONCE - `right` aliases `left`
        when there is one channel, and advancing a per-channel filter twice in
        a sample with different inputs corrupts its state. The phase then
        advances once per sample too, which is still right. */
    template <typename Effect>
    static void runPerChannel (Effect& effect, float* left, float* right,
                               std::size_t numSamples, std::size_t numChannels) noexcept
    {
        if (numChannels > 1)
        {
            for (std::size_t i = 0; i < numSamples; ++i)
            {
                left[i] = effect.processSample (0, left[i]);
                right[i] = effect.processSample (1, right[i]);
            }

            return;
        }

        for (std::size_t i = 0; i < numSamples; ++i)
            left[i] = effect.processSample (0, left[i]);
    }

    /** For the effects that genuinely couple the two channels. */
    template <typename Effect>
    static void runStereo (Effect& effect, float* left, float* right,
                           std::size_t numSamples) noexcept
    {
        for (std::size_t i = 0; i < numSamples; ++i)
            effect.processSample (left[i], right[i]);
    }

    struct DistortionParams
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* drive = nullptr;
        std::atomic<float>* tone = nullptr;
        std::atomic<float>* bias = nullptr;
        std::atomic<float>* output = nullptr;
    };

    struct EqParams
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* highPassFreq = nullptr;
        std::atomic<float>* lowShelfFreq = nullptr;
        std::atomic<float>* lowShelfGain = nullptr;
        std::atomic<float>* band1Freq = nullptr;
        std::atomic<float>* band1Gain = nullptr;
        std::atomic<float>* band1Q = nullptr;
        std::atomic<float>* band2Freq = nullptr;
        std::atomic<float>* band2Gain = nullptr;
        std::atomic<float>* band2Q = nullptr;
        std::atomic<float>* highShelfFreq = nullptr;
        std::atomic<float>* highShelfGain = nullptr;
        std::atomic<float>* lowPassFreq = nullptr;
    };

    struct FilterParams
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* cutoff = nullptr;
        std::atomic<float>* resonance = nullptr;
        std::atomic<float>* drive = nullptr;
    };

    struct ChorusParams
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* rate = nullptr;
        std::atomic<float>* depth = nullptr;
        std::atomic<float>* voices = nullptr;
        std::atomic<float>* spread = nullptr;
        std::atomic<float>* feedback = nullptr;
    };

    struct FlangerParams
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* rate = nullptr;
        std::atomic<float>* depth = nullptr;
        std::atomic<float>* feedback = nullptr;
        std::atomic<float>* manual = nullptr;
        std::atomic<float>* stereo = nullptr;
    };

    struct PhaserParams
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* rate = nullptr;
        std::atomic<float>* depth = nullptr;
        std::atomic<float>* stages = nullptr;
        std::atomic<float>* centre = nullptr;
        std::atomic<float>* feedback = nullptr;
        std::atomic<float>* stereo = nullptr;
    };

    struct HyperParams
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* amount = nullptr;
        std::atomic<float>* detune = nullptr;
        std::atomic<float>* voices = nullptr;
        std::atomic<float>* width = nullptr;
    };

    struct DimensionParams
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* amount = nullptr;
        std::atomic<float>* width = nullptr;
        std::atomic<float>* timeMs = nullptr;
    };

    struct DelayParams
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* syncEnabled = nullptr;
        std::atomic<float>* division = nullptr;
        std::atomic<float>* timeMs = nullptr;
        std::atomic<float>* feedback = nullptr;
        std::atomic<float>* pingPong = nullptr;
        std::atomic<float>* width = nullptr;
        std::atomic<float>* lowCut = nullptr;
        std::atomic<float>* highCut = nullptr;
        std::atomic<float>* modRate = nullptr;
        std::atomic<float>* modDepth = nullptr;
    };

    struct ReverbParams
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* size = nullptr;
        std::atomic<float>* decay = nullptr;
        std::atomic<float>* damping = nullptr;
        std::atomic<float>* preDelay = nullptr;
        std::atomic<float>* width = nullptr;
        std::atomic<float>* lowCut = nullptr;
        std::atomic<float>* highCut = nullptr;
        std::atomic<float>* modDepth = nullptr;
    };

    struct LimiterParams
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* threshold = nullptr;
        std::atomic<float>* release = nullptr;
        std::atomic<float>* ceiling = nullptr;
    };

    double baseSampleRate = 44100.0;
    double oversampledRate = 88200.0;
    double currentSampleRate = 44100.0;

    /** Atomic because the processor reads it from the MESSAGE thread to
        report latency, while the audio thread writes it every block. */
    std::atomic<bool> anyNonlinearEnabled { false };

    FxOrder order;

    std::array<FxDistortion, pid::kNumFxDistortions> distortions {};
    std::array<FxEq, pid::kNumFxEqs> eqs {};
    std::array<FxFilter, pid::kNumFxFilters> filters {};

    FxChorus chorus;
    FxFlanger flanger;
    FxPhaser phaser;
    FxHyper hyper;
    FxDimension dimension;
    FxDelay delay;
    FxReverb reverb;
    FxLimiter limiter;

    std::array<DistortionParams, pid::kNumFxDistortions> distortionParams {};
    std::array<EqParams, pid::kNumFxEqs> eqParams {};
    std::array<FilterParams, pid::kNumFxFilters> filterParams {};

    ChorusParams chorusParams {};
    FlangerParams flangerParams {};
    PhaserParams phaserParams {};
    HyperParams hyperParams {};
    DimensionParams dimensionParams {};
    DelayParams delayParams {};
    ReverbParams reverbParams {};
    LimiterParams limiterParams {};

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
};

} // namespace gnarl::dsp
