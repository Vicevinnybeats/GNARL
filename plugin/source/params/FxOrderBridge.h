#pragma once

#include "../dsp/FxChain.h"
#include "ParameterChoices.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <array>

namespace gnarl::params
{

/**
    Carries the FX chain's ORDER from the ValueTree to the audio thread.

    The same shape as ModStateBridge, and for the same reason: the order is not
    a parameter (see docs/fx-architecture.md and FxOrder), so it lives in the
    ValueTree, and the audio thread cannot read a ValueTree - that would mean
    taking a lock and allocating. It is built on the message thread and
    published through a rotating set of snapshots with an atomic index.

    THE ORDER IS STORED AS SLOT NAMES, NOT INDICES. A preset holding
    "Reverb, Limiter, ..." still means the same chain if the enum ever gains an
    entry; a preset holding "12, 13, ..." would silently mean something else.
    The choice-list order is frozen anyway, but the chain order is the one
    piece of state where a name costs nothing - it is written once on a
    reorder, not per block.

    A MALFORMED ORDER FALLS BACK TO THE DEFAULT rather than being patched up.
    FxOrder::setOrder rejects anything that is not a permutation, because a
    chain that runs one effect twice and another never is not a chain that can
    exist - and a hand-edited or truncated preset must not be able to produce
    one.
*/
class FxOrderBridge : private juce::ValueTree::Listener
{
public:
    using Slot = choices::FxSlot;

    static constexpr int kNumSnapshots = 3;

    /** Takes the APVTS rather than its ValueTree, and that is not a
        convenience. `replaceState` swaps the tree for a DIFFERENT object, so a
        bridge holding a copy of the old one keeps listening to a tree nothing
        writes to any more - the order then survives in the saved state and
        silently fails to come back on load, which is exactly what the round
        trip test caught. Going through the APVTS means every read sees
        whatever tree is current. */
    explicit FxOrderBridge (juce::AudioProcessorValueTreeState& apvtsToUse)
        : apvts (apvtsToUse)
    {
        apvts.state.addListener (this);
        refresh();
    }

    ~FxOrderBridge() override { apvts.state.removeListener (this); }

    /** AUDIO THREAD. The live order. Never blocks and never allocates. */
    const dsp::FxOrder& getOrder() const noexcept
    {
        return snapshots[static_cast<std::size_t> (liveSlot.load (std::memory_order_acquire))];
    }

    /** MESSAGE THREAD. Re-reads the tree and publishes.

        Also re-attaches the listener, because `replaceState` gives the APVTS a
        new tree and the old one's listener list does not come with it. Adding
        a listener twice is a no-op in JUCE, so this is safe to call on every
        refresh. */
    void refresh()
    {
        apvts.state.addListener (this);

        dsp::FxOrder built;

        auto branch = apvts.state.getChildWithName (kOrderType);

        if (branch.isValid())
        {
            const auto text = branch.getProperty (kSlotsProperty).toString();

            std::array<Slot, dsp::FxOrder::kNumSlots> parsed {};
            auto count = std::size_t { 0 };
            auto wellFormed = true;

            for (const auto& name : juce::StringArray::fromTokens (text, "|", ""))
            {
                const auto index = choices::fxSlotName.indexOf (name.trim());

                if (index < 0 || count >= parsed.size())
                {
                    wellFormed = false;
                    break;
                }

                parsed[count++] = static_cast<Slot> (index);
            }

            // setOrder does the permutation check itself; this only has to
            // catch a name that is not a slot at all, and a list of the wrong
            // length.
            if (wellFormed && count == parsed.size())
                built.setOrder (parsed);
        }

        publish (built);
    }

    /** MESSAGE THREAD. Writes an order into the tree, which republishes
        through the listener. Returns false and changes nothing if the order is
        not a permutation. */
    bool setOrder (const std::array<Slot, dsp::FxOrder::kNumSlots>& newOrder)
    {
        dsp::FxOrder candidate;

        if (! candidate.setOrder (newOrder))
            return false;

        juce::StringArray names;

        for (std::size_t i = 0; i < dsp::FxOrder::kNumSlots; ++i)
            names.add (choices::fxSlotName[static_cast<int> (candidate.getSlot (i))]);

        auto branch = apvts.state.getOrCreateChildWithName (kOrderType, nullptr);
        branch.setProperty (kSlotsProperty, names.joinIntoString ("|"), nullptr);

        return true;
    }

    /** MESSAGE THREAD. Moves one slot to a new position, which is what a drag
        in the UI does. */
    bool move (std::size_t from, std::size_t to)
    {
        if (from >= dsp::FxOrder::kNumSlots || to >= dsp::FxOrder::kNumSlots)
            return false;

        // A copy, so a rejected write cannot leave the live order half moved.
        auto current = getOrder();
        current.move (from, to);

        return setOrder (current.getSlots());
    }

    static const juce::Identifier kOrderType;
    static const juce::Identifier kSlotsProperty;

private:
    void publish (const dsp::FxOrder& order)
    {
        const auto current = liveSlot.load (std::memory_order_relaxed);
        const auto next = (current + 1) % kNumSnapshots;

        snapshots[static_cast<std::size_t> (next)] = order;

        // Release, so the audio thread cannot see the new index before the
        // snapshot it points at.
        liveSlot.store (next, std::memory_order_release);
    }

    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier&) override
    {
        if (tree.hasType (kOrderType))
            refresh();
    }

    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& child) override
    {
        if (child.hasType (kOrderType))
            refresh();
    }

    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& child, int) override
    {
        if (child.hasType (kOrderType))
            refresh();
    }

    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override {}

    juce::AudioProcessorValueTreeState& apvts;

    std::array<dsp::FxOrder, kNumSnapshots> snapshots {};
    std::atomic<int> liveSlot { 0 };
};

inline const juce::Identifier FxOrderBridge::kOrderType { "FXORDER" };
inline const juce::Identifier FxOrderBridge::kSlotsProperty { "slots" };

} // namespace gnarl::params
