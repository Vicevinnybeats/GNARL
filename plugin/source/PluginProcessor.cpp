#include "PluginProcessor.h"

#include "preset/FactoryBank.h"

#include <array>

#include "params/ParameterChoices.h"
#include "params/ParameterIDs.h"
#include "params/ParameterLayout.h"
#include "params/ParameterRanges.h"
#include "WebUIEditor.h"

namespace gnarl
{

GnarlProcessor::GnarlProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GNARL", params::createParameterLayout())
{
    // A choice list that has drifted from its enum would map a parameter index
    // to a mode the engine does not have.
    jassert (choices::choiceListsAreConsistent());

    // Before the reader: the reader borrows a reference to it, and reads its
    // published snapshot every block.
    modStateBridge = std::make_unique<params::ModStateBridge> (apvts);
    fxOrderBridge = std::make_unique<params::FxOrderBridge> (apvts);

    // Cached here so the audio thread never looks an FX parameter up by
    // string: 114 of them, once per block, would be 114 hash lookups.
    fxRack.bind (apvts);

    presetManager = std::make_unique<preset::PresetManager> (*this);

    /*  THE DEFAULT STATE, CAPTURED HERE AND NOWHERE ELSE. The factory bank is
        built by copying this tree and overriding a named set of parameters,
        and it has to be the state before ANY editing: APVTS creates a node
        for a parameter missing from a tree it is given, using whatever that
        parameter currently holds, so a factory preset built from a later
        snapshot would inherit the user's edits for everything it did not
        mention - and would then sound different depending on what was loaded
        before it. The constructor is the only point at which this is
        guaranteed to be the defaults. */
    presetManager->setFactoryPresets (
        preset::FactoryBank::build (apvts, apvts.copyState()));

    tableLoader = std::make_unique<preset::TableLoader> (wavetableLibrary);

    // Publishing is the message thread's job, so the loader hands back here
    // rather than touching the pointers the audio thread reads.
    tableLoader->setCallback ([this]
    {
        if (settingsReader != nullptr)
        {
            // publish, not ensure: the loader has just built them, so there is
            // nothing left to generate - and calling the generating version
            // here would take the same lock the loader thread just released,
            // for no work.
            settingsReader->publishLoadedTables();
            settingsReader->read (voiceSettings);
        }
    });

    settingsReader = std::make_unique<params::SettingsReader> (apvts, wavetableLibrary,
                                                               *modStateBridge);

    masterGain.bind (apvts, pid::masterGain, dsp::ramp::gainSeconds);

    {
        const auto bind = [this] (const char* id) { return apvts.getRawParameterValue (id); };
        const auto& ids = pid::ott;

        ottParams.enabled       = bind (ids.enabled);
        ottParams.depth         = bind (ids.depth);
        ottParams.time          = bind (ids.time);
        ottParams.mix           = bind (ids.mix);
        ottParams.inputGain     = bind (ids.inputGain);
        ottParams.outputGain    = bind (ids.outputGain);
        ottParams.crossoverLow  = bind (ids.crossoverLow);
        ottParams.crossoverHigh = bind (ids.crossoverHigh);
        ottParams.lowGain       = bind (ids.lowGain);
        ottParams.midGain       = bind (ids.midGain);
        ottParams.highGain      = bind (ids.highGain);
        ottParams.lowUpward     = bind (ids.lowUpward);
        ottParams.midUpward     = bind (ids.midUpward);
        ottParams.highUpward    = bind (ids.highUpward);
        ottParams.lowDownward   = bind (ids.lowDownward);
        ottParams.midDownward   = bind (ids.midDownward);
        ottParams.highDownward  = bind (ids.highDownward);
    }

    bypassParam       = apvts.getRawParameterValue (pid::bypass);
    oversamplingParam = apvts.getRawParameterValue (pid::oversampling);
    maxVoicesParam   = apvts.getRawParameterValue (pid::maxVoices);
    polyModeParam    = apvts.getRawParameterValue (pid::polyMode);
    glideTimeParam   = apvts.getRawParameterValue (pid::glideTime);
    glideAlwaysParam = apvts.getRawParameterValue (pid::glideAlways);

    jassert (bypassParam != nullptr && oversamplingParam != nullptr
          && maxVoicesParam != nullptr
          && polyModeParam != nullptr && glideTimeParam != nullptr
          && glideAlwaysParam != nullptr);
}

GnarlProcessor::~GnarlProcessor()
{
    /*  THE LOADER GOES FIRST, EXPLICITLY. Members are destroyed in reverse
        declaration order, and the table loader is declared BEFORE the settings
        reader - so the default order would tear the reader down while a
        generation pass was still running a callback against it. Resetting the
        loader here joins its thread and cancels its pending update before
        anything it touches goes away.

        This is the kind of ordering that works by accident until somebody
        reorders two member declarations for tidiness. */
    tableLoader.reset();
}

void GnarlProcessor::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = maximumExpectedSamplesPerBlock;

