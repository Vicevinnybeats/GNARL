#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace gnarl
{

/**
    GNARL's AudioProcessor.

    Phase 0 scope: a valid, host-loadable synth that produces silence and owns
    the parameter tree. The voice architecture arrives in Phase 1 and the sound
    engine in Phase 2.

    Real-time contract for processBlock and anything it calls: no allocation,
    no locks, no logging, no file or network IO. See CLAUDE.md.
*/
class GnarlProcessor final : public juce::AudioProcessor
{
public:
    GnarlProcessor();
    ~GnarlProcessor() override;

    // --- Lifecycle ---------------------------------------------------------
    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    // --- Audio -------------------------------------------------------------
    void processBlock (juce::AudioBuffer<float>&,  juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    // --- Editor ------------------------------------------------------------
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // --- Identity ----------------------------------------------------------
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override           { return true; }
    bool producesMidi() const override          { return false; }
    bool isMidiEffect() const override           { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // --- Programs (GNARL uses its own preset system, not host programs) ----
    int getNumPrograms() override                            { return 1; }
    int getCurrentProgram() override                         { return 0; }
    void setCurrentProgram (int) override                    {}
    const juce::String getProgramName (int) override         { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    // --- State -------------------------------------------------------------
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /** Shared by the float and double processBlock overloads. */
    template <typename SampleType>
    void processInternal (juce::AudioBuffer<SampleType>&, juce::MidiBuffer&);

    juce::AudioProcessorValueTreeState apvts;

    // Cached raw pointers: looking a parameter up by string on the audio thread
    // is a hash lookup per block for no reason.
    std::atomic<float>* masterGainParam = nullptr;

    juce::LinearSmoothedValue<float> masterGainSmoothed { 1.0f };

    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GnarlProcessor)
};

} // namespace gnarl
