#pragma once

#include "Voice.h"
#include "../params/ParameterChoices.h"
#include "../params/ParameterIDs.h"

#include <array>
#include <cstdint>

namespace gnarl::dsp
{

/**
    Owns GNARL's voices and decides which one plays a note.

    Voice stealing policy, in order:
      1. An idle voice.
      2. The voice released longest ago. A note the player has already let go
         of is the least likely to be missed.
      3. The quietest voice, breaking ties by age. Stealing the quietest is
         what makes a dense chord stack degrade gracefully instead of chopping
         the note the player is leaning on.

    A stolen voice always fades out over a few milliseconds before the new note
    starts (see Voice::stealWith), because stealing is exactly the moment a
    synth is most likely to click.

    Real-time contract: every method here is callable from the audio thread and
    allocates nothing. The voice array is fixed at kMaxVoices.
*/
class VoiceManager
{
public:
    VoiceManager();

    // --- Setup (message thread) --------------------------------------------

    /** MESSAGE THREAD. Pass the highest rate the voices will run at. */
    void prepare (double maximumSampleRate, int maximumBlockSize);

    /** AUDIO THREAD SAFE. */
    void setSampleRate (double sampleRate) noexcept;

    /** AUDIO THREAD SAFE. */
    void setOversamplingRatio (float ratio) noexcept;

    void reset();

    // --- Configuration (audio thread, block-rate) -------------------------

    /** Caps how many voices may sound at once, 1..kMaxVoices. Lowering it does
        not cut sounding voices; they are simply not reused until they fall
        below the new limit. */
    void setVoiceLimit (int limit) noexcept;
    int getVoiceLimit() const noexcept { return voiceLimit; }

    void setPolyMode (choices::PolyMode mode) noexcept { polyMode = mode; }
    choices::PolyMode getPolyMode() const noexcept { return polyMode; }

    void setGlideTime (float seconds) noexcept { glideTimeSeconds = juce::jmax (0.0f, seconds); }

    /** When true, glide applies between every note; when false, only between
        overlapping notes, which is what a player expects from a legato line. */
    void setGlideAlways (bool shouldGlideAlways) noexcept { glideAlways = shouldGlideAlways; }

    // --- Note events (audio thread) ---------------------------------------

    /** Returns the voice that took the note, or nullptr if none could (only
        possible if the limit is somehow zero). */
    Voice* noteOn (int noteNumber, float velocity, int channel);

    void noteOff (int noteNumber, int channel, bool allowTailOff);

    /** Releases everything with a tail. */
    void allNotesOff (bool allowTailOff);

    // --- Rendering (audio thread) -----------------------------------------

    /** Renders every sounding voice, summing into `buffer`.

        Chunks internally to the scratch capacity, so a host exceeding the
        block size it declared in prepareToPlay cannot overrun the scratch
        buffers or force an allocation here. */
    void render (juce::AudioBuffer<float>& buffer,
                 int startSample,
                 int numSamples,
                 const VoiceSettings& settings);

    /** Advances voice state without producing audio. For the case where there
        is nothing to render with, and for tests. */
    void advanceSilently (int numSamples);

    // --- Queries ----------------------------------------------------------

    int getActiveVoiceCount() const noexcept;
    int getSoundingVoiceCount() const noexcept;

    Voice& getVoice (std::size_t index) noexcept { return voices[index]; }
    const Voice& getVoice (std::size_t index) const noexcept { return voices[index]; }
    static constexpr std::size_t getNumVoices() { return static_cast<std::size_t> (pid::kMaxVoices); }

private:
    Voice* findVoiceToUse (int noteNumber, int channel);
    Voice* findIdleVoice();
    Voice* findOldestReleasedVoice();
    Voice* findQuietestVoice();
    Voice* findVoicePlayingNote (int noteNumber, int channel);

    /** Mono/legato: notes the player is still holding, most recent last, so
        releasing the top note falls back to the one underneath. Fixed capacity
        - no allocation on the audio thread. */
    void pushHeldNote (int noteNumber, float velocity, int channel);
    void removeHeldNote (int noteNumber, int channel);

    Voice* handleMonoNoteOn (int noteNumber, float velocity, int channel);
    void handleMonoNoteOff (int noteNumber, int channel, bool allowTailOff);

    /** Glide source for a new note: the pitch the voice is leaving, or nothing
        when this note should not glide. */
    std::optional<int> resolveGlideSource (const Voice& voice) const;

    std::array<Voice, static_cast<std::size_t> (pid::kMaxVoices)> voices {};

    /** Shared by every voice, because voices render one at a time. Sixteen
        private copies would multiply this memory for no benefit. */
    VoiceScratch scratch;

    int voiceLimit = pid::kMaxVoices;
    choices::PolyMode polyMode = choices::PolyMode::poly;
    float glideTimeSeconds = 0.0f;
    bool glideAlways = false;

    /** Monotonic, so "older" is a comparison rather than a timestamp. */
    std::uint64_t eventCounter = 0;

    struct HeldNote
    {
        int noteNumber = -1;
        float velocity = 0.0f;
        int channel = 0;
    };

    static constexpr std::size_t kMaxHeldNotes = 128;
    std::array<HeldNote, kMaxHeldNotes> heldNotes {};
    std::size_t numHeldNotes = 0;

    /** Pitch the mono voice last played, for gliding into the next note. */
    int lastMonoNote = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoiceManager)
};

} // namespace gnarl::dsp
