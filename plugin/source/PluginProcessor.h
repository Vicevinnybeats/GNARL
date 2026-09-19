#pragma once

#include "dsp/FxRack.h"
#include "dsp/OttCompressor.h"
#include "dsp/SmoothedParameter.h"
#include "dsp/VoiceManager.h"
#include "dsp/VoiceOversampler.h"
#include "dsp/VoiceSettings.h"
#include "dsp/WavetableLibrary.h"
#include "params/FxOrderBridge.h"
#include "params/ModStateBridge.h"
#include "preset/PresetManager.h"
#include "license/LicenseManager.h"
#include "preset/TableLoader.h"
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

    /** The FX chain order, which is ValueTree state rather than a parameter -
        see docs/fx-architecture.md. The UI reorders the rack through this. */
    params::FxOrderBridge& getFxOrder() noexcept { return *fxOrderBridge; }

    preset::PresetManager& getPresets() noexcept { return *presetManager; }

    /** MESSAGE THREAD. Replaces the whole patch from a preset's state tree.

        Shared by setStateInformation and the preset browser on purpose: a
        preset and a session must recall identically, and two code paths that
        both "load a patch" are two paths that will one day disagree. */
    void applyPresetState (const juce::ValueTree& state);

    /** True while wavetables a preset asked for are still being generated.
        The UI marks the patch as still arriving rather than pretending it is
        complete. */
    bool isLoadingTables() const noexcept;

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

    /** Everything below this reads as silence. Also the bottom of the UI's
        meter scale, so the two agree by construction rather than by two
        people picking the same number twice. */
    static constexpr float kMeterFloorDb = -60.0f;

    /** What the output and gain-reduction meters draw.

        Same message-thread-reads-while-audio-writes trade as the modulation
        snapshot above: a stale meter frame is invisible, a lock in the render
        path is not. */
    struct MeterSnapshot
    {
        /** Post-master output, dBFS, per channel, floored at kMeterFloorDb. */
        std::array<float, 2> outputDb { kMeterFloorDb, kMeterFloorDb };

        /** The OTT's gain change PER BAND, in dB and signed: negative is
            downward compression, positive the upward lift.

            Per band rather than one combined figure, because the FX tab
            draws one meter per band - and because the three routinely move
            in opposite directions at once. Lifting the quiet top while
            holding the loud low down is the whole point of an OTT, and any
            average or sum of those reads "nothing is happening" at exactly
            the moment the most is. */
        std::array<float, dsp::OttCompressor::kNumBands> ottGainDb {};
    };

    /** MESSAGE THREAD. */
    MeterSnapshot getMeterSnapshot() const noexcept;

    /** The licence, for the banner and the feature gates.

        NOT on the audio thread, ever. `processBlock` does not read this and
        never will - `State::audioAllowed()` is a compile-time `true` so that
        anyone looking for the path that silences the plugin finds the
        promise instead of a bug. */
    license::LicenseManager& getLicense() noexcept { return licensing; }
    const license::LicenseManager& getLicense() const noexcept { return licensing; }

