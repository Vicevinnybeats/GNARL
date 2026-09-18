#pragma once

#include "dsp/SmoothedParameter.h"
#include "dsp/VoiceManager.h"
#include "dsp/OttCompressor.h"
#include "dsp/VoiceOversampler.h"
#include "dsp/VoiceSettings.h"
#include "dsp/WavetableLibrary.h"
#include "params/ModStateBridge.h"
#include "params/SettingsReader.h"

#include <array>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

namespace gnarl
{

/**
    GNARL's AudioProcessor.

    Signal path: voices (oscillators -> sends -> filters) -> mix -> master.
    The FX chain lands in Phase 4 between the mix and the master.

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
    dsp::WavetableLibrary& getWavetableLibrary() noexcept { return wavetableLibrary; }

    /** The LFO curves and mod destinations, which are ValueTree state rather
        than host parameters. The editor reads and writes through this. */
    params::ModStateBridge& getModState() noexcept { return *modStateBridge; }

    /** Voice count for the UI's readout. Read from the message thread. */
    int getSoundingVoiceCount() const noexcept { return voiceManager.getSoundingVoiceCount(); }

    /** Live modulation state for the UI, so the display can show what the
        engine is doing rather than what the user set.

        Read from the message thread while the audio thread writes, without a
        lock: the worst case is one frame of a slightly stale meter, and a lock
        in the render path to avoid that would be a bad trade. */
    struct ModulationSnapshot
    {
        std::array<float, pid::kNumLfos> lfoValues {};
        std::array<float, pid::kNumLfos> lfoPhases {};

        /** Post-modulation table position per oscillator, 0..1 - what the
            wavetable display draws so it moves on its own. */
        std::array<float, pid::kNumOscillators> tablePositions {};

        /** Post-modulation cutoff per filter, in Hz. */
        std::array<float, pid::kNumFilters> filterCutoffHz {};

        /** True when at least one voice is sounding. With nothing playing
            there is no per-voice LFO to read, and the UI should idle rather
            than freeze on the last note's values. */
        bool hasVoice = false;
    };

    /** MESSAGE THREAD. */
    ModulationSnapshot getModulationSnapshot() const noexcept;

private:
    /** Shared by the float and double processBlock overloads. */
    template <typename SampleType>
    void processInternal (juce::AudioBuffer<SampleType>&, juce::MidiBuffer&);

    /** BLOCK-RATE. Copies the voice-related parameters into the manager. */
    void updateVoiceManagerSettings() noexcept;

    /** BLOCK-RATE. Applies the oversampling parameter, switching the voices'
        working rate and reporting the new latency if it changed. */
    void updateOversampling();

    /** BLOCK-RATE. */
    void updateOtt() noexcept;

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

    /** BLOCK-RATE. Copies the host's tempo and position into the settings, so
        free-run LFOs are locked to the timeline. */
    void updateTransport() noexcept;

    juce::AudioProcessorValueTreeState apvts;

    /** Owned here, on the message thread's side of the fence. The audio thread
        only ever sees borrowed const pointers to already-generated tables. */
    dsp::WavetableLibrary wavetableLibrary;

    /** Constructed after apvts, because it listens to its tree. Declared
        before the reader, which borrows a reference to it. */
    std::unique_ptr<params::ModStateBridge> modStateBridge;

    /** Constructed after apvts, because it caches raw parameter pointers. */
    std::unique_ptr<params::SettingsReader> settingsReader;

    /** Filled once per block, shared by every voice. */
    dsp::VoiceSettings voiceSettings;

    dsp::VoiceManager voiceManager;

    /** Wraps the whole voice section, because a nonlinear stage has to RUN at
        the higher rate - aliasing it creates cannot be filtered out
        afterwards. */
    dsp::VoiceOversampler oversampler;

    /** Sits between the voice mix and the master gain, so it sees the voices
        summed but not the master fader - riding the master must not change
        how hard the compressor works. */
    dsp::OttCompressor ott;

    std::atomic<float>* oversamplingParam = nullptr;

    struct OttParams
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* depth = nullptr;
        std::atomic<float>* time = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* inputGain = nullptr;
        std::atomic<float>* outputGain = nullptr;
        std::atomic<float>* crossoverLow = nullptr;
        std::atomic<float>* crossoverHigh = nullptr;
        std::atomic<float>* lowGain = nullptr;
        std::atomic<float>* midGain = nullptr;
        std::atomic<float>* highGain = nullptr;
        std::atomic<float>* lowUpward = nullptr;
        std::atomic<float>* midUpward = nullptr;
        std::atomic<float>* highUpward = nullptr;
        std::atomic<float>* lowDownward = nullptr;
        std::atomic<float>* midDownward = nullptr;
        std::atomic<float>* highDownward = nullptr;
    };

    OttParams ottParams {};

    /** Tracked so the reported latency is only updated when it changes:
        setLatencySamples notifies the host, which is not free. */
    dsp::VoiceOversampler::Factor activeOversamplingFactor =
        dsp::VoiceOversampler::Factor::twoTimes;

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

    /** MIDI controller state, written from the audio thread as messages
        arrive and read back into the settings each block. Members rather than
        parameters: these are performance controls, and a host that recorded
        them as automation would fight the player's own controller. */
    float modWheelValue = 0.0f;
    float pitchBendValue = 0.5f;
    float aftertouchValue = 0.0f;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GnarlProcessor)
};

} // namespace gnarl
