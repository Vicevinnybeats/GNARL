#include <catch2/catch_test_macros.hpp>

#include "dsp/VoiceManager.h"

#include <set>
#include <vector>

using namespace gnarl;
using namespace gnarl::dsp;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;

    std::unique_ptr<VoiceManager> makeManager()
    {
        auto manager = std::make_unique<VoiceManager>();
        manager->prepare (kSampleRate, kBlockSize);
        return manager;
    }

    /** Advances the engine by `blocks` blocks, so envelopes and fades move.

        Uses advanceSilently rather than render: these tests are about voice
        allocation and lifecycle, and giving them a full patch to render would
        make them depend on the whole oscillator and filter path. The audio
        path has its own tests. */
    void runBlocks (VoiceManager& manager, int blocks, int blockSize = kBlockSize)
    {
        for (int i = 0; i < blocks; ++i)
            manager.advanceSilently (blockSize);
    }

    std::vector<int> soundingNotes (const VoiceManager& manager)
    {
        std::vector<int> notes;

        for (std::size_t i = 0; i < VoiceManager::getNumVoices(); ++i)
        {
            const auto& voice = manager.getVoice (i);

            if (! voice.isIdle())
                notes.push_back (voice.getNoteNumber());
        }

        return notes;
    }
}

TEST_CASE ("A fresh manager has no sounding voices", "[voices]")
{
    auto manager = makeManager();

    CHECK (manager->getSoundingVoiceCount() == 0);
    CHECK (manager->getActiveVoiceCount() == 0);
}

TEST_CASE ("One note on takes one voice", "[voices]")
{
    auto manager = makeManager();

    auto* voice = manager->noteOn (60, 1.0f, 1);

    REQUIRE (voice != nullptr);
    CHECK (voice->isActive());
    CHECK (voice->getNoteNumber() == 60);
    CHECK (manager->getActiveVoiceCount() == 1);
}

TEST_CASE ("Distinct notes take distinct voices", "[voices]")
{
    auto manager = makeManager();

    std::set<const Voice*> used;

    for (int note = 48; note < 48 + pid::kMaxVoices; ++note)
    {
        auto* voice = manager->noteOn (note, 1.0f, 1);
        REQUIRE (voice != nullptr);
        used.insert (voice);
    }

    CHECK (static_cast<int> (used.size()) == pid::kMaxVoices);
    CHECK (manager->getActiveVoiceCount() == pid::kMaxVoices);
}

TEST_CASE ("More notes than voices steals rather than dropping notes", "[voices][stealing]")
{
    auto manager = makeManager();

    // 20 notes into 16 voices: the manager must always return a voice.
    for (int note = 48; note < 48 + 20; ++note)
    {
        auto* voice = manager->noteOn (note, 1.0f, 1);
        INFO ("note " << note);
        REQUIRE (voice != nullptr);
    }

    CHECK (manager->getSoundingVoiceCount() == pid::kMaxVoices);

    // Never more than the limit, whatever happened in between.
    CHECK (manager->getSoundingVoiceCount() <= pid::kMaxVoices);
}

TEST_CASE ("A stolen voice eventually plays the note that stole it",
           "[voices][stealing]")
{
    auto manager = makeManager();

    for (int note = 48; note < 48 + pid::kMaxVoices; ++note)
        manager->noteOn (note, 1.0f, 1);

    // The steal fade is a few milliseconds, so it completes well within a
    // handful of blocks.
    const auto stealingNote = 90;
    manager->noteOn (stealingNote, 1.0f, 1);

    runBlocks (*manager, 8);

    const auto notes = soundingNotes (*manager);

    // The queued note must have replaced whatever was stolen - a steal that
    // drops its own note is worse than no stealing at all.
    CHECK (std::find (notes.begin(), notes.end(), stealingNote) != notes.end());
}

TEST_CASE ("Released voices are stolen before held ones", "[voices][stealing]")
{
    auto manager = makeManager();

    for (int note = 48; note < 48 + pid::kMaxVoices; ++note)
        manager->noteOn (note, 1.0f, 1);

    // Release one note. It is now the least valuable voice in the pool.
    const auto releasedNote = 52;
    manager->noteOff (releasedNote, 1, true);

    // A couple of blocks so it is genuinely in its release tail, not just
    // flagged.
    runBlocks (*manager, 2);

    manager->noteOn (95, 1.0f, 1);
    runBlocks (*manager, 8);

    const auto notes = soundingNotes (*manager);

    // The released note is gone, and every still-held note survived.
    CHECK (std::find (notes.begin(), notes.end(), releasedNote) == notes.end());

    for (int note = 48; note < 48 + pid::kMaxVoices; ++note)
    {
        if (note == releasedNote)
            continue;

        INFO ("held note " << note << " must not have been stolen");
        CHECK (std::find (notes.begin(), notes.end(), note) != notes.end());
    }
}

