#include "VoiceManager.h"

#include <cmath>

namespace gnarl::dsp
{

namespace
{
    /** Base seed for per-voice random streams. Fixed, so a patch renders
        identically every time - a bounce that differs between runs is a bug
        report waiting to happen. */
    constexpr juce::int64 kRandomSeedBase = 0x476E'6172'6C00'0001;

    /** Two voices whose amplitudes differ by less than this are treated as
        equally loud, so the age tie-break decides. Without a tolerance, two
        notes held at the same velocity would be separated by float noise
        rather than by age, making which one gets stolen unpredictable. */
    constexpr float kAmplitudeTieEpsilon = 1.0e-6f;
}

VoiceManager::VoiceManager()
{
    for (std::size_t i = 0; i < voices.size(); ++i)
        voices[i].setRandomSeed (kRandomSeedBase + static_cast<juce::int64> (i) * 7919);
}

void VoiceManager::prepare (double sampleRate, int maximumBlockSize)
{
    for (auto& voice : voices)
        voice.prepare (sampleRate, maximumBlockSize);

    reset();
}

void VoiceManager::reset()
{
    for (auto& voice : voices)
        voice.reset();

    numHeldNotes = 0;
    lastMonoNote = -1;
    eventCounter = 0;
}

void VoiceManager::setVoiceLimit (int limit) noexcept
{
    voiceLimit = juce::jlimit (1, pid::kMaxVoices, limit);
}

// --- Voice selection -------------------------------------------------------

Voice* VoiceManager::findIdleVoice()
{
    const auto limit = static_cast<std::size_t> (voiceLimit);

    for (std::size_t i = 0; i < limit; ++i)
        if (voices[i].isIdle())
            return &voices[i];

    return nullptr;
}

Voice* VoiceManager::findOldestReleasedVoice()
{
    Voice* best = nullptr;
    const auto limit = static_cast<std::size_t> (voiceLimit);

    for (std::size_t i = 0; i < limit; ++i)
    {
        auto& voice = voices[i];

        if (! voice.isReleasing())
            continue;

        if (best == nullptr || voice.getReleaseOrder() < best->getReleaseOrder())
            best = &voice;
    }

    return best;
}

Voice* VoiceManager::findQuietestVoice()
{
    Voice* best = nullptr;
    const auto limit = static_cast<std::size_t> (voiceLimit);

    for (std::size_t i = 0; i < limit; ++i)
    {
        auto& voice = voices[i];

        if (voice.isIdle())
            continue;

        // A voice already fading out for a steal must not be stolen again:
        // its queued note would be dropped.
        if (voice.getState() == Voice::State::stealing)
            continue;

        if (best == nullptr)
        {
            best = &voice;
            continue;
        }

        const auto amplitude = voice.getCurrentAmplitude();
        const auto bestAmplitude = best->getCurrentAmplitude();

        const auto isQuieter = amplitude < bestAmplitude - kAmplitudeTieEpsilon;
        const auto isTied = std::abs (amplitude - bestAmplitude) <= kAmplitudeTieEpsilon;

        // Ties broken by age, so identical held notes are stolen oldest first
        // rather than in whatever order the array happens to be in.
        if (isQuieter || (isTied && voice.getStartOrder() < best->getStartOrder()))
            best = &voice;
    }

    return best;
}

Voice* VoiceManager::findVoicePlayingNote (int noteNumber, int channel)
{
    const auto limit = static_cast<std::size_t> (voiceLimit);

    for (std::size_t i = 0; i < limit; ++i)
    {
        auto& voice = voices[i];

        if (voice.isIdle() || voice.getState() == Voice::State::stealing)
            continue;

        if (voice.getNoteNumber() == noteNumber && voice.getChannel() == channel)
            return &voice;
    }

    return nullptr;
}

Voice* VoiceManager::findVoiceToUse (int noteNumber, int channel)
{
    // Retrigger a voice already holding this note rather than stacking a
    // second one. Holding the same note twice doubles its level and wastes a
    // voice, and hosts do send duplicate note-ons.
    if (auto* existing = findVoicePlayingNote (noteNumber, channel))
        return existing;

    if (auto* idle = findIdleVoice())
        return idle;

    if (auto* released = findOldestReleasedVoice())
        return released;

    return findQuietestVoice();
}

// --- Held-note stack (mono/legato) ----------------------------------------

void VoiceManager::pushHeldNote (int noteNumber, float velocity, int channel)
{
    // Re-pressing a held note moves it to the top rather than duplicating it.
    removeHeldNote (noteNumber, channel);

    if (numHeldNotes >= kMaxHeldNotes)
    {
        // 128 simultaneously held notes is already beyond a keyboard; drop the
        // oldest rather than the new one, which is what the player just played.
        for (std::size_t i = 1; i < kMaxHeldNotes; ++i)
            heldNotes[i - 1] = heldNotes[i];

        numHeldNotes = kMaxHeldNotes - 1;
    }

    heldNotes[numHeldNotes++] = HeldNote { noteNumber, velocity, channel };
}

void VoiceManager::removeHeldNote (int noteNumber, int channel)
{
    std::size_t write = 0;

    for (std::size_t read = 0; read < numHeldNotes; ++read)
    {
        const auto& note = heldNotes[read];

        if (note.noteNumber == noteNumber && note.channel == channel)
            continue;

        heldNotes[write++] = note;
    }

    numHeldNotes = write;
}

// --- Glide -----------------------------------------------------------------

std::optional<int> VoiceManager::resolveGlideSource (const Voice& voice) const
{
    if (glideTimeSeconds <= 0.0f)
        return {};

    // "Glide always" glides from the previous note even after a gap of
    // silence; otherwise a glide only makes sense while a note is sounding.
    if (glideAlways || ! voice.isIdle())
        return voice.getNoteNumber();

    return {};
}

// --- Mono and legato -------------------------------------------------------

Voice* VoiceManager::handleMonoNoteOn (int noteNumber, float velocity, int channel)
{
    const auto wasHolding = numHeldNotes > 0;

    pushHeldNote (noteNumber, velocity, channel);

    auto& voice = voices[0];

    NoteRequest request;
    request.noteNumber = noteNumber;
    request.velocity = velocity;
    request.channel = channel;

    if (glideTimeSeconds > 0.0f && (glideAlways || wasHolding) && lastMonoNote >= 0)
        request.glideFromNote = lastMonoNote;

    lastMonoNote = noteNumber;

    // Legato only skips the envelope restart when a note was ALREADY held.
    // The first note of a phrase must still trigger the envelope, or the patch
    // is silent until the second note.
    const auto shouldGlideOnly = polyMode == choices::PolyMode::legato && wasHolding;

    if (shouldGlideOnly)
    {
        voice.changeNote (request, glideTimeSeconds);
    }
    else
    {
        voice.startNote (request, glideTimeSeconds);
        voice.setStartOrder (++eventCounter);
    }

    return &voice;
}

void VoiceManager::handleMonoNoteOff (int noteNumber, int channel, bool allowTailOff)
{
    removeHeldNote (noteNumber, channel);

    auto& voice = voices[0];

    if (numHeldNotes == 0)
    {
        voice.stopNote (allowTailOff);
        voice.setReleaseOrder (++eventCounter);
        return;
    }

    // Fall back to the note still underneath the one just released.
    const auto& previous = heldNotes[numHeldNotes - 1];

    NoteRequest request;
    request.noteNumber = previous.noteNumber;
    request.velocity = previous.velocity;
    request.channel = previous.channel;

    if (glideTimeSeconds > 0.0f && lastMonoNote >= 0)
        request.glideFromNote = lastMonoNote;

    lastMonoNote = previous.noteNumber;

    voice.changeNote (request, glideTimeSeconds);
}

// --- Note events -----------------------------------------------------------

Voice* VoiceManager::noteOn (int noteNumber, float velocity, int channel)
{
    if (voiceLimit <= 0)
        return nullptr;

    if (polyMode != choices::PolyMode::poly)
        return handleMonoNoteOn (noteNumber, velocity, channel);

    auto* voice = findVoiceToUse (noteNumber, channel);

    if (voice == nullptr)
        return nullptr;

    NoteRequest request;
    request.noteNumber = noteNumber;
    request.velocity = velocity;
    request.channel = channel;
    request.glideFromNote = resolveGlideSource (*voice);

    // An idle or same-note voice starts immediately. Anything else is being
    // taken from a note that is still sounding, so it fades first.
    if (voice->isIdle() || voice->getNoteNumber() == noteNumber)
        voice->startNote (request, glideTimeSeconds);
    else
        voice->stealWith (request, glideTimeSeconds);

    voice->setStartOrder (++eventCounter);

    return voice;
}

void VoiceManager::noteOff (int noteNumber, int channel, bool allowTailOff)
{
    if (polyMode != choices::PolyMode::poly)
    {
        handleMonoNoteOff (noteNumber, channel, allowTailOff);
        return;
    }

    for (auto& voice : voices)
    {
        if (voice.getState() != Voice::State::active)
            continue;

        if (voice.getNoteNumber() != noteNumber || voice.getChannel() != channel)
            continue;

        voice.stopNote (allowTailOff);
        voice.setReleaseOrder (++eventCounter);
    }
}

void VoiceManager::allNotesOff (bool allowTailOff)
{
    numHeldNotes = 0;
    lastMonoNote = -1;

    for (auto& voice : voices)
    {
        if (voice.isIdle())
            continue;

        voice.stopNote (allowTailOff);
        voice.setReleaseOrder (++eventCounter);
    }
}

// --- Rendering -------------------------------------------------------------

void VoiceManager::render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    for (auto& voice : voices)
        voice.render (buffer, startSample, numSamples);
}

// --- Queries ---------------------------------------------------------------

int VoiceManager::getActiveVoiceCount() const noexcept
{
    int count = 0;

    for (const auto& voice : voices)
        if (voice.isActive())
            ++count;

    return count;
}

int VoiceManager::getSoundingVoiceCount() const noexcept
{
    int count = 0;

    for (const auto& voice : voices)
        if (! voice.isIdle())
            ++count;

    return count;
}

} // namespace gnarl::dsp