    // Every allocation and reset belongs here, never in processBlock.
    masterGain.prepare (sampleRate);
    masterGain.snapToTarget();

    // Stereo, because voices are panned and unison is spread; the output bus
    // may be mono, and renderVoices folds down when it is.
    voiceMixBuffer.setSize (2, juce::jmax (1, maximumExpectedSamplesPerBlock), false, true, true);
    voiceMixBuffer.clear();

    fxScratchBuffer.setSize (2, juce::jmax (1, maximumExpectedSamplesPerBlock), false, true, true);
    fxScratchBuffer.clear();

    oversampler.prepare (2, maximumExpectedSamplesPerBlock);

    // At the base rate: OTT runs after downsampling, on the voice mix. There
    // is nothing nonlinear enough in it to need oversampling, and running it
    // at 4x would triple the cost of the busiest filter set in the plugin.
    ott.prepare (sampleRate, 2);

    // Prepared at the HIGHEST rate the voices can run at, so switching the
    // oversampling factor later never allocates. The working rate is set
    // separately, per block.
    ott.reset();

    // The rack sizes its own buffers for its OVERSAMPLED rate internally, so
    // enabling a distortion mid-session never allocates.
    fxRack.prepare (sampleRate, maximumExpectedSamplesPerBlock);

    voiceManager.prepare (sampleRate * dsp::VoiceOversampler::kMaxRatio,
                          maximumExpectedSamplesPerBlock
                              * dsp::VoiceOversampler::kMaxRatio);

    /*  Meter ballistics, as a per-sample rate; updateOutputMeter raises it
        to the block's actual length.

        prepareToPlay can be called mid-session, so the peaks are reset here
        too - a rate change must not leave a needle parked. */
    {
        // dB per second into nepers per sample. 8.6859 dB is one neper of
        // amplitude (20 / ln 10).
        constexpr auto kDbPerNeper = 8.685889638f;

        meterDecayPerSample = (kMeterReleaseDbPerSecond / kDbPerNeper)
                            / static_cast<float> (juce::jmax (1.0, sampleRate));
    }

    for (auto& peak : outputPeak)
        peak.store (0.0f, std::memory_order_relaxed);

    // Force the latency to be recomputed and reported.
    activeOversamplingFactor = dsp::VoiceOversampler::Factor::count;
    updateOversampling();
    updateVoiceManagerSettings();

    // Generating a wavetable takes tens of milliseconds, so it happens HERE,
    // on the message thread, and never in processBlock.
    settingsReader->ensureTablesLoaded();
    settingsReader->read (voiceSettings);
}

void GnarlProcessor::releaseResources()
{
}

bool GnarlProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Synth: no inputs, stereo (or mono) out.
    if (layouts.getMainInputChannels() != 0)
        return false;

    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        || out == juce::AudioChannelSet::mono();
}

void GnarlProcessor::updateVoiceManagerSettings() noexcept
{
    voiceManager.setVoiceLimit (static_cast<int> (maxVoicesParam->load()));

    const auto modeIndex = juce::jlimit (0,
        static_cast<int> (choices::PolyMode::count) - 1,
        static_cast<int> (polyModeParam->load()));

    voiceManager.setPolyMode (static_cast<choices::PolyMode> (modeIndex));
    voiceManager.setGlideTime (glideTimeParam->load());
    voiceManager.setGlideAlways (glideAlwaysParam->load() > 0.5f);
}