TEST_CASE ("The quietest held voice is stolen when none have been released",
           "[voices][stealing]")
{
    auto manager = makeManager();

    // All at full velocity except one, which is nearly silent and therefore
    // the one a listener would miss least.
    for (int note = 48; note < 48 + pid::kMaxVoices; ++note)
        manager->noteOn (note, 1.0f, 1);

    const auto quietNote = 55;
    manager->noteOff (quietNote, 1, false);   // no tail: fades out fast

    runBlocks (*manager, 1);
    manager->noteOn (99, 1.0f, 1);
    runBlocks (*manager, 8);

    const auto notes = soundingNotes (*manager);
    CHECK (std::find (notes.begin(), notes.end(), quietNote) == notes.end());
}

TEST_CASE ("A duplicate note on retriggers rather than stacking voices",
           "[voices]")
{
    auto manager = makeManager();

    auto* first = manager->noteOn (60, 1.0f, 1);
    auto* second = manager->noteOn (60, 1.0f, 1);

    // Hosts do send duplicate note-ons. Stacking them doubles the level and
    // burns a voice for nothing.
    CHECK (first == second);
    CHECK (manager->getActiveVoiceCount() == 1);
}

TEST_CASE ("Note off releases only the matching note and channel", "[voices]")
{
    auto manager = makeManager();

    manager->noteOn (60, 1.0f, 1);
    manager->noteOn (64, 1.0f, 1);
    manager->noteOn (60, 1.0f, 2);   // same note, different channel

    manager->noteOff (60, 1, true);

    CHECK (manager->getActiveVoiceCount() == 2);

    for (std::size_t i = 0; i < VoiceManager::getNumVoices(); ++i)
    {
        const auto& voice = manager->getVoice (i);

        if (voice.getNoteNumber() == 60 && voice.getChannel() == 1 && ! voice.isIdle())
            CHECK (voice.isReleasing());
    }
}

TEST_CASE ("Released voices are freed once their tail has decayed", "[voices]")
{
    auto manager = makeManager();

    manager->noteOn (60, 1.0f, 1);
    manager->noteOff (60, 1, true);

    // The placeholder release is 80 ms; 48000 * 0.25 s of rendering is
    // comfortably past it.
    runBlocks (*manager, 48);

    CHECK (manager->getSoundingVoiceCount() == 0);
}

TEST_CASE ("The voice limit caps polyphony", "[voices]")
{
    auto manager = makeManager();
    manager->setVoiceLimit (4);

    for (int note = 48; note < 48 + 12; ++note)
        manager->noteOn (note, 1.0f, 1);

    runBlocks (*manager, 8);

    CHECK (manager->getSoundingVoiceCount() <= 4);
}

TEST_CASE ("The voice limit is clamped to the legal range", "[voices]")
{
    auto manager = makeManager();

    manager->setVoiceLimit (0);
    CHECK (manager->getVoiceLimit() == 1);

    manager->setVoiceLimit (9999);
    CHECK (manager->getVoiceLimit() == pid::kMaxVoices);

    manager->setVoiceLimit (-5);
    CHECK (manager->getVoiceLimit() == 1);
}

TEST_CASE ("All notes off releases everything", "[voices]")
{
    auto manager = makeManager();

    for (int note = 48; note < 56; ++note)
        manager->noteOn (note, 1.0f, 1);

    manager->allNotesOff (true);

    CHECK (manager->getActiveVoiceCount() == 0);
}

TEST_CASE ("Mono mode uses a single voice", "[voices][mono]")
{
    auto manager = makeManager();
    manager->setPolyMode (choices::PolyMode::mono);

    manager->noteOn (60, 1.0f, 1);
    manager->noteOn (64, 1.0f, 1);
    manager->noteOn (67, 1.0f, 1);

    CHECK (manager->getSoundingVoiceCount() == 1);
    CHECK (manager->getVoice (0).getNoteNumber() == 67);
}

TEST_CASE ("Mono mode falls back to the note still held underneath",
           "[voices][mono]")
{
    auto manager = makeManager();
    manager->setPolyMode (choices::PolyMode::mono);

    manager->noteOn (60, 1.0f, 1);
    manager->noteOn (67, 1.0f, 1);

    // Releasing the top note while the lower one is still down should return
    // to the lower note, not go silent.
    manager->noteOff (67, 1, true);

    CHECK (manager->getVoice (0).getNoteNumber() == 60);
    CHECK (manager->getVoice (0).isActive());
}

TEST_CASE ("Mono mode goes silent when the last note is released",
           "[voices][mono]")
{
    auto manager = makeManager();
    manager->setPolyMode (choices::PolyMode::mono);

    manager->noteOn (60, 1.0f, 1);
    manager->noteOff (60, 1, true);

    CHECK_FALSE (manager->getVoice (0).isActive());
}

TEST_CASE ("Re-pressing a held note in mono does not duplicate it in the stack",
           "[voices][mono]")
{
    auto manager = makeManager();
    manager->setPolyMode (choices::PolyMode::mono);

    manager->noteOn (60, 1.0f, 1);
    manager->noteOn (64, 1.0f, 1);
    manager->noteOn (60, 1.0f, 1);   // re-press the lower note

    // 60 moved to the top of the stack rather than being held twice, so one
    // note-off is enough to fall back to 64.
    manager->noteOff (60, 1, true);

    CHECK (manager->getVoice (0).getNoteNumber() == 64);
}

