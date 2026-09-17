#pragma once

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
    One polyphonic voice.

    Phase 1 scope: lifecycle, pitch with portamento, a per-voice random seed,
    and a placeholder amplitude envelope. It renders SILENCE - the oscillators
    and filters arrive in Phase 2.

    The amplitude envelope here is not the real Env 1. It exists because voice
    stealing has to be able to ask "which voice is quietest", and because a
    stolen voice must fade out rather than cut. Phase 3 replaces it with the
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

    void prepare (double sampleRate, int maximumBlockSize);

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
        Phase 1 adds silence; the envelope and pitch still advance, so voice
        stealing and glide are already testable. */
    void render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

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

private:
    void beginAmplitudeAttack();
    void updatePitchTarget (const NoteRequest& request, float glideTimeSeconds);

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

    JUCE_LEAK_DETECTOR (Voice)
};

} // namespace gnarl::dsp
