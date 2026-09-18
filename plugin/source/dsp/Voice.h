#pragma once

#include "NoiseGenerator.h"
#include "VoiceSettings.h"
#include "WavetableOscillator.h"
#include "../params/ParameterIDs.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cstdint>
#include <optional>

namespace gnarl::dsp
{

/** A request to start a note, as handed to a voice. */
struct NoteRequest
{
    int noteNumber = 60;
    float velocity = 1.0f;     // 0..1
    int channel = 1;           // MIDI channel, 1-16
    /** Note to glide FROM, when portamento applies. */
    std::optional<int> glideFromNote {};
};

/**
    Shared scratch buffers for voice rendering.

    Owned by the VoiceManager and lent to each voice in turn, because voices
    render sequentially. Giving every voice its own set would multiply this
    memory by sixteen for no benefit.
*/
struct VoiceScratch
{
    void prepare (int maximumBlockSize)
    {
        const auto size = juce::jmax (1, maximumBlockSize);

        source.setSize (2, size, false, true, true);
        filter1.setSize (2, size, false, true, true);
        filter2.setSize (2, size, false, true, true);
        direct.setSize (2, size, false, true, true);
        modulator.setSize (1, size, false, true, true);

        clear();
    }

    void clear() noexcept
    {
        source.clear();
        filter1.clear();
        filter2.clear();
        direct.clear();
        modulator.clear();
    }

    int getCapacity() const noexcept { return source.getNumSamples(); }

    /** One source's output, before it is split across the sends. */
    juce::AudioBuffer<float> source;

    /** The two filter input busses and the unfiltered path. */
    juce::AudioBuffer<float> filter1;
    juce::AudioBuffer<float> filter2;
    juce::AudioBuffer<float> direct;

    /** Oscillator 2's mono output, for FM and ring mod into oscillator 1. */
    juce::AudioBuffer<float> modulator;
};

/**
    One polyphonic voice: two wavetable oscillators, a sub, noise, and two
    filters with send routing.

    The amplitude envelope is still the Phase 1 placeholder. It exists because
    voice stealing has to be able to ask "which voice is quietest", and because
    a stolen voice must fade out rather than cut. Phase 3 replaces it with the
    real per-segment envelope and keeps this interface.

    Real-time contract: every method except prepare() and reset() is callable
    from the audio thread and allocates nothing.
*/
class Voice
{
public:
    enum class State
    {
        idle,       // available
        active,     // note held
        releasing,  // note let go, tail still sounding
        stealing    // fading out, with a queued note to start when the fade ends
    };

    Voice() = default;

    // --- Setup (message thread) --------------------------------------------

    /** MESSAGE THREAD. Pass the HIGHEST rate this voice will run at,
        including any oversampled rate - the comb filter's delay line is sized
        here and must never be reallocated on the audio thread. */
    void prepare (double maximumSampleRate, int maximumBlockSize);

    /** AUDIO THREAD SAFE. Switches the working rate without allocating, which
        is what lets the oversampling factor be a live parameter. */
    void setSampleRate (double sampleRate) noexcept;

    /** AUDIO THREAD SAFE. Keeps the oscillators band-limited to the base
        rate's Nyquist while running oversampled. */
    void setOversamplingRatio (float ratio) noexcept;

    /** Seeds this voice's deterministic random stream. Deterministic so a
        patch sounds the same on every load: "analog drift" that changes
        between renders would make a bounce irreproducible. */
    void setRandomSeed (juce::int64 seed);

    void reset();

    // --- Note lifecycle (audio thread) ------------------------------------

    void startNote (const NoteRequest& request, float glideTimeSeconds);

    /** Retriggers without restarting the amplitude envelope, for legato. */
    void changeNote (const NoteRequest& request, float glideTimeSeconds);

    void stopNote (bool allowTailOff);

    /** Begins a fade-out, then starts `request` when the fade completes.
        This is how a stolen voice avoids clicking. */
    void stealWith (const NoteRequest& request, float glideTimeSeconds);

    // --- Rendering (audio thread) -----------------------------------------

    /** Advances the voice by `numSamples` and adds its output into `buffer`.

        `settings` is read once per block by the processor and shared by every
        voice. `scratch` is borrowed, not owned - voices render one at a time. */
    void render (juce::AudioBuffer<float>& buffer,
                 int startSample,
                 int numSamples,
                 const VoiceSettings& settings,
                 VoiceScratch& scratch);

