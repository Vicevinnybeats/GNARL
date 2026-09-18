#include "Voice.h"

#include "SmoothedParameter.h"
#include "../params/ParameterIDs.h"

namespace gnarl::dsp
{

namespace
{
    /** Placeholder amplitude envelope times, until Env 1 lands in Phase 3.
        Short attack so a note is immediate; release long enough that stopping
        a note is a fade rather than a cut. */
    constexpr double kPlaceholderAttackSeconds  = 0.005;
    constexpr double kPlaceholderReleaseSeconds = 0.080;

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
    for (auto& filter : filters)
        filter.prepare (maximumSampleRate);

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

    for (auto& filter : filters)
        filter.setSampleRate (sampleRateHz);

    // Smoothers keep their current values: this is a rate change, not a
    // reset, and it can happen while a note is sounding.
    const auto pitchValue = pitch.getCurrentValue();
    const auto amplitudeValue = amplitude.getCurrentValue();
    const auto fadeValue = stealFade.getCurrentValue();

    pitch.reset (sampleRateHz, 0.0);
    amplitude.reset (sampleRateHz, kPlaceholderAttackSeconds);
    stealFade.reset (sampleRateHz, ramp::stealFadeSeconds);

    pitch.setCurrentAndTargetValue (pitchValue);
    amplitude.setCurrentAndTargetValue (amplitudeValue);
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
    amplitude.setCurrentAndTargetValue (0.0f);
    stealFade.setCurrentAndTargetValue (1.0f);

    queuedNote.reset();
    queuedGlideTime = 0.0f;

    for (auto& oscillator : oscillators)
        oscillator.reset();

    subOscillator.reset();
    noise.reset();

    for (auto& filter : filters)
        filter.reset();
}

float Voice::getUnisonRandom (int unisonIndex) const noexcept
{
    const auto clamped = juce::jlimit (0, static_cast<int> (unisonRandom.size()) - 1, unisonIndex);
    return unisonRandom[static_cast<std::size_t> (clamped)];
}

float Voice::getCurrentAmplitude() const noexcept
{
    return amplitude.getCurrentValue() * currentVelocity * stealFade.getCurrentValue();
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

void Voice::beginAmplitudeAttack()
{
    // Preserves the current level, so retriggering a voice that is still
    // releasing ramps up from where it actually is rather than jumping to zero.
    setRampPreservingValue (amplitude, sampleRateHz, kPlaceholderAttackSeconds);
    amplitude.setTargetValue (1.0f);
}

void Voice::startNote (const NoteRequest& request, float glideTimeSeconds)
{
    currentNote = request.noteNumber;
    currentChannel = request.channel;
    currentVelocity = juce::jlimit (0.0f, 1.0f, request.velocity);

    updatePitchTarget (request, glideTimeSeconds);

    // Starts from wherever the envelope currently is, not from zero, so
    // retriggering a still-sounding voice does not produce a gap.
    beginAmplitudeAttack();

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
        beginAmplitudeAttack();
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

    setRampPreservingValue (amplitude, sampleRateHz, kPlaceholderReleaseSeconds);
    amplitude.setTargetValue (0.0f);

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

    if (filter1Enabled)
        filters[0].setSettings (settings.filters[0]);

    if (filter2Enabled)
        filters[1].setSettings (settings.filters[1]);

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

        auto& slot1 = filters[0];
        auto& slot2 = filters[1];

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

    amplitude.skip (numSamples);
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
                amplitude.setCurrentAndTargetValue (0.0f);
                startNote (request, glide);
            }
            else
            {
                reset();
            }
        }

        return;
    }

    if (state == State::releasing && amplitude.getCurrentValue() <= kSilenceThreshold)
        reset();
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
        // happen. Advancing rather than rendering keeps the voice's lifecycle
        // correct instead of stalling it.
        advanceSilently (numSamples);
        return;
    }

    // Pitch at the block's start and end, so the oscillators can interpolate
    // across it rather than stepping once per block.
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
    // warps read it as their modulator. Doing it the other way round would
    // make those modes read the previous block's audio.
    const auto& osc2State = settings.oscillators[1];
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

        // Mono sum, since FM takes a single modulating signal.
        auto* modulator = scratch.modulator.getWritePointer (0);
        const auto* left = scratch.source.getReadPointer (0);
        const auto* right = scratch.source.getReadPointer (1);

        for (int i = 0; i < numSamples; ++i)
            modulator[i] = 0.5f * (left[i] + right[i]);

        modulatorReady = true;

        addSourceToBusses (osc2State.sends, scratch, numSamples);
    }

    // --- Oscillator 1 ------------------------------------------------------
    const auto& osc1State = settings.oscillators[0];

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
    if (settings.sub.enabled && settings.sub.table != nullptr
        && ! settings.sub.sends.isSilent())
    {
        scratch.source.clear (0, numSamples);

        subOscillator.setTable (settings.sub.table);

        const auto offset = settings.sub.pitchOffsetSemitones;

        subOscillator.render (scratch.source.getWritePointer (0),
                              scratch.source.getWritePointer (1),
                              numSamples,
                              getFrequencyHz (startPitch, offset),
                              getFrequencyHz (endPitch, offset),
                              settings.sub.settings);

        addSourceToBusses (settings.sub.sends, scratch, numSamples);
    }

    // --- Noise -------------------------------------------------------------
    if (settings.noise.enabled && ! settings.noise.sends.isSilent())
    {
        scratch.source.clear (0, numSamples);
        renderNoise (scratch, numSamples, settings.noise);
        addSourceToBusses (settings.noise.sends, scratch, numSamples);
    }

    // --- Filters and output ------------------------------------------------
    // Rendered into the scratch direct bus, then scaled by the voice's
    // amplitude on the way out, so the envelope applies to everything
    // including the filters' own resonance tails.
    scratch.source.clear (0, numSamples);
    renderFilters (scratch.source, 0, numSamples, settings, scratch);

    const auto channels = juce::jmin (2, buffer.getNumChannels());

    for (int channel = 0; channel < channels; ++channel)
    {
        const auto* source = scratch.source.getReadPointer (channel);
        auto* destination = buffer.getWritePointer (channel, startSample);

        // The envelope and steal fade are advanced once per channel loop, so
        // read them from a copy rather than consuming them twice.
        auto envelope = amplitude;
        auto fade = stealFade;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto gain = envelope.getNextValue() * currentVelocity * fade.getNextValue();
            destination[i] += source[i] * gain;
        }
    }

    // Now advance the real smoothers once, by the block length.
    amplitude.skip (numSamples);
    stealFade.skip (numSamples);

    if (state == State::stealing && ! stealFade.isSmoothing())
    {
        if (queuedNote.has_value())
        {
            const auto request = *queuedNote;
            const auto glide = queuedGlideTime;
            queuedNote.reset();
            queuedGlideTime = 0.0f;

            stealFade.setCurrentAndTargetValue (1.0f);
            amplitude.setCurrentAndTargetValue (0.0f);
            startNote (request, glide);
        }
        else
        {
            reset();
        }
    }
    else if (state == State::releasing
             && amplitude.getCurrentValue() <= kSilenceThreshold)
    {
        reset();
    }
}

} // namespace gnarl::dsp