void GnarlProcessor::updateOversampling()
{
    const auto index = juce::jlimit (0,
        static_cast<int> (choices::Oversampling::count) - 1,
        static_cast<int> (oversamplingParam->load()));

    const auto factor = static_cast<dsp::VoiceOversampler::Factor> (index);

    if (factor == activeOversamplingFactor)
        return;

    activeOversamplingFactor = factor;
    oversampler.setFactor (factor);
    oversampler.reset();

    const auto ratio = 1 << juce::jmax (0, dsp::VoiceOversampler::getStagesForFactor (factor));
    voiceManager.setSampleRate (currentSampleRate * ratio);

    // Oversampling must give the NONLINEAR stages headroom without also
    // making the oscillators brighter: the extra bandwidth is inaudible after
    // downsampling but intermodulates in the drive stage and folds back.
    voiceManager.setOversamplingRatio (static_cast<float> (ratio));

    updateReportedLatency();
}

void GnarlProcessor::updateReportedLatency()
{
    // BOTH contributors, because the FX rack's is conditional: its limiter's
    // look-ahead and its own 2x oversampling both come and go with the patch.
    // Reporting only the voice oversampler's would put the instrument early
    // against the session by however much the rack was adding.
    const auto total = juce::roundToInt (oversampler.getLatencySamples())
                     + fxRack.getLatencySamples();

    if (total == reportedLatencySamples)
        return;

    reportedLatencySamples = total;
    setLatencySamples (total);
}

void GnarlProcessor::updateOtt() noexcept
{
    const auto read = [] (const std::atomic<float>* parameter)
    {
        return parameter != nullptr ? parameter->load() : 0.0f;
    };

    dsp::OttCompressor::Settings settings;

    settings.enabled = read (ottParams.enabled) > 0.5f;
    settings.depth = read (ottParams.depth);
    settings.time = read (ottParams.time);
    settings.mix = read (ottParams.mix);
    settings.inputGainDb = read (ottParams.inputGain);
    settings.outputGainDb = read (ottParams.outputGain);
    settings.crossoverLowHz = read (ottParams.crossoverLow);
    settings.crossoverHighHz = read (ottParams.crossoverHigh);

    settings.bandGainDb = { read (ottParams.lowGain),
                            read (ottParams.midGain),
                            read (ottParams.highGain) };

    settings.upwardAmount = { read (ottParams.lowUpward),
                              read (ottParams.midUpward),
                              read (ottParams.highUpward) };

    settings.downwardAmount = { read (ottParams.lowDownward),
                                read (ottParams.midDownward),
                                read (ottParams.highDownward) };

    ott.setSettings (settings);
}

GnarlProcessor::ModulationSnapshot GnarlProcessor::getModulationSnapshot() const noexcept
{
    ModulationSnapshot snapshot;

    // The base values, so the UI has something meaningful to draw with
    // nothing playing: the knob positions, unmodulated.
    for (std::size_t i = 0; i < pid::kNumOscillators; ++i)
        snapshot.tablePositions[i] = voiceSettings.oscillators[i].settings.tablePosition;

    for (std::size_t i = 0; i < pid::kNumFilters; ++i)
        snapshot.filterCutoffHz[i] = voiceSettings.filters[i].cutoffHz;

    // The newest sounding voice, not an average across voices. Averaging the
    // modulation of a held chord would draw a shape no single note is
    // following, which is worse than showing one note honestly.
    const dsp::Voice* newest = nullptr;

    for (std::size_t i = 0; i < dsp::VoiceManager::getNumVoices(); ++i)
    {
        const auto& voice = voiceManager.getVoice (i);

        if (voice.isIdle())
            continue;

        if (newest == nullptr || voice.getStartOrder() > newest->getStartOrder())
            newest = &voice;
    }

    if (newest == nullptr)
        return snapshot;

    snapshot.hasVoice = true;

    for (std::size_t i = 0; i < pid::kNumLfos; ++i)
    {
        snapshot.lfoValues[i] = newest->getLfoValue (i);
        snapshot.lfoPhases[i] = newest->getLfoPhase (i);
    }

    // Applied the same way the engine applies them, so what the UI draws is
    // what the oscillator reads - a display that computed its own version of
    // the modulation would drift from the sound.
    dsp::ModulationOffsets offsets;

    for (std::size_t i = 0; i < static_cast<std::size_t> (dsp::ModDestination::count); ++i)
        offsets.values[i] = newest->getModulationOffset (static_cast<dsp::ModDestination> (i));

    dsp::VoiceSettings::OscillatorState oscillators[pid::kNumOscillators] {};
    dsp::VoiceSettings::SubState sub {};
    dsp::VoiceSettings::NoiseState noise {};
    dsp::FilterSlot::Settings filters[pid::kNumFilters] {};

    dsp::applyModulation (voiceSettings, offsets, oscillators, sub, noise, filters);

    for (std::size_t i = 0; i < pid::kNumOscillators; ++i)
        snapshot.tablePositions[i] = oscillators[i].settings.tablePosition;

    for (std::size_t i = 0; i < pid::kNumFilters; ++i)
        snapshot.filterCutoffHz[i] = filters[i].cutoffHz;

    return snapshot;
}

