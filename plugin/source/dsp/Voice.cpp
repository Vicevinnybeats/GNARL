#include "Voice.h"

#include "SmoothedParameter.h"
#include "../params/ParameterIDs.h"

namespace gnarl::dsp
{

namespace
{
    /** Below this level a releasing voice is inaudible, so it is freed for
        reuse. -80 dB, chosen because it is below the noise floor of any
        24-bit render. */
    constexpr float kSilenceThreshold = 0.0001f;

    /** Changes a smoother's ramp length WITHOUT losing where it currently is.

        juce::SmoothedValue::reset() snaps the current value to the target as a
        side effect, so the obvious `reset(); setTargetValue(x);` silently
        teleports the value first - which is exactly the discontinuity the
        smoother exists to prevent. This preserves the current value across the
        ramp change. */
    template <typename SmoothedType>
    void setRampPreservingValue (SmoothedType& smoothed, double sampleRate, double rampSeconds)
    {
        const auto current = smoothed.getCurrentValue();
        smoothed.reset (sampleRate, rampSeconds);
        smoothed.setCurrentAndTargetValue (current);
    }
}

void Voice::prepare (double maximumSampleRate, int maximumBlockSize)
{
    juce::ignoreUnused (maximumBlockSize);

    for (auto& oscillator : oscillators)
        oscillator.prepare (maximumSampleRate);

    subOscillator.prepare (maximumSampleRate);
    noise.prepare (maximumSampleRate);

    // The highest rate, because the comb filter allocates its delay line from
    // it and must not resize when the oversampling factor changes.
    for (auto& slot : filters)
        for (auto& filter : slot)
            filter.prepare (maximumSampleRate);

    for (auto& envelope : envelopes)
        envelope.prepare (maximumSampleRate);

    for (auto& lfo : lfos)
        lfo.prepare (maximumSampleRate);

    setSampleRate (maximumSampleRate);
    reset();
}

void Voice::setOversamplingRatio (float ratio) noexcept
{
    for (auto& oscillator : oscillators)
        oscillator.setOversamplingRatio (ratio);

    subOscillator.setOversamplingRatio (ratio);
}

void Voice::setSampleRate (double sampleRate) noexcept
{
    sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;

    for (auto& oscillator : oscillators)
        oscillator.setSampleRate (sampleRateHz);

    subOscillator.setSampleRate (sampleRateHz);
    noise.setSampleRate (sampleRateHz);

    for (auto& slot : filters)
        for (auto& filter : slot)
            filter.setSampleRate (sampleRateHz);

    // setSampleRate, NOT prepare: prepare() resets, and this runs while notes
    // are sounding because the oversampling factor is a live parameter.
    // Resetting here silenced every held voice the moment the user touched
    // the oversampling control.
    for (auto& envelope : envelopes)
        envelope.setSampleRate (sampleRateHz);

    for (auto& lfo : lfos)
        lfo.setSampleRate (sampleRateHz);

    // Smoothers keep their current values: this is a rate change, not a
    // reset, and it can happen while a note is sounding.
    const auto pitchValue = pitch.getCurrentValue();
    const auto fadeValue = stealFade.getCurrentValue();

    pitch.reset (sampleRateHz, 0.0);
    stealFade.reset (sampleRateHz, ramp::stealFadeSeconds);

    pitch.setCurrentAndTargetValue (pitchValue);
    stealFade.setCurrentAndTargetValue (fadeValue);
}

void Voice::setRandomSeed (juce::int64 seed)
{
    // A dedicated Random per voice, seeded deterministically, so the values
    // below are identical on every load of the same patch.
    juce::Random random (seed);

    for (auto& value : unisonRandom)
        value = random.nextFloat() * 2.0f - 1.0f;

    driftAmount = random.nextFloat() * 2.0f - 1.0f;

    // Noise gets its own stream, seeded from the same place, so stacked notes
    // are not correlated - correlated noise sums to a louder single source
    // rather than to a wider one.
    noise.setSeed (static_cast<std::uint32_t> (seed) | 1u);
}

void Voice::reset()
{
    state = State::idle;

    pitch.setCurrentAndTargetValue (static_cast<float> (currentNote));
    stealFade.setCurrentAndTargetValue (1.0f);

    queuedNote.reset();
    queuedGlideTime = 0.0f;

    for (auto& oscillator : oscillators)
        oscillator.reset();

    subOscillator.reset();
    noise.reset();

    for (auto& slot : filters)
        for (auto& filter : slot)
            filter.reset();

    for (auto& envelope : envelopes)
        envelope.reset();

    for (auto& lfo : lfos)
        lfo.reset();

    lastOffsets.clear();
    sourceValues.clear();
}

float Voice::getUnisonRandom (int unisonIndex) const noexcept
{
    const auto clamped = juce::jlimit (0, static_cast<int> (unisonRandom.size()) - 1, unisonIndex);
    return unisonRandom[static_cast<std::size_t> (clamped)];
}

float Voice::getCurrentAmplitude() const noexcept
{
    // Env 1 is the amplitude envelope, so its level IS the voice's amplitude.
    return envelopes[0].getLevel() * stealFade.getCurrentValue();
}

float Voice::getLfoValue (std::size_t index) const noexcept
{
    return lfos[juce::jmin (index, lfos.size() - 1)].getCurrentValue();
}

float Voice::getLfoPhase (std::size_t index) const noexcept
{
    return lfos[juce::jmin (index, lfos.size() - 1)].getCurrentPhase();
}

void Voice::updatePitchTarget (const NoteRequest& request, float glideTimeSeconds)
{
    const auto target = static_cast<float> (request.noteNumber);

    if (request.glideFromNote.has_value() && glideTimeSeconds > 0.0f)
    {
        pitch.reset (sampleRateHz, glideTimeSeconds);
        pitch.setCurrentAndTargetValue (static_cast<float> (*request.glideFromNote));
        pitch.setTargetValue (target);
    }
    else
    {
        pitch.reset (sampleRateHz, 0.0);
        pitch.setCurrentAndTargetValue (target);
    }
}

void Voice::startNote (const NoteRequest& request, float glideTimeSeconds)
{
    currentNote = request.noteNumber;
    currentChannel = request.channel;
    currentVelocity = juce::jlimit (0.0f, 1.0f, request.velocity);

    updatePitchTarget (request, glideTimeSeconds);

    // Env 1's own attack starts from its current level, so retriggering a
    // still-sounding voice does not produce a gap.

    // Oscillator phases are seeded from this voice's random stream, so unison
    // spread is stable for the note but differs between voices.
    for (auto& oscillator : oscillators)
        oscillator.noteOn (unisonRandom.data(),
                           static_cast<int> (unisonRandom.size()),
                           0.0f,
                           0.0f);

    subOscillator.noteOn (unisonRandom.data(),
                          static_cast<int> (unisonRandom.size()),
                          0.0f,
                          0.0f);

    for (auto& envelope : envelopes)
        envelope.noteOn (currentVelocity);

    // Trigger and envelope mode LFOs restart here; free-run ones deliberately
    // do not, which is the whole point of that mode.
    for (auto& lfo : lfos)
        lfo.noteOn();

    stealFade.setCurrentAndTargetValue (1.0f);

    state = State::active;
}

void Voice::changeNote (const NoteRequest& request, float glideTimeSeconds)
{
    // Legato: pitch moves, the envelope is left alone.
    currentNote = request.noteNumber;
    currentChannel = request.channel;

    updatePitchTarget (request, glideTimeSeconds);

    if (state == State::releasing)
    {
        for (auto& envelope : envelopes)
            envelope.noteOn (currentVelocity);

        state = State::active;
    }
}

void Voice::stopNote (bool allowTailOff)
{
    if (state == State::idle)
        return;

    if (! allowTailOff)
    {
        // The host wants the voice gone now. Still fade rather than hard-zero:
        // an instantaneous cut is an audible click.
        setRampPreservingValue (stealFade, sampleRateHz, ramp::stealFadeSeconds);
        stealFade.setTargetValue (0.0f);
        queuedNote.reset();
        state = State::stealing;
        return;
    }

    for (auto& envelope : envelopes)
        envelope.noteOff();

    state = State::releasing;
}

void Voice::stealWith (const NoteRequest& request, float glideTimeSeconds)
{
    queuedNote = request;
    queuedGlideTime = glideTimeSeconds;

    if (state == State::idle)
    {
        // Nothing to fade out.
        startNote (request, glideTimeSeconds);
        return;
    }

    setRampPreservingValue (stealFade, sampleRateHz, ramp::stealFadeSeconds);
    stealFade.setTargetValue (0.0f);

    state = State::stealing;
}

float Voice::getFrequencyHz (float pitchInNotes, float offsetSemitones) const noexcept
{
    // Drift is per-voice and constant for the note, so stacked notes are not
    // phase-identical. Applied as cents rather than Hz so it is musically even
    // across the keyboard.
    const auto note = pitchInNotes + offsetSemitones;

    return 440.0f * std::exp2 ((note - 69.0f) * (1.0f / 12.0f));
}

void Voice::addSourceToBusses (const VoiceSettings::Sends& sends,
                               VoiceScratch& scratch,
                               int numSamples) noexcept
{
    const auto addTo = [&scratch, numSamples] (juce::AudioBuffer<float>& destination,
                                               float gain)
    {
        if (gain <= 0.0f)
            return;

        for (int channel = 0; channel < 2; ++channel)
            destination.addFrom (channel, 0, scratch.source, channel, 0, numSamples, gain);
    };

    addTo (scratch.filter1, sends.toFilter1);
    addTo (scratch.filter2, sends.toFilter2);
    addTo (scratch.direct, sends.direct);
}

void Voice::renderNoise (VoiceScratch& scratch,
                         int numSamples,
                         const VoiceSettings::NoiseState& noiseState) noexcept
{
    noise.setType (noiseState.type);

    // Equal-power pan, matching the oscillators, so panning a source does not
    // dip in level at the centre.
    const auto normalised = (juce::jlimit (-1.0f, 1.0f, noiseState.pan) + 1.0f) * 0.5f;
    const auto angle = normalised * juce::MathConstants<float>::halfPi;
    const auto leftGain = std::cos (angle) * noiseState.level;
    const auto rightGain = std::sin (angle) * noiseState.level;

    auto* left = scratch.source.getWritePointer (0);
    auto* right = scratch.source.getWritePointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto sample = noise.processSample();
        left[i] += sample * leftGain;
        right[i] += sample * rightGain;
    }
}