    /** Advances state without producing audio. Used by the tests and by any
        caller with no settings to render with. */
    void advanceSilently (int numSamples);

    // --- Queries (audio thread) -------------------------------------------

    State getState() const noexcept { return state; }
    bool isIdle() const noexcept { return state == State::idle; }
    bool isActive() const noexcept { return state == State::active; }
    bool isReleasing() const noexcept { return state == State::releasing; }

    int getNoteNumber() const noexcept { return currentNote; }
    int getChannel() const noexcept { return currentChannel; }
    float getVelocity() const noexcept { return currentVelocity; }

    /** Monotonic counter set when the note started. Lower means older. */
    std::uint64_t getStartOrder() const noexcept { return startOrder; }

    /** Monotonic counter set when the note was released. Lower means released
        longer ago. Only meaningful while releasing. */
    std::uint64_t getReleaseOrder() const noexcept { return releaseOrder; }

    void setStartOrder (std::uint64_t order) noexcept { startOrder = order; }
    void setReleaseOrder (std::uint64_t order) noexcept { releaseOrder = order; }

    /** Current envelope level scaled by velocity, 0..1. The "steal quietest"
        heuristic reads this. */
    float getCurrentAmplitude() const noexcept;

    /** Current pitch in MIDI note units, including any in-progress glide. */
    float getCurrentPitch() const noexcept { return pitch.getCurrentValue(); }

    /** Per-voice random values, one per unison slot, in -1..1. Stable for the
        lifetime of the note, so unison detune does not shimmer. */
    float getUnisonRandom (int unisonIndex) const noexcept;

    /** Per-voice drift value in -1..1, stable for the note. */
    float getDriftAmount() const noexcept { return driftAmount; }

    /** Oscillators are exposed so the manager can hand them their tables when
        a patch changes, without the voice knowing about the library. */
    WavetableOscillator& getOscillator (std::size_t index) noexcept
    {
        return oscillators[juce::jmin (index, oscillators.size() - 1)];
    }

    WavetableOscillator& getSubOscillator() noexcept { return subOscillator; }

private:
    void beginAmplitudeAttack();
    void updatePitchTarget (const NoteRequest& request, float glideTimeSeconds);

    /** Frequency in Hz for a pitch in MIDI note units plus an offset. */
    float getFrequencyHz (float pitchInNotes, float offsetSemitones) const noexcept;

    /** Renders one source into scratch.source and distributes it across the
        filter and direct busses by its send levels. */
    void addSourceToBusses (const VoiceSettings::Sends& sends,
                            VoiceScratch& scratch,
                            int numSamples) noexcept;

    void renderNoise (VoiceScratch& scratch,
                      int numSamples,
                      const VoiceSettings::NoiseState& noiseState) noexcept;

    /** Runs the filter busses and sums everything into `destination`. */
    void renderFilters (juce::AudioBuffer<float>& destination,
                        int startSample,
                        int numSamples,
                        const VoiceSettings& settings,
                        VoiceScratch& scratch) noexcept;

    State state = State::idle;

    int currentNote = 60;
    int currentChannel = 1;
    float currentVelocity = 1.0f;

    std::uint64_t startOrder = 0;
    std::uint64_t releaseOrder = 0;

    double sampleRateHz = 44100.0;

    /** Pitch in MIDI note units. Smoothing note number rather than frequency
        makes the glide exponential in Hz, which is what sounds linear. */
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> pitch { 60.0f };

    /** Placeholder amplitude envelope - a simple attack/release ramp, replaced
        by Env 1 in Phase 3. Present so stealing has an amplitude to compare
        and a fade to apply. */
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> amplitude { 0.0f };

    /** Fade applied while stealing. Separate from `amplitude` so a steal
        cannot be lengthened by a long release setting. */
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> stealFade { 1.0f };

    /** Optional, not a default-constructed NoteRequest: a default request is
        indistinguishable from a real middle-C note at full velocity. */
    std::optional<NoteRequest> queuedNote {};
    float queuedGlideTime = 0.0f;

    std::array<float, 16> unisonRandom {};
    float driftAmount = 0.0f;

    std::array<WavetableOscillator, pid::kNumOscillators> oscillators {};
    WavetableOscillator subOscillator;
    NoiseGenerator noise;

    std::array<FilterSlot, pid::kNumFilters> filters {};

    JUCE_LEAK_DETECTOR (Voice)
};

} // namespace gnarl::dsp