void GnarlProcessor::updateTransport() noexcept
{
    auto& transport = voiceSettings.transport;

    // No playhead at all is a valid answer - an offline render, or a host that
    // does not provide one. Free-run LFOs then fall back to accumulating from
    // the note, which is the only sensible thing without a timeline.
    if (auto* host = getPlayHead())
    {
        if (const auto position = host->getPosition())
        {
            transport.bpm = position->getBpm().orFallback (transport.bpm);
            transport.ppqPosition = position->getPpqPosition().orFallback (0.0);
            transport.isPlaying = position->getIsPlaying();

            return;
        }
    }

    transport.isPlaying = false;
}

void GnarlProcessor::handleMidiMessage (const juce::MidiMessage& message)
{
    if (message.isController() && message.getControllerNumber() == 1)
    {
        modWheelValue = static_cast<float> (message.getControllerValue()) / 127.0f;
        return;
    }

    if (message.isPitchWheel())
    {
        // 0..16383 with 8192 at centre; the engine wants 0..1 with 0.5 at
        // centre, so a mod slot can use it like any other unipolar source.
        pitchBendValue = static_cast<float> (message.getPitchWheelValue())
                       / 16383.0f;
        return;
    }

    if (message.isChannelPressure())
    {
        aftertouchValue = static_cast<float> (message.getChannelPressureValue()) / 127.0f;
        return;
    }

    if (message.isAftertouch())
    {
        // Polyphonic aftertouch collapsed to one global value. Per-voice
        // aftertouch needs the voice manager to route by note, which is not
        // worth the routing until something asks for it - and a controller
        // that sends poly pressure should still do something.
        aftertouchValue = static_cast<float> (message.getAfterTouchValue()) / 127.0f;
        return;
    }

    if (message.isNoteOn())
    {
        voiceManager.noteOn (message.getNoteNumber(),
                             message.getFloatVelocity(),
                             message.getChannel());
    }
    else if (message.isNoteOff())
    {
        voiceManager.noteOff (message.getNoteNumber(), message.getChannel(), true);
    }
    else if (message.isAllNotesOff())
    {
        voiceManager.allNotesOff (true);
    }
    else if (message.isAllSoundOff())
    {
        // "All sound off" means now, not "when the tails finish".
        voiceManager.allNotesOff (false);
    }
}