void Voice::renderFilters (juce::AudioBuffer<float>& destination,
                           int startSample,
                           int numSamples,
                           const VoiceSettings& settings,
                           VoiceScratch& scratch) noexcept
{
    const auto routing = settings.filterRouting;
    const auto filter1Enabled = settings.filterEnabled[0];
    const auto filter2Enabled = settings.filterEnabled[1];

    // The MODULATED settings, not `settings.filters`. Reading the base values
    // here silently threw away every filter modulation - and a filter wobble
    // is the single most-used modulation in this genre, so the bug was the
    // whole feature missing rather than a detail.
    //
    // Both channels' instances get the same settings; only their STATE is
    // separate, which is the whole point.
    if (filter1Enabled)
        for (auto& channelFilter : filters[0])
            channelFilter.setSettings (modulatedFilters[0]);

    if (filter2Enabled)
        for (auto& channelFilter : filters[1])
            channelFilter.setSettings (modulatedFilters[1]);

    // SERIES means filter 2 takes filter 1's output, so a source sent only to
    // filter 2 still reaches it - the send levels choose where a source
    // ENTERS the chain, not which filters exist.
    const auto series = routing == choices::FilterRouting::series;

    for (int channel = 0; channel < 2; ++channel)
    {
        auto* bus1 = scratch.filter1.getWritePointer (channel);
        auto* bus2 = scratch.filter2.getWritePointer (channel);
        const auto* directBus = scratch.direct.getReadPointer (channel);
        auto* out = destination.getWritePointer (channel, startSample);

        // This channel's own filter instances.
        auto& slot1 = filters[0][static_cast<std::size_t> (channel)];
        auto& slot2 = filters[1][static_cast<std::size_t> (channel)];

        for (int i = 0; i < numSamples; ++i)
        {
            auto path1 = bus1[i];

            if (filter1Enabled)
                path1 = slot1.processSample (path1);

            auto path2 = bus2[i];

            if (series)
                path2 += path1;

            if (filter2Enabled)
                path2 = slot2.processSample (path2);

            // In series the first filter's output has already been folded into
            // the second, so adding it again would double it.
            const auto filtered = series ? path2 : path1 + path2;

            out[i] += filtered + directBus[i];
        }
    }
}

