#pragma once

#include "dsp/SmoothedParameter.h"
#include "dsp/VoiceManager.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace gnarl
{

/**
    GNARL's AudioProcessor.

    Phase 1 scope: the complete parameter tree, the polyphonic voice
    architecture, and MIDI handling. It renders SILENCE - the oscillators and
    filters arrive in Phase 2.

    Real-time contract for processBlock and anything it calls: no allocation,
    no locks, no logging, no file or network IO. See CLAUDE.md section 3.
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
    bool acceptsMidi() const override            { return true; }
    bool producesMidi() const override           { return false; }
    bool isMidiEffect() const override           { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // --- Programs (GNARL uses its own preset system, not host programs) ----
    int getNumPrograms() override                              { return 1; }
    int getCurrentProgram() override                           { return 0; }
    void setCurrentProgram (int) override                      {}
    const juce::String getProgramName (int) override           { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    // --- State -------------------------------------------------------------
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }
    dsp::VoiceManager& getVoiceManager() noexcept { return voiceManager; }

    /** Voice count for the UI's readout. Read from the message thread. */
    int getSoundingVoiceCount() const noexcept { return voiceManager.getSoundingVoiceCount(); }

private:
    /** Shared by the float and double processBlock overloads. */
    template <typename SampleType>
    void processInternal (juce::AudioBuffer<SampleType>&, juce::MidiBuffer&);

    /** BLOCK-RATE. Copies the voice-related parameters into the manager. */
    void updateVoiceManagerSettings() noexcept;

    /** Renders the voices into voiceMixBuffer and sums that into `output`.

        Voices always work in float, whatever precision the host asked for, and
        always into their own buffer. That is the shape the signal path needs
        from Phase 2 on: voices -> mix -> filters -> FX -> output. Doing it now
        means the engine does not have to be rewired later.

        Chunks internally, so a host handing us more samples than it declared
        in prepareToPlay cannot overrun the mix buffer - and cannot make us
        allocate on the audio thread to cope. */
    template <typename SampleType>
    void renderVoices (juce::AudioBuffer<SampleType>& output, int startSample, int numSamples);

    void handleMidiMessage (const juce::MidiMessage& message);

    juce::AudioProcessorValueTreeState apvts;

    dsp::VoiceManager voiceManager;

    /** Voice output, always float. Sized in prepareToPlay. */
    juce::AudioBuffer<float> voiceMixBuffer;

    // Cached raw pointers: looking a parameter up by string on the audio
    // thread is a hash lookup per block for no reason.
    dsp::SmoothedParameter masterGain;
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<float>* maxVoicesParam = nullptr;
    std::atomic<float>* polyModeParam = nullptr;
    std::atomic<float>* glideTimeParam = nullptr;
    std::atomic<float>* glideAlwaysParam = nullptr;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GnarlProcessor)
};

} // namespace gnarl
