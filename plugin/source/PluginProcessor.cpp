#include "PluginProcessor.h"

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

    masterGain.bind (apvts, pid::masterGain, dsp::ramp::gainSeconds);

    bypassParam      = apvts.getRawParameterValue (pid::bypass);
    maxVoicesParam   = apvts.getRawParameterValue (pid::maxVoices);
    polyModeParam    = apvts.getRawParameterValue (pid::polyMode);
    glideTimeParam   = apvts.getRawParameterValue (pid::glideTime);
    glideAlwaysParam = apvts.getRawParameterValue (pid::glideAlways);

    jassert (bypassParam != nullptr && maxVoicesParam != nullptr
          && polyModeParam != nullptr && glideTimeParam != nullptr
          && glideAlwaysParam != nullptr);
}

GnarlProcessor::~GnarlProcessor() = default;

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

    voiceManager.prepare (sampleRate, maximumExpectedSamplesPerBlock);
    updateVoiceManagerSettings();
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

void GnarlProcessor::handleMidiMessage (const juce::MidiMessage& message)
{
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
        voiceManager.render (voiceMixBuffer, 0, chunk);

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
void GnarlProcessor::processInternal (juce::AudioBuffer<SampleType>& buffer,
                                      juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // A synth owns its output buffer: never pass on whatever the host left in
    // it.
    buffer.clear();

    updateVoiceManagerSettings();

    const auto numSamples = buffer.getNumSamples();

    // Voices render into a float buffer regardless of the host's sample type.
    // Phase 1 renders silence, so the only thing that matters here is that
    // note events are dispatched at their correct sample offsets: getting the
    // timing right now means the engine in Phase 2 is already sample-accurate.
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
        return;
    }

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

    apvts.replaceState (tree);

    // Skip the ramps: a preset load should be a change, not a glide from the
    // old patch's values to the new ones.
    masterGain.snapToTarget();
}

} // namespace gnarl

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new gnarl::GnarlProcessor();
}