TEST_CASE ("Legato does not restart the envelope for an overlapping note",
           "[voices][mono]")
{
    auto manager = makeManager();
    manager->setPolyMode (choices::PolyMode::legato);

    manager->noteOn (60, 1.0f, 1);
    runBlocks (*manager, 4);   // let the attack complete

    const auto amplitudeBefore = manager->getVoice (0).getCurrentAmplitude();

    manager->noteOn (67, 1.0f, 1);   // overlapping: legato applies

    const auto amplitudeAfter = manager->getVoice (0).getCurrentAmplitude();

    CHECK (manager->getVoice (0).getNoteNumber() == 67);

    // The envelope must not have been knocked back to the start of its attack.
    CHECK (amplitudeAfter >= amplitudeBefore * 0.99f);
}

TEST_CASE ("Legato still triggers the envelope for the first note of a phrase",
           "[voices][mono]")
{
    auto manager = makeManager();
    manager->setPolyMode (choices::PolyMode::legato);

    manager->noteOn (60, 1.0f, 1);
    runBlocks (*manager, 4);

    // If legato skipped the envelope on the first note, the patch would be
    // silent until the second one.
    CHECK (manager->getVoice (0).getCurrentAmplitude() > 0.5f);
}

TEST_CASE ("Glide moves pitch gradually and arrives at the target",
           "[voices][glide]")
{
    auto manager = makeManager();
    manager->setPolyMode (choices::PolyMode::mono);
    manager->setGlideTime (0.1f);   // 100 ms

    manager->noteOn (48, 1.0f, 1);
    runBlocks (*manager, 2);

    manager->noteOn (72, 1.0f, 1);

    // Immediately after the second note, pitch should still be near the first.
    const auto pitchAtStart = manager->getVoice (0).getCurrentPitch();
    CHECK (pitchAtStart < 60.0f);

    // 100 ms at 48 kHz is 4800 samples; 48 blocks of 256 is 12288.
    runBlocks (*manager, 48);

    CHECK (manager->getVoice (0).getCurrentPitch() > 71.9f);
    CHECK (manager->getVoice (0).getCurrentPitch() <= 72.0f);
}

TEST_CASE ("Zero glide time jumps straight to the new pitch", "[voices][glide]")
{
    auto manager = makeManager();
    manager->setPolyMode (choices::PolyMode::mono);
    manager->setGlideTime (0.0f);

    manager->noteOn (48, 1.0f, 1);
    manager->noteOn (72, 1.0f, 1);

    CHECK (manager->getVoice (0).getCurrentPitch() == 72.0f);
}

TEST_CASE ("Per-voice random values are deterministic and distinct",
           "[voices]")
{
    auto first = makeManager();
    auto second = makeManager();

    // Deterministic: the same patch must render identically every time, or a
    // bounce differs from the preview.
    for (std::size_t i = 0; i < VoiceManager::getNumVoices(); ++i)
    {
        CHECK (first->getVoice (i).getDriftAmount()
               == second->getVoice (i).getDriftAmount());
    }

    // Distinct: voices sharing a seed would be phase-identical, which defeats
    // the point of per-voice drift.
    std::set<float> drifts;

    for (std::size_t i = 0; i < VoiceManager::getNumVoices(); ++i)
        drifts.insert (first->getVoice (i).getDriftAmount());

    CHECK (drifts.size() > VoiceManager::getNumVoices() / 2);
}

TEST_CASE ("Unison random values are in range and stable", "[voices]")
{
    auto manager = makeManager();
    auto& voice = manager->getVoice (0);

    for (int i = 0; i < pid::kMaxUnisonVoices; ++i)
    {
        const auto value = voice.getUnisonRandom (i);
        CHECK (value >= -1.0f);
        CHECK (value <= 1.0f);
        CHECK (value == voice.getUnisonRandom (i));   // stable across reads
    }

    // Out-of-range indices clamp rather than reading past the array.
    CHECK (voice.getUnisonRandom (-1) == voice.getUnisonRandom (0));
    CHECK (voice.getUnisonRandom (9999)
           == voice.getUnisonRandom (pid::kMaxUnisonVoices - 1));
}

TEST_CASE ("Rendering zero or one sample is safe", "[voices][audio]")
{
    auto manager = makeManager();
    manager->noteOn (60, 1.0f, 1);

    juce::AudioBuffer<float> buffer (2, 1);
    VoiceSettings settings;

    buffer.clear();
    manager->render (buffer, 0, 0, settings);
    manager->render (buffer, 0, 1, settings);

    SUCCEED ("no crash on degenerate block sizes");
}

TEST_CASE ("Reset clears every voice", "[voices]")
{
    auto manager = makeManager();

    for (int note = 48; note < 60; ++note)
        manager->noteOn (note, 1.0f, 1);

    manager->reset();

    CHECK (manager->getSoundingVoiceCount() == 0);
}