template <typename SampleType>
void GnarlProcessor::renderVoices (juce::AudioBuffer<SampleType>& output,
                                   int startSample,
                                   int numSamples)
{
    const auto capacity = voiceMixBuffer.getNumSamples();

    if (capacity <= 0 || numSamples <= 0)
        return;

    const auto outputChannels = output.getNumChannels();

    if (outputChannels <= 0)
        return;

    for (int offset = 0; offset < numSamples;)
    {
        const auto chunk = juce::jmin (capacity, numSamples - offset);

        voiceMixBuffer.clear (0, chunk);

        // The voices render INSIDE the oversampler, at the higher rate. A
        // nonlinear stage aliases the moment it runs, and no filter applied
        // afterwards can remove those partials - they are already inside the
        // audible band.
        oversampler.process (voiceMixBuffer, chunk, currentSampleRate,
            [this] (juce::dsp::AudioBlock<float> block, double)
            {
                const auto blockChannels = static_cast<int> (block.getNumChannels());
                const auto blockSamples = static_cast<int> (block.getNumSamples());

                if (blockChannels <= 0 || blockSamples <= 0)
                    return;

                // A non-owning AudioBuffer view over the block, so the voices
                // keep their existing buffer-based interface. On the stack:
                // this runs on the audio thread.
                std::array<float*, 2> channels {};

                for (int channel = 0; channel < 2; ++channel)
                    channels[static_cast<std::size_t> (channel)] =
                        block.getChannelPointer (
                            static_cast<std::size_t> (juce::jmin (channel, blockChannels - 1)));

                juce::AudioBuffer<float> view (channels.data(), 2, blockSamples);

                voiceManager.render (view, 0, blockSamples, voiceSettings);
            });

        if (outputChannels == 1)
        {
            // Mono bus: fold the voices' stereo image down rather than
            // dropping the right channel, which would silence anything panned
            // hard right.
            const auto* left = voiceMixBuffer.getReadPointer (0);
            const auto* right = voiceMixBuffer.getReadPointer (1);
            auto* destination = output.getWritePointer (0, startSample + offset);

            for (int i = 0; i < chunk; ++i)
                destination[i] += static_cast<SampleType> (0.5f * (left[i] + right[i]));
        }
        else
        {
            const auto channels = juce::jmin (outputChannels, voiceMixBuffer.getNumChannels());

            for (int ch = 0; ch < channels; ++ch)
            {
                const auto* source = voiceMixBuffer.getReadPointer (ch);
                auto* destination = output.getWritePointer (ch, startSample + offset);

                for (int i = 0; i < chunk; ++i)
                    destination[i] += static_cast<SampleType> (source[i]);
            }
        }

        offset += chunk;
    }
}

template <typename SampleType>
void GnarlProcessor::runFxRack (juce::AudioBuffer<SampleType>& buffer, int numSamples)
{
    const auto numChannels = juce::jmin (buffer.getNumChannels(),
                                         fxScratchBuffer.getNumChannels());

    if (numChannels <= 0 || numSamples <= 0)
        return;

    if constexpr (std::is_same_v<SampleType, float>)
    {
        // The common case: the rack works in place on the host's own buffer.
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(),
                                            static_cast<std::size_t> (numChannels),
                                            0,
                                            static_cast<std::size_t> (numSamples));
        fxRack.process (block);
    }
    else
    {
        // A host handing over MORE samples than it declared in prepareToPlay
        // would overrun the scratch buffer, and growing it here would allocate
        // on the audio thread. renderVoices chunks for the same reason.
        const auto chunk = juce::jmin (numSamples, fxScratchBuffer.getNumSamples());

        for (auto offset = 0; offset < numSamples; offset += chunk)
        {
            const auto span = juce::jmin (chunk, numSamples - offset);

            for (int channel = 0; channel < numChannels; ++channel)
            {
                const auto* source = buffer.getReadPointer (channel) + offset;
                auto* destination = fxScratchBuffer.getWritePointer (channel);

                for (int i = 0; i < span; ++i)
                    destination[i] = static_cast<float> (source[i]);
            }

            juce::dsp::AudioBlock<float> block (fxScratchBuffer.getArrayOfWritePointers(),
                                                static_cast<std::size_t> (numChannels),
                                                0,
                                                static_cast<std::size_t> (span));
            fxRack.process (block);

            for (int channel = 0; channel < numChannels; ++channel)
            {
                const auto* source = fxScratchBuffer.getReadPointer (channel);
                auto* destination = buffer.getWritePointer (channel) + offset;

                for (int i = 0; i < span; ++i)
                    destination[i] = static_cast<SampleType> (source[i]);
            }
        }
    }
}

