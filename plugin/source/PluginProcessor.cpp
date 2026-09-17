#include "PluginProcessor.h"

#include "params/ParameterIDs.h"
#include "WebUIEditor.h"

namespace gnarl
{

namespace
{
    constexpr float kMasterGainMinDb = -60.0f;
    constexpr float kMasterGainMaxDb =  12.0f;

    /** Master gain ramps over 20 ms: long enough to never click, short enough
        that a producer riding the fader does not feel lag. */
    constexpr double kMasterGainRampSeconds = 0.02;
}

juce::AudioProcessorValueTreeState::ParameterLayout GnarlProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::masterGain, pid::kStateVersion },
        "Master",
        juce::NormalisableRange<float> { kMasterGainMinDb, kMasterGainMaxDb, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return layout;
}

GnarlProcessor::GnarlProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GNARL", createParameterLayout())
{
    masterGainParam = apvts.getRawParameterValue (pid::masterGain);
    jassert (masterGainParam != nullptr);
}

GnarlProcessor::~GnarlProcessor() = default;

void GnarlProcessor::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = maximumExpectedSamplesPerBlock;

    // Every allocation and reset belongs here, never in processBlock.
    masterGainSmoothed.reset (sampleRate, kMasterGainRampSeconds);
    masterGainSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (masterGainParam->load(), kMasterGainMinDb));
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

template <typename SampleType>
void GnarlProcessor::processInternal (juce::AudioBuffer<SampleType>& buffer,
                                      juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Phase 0: no voices yet, so start from silence rather than whatever the
    // host handed us. Phase 1 replaces this with the voice render.
    buffer.clear();

    juce::ignoreUnused (midiMessages);

    masterGainSmoothed.setTargetValue (
        juce::Decibels::decibelsToGain (masterGainParam->load(), kMasterGainMinDb));

    const auto numSamples = buffer.getNumSamples();

    if (masterGainSmoothed.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto g = static_cast<SampleType> (masterGainSmoothed.getNextValue());

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }
    else
    {
        buffer.applyGain (static_cast<SampleType> (masterGainSmoothed.getCurrentValue()));
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

    // Unknown/newer state is loaded on a best-effort basis: APVTS ignores IDs
    // it does not know, and parameters absent from the tree keep their default.
    const auto version = static_cast<int> (tree.getProperty ("stateVersion", 1));
    juce::ignoreUnused (version); // migrations land here as the format evolves

    apvts.replaceState (tree);
}

} // namespace gnarl

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new gnarl::GnarlProcessor();
}
