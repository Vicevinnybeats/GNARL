#pragma once

#include "../dsp/VoiceSettings.h"
#include "../dsp/WavetableLibrary.h"
#include "ModStateBridge.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace gnarl::params
{

/**
    Reads the parameter tree into a dsp::VoiceSettings, once per block.

    All the raw `std::atomic<float>*` pointers are cached at construction, so
    reading a block costs a few hundred atomic loads and no string lookups at
    all. Doing this per voice instead would repeat the work sixteen times and,
    worse, could see a parameter change mid-loop - so two notes of the same
    chord would end up filtered differently.

    Wavetables are resolved through the library, which is on the message
    thread's side of the fence. The audio thread only ever sees a borrowed
    const pointer to an already-generated table, and a null one if the table
    is not ready yet.
*/
class SettingsReader
{
public:
    SettingsReader (juce::AudioProcessorValueTreeState& state,
                    dsp::WavetableLibrary& wavetableLibrary,
                    const ModStateBridge& modState);

    /** BLOCK-RATE. Fills `settings` from the current parameter values. */
    void read (dsp::VoiceSettings& settings) const noexcept;

    /** MESSAGE THREAD. Generates any table a current parameter value refers to
        but which has not been built yet, and publishes it for the audio
        thread. Called from prepareToPlay and after a state load. */
    void ensureTablesLoaded();

    /** Table indices the parameters currently select. */
    std::array<int, pid::kNumOscillators> getSelectedTableIndices() const noexcept;

private:
    struct OscillatorParams
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* mode = nullptr;
        std::atomic<float>* wavetable = nullptr;
        std::atomic<float>* tablePos = nullptr;
        std::atomic<float>* pitchSemi = nullptr;
        std::atomic<float>* pitchFine = nullptr;
        std::atomic<float>* phase = nullptr;
        std::atomic<float>* phaseRandom = nullptr;
        std::atomic<float>* pan = nullptr;
        std::atomic<float>* level = nullptr;
        std::atomic<float>* unisonVoices = nullptr;
        std::atomic<float>* unisonDetune = nullptr;
        std::atomic<float>* unisonBlend = nullptr;
        std::atomic<float>* unisonSpread = nullptr;
        std::atomic<float>* warpMode = nullptr;
        std::atomic<float>* warpAmount = nullptr;
        std::atomic<float>* grainSize = nullptr;
        std::atomic<float>* grainDensity = nullptr;
        std::atomic<float>* grainPosJitter = nullptr;
        std::atomic<float>* grainPitchJitter = nullptr;
        std::atomic<float>* sendFilter1 = nullptr;
        std::atomic<float>* sendFilter2 = nullptr;
        std::atomic<float>* sendDirect = nullptr;
    };

    struct SubParams
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* waveform = nullptr;
        std::atomic<float>* octave = nullptr;
        std::atomic<float>* pitchFine = nullptr;
        std::atomic<float>* phase = nullptr;
        std::atomic<float>* pan = nullptr;
        std::atomic<float>* level = nullptr;
        std::atomic<float>* sendFilter1 = nullptr;
        std::atomic<float>* sendFilter2 = nullptr;
        std::atomic<float>* sendDirect = nullptr;
    };

    struct NoiseParams
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* level = nullptr;
        std::atomic<float>* pan = nullptr;
        std::atomic<float>* pitchSemi = nullptr;
        std::atomic<float>* pitchFine = nullptr;
        std::atomic<float>* phaseRandom = nullptr;
        std::atomic<float>* sendFilter1 = nullptr;
        std::atomic<float>* sendFilter2 = nullptr;
        std::atomic<float>* sendDirect = nullptr;
    };

    struct FilterParams
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* cutoff = nullptr;
        std::atomic<float>* resonance = nullptr;
        std::atomic<float>* drive = nullptr;
        std::atomic<float>* driveCurve = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* keyTrack = nullptr;
        std::atomic<float>* formantX = nullptr;
        std::atomic<float>* formantY = nullptr;
        std::atomic<float>* formantThroat = nullptr;
        std::atomic<float>* combFeedback = nullptr;
        std::atomic<float>* combDamping = nullptr;
    };

    struct EnvelopeParams
    {
        std::atomic<float>* mode = nullptr;
        std::atomic<float>* delay = nullptr;
        std::atomic<float>* attack = nullptr;
        std::atomic<float>* hold = nullptr;
        std::atomic<float>* decay = nullptr;
        std::atomic<float>* sustain = nullptr;
        std::atomic<float>* release = nullptr;
        std::atomic<float>* attackCurve = nullptr;
        std::atomic<float>* decayCurve = nullptr;
        std::atomic<float>* releaseCurve = nullptr;
        std::atomic<float>* velocityAmount = nullptr;
    };

    struct LfoParams
    {
        std::atomic<float>* shape = nullptr;
        std::atomic<float>* syncEnabled = nullptr;
        std::atomic<float>* rateHz = nullptr;
        std::atomic<float>* rateDivision = nullptr;
        std::atomic<float>* mode = nullptr;
        std::atomic<float>* phase = nullptr;
        std::atomic<float>* smooth = nullptr;
        std::atomic<float>* gridDivision = nullptr;
        std::atomic<float>* bipolar = nullptr;
    };

    struct ModSlotParams
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* source = nullptr;
        std::atomic<float>* depth = nullptr;
        std::atomic<float>* curve = nullptr;
        std::atomic<float>* auxSource = nullptr;
        std::atomic<float>* auxAmount = nullptr;
        std::atomic<float>* bipolar = nullptr;
    };

    juce::AudioProcessorValueTreeState& apvts;
    dsp::WavetableLibrary& library;

    /** The curves and destinations that are not host parameters. Borrowed;
        owned by the processor. */
    const ModStateBridge& modStateBridge;

    std::array<OscillatorParams, pid::kNumOscillators> oscillators {};
    SubParams sub {};
    NoiseParams noise {};
    std::array<FilterParams, pid::kNumFilters> filters {};

    std::array<EnvelopeParams, pid::kNumEnvelopes> envelopes {};
    std::array<LfoParams, pid::kNumLfos> lfos {};
    std::array<ModSlotParams, pid::kNumModSlots> modSlots {};
    std::array<std::atomic<float>*, pid::kNumMacros> macros {};

    std::atomic<float>* filterRouting = nullptr;
    std::atomic<float>* analogDrift = nullptr;

    /** Published by ensureTablesLoaded on the message thread and read by the
        audio thread. A raw pointer swap of an already-built, immutable table:
        the table is never mutated after publication, and the library keeps it
        alive for the plugin's lifetime, so there is nothing to free. */
    std::array<std::atomic<const dsp::Wavetable*>, pid::kNumOscillators> oscillatorTables {};
    std::atomic<const dsp::Wavetable*> subTable { nullptr };
};

} // namespace gnarl::params