template <typename SampleType>
void GnarlProcessor::processInternal (juce::AudioBuffer<SampleType>& buffer,
                                      juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // A synth owns its output buffer: never pass on whatever the host left in
    // it.
    buffer.clear();

    updateVoiceManagerSettings();
    updateOversampling();
    updateOtt();

    // Read once per block, then shared by every voice. Reading per voice would
    // repeat the work sixteen times and could see a parameter change mid-loop,
    // so two notes of one chord would be filtered differently.
    settingsReader->read (voiceSettings);

    // The host transport and the MIDI controllers are not in the parameter
    // tree, so they are applied after the read rather than by it.
    updateTransport();

    voiceSettings.modWheel = modWheelValue;
    voiceSettings.pitchBend = pitchBendValue;
    voiceSettings.aftertouch = aftertouchValue;

    const auto numSamples = buffer.getNumSamples();

    // Note events are dispatched at their exact sample offsets, so the engine
    // is sample-accurate rather than quantised to block boundaries.
    int lastEventSample = 0;

    for (const auto metadata : midiMessages)
    {
        const auto eventSample = juce::jlimit (0, numSamples, metadata.samplePosition);

        if (const auto span = eventSample - lastEventSample; span > 0)
        {
            renderVoices (buffer, lastEventSample, span);
            lastEventSample = eventSample;
        }

        handleMidiMessage (metadata.getMessage());
    }

    if (const auto span = numSamples - lastEventSample; span > 0)
        renderVoices (buffer, lastEventSample, span);

    if (bypassParam->load() > 0.5f)
    {
        buffer.clear();
        masterGain.snapToTarget();

        // The meter still gets its block. Returning early without it would
        // freeze the needle at whatever it read the instant bypass was
        // pressed, and leave it there - a meter showing signal on a silent
        // plugin, for as long as the window stays open.
        updateOutputMeter (buffer, numSamples);
        return;
    }

    // OTT sits between the voice mix and the master gain, so riding the master
    // fader does not change how hard it compresses.
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < numSamples; ++i)
            data[i] = static_cast<SampleType> (
                ott.processSample (channel, static_cast<float> (data[i])));
    }

    /*  Then the FX rack: after the OTT, before the master gain. The OTT is
        part of the instrument's voice rather than an effect the user placed in
        the chain, and the master fader has to be last so that riding it does
        not change how anything upstream behaves.

        The order comes from the bridge rather than being held by the rack,
        because it is ValueTree state that the UI writes - see
        docs/fx-architecture.md. */
    fxRack.setOrder (fxOrderBridge->getOrder());
    fxRack.updateFromParameters (voiceSettings.transport.bpm);
    runFxRack (buffer, numSamples);
    updateReportedLatency();

    masterGain.updateTarget();

    if (masterGain.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto db = masterGain.getNextValue();
            const auto gain = static_cast<SampleType> (
                juce::Decibels::decibelsToGain (db, ranges::kMasterGainMinDb));

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.getWritePointer (ch)[i] *= gain;
        }
    }
    else
    {
        const auto gain = juce::Decibels::decibelsToGain (
            masterGain.getCurrentValue(), ranges::kMasterGainMinDb);

        buffer.applyGain (static_cast<SampleType> (gain));
    }

    // AFTER the master gain, so the meter reads what actually leaves the
    // plugin. A meter taken before the fader tells the user about a signal
    // nobody can hear.
    updateOutputMeter (buffer, numSamples);
}

template <typename SampleType>
void GnarlProcessor::updateOutputMeter (const juce::AudioBuffer<SampleType>& buffer,
                                        int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    const auto channels = buffer.getNumChannels();

    /*  Over this block's ACTUAL length. An exponential composes exactly -
        exp(-a)exp(-b) = exp(-(a+b)) - so the reading after a given number of
        seconds is the same however the host chose to chunk them, which is
        the whole property being bought here. */
    const auto decay = std::exp (-meterDecayPerSample * static_cast<float> (numSamples));

    for (std::size_t channel = 0; channel < outputPeak.size(); ++channel)
    {
        auto blockPeak = 0.0f;

        // A mono host gets the one channel on both meters rather than a dead
        // right needle.
        const auto source = juce::jmin (static_cast<int> (channel), channels - 1);

        if (source >= 0)
        {
            const auto* data = buffer.getReadPointer (source);

            for (int i = 0; i < numSamples; ++i)
                blockPeak = juce::jmax (blockPeak,
                                        std::abs (static_cast<float> (data[i])));
        }

        // relaxed: nothing else is published alongside this, so there is no
        // ordering for an acquire/release pair to establish. The meter is one
        // independent float per channel.
        const auto decayed = outputPeak[channel].load (std::memory_order_relaxed)
                           * decay;

        auto held = juce::jmax (blockPeak, decayed);

        if (held < kMeterSilence)
            held = 0.0f;

        outputPeak[channel].store (held, std::memory_order_relaxed);
    }
}