private:
    /** Shared by the float and double processBlock overloads. */
    template <typename SampleType>
    void processInternal (juce::AudioBuffer<SampleType>&, juce::MidiBuffer&);

    /** BLOCK-RATE. Copies the voice-related parameters into the manager. */
    void updateVoiceManagerSettings() noexcept;

    /** BLOCK-RATE. Applies the oversampling parameter, switching the voices'
        working rate and reporting the new latency if it changed. */
    void updateOversampling();

    /** Sums the voice oversampler's latency and the FX rack's and reports it,
        but only when the total changed. Both contributors are conditional -
        the rack's limiter and its own oversampling switch on and off with the
        patch - so this is checked every block rather than only in
        prepareToPlay. */
    void updateReportedLatency();

    /** AUDIO THREAD. Folds this block's peak into the decaying meter state.
        Templated for the same reason runFxRack is: the host may hand over
        doubles. */
    template <typename SampleType>
    void updateOutputMeter (const juce::AudioBuffer<SampleType>& buffer,
                            int numSamples) noexcept;

    /** Runs the FX rack over the output buffer.

        Templated because the host may hand over doubles and the rack is
        float-only - as the voices are. A double buffer is copied into a float
        scratch, processed and copied back; the conversion is exact in one
        direction and the rounding in the other is far below the noise floor of
        anything the rack does. */
    template <typename SampleType>
    void runFxRack (juce::AudioBuffer<SampleType>& buffer, int numSamples);

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
    std::unique_ptr<params::FxOrderBridge> fxOrderBridge;
    std::unique_ptr<preset::PresetManager> presetManager;

    /** Generates wavetables off the message thread, so clicking through a
        preset bank does not freeze the window for a tenth of a second per
        click. See preset/TableLoader.h. */
    std::unique_ptr<preset::TableLoader> tableLoader;

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

    /** The fourteen effects, run in the user's order. Sits AFTER the OTT and
        before the master gain: the OTT is part of the instrument's voice
        rather than an effect the user placed, and the master fader must be
        last so that riding it does not change how anything upstream behaves.

        Its latency varies - the limiter's look-ahead and the oversampler's
        filters are both conditional - so it is re-reported when it changes.
        See updateFxLatency. */
    dsp::FxRack fxRack;

    /** Tracked so setLatencySamples is only called when the total actually
        changes: notifying the host is not free, and the FX rack's latency can
        change on any block. */
    int reportedLatencySamples = -1;

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

    /** Only used when the host is running in double precision: the rack is
        float-only, so the block is copied through here. Sized in
        prepareToPlay, never on the audio thread. */
    juce::AudioBuffer<float> fxScratchBuffer;

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

    /*  METERING.

        The audio thread holds a DECAYING peak rather than the last block's
        peak, and that is the whole design. The UI samples at 60 Hz while the
        host calls back at (48 kHz / 256) 187 Hz, so a "last block" meter
        shows whichever two blocks in three the timer happened to land on -
        the needle would then be a function of the BLOCK SIZE rather than of
        the signal, which is the same mistake as the filter-per-channel and
        shared-LFO bugs in CLAUDE.md section 3, just where it is merely ugly
        instead of audible.

        An exponential decay composes exactly (exp(-a)exp(-b) = exp(-(a+b))),
        so the reading after a given number of seconds is the same however the
        host chunks them. */

    /** 20 dB per second, which is close to an IEC PPM's fallback and reads as
        a meter rather than as a light switch. */
    static constexpr float kMeterReleaseDbPerSecond = 20.0f;

    /** Below this the peak snaps to exactly zero instead of approaching it
        forever. Not a denormal guard - it is so that an idle editor produces
        BYTE-IDENTICAL frames again and the emitter's identical-frame drop
        keeps working. Without it a silent plugin pushes 60 messages a second
        for as long as its window is open, chasing a number that never
        arrives. Same shape as the TPT fixed point in section 3: decaying
        state does not reach zero on its own. */
    static constexpr float kMeterSilence = 1.0e-4f;   // -80 dBFS

    /** Nepers of decay per SAMPLE, not per block.

        Per block would have to use the block size prepareToPlay was given,
        and a host is free to send shorter blocks than it declared - which
        would make the fall time a function of the block layout, the very
        thing the decaying peak exists to remove. One `std::exp` per block
        over the block's actual length is exact and costs nothing next to
        rendering it. */
    float meterDecayPerSample = 0.0f;

    /** Linear, not dB: the audio thread only takes a max and a multiply, and
        the log runs once per UI frame on the message thread instead of once
        per block on the audio thread. */
    std::array<std::atomic<float>, 2> outputPeak { { { 0.0f }, { 0.0f } } };

    /*  Declared BEFORE the members whose destruction could outlive a
        callback it makes - it joins its worker and cancels its pending
        AsyncUpdater in its own destructor, and members are destroyed in
        reverse declaration order. */
    license::LicenseManager licensing;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GnarlProcessor)
};

} // namespace gnarl