void Voice::advanceSilently (int numSamples)
{
    if (state == State::idle || numSamples <= 0)
        return;

    for (auto& envelope : envelopes)
        envelope.process (numSamples);

    pitch.skip (numSamples);

    if (state == State::stealing)
    {
        stealFade.skip (numSamples);

        if (! stealFade.isSmoothing())
        {
            if (queuedNote.has_value())
            {
                const auto request = *queuedNote;
                const auto glide = queuedGlideTime;
                queuedNote.reset();
                queuedGlideTime = 0.0f;

                stealFade.setCurrentAndTargetValue (1.0f);

                for (auto& envelope : envelopes)
                    envelope.kill();

                startNote (request, glide);
            }
            else
            {
                reset();
            }
        }

        return;
    }

    if (state == State::releasing && ! envelopes[0].isActive())
        reset();
}

void Voice::updateModulation (const VoiceSettings& settings, int numSamples) noexcept
{
    // Every source's value gathered once, so a slot lookup in the matrix is
    // an array index rather than a switch over twenty cases.
    sourceValues.clear();

    for (std::size_t i = 0; i < pid::kNumEnvelopes; ++i)
    {
        envelopes[i].setSettings (settings.envelopes[i]);

        const auto level = envelopes[i].process (numSamples);

        sourceValues.set (static_cast<choices::ModSource> (
            static_cast<int> (choices::ModSource::env1) + static_cast<int> (i)), level);
    }

    for (std::size_t i = 0; i < pid::kNumLfos; ++i)
    {
        lfos[i].setSettings (settings.lfos[i]);
        lfos[i].setCurve (settings.lfoCurves[i]);

        const auto value = lfos[i].process (numSamples, settings.transport);

        sourceValues.set (static_cast<choices::ModSource> (
            static_cast<int> (choices::ModSource::lfo1) + static_cast<int> (i)), value);
    }

    // Note-rate sources: constant for the life of the note, but still written
    // every chunk because that costs nothing and removes a class of
    // stale-state bug.
    sourceValues.set (choices::ModSource::velocity, currentVelocity);
    sourceValues.set (choices::ModSource::noteNumber,
                      juce::jlimit (0.0f, 1.0f, static_cast<float> (currentNote) / 127.0f));
    sourceValues.set (choices::ModSource::noteOnRandom, driftAmount * 0.5f + 0.5f);
    sourceValues.set (choices::ModSource::unisonVoiceIndex, unisonRandom[0] * 0.5f + 0.5f);

    sourceValues.set (choices::ModSource::modWheel, settings.modWheel);
    sourceValues.set (choices::ModSource::pitchBend, settings.pitchBend);
    sourceValues.set (choices::ModSource::aftertouch, settings.aftertouch);

    for (std::size_t i = 0; i < pid::kNumMacros; ++i)
        sourceValues.set (static_cast<choices::ModSource> (
            static_cast<int> (choices::ModSource::macro1) + static_cast<int> (i)),
            settings.macros[i]);

    lastOffsets.clear();
    settings.modMatrix.apply (sourceValues, lastOffsets);

    applyModulation (settings, lastOffsets,
                     modulatedOscillators, modulatedSub, modulatedNoise, modulatedFilters);
}

