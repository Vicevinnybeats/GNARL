#pragma once

#include "../params/ParameterChoices.h"
#include "../params/ParameterIDs.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>

namespace gnarl::dsp
{

/**
    The order the FX rack runs its instances in.

    A PERMUTATION, not a set of slots. The rack is a fixed roster of effect
    instances, each with its own named parameters, and this says what order
    they run in - see docs/fx-architecture.md for why it is that way round.

    Why the order is not a parameter: a host parameter is a number, and a
    number indexing a list of effects shifts meaning the moment the list
    changes. It is also not something anyone automates - an automation lane
    that reorders a chain mid-note would be a glitch generator, not a
    feature. So the order lives in the ValueTree and reaches the audio thread
    through a published snapshot, exactly like the drawable LFO curves.

    Fixed capacity and trivially copyable, so publishing one costs a memcpy of
    fourteen bytes and the audio thread never allocates.
*/
class FxOrder
{
public:
    using Slot = choices::FxSlot;

    static constexpr std::size_t kNumSlots = pid::kNumFxInstances;

    FxOrder() { setDefault(); }

    /** The enum's own order: shape and distort, then modulate, then space,
        then catch the peaks. Chosen to be the order that needs the least
        rearranging, so the default chain is useful without being touched. */
    void setDefault() noexcept
    {
        for (std::size_t i = 0; i < kNumSlots; ++i)
            slots[i] = static_cast<Slot> (i);
    }

    Slot getSlot (std::size_t position) const noexcept
    {
        return slots[juce::jmin (position, kNumSlots - 1)];
    }

    /** MESSAGE THREAD. Replaces the whole order.

        Rejects anything that is not a permutation rather than accepting it
        and running some effects twice and others never - a malformed preset
        must not be able to produce a chain that cannot exist. */
    bool setOrder (const std::array<Slot, kNumSlots>& newOrder) noexcept
    {
        std::array<bool, kNumSlots> seen {};

        for (const auto slot : newOrder)
        {
            const auto index = static_cast<std::size_t> (slot);

            if (index >= kNumSlots || seen[index])
                return false;

            seen[index] = true;
        }

        slots = newOrder;
        return true;
    }

    /** MESSAGE THREAD. Moves one slot to a new position, shifting the rest -
        which is what dragging a row in the rack does. */
    void move (std::size_t from, std::size_t to) noexcept
    {
        if (from >= kNumSlots || to >= kNumSlots || from == to)
            return;

        const auto moved = slots[from];

        if (from < to)
            for (auto i = from; i < to; ++i)
                slots[i] = slots[i + 1];
        else
            for (auto i = from; i > to; --i)
                slots[i] = slots[i - 1];

        slots[to] = moved;
    }

    /** Where a given effect currently sits, or kNumSlots if somehow absent.
        The UI needs this to draw an instance's panel next to its row. */
    std::size_t getPositionOf (Slot slot) const noexcept
    {
        for (std::size_t i = 0; i < kNumSlots; ++i)
            if (slots[i] == slot)
                return i;

        return kNumSlots;
    }

    bool isDefault() const noexcept
    {
        for (std::size_t i = 0; i < kNumSlots; ++i)
            if (slots[i] != static_cast<Slot> (i))
                return false;

        return true;
    }

    const std::array<Slot, kNumSlots>& getSlots() const noexcept { return slots; }

private:
    std::array<Slot, kNumSlots> slots {};
};

static_assert (std::is_trivially_copyable_v<std::array<choices::FxSlot, pid::kNumFxInstances>>,
               "The order is published to the audio thread by value, so it has to be "
               "trivially copyable - see the handover rule in CLAUDE.md section 3.");

} // namespace gnarl::dsp
