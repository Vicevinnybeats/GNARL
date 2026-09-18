#include "ModStateBridge.h"

namespace gnarl::params
{

namespace
{
    const juce::Identifier kIndexProperty { "index" };
    const juce::Identifier kDestinationProperty { "destination" };
    const juce::Identifier kTimeProperty { "time" };
    const juce::Identifier kValueProperty { "value" };
    const juce::Identifier kTensionProperty { "tension" };
    const juce::Identifier kShapeProperty { "shape" };
}

const juce::Identifier& ModStateBridge::getModStateType()
{
    static const juce::Identifier type { "MODSTATE" };
    return type;
}

const juce::Identifier& ModStateBridge::getCurveType()
{
    static const juce::Identifier type { "LFOCURVE" };
    return type;
}

const juce::Identifier& ModStateBridge::getPointType()
{
    static const juce::Identifier type { "POINT" };
    return type;
}

const juce::Identifier& ModStateBridge::getSlotType()
{
    static const juce::Identifier type { "MODSLOT" };
    return type;
}

ModStateBridge::ModStateBridge (juce::AudioProcessorValueTreeState& state)
    : apvts (state)
{
    refresh();

    // Listening rather than relying on every editor going through this class:
    // the preset system and the host's own state handling both write the tree
    // directly, and a curve that only reached the engine when the UI happened
    // to call us would be a silent bug in exactly those paths.
    apvts.state.addListener (this);
}

ModStateBridge::~ModStateBridge()
{
    apvts.state.removeListener (this);
}

void ModStateBridge::refresh()
{
    rebuildFromTree();
    publish();
}

juce::ValueTree ModStateBridge::getOrCreateModState()
{
    auto modState = apvts.state.getOrCreateChildWithName (getModStateType(), nullptr);
    return modState;
}

juce::ValueTree ModStateBridge::getOrCreateCurveTree (std::size_t lfoIndex)
{
    auto modState = getOrCreateModState();
    const auto index = static_cast<int> (lfoIndex);

    for (auto child : modState)
        if (child.hasType (getCurveType()) && static_cast<int> (child[kIndexProperty]) == index)
            return child;

    juce::ValueTree curve { getCurveType() };
    curve.setProperty (kIndexProperty, index, nullptr);
    modState.appendChild (curve, nullptr);

    return curve;
}

juce::ValueTree ModStateBridge::getOrCreateSlotTree (std::size_t slotIndex)
{
    auto modState = getOrCreateModState();
    const auto index = static_cast<int> (slotIndex);

    for (auto child : modState)
        if (child.hasType (getSlotType()) && static_cast<int> (child[kIndexProperty]) == index)
            return child;

    juce::ValueTree slot { getSlotType() };
    slot.setProperty (kIndexProperty, index, nullptr);
    modState.appendChild (slot, nullptr);

    return slot;
}

void ModStateBridge::setCurve (std::size_t lfoIndex, const dsp::LfoCurve& curve)
{
    if (lfoIndex >= pid::kNumLfos)
        return;

    const juce::ScopedValueSetter<bool> writing (isWritingTree, true);

    auto tree = getOrCreateCurveTree (lfoIndex);
    tree.removeAllChildren (nullptr);

    for (int i = 0; i < curve.getNumPoints(); ++i)
    {
        const auto& point = curve.getPoint (i);

        juce::ValueTree pointTree { getPointType() };
        pointTree.setProperty (kTimeProperty, point.time, nullptr);
        pointTree.setProperty (kValueProperty, point.value, nullptr);
        pointTree.setProperty (kTensionProperty, point.tension, nullptr);
        pointTree.setProperty (kShapeProperty, static_cast<int> (point.shape), nullptr);

        tree.appendChild (pointTree, nullptr);
    }

    editState.curves[lfoIndex] = curve;
    publish();
}

dsp::LfoCurve ModStateBridge::getCurve (std::size_t lfoIndex) const
{
    if (lfoIndex >= pid::kNumLfos)
        return {};

    return editState.curves[lfoIndex];
}

void ModStateBridge::setDestination (std::size_t slotIndex, const juce::String& parameterID)
{
    if (slotIndex >= pid::kNumModSlots)
        return;

    const juce::ScopedValueSetter<bool> writing (isWritingTree, true);

    getOrCreateSlotTree (slotIndex).setProperty (kDestinationProperty, parameterID, nullptr);

    editState.destinations[slotIndex] = dsp::getDestinationForParameterID (parameterID);
    publish();
}

juce::String ModStateBridge::getDestinationParameterID (std::size_t slotIndex) const
{
    if (slotIndex >= pid::kNumModSlots)
        return {};

    return dsp::getParameterIDForDestination (editState.destinations[slotIndex]);
}

void ModStateBridge::rebuildFromTree()
{
    // Defaults first, so a tree with nothing in it - a fresh instance - gives
    // every LFO the falling ramp rather than a flat line.
    for (auto& curve : editState.curves)
        curve.setDefaultRamp();

    editState.destinations.fill (dsp::ModDestination::none);

    const auto modState = apvts.state.getChildWithName (getModStateType());

    if (! modState.isValid())
        return;

    for (auto child : modState)
    {
        const auto index = static_cast<int> (child[kIndexProperty]);

        if (child.hasType (getCurveType()))
        {
            if (index < 0 || index >= static_cast<int> (pid::kNumLfos))
                continue;

            // An empty curve element means "the default", not "silence": a
            // preset saved before a curve was ever touched should still open
            // with a usable shape.
            if (child.getNumChildren() == 0)
                continue;

            dsp::LfoCurve curve;
            curve.clear();

            for (auto pointTree : child)
            {
                if (! pointTree.hasType (getPointType()))
                    continue;

                dsp::LfoCurve::Point point {};
                point.time = juce::jlimit (0.0f, 1.0f,
                    static_cast<float> (static_cast<double> (pointTree[kTimeProperty])));
                point.value = juce::jlimit (0.0f, 1.0f,
                    static_cast<float> (static_cast<double> (pointTree[kValueProperty])));
                point.tension = juce::jlimit (-1.0f, 1.0f,
                    static_cast<float> (static_cast<double> (pointTree[kTensionProperty])));
                point.shape = static_cast<int> (pointTree[kShapeProperty]) == 1
                    ? dsp::LfoCurve::Shape::step
                    : dsp::LfoCurve::Shape::curved;

                curve.addPoint (point);
            }

            // A curve with nothing usable in it would evaluate to zero at
            // every phase, which looks like a broken LFO rather than a broken
            // preset. Fall back instead.
            if (curve.getNumPoints() == 0)
                curve.setDefaultRamp();

            editState.curves[static_cast<std::size_t> (index)] = curve;
        }
        else if (child.hasType (getSlotType()))
        {
            if (index < 0 || index >= static_cast<int> (pid::kNumModSlots))
                continue;

            editState.destinations[static_cast<std::size_t> (index)] =
                dsp::getDestinationForParameterID (child[kDestinationProperty].toString());
        }
    }
}

void ModStateBridge::publish()
{
    const auto slot = nextSlot;

    snapshots[static_cast<std::size_t> (slot)] = editState;

    liveSlot.store (slot, std::memory_order_release);

    nextSlot = (slot + 1) % kNumSnapshots;
}

void ModStateBridge::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier&)
{
    if (isWritingTree)
        return;

    // Parameter changes come through here too, and there are 316 of them.
    // Rebuilding the whole modulation state on every knob turn would make
    // automation cost a tree walk per sample-accurate event.
    if (tree.hasType (getModStateType()) || tree.hasType (getCurveType())
        || tree.hasType (getPointType()) || tree.hasType (getSlotType()))
    {
        refresh();
    }
}

void ModStateBridge::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child)
{
    if (isWritingTree)
        return;

    if (parent.hasType (getModStateType()) || child.hasType (getModStateType())
        || parent.hasType (getCurveType()))
    {
        refresh();
    }
}

void ModStateBridge::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int)
{
    if (isWritingTree)
        return;

    if (parent.hasType (getModStateType()) || child.hasType (getModStateType())
        || parent.hasType (getCurveType()))
    {
        refresh();
    }
}

void ModStateBridge::valueTreeChildOrderChanged (juce::ValueTree& parent, int, int)
{
    if (! isWritingTree && parent.hasType (getModStateType()))
        refresh();
}

void ModStateBridge::valueTreeParentChanged (juce::ValueTree&)
{
}

void ModStateBridge::valueTreeRedirected (juce::ValueTree&)
{
    // The whole tree was replaced - a preset load, or the host restoring
    // state. Everything we hold is stale.
    refresh();
}

} // namespace gnarl::params