GnarlProcessor::MeterSnapshot GnarlProcessor::getMeterSnapshot() const noexcept
{
    MeterSnapshot snapshot;

    for (std::size_t channel = 0; channel < outputPeak.size(); ++channel)
        snapshot.outputDb[channel] = juce::Decibels::gainToDecibels (
            outputPeak[channel].load (std::memory_order_relaxed), kMeterFloorDb);

    for (int band = 0; band < dsp::OttCompressor::kNumBands; ++band)
        snapshot.ottGainDb[static_cast<std::size_t> (band)] = ott.getBandGainDb (band);

    return snapshot;
}

void GnarlProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    processInternal (buffer, midi);
}

void GnarlProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi)
{
    processInternal (buffer, midi);
}

juce::AudioProcessorEditor* GnarlProcessor::createEditor()
{
    return new WebUIEditor (*this);
}

void GnarlProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", pid::kStateVersion, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void GnarlProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);

    // Unknown or newer state loads on a best-effort basis: APVTS ignores IDs
    // it does not know, and parameters absent from the tree keep their
    // default.
    const auto version = static_cast<int> (tree.getProperty ("stateVersion", 1));
    juce::ignoreUnused (version); // migrations land here as the format evolves

    // One path for both. A preset and a session must recall identically, and
    // two functions that both "load a patch" are two that will one day
    // disagree - which the customer meets as a preset that sounds right from
    // the browser and wrong after reopening the project.
    applyPresetState (tree);
}

void GnarlProcessor::applyPresetState (const juce::ValueTree& state)
{
    if (! state.isValid())
        return;

    apvts.replaceState (state);

    // Skip the ramps: a preset load should be a change, not a glide from the
    // old patch's values to the new ones.
    masterGain.snapToTarget();

    // The MODSTATE and FXORDER branches came in with the tree, so the curves,
    // the slot destinations and the chain order all have to be re-read and
    // republished before the next block. Each bridge's own listener covers an
    // ordinary edit; calling them here makes the ordering explicit for a whole
    // tree arriving at once rather than incidental.
    if (modStateBridge != nullptr)
        modStateBridge->refresh();

    if (fxOrderBridge != nullptr)
        fxOrderBridge->refresh();

    if (settingsReader == nullptr)
        return;

    /*  THE TABLES ARE GENERATED OFF THIS THREAD. Building one costs tens of
        milliseconds, so a preset changing both oscillators' tables would
        freeze the window for over a tenth of a second - on every click
        through a browser, which is how people audition. The parameters are
        applied now and the tables follow; until one is ready the audio thread
        keeps playing the table it already had, so the patch arrives in two
        steps instead of arriving late.

        ensureTablesLoaded still runs, and is what publishes the pointers: for
        a table that is already built it is the whole job, and for one that is
        not it publishes as soon as the loader says it exists. */
    if (tableLoader != nullptr)
    {
        // Whatever is already built is published immediately, so a patch that
        // reuses a table the user has already heard is instant; the rest
        // follows from the loader's thread.
        settingsReader->publishLoadedTables();

        const auto indices = settingsReader->getSelectedTableIndices();

        tableLoader->request ({ indices.begin(), indices.end() });
    }
    else
    {
        settingsReader->ensureTablesLoaded();
    }

    settingsReader->read (voiceSettings);
}

bool GnarlProcessor::isLoadingTables() const noexcept
{
    return tableLoader != nullptr && tableLoader->isWorking();
}

} // namespace gnarl

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new gnarl::GnarlProcessor();
}