void Voice::renderChunk (juce::AudioBuffer<float>& buffer,
                         int startSample,
                         int numSamples,
                         const VoiceSettings& settings,
                         VoiceScratch& scratch) noexcept
{
    // Pitch at the chunk's start and end, so the oscillators interpolate
    // across it rather than stepping once per chunk.
    const auto startPitch = pitch.getCurrentValue();
    pitch.skip (numSamples);
    const auto endPitch = pitch.getCurrentValue();

    const auto driftSemitones = driftAmount * settings.analogDrift
                              * VoiceSettings::kMaxDriftCents * (1.0f / 100.0f);

    scratch.filter1.clear (0, numSamples);
    scratch.filter2.clear (0, numSamples);
    scratch.direct.clear (0, numSamples);

    // --- Oscillator 2 first ------------------------------------------------
    // Rendered before oscillator 1 because oscillator 1's FM and ring mod
    // warps read it as their modulator. The other order would make those
    // modes read the previous chunk's audio.
    const auto& osc2State = modulatedOscillators[1];
    auto modulatorReady = false;

    if (osc2State.enabled && osc2State.table != nullptr && ! osc2State.sends.isSilent())
    {
        scratch.source.clear (0, numSamples);

        auto& oscillator = oscillators[1];
        oscillator.setTable (osc2State.table);

        const auto offset = osc2State.pitchOffsetSemitones + driftSemitones;

        oscillator.render (scratch.source.getWritePointer (0),
                           scratch.source.getWritePointer (1),
                           numSamples,
                           getFrequencyHz (startPitch, offset),
                           getFrequencyHz (endPitch, offset),
                           osc2State.settings);

        auto* modulator = scratch.modulator.getWritePointer (0);
        const auto* left = scratch.source.getReadPointer (0);
        const auto* right = scratch.source.getReadPointer (1);

        for (int i = 0; i < numSamples; ++i)
            modulator[i] = 0.5f * (left[i] + right[i]);

        modulatorReady = true;

        addSourceToBusses (osc2State.sends, scratch, numSamples);
    }

    // --- Oscillator 1 ------------------------------------------------------
    const auto& osc1State = modulatedOscillators[0];

    if (osc1State.enabled && osc1State.table != nullptr && ! osc1State.sends.isSilent())
    {
        scratch.source.clear (0, numSamples);

        auto& oscillator = oscillators[0];
        oscillator.setTable (osc1State.table);

        const auto offset = osc1State.pitchOffsetSemitones - driftSemitones;

        oscillator.render (scratch.source.getWritePointer (0),
                           scratch.source.getWritePointer (1),
                           numSamples,
                           getFrequencyHz (startPitch, offset),
                           getFrequencyHz (endPitch, offset),
                           osc1State.settings,
                           modulatorReady ? scratch.modulator.getReadPointer (0) : nullptr);

        addSourceToBusses (osc1State.sends, scratch, numSamples);
    }

    // --- Sub ---------------------------------------------------------------
    if (modulatedSub.enabled && modulatedSub.table != nullptr
        && ! modulatedSub.sends.isSilent())
    {
        scratch.source.clear (0, numSamples);

        subOscillator.setTable (modulatedSub.table);

        const auto offset = modulatedSub.pitchOffsetSemitones;

        subOscillator.render (scratch.source.getWritePointer (0),
                              scratch.source.getWritePointer (1),
                              numSamples,
                              getFrequencyHz (startPitch, offset),
                              getFrequencyHz (endPitch, offset),
                              modulatedSub.settings);

        addSourceToBusses (modulatedSub.sends, scratch, numSamples);
    }

    // --- Noise -------------------------------------------------------------
    if (modulatedNoise.enabled && ! modulatedNoise.sends.isSilent())
    {
        scratch.source.clear (0, numSamples);
        renderNoise (scratch, numSamples, modulatedNoise);
        addSourceToBusses (modulatedNoise.sends, scratch, numSamples);
    }

    // --- Filters -----------------------------------------------------------
    scratch.source.clear (0, numSamples);
    renderFilters (scratch.source, 0, numSamples, settings, scratch);

    // --- Amplitude ---------------------------------------------------------
    // Env 1 scaled by the steal fade. Both are evaluated per chunk rather than
    // per sample, and the chunk is short enough (0.67 ms) that a fast attack
    // is not audibly stepped.
    const auto gain = envelopes[0].getLevel() * stealFade.getCurrentValue();
    stealFade.skip (numSamples);
    const auto endGain = envelopes[0].getLevel() * stealFade.getCurrentValue();

    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto lastSample = juce::jmax (1, numSamples - 1);

    for (int channel = 0; channel < channels; ++channel)
    {
        const auto* source = scratch.source.getReadPointer (channel);
        auto* destination = buffer.getWritePointer (channel, startSample);

        // Ramped across the chunk, so a chunk boundary is never a step in
        // level even when the envelope is moving fast.
        for (int i = 0; i < numSamples; ++i)
        {
            const auto t = static_cast<float> (i) / static_cast<float> (lastSample);
            destination[i] += source[i] * (gain + (endGain - gain) * t);
        }
    }
}

