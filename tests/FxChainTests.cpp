#include <catch2/catch_test_macros.hpp>

#include "dsp/FxChain.h"
#include "params/ParameterChoices.h"
#include "params/ParameterIDs.h"

using namespace gnarl;
using namespace gnarl::dsp;
using Slot = choices::FxSlot;

TEST_CASE ("The roster, the enum and the declared count agree", "[fx]")
{
    /*  Three places say how many effect instances there are: the FxSlot enum,
        pid::kNumFxInstances, and the name list the UI draws. They are used to
        index each other, so a disagreement is an out-of-range read or a row
        with no label - and neither announces itself.
    */
    CHECK (static_cast<std::size_t> (Slot::count) == pid::kNumFxInstances);
    CHECK (static_cast<std::size_t> (choices::fxSlotName.size()) == pid::kNumFxInstances);
    CHECK (FxOrder::kNumSlots == pid::kNumFxInstances);
}

TEST_CASE ("A fresh order is the default chain", "[fx]")
{
    const FxOrder order;

    REQUIRE (order.isDefault());

    // The enum's order IS the default, so position i holds effect i.
    for (std::size_t i = 0; i < FxOrder::kNumSlots; ++i)
        CHECK (order.getSlot (i) == static_cast<Slot> (i));
}

TEST_CASE ("Moving a slot shifts the rest rather than swapping", "[fx]")
{
    // Dragging a row in a rack SHIFTS the rows it passes; swapping the two
    // ends would reorder two things when the user asked to reorder one.
    FxOrder order;

    order.move (0, 3);

    CHECK (order.getSlot (0) == Slot::eq1);
    CHECK (order.getSlot (1) == Slot::filter1);
    CHECK (order.getSlot (2) == Slot::distortion2);
    CHECK (order.getSlot (3) == Slot::distortion1);
    CHECK (order.getSlot (4) == Slot::eq2);

    // And back the other way.
    order.move (3, 0);
    CHECK (order.isDefault());
}

TEST_CASE ("Moving is still a permutation afterwards", "[fx]")
{
    FxOrder order;

    order.move (13, 0);
    order.move (5, 9);
    order.move (2, 11);

    std::array<bool, FxOrder::kNumSlots> seen {};

    for (std::size_t i = 0; i < FxOrder::kNumSlots; ++i)
    {
        const auto index = static_cast<std::size_t> (order.getSlot (i));

        REQUIRE (index < FxOrder::kNumSlots);
        CHECK_FALSE (seen[index]);
        seen[index] = true;
    }

    for (const auto present : seen)
        CHECK (present);
}

TEST_CASE ("Out-of-range and no-op moves change nothing", "[fx]")
{
    FxOrder order;

    order.move (0, 0);
    order.move (FxOrder::kNumSlots, 0);
    order.move (0, FxOrder::kNumSlots + 5);

    CHECK (order.isDefault());
}

TEST_CASE ("A malformed order is rejected rather than applied", "[fx]")
{
    /*  A preset from a later build, or a corrupted one, must not be able to
        produce a chain where one effect runs twice and another never runs.
        Rejecting leaves the previous order in place, which is always valid.
    */
    FxOrder order;

    std::array<Slot, FxOrder::kNumSlots> duplicated {};
    duplicated.fill (Slot::reverb);

    CHECK_FALSE (order.setOrder (duplicated));
    CHECK (order.isDefault());

    auto outOfRange = order.getSlots();
    outOfRange[4] = static_cast<Slot> (FxOrder::kNumSlots + 2);

    CHECK_FALSE (order.setOrder (outOfRange));
    CHECK (order.isDefault());
}

TEST_CASE ("A valid order is applied whole", "[fx]")
{
    FxOrder order;

    auto reversed = order.getSlots();

    for (std::size_t i = 0; i < FxOrder::kNumSlots; ++i)
        reversed[i] = static_cast<Slot> (FxOrder::kNumSlots - 1 - i);

    REQUIRE (order.setOrder (reversed));
    CHECK_FALSE (order.isDefault());
    CHECK (order.getSlot (0) == Slot::limiter);
    CHECK (order.getSlot (FxOrder::kNumSlots - 1) == Slot::distortion1);
}

TEST_CASE ("Every effect can be found by position", "[fx]")
{
    FxOrder order;
    order.move (9, 1);
    order.move (0, 7);

    for (std::size_t i = 0; i < FxOrder::kNumSlots; ++i)
    {
        const auto slot = static_cast<Slot> (i);
        const auto position = order.getPositionOf (slot);

        REQUIRE (position < FxOrder::kNumSlots);
        CHECK (order.getSlot (position) == slot);
    }
}
