#pragma once

#include "../dsp/LfoCurve.h"
#include "../dsp/Modulation.h"
#include "ParameterIDs.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace gnarl::params
{

/**
    The modulation state that is NOT a host parameter.

    Two things in the modulation section cannot be AudioProcessorValueTreeState
    parameters:

      - The drawable LFO curves. A curve is up to 32 breakpoints, each with a
        time, a value, a tension and a shape. Exposing that as host parameters
        would mean 384 automatable floats that no producer would ever automate
        individually, and a preset format that breaks the moment the point
        limit changes.

      - The mod slots' destinations. A host parameter is a number, and a
        number indexing a list of destinations cannot stay stable across
        releases: inserting one destination would silently repoint every
        preset's slots. Destinations are stored as parameter-ID STRINGS
        instead, and resolved to a dsp::ModDestination here, on the message
        thread, because the audio thread cannot compare strings.

    Both live in the plugin's ValueTree, under a MODSTATE child of the APVTS
    state, so getStateInformation carries them with no extra code.

    THREADING. The message thread authors; the audio thread reads once per
    block through getSnapshot(). Publication is a rotating set of three
    snapshots with an atomic index: the writer fills the slot after the live
    one and then makes it live, so the reader's slot is never the one being
    written. The reader could only be caught out by TWO publications landing
    inside one block's read - that is, two UI gestures inside a few
    milliseconds - and the cost would be one block of a half-updated curve, so
    the extra slot buys more than a lock would.
*/
class ModStateBridge : private juce::ValueTree::Listener
{
public:
    /** What the audio thread reads. */
    struct Snapshot
    {
        std::array<dsp::LfoCurve, pid::kNumLfos> curves {};
        std::array<dsp::ModDestination, pid::kNumModSlots> destinations {};
    };

    explicit ModStateBridge (juce::AudioProcessorValueTreeState& state);
    ~ModStateBridge() override;

    /** AUDIO THREAD. Valid until the next block. */
    const Snapshot& getSnapshot() const noexcept
    {
        return snapshots[static_cast<std::size_t> (liveSlot.load (std::memory_order_acquire))];
    }

    // --- Message thread ----------------------------------------------------

    /** Rebuilds everything from the ValueTree and republishes. Call after a
        state load; the listener covers ordinary edits. */
    void refresh();

    void setCurve (std::size_t lfoIndex, const dsp::LfoCurve& curve);
    dsp::LfoCurve getCurve (std::size_t lfoIndex) const;

    /** `parameterID` is a destination's parameter ID, or an empty string for
        "no destination". An ID this build does not know resolves to none,
        which is what makes a preset from a later version load harmlessly
        instead of failing. */
    void setDestination (std::size_t slotIndex, const juce::String& parameterID);
    juce::String getDestinationParameterID (std::size_t slotIndex) const;

    /** The ValueTree identifiers, exposed for the tests and for the preset
        code. */
    static const juce::Identifier& getModStateType();
    static const juce::Identifier& getCurveType();
    static const juce::Identifier& getPointType();
    static const juce::Identifier& getSlotType();

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override;
    void valueTreeParentChanged (juce::ValueTree&) override;
    void valueTreeRedirected (juce::ValueTree&) override;

    /** Creates the MODSTATE branch and its children if they are missing, so
        every accessor below can assume they exist. */
    juce::ValueTree getOrCreateModState();
    juce::ValueTree getOrCreateCurveTree (std::size_t lfoIndex);
    juce::ValueTree getOrCreateSlotTree (std::size_t slotIndex);

    void rebuildFromTree();
    void publish();

    juce::AudioProcessorValueTreeState& apvts;

    /** The message thread's authoritative copy. Built from the tree, then
        copied into a snapshot slot for the audio thread. */
    Snapshot editState {};

    static constexpr int kNumSnapshots = 3;

    std::array<Snapshot, kNumSnapshots> snapshots {};
    std::atomic<int> liveSlot { 0 };

    /** Message thread only. */
    int nextSlot = 1;

    /** Set while this class is the one writing the tree, so its own edits do
        not cost a full rebuild per property. */
    bool isWritingTree = false;
};

} // namespace gnarl::params