void Voice::render (juce::AudioBuffer<float>& buffer,
                    int startSample,
                    int numSamples,
                    const VoiceSettings& settings,
                    VoiceScratch& scratch)
{
    if (state == State::idle || numSamples <= 0)
        return;

    if (numSamples > scratch.getCapacity())
    {
        // The manager chunks to the scratch capacity, so this cannot normally
        // happen. Advancing rather than rendering keeps the lifecycle correct
        // instead of stalling it.
        advanceSilently (numSamples);
        return;
    }

    // MODULATION CHUNKS. Each chunk re-evaluates every envelope and LFO and
    // re-applies the matrix, which is what makes a wobble continuous rather
    // than stepped once per block.
    for (int offset = 0; offset < numSamples;)
    {
        const auto chunk = juce::jmin (kModulationChunkSamples, numSamples - offset);

        updateModulation (settings, chunk);
        renderChunk (buffer, startSample + offset, chunk, settings, scratch);

        offset += chunk;
    }

    if (state == State::stealing && ! stealFade.isSmoothing())
    {
        if (queuedNote.has_value())
        {
            const auto request = *queuedNote;
            const auto glide = queuedGlideTime;
            queuedNote.reset();
            queuedGlideTime = 0.0f;

            stealFade.setCurrentAndTargetValue (1.0f);

            for (auto& envelope : envelopes)
                envelope.kill();

            startNote (request, glide);
        }
        else
        {
            reset();
        }
    }
    else if (state == State::releasing && ! envelopes[0].isActive())
    {
        reset();
    }
}

} // namespace gnarl::dsp
