#pragma once

#include "Envelope.h"
#include "Lfo.h"
#include "Modulation.h"
#include "../params/ParameterChoices.h"
#include "../params/ParameterIDs.h"

#include <array>

namespace gnarl::dsp
{

/**
    The modulation matrix: 16 slots routing sources to destinations.

    Evaluated PER VOICE PER MODULATION CHUNK, not per block. Envelopes and
    trigger-mode LFOs are per-voice, so a single global evaluation would make
    every voice share one envelope - which is not a polyphonic synth.

    Each slot has a secondary "amount" modulator, which is what lets an LFO be
    faded in by an envelope without burning a second slot. That pattern is
    common enough in this genre that making it cost two of sixteen slots would
    be a real limitation.
*/
class ModMatrix
{
public:
    struct Slot
    {
        bool enabled = false;
        choices::ModSource source = choices::ModSource::none;
        ModDestination destination = ModDestination::none;
        float depth = 0.0f;             // -1..1
        choices::ModCurve curve = choices::ModCurve::linear;
        choices::ModSource auxSource = choices::ModSource::none;
        float auxAmount = 0.0f;         // 0..1
        bool bipolar = true;
    };

    /** The per-voice values every source can supply, gathered once per chunk
        so a slot lookup is an array index rather than a switch. */
    struct SourceValues
    {
        std::array<float, static_cast<std::size_t> (choices::ModSource::count)> values {};

        void clear() noexcept { values.fill (0.0f); }

        void set (choices::ModSource source, float value) noexcept
        {
            values[static_cast<std::size_t> (source)] = value;
        }

        float get (choices::ModSource source) const noexcept
        {
            return values[static_cast<std::size_t> (source)];
        }
    };

    void setSlot (std::size_t index, const Slot& slot) noexcept
    {
        if (index < slots.size())
            slots[index] = slot;
    }

    const Slot& getSlot (std::size_t index) const noexcept
    {
        return slots[juce::jmin (index, slots.size() - 1)];
    }

    /** Applies every enabled slot, accumulating into `offsets`. */
    void apply (const SourceValues& sources, ModulationOffsets& offsets) const noexcept
    {
        for (const auto& slot : slots)
        {
            if (! slot.enabled
                || slot.source == choices::ModSource::none
                || slot.destination == ModDestination::none)
            {
                continue;
            }

            auto value = sources.get (slot.source);

            // Sources are 0..1; a bipolar slot re-centres them so the
            // destination is modulated either side of its set value rather
            // than only upwards. That is the difference between a wobble that
            // dips and one that only ever pushes the parameter up.
            if (slot.bipolar)
                value = value * 2.0f - 1.0f;

            value = applyCurve (value, slot.curve);

            auto depth = slot.depth;

            // The secondary modulator scales this slot's depth.
            if (slot.auxSource != choices::ModSource::none && slot.auxAmount > 0.0f)
            {
                const auto aux = juce::jlimit (0.0f, 1.0f, sources.get (slot.auxSource));
                depth *= 1.0f - slot.auxAmount * (1.0f - aux);
            }

            offsets.add (slot.destination, value * depth);
        }
    }

    static constexpr std::size_t getNumSlots() { return pid::kNumModSlots; }

private:
    /** Shapes a source before it scales the depth. Curves are applied to the
        MAGNITUDE so a bipolar source keeps its sign - an exponential curve
        that flipped the negative half would be a different modulation, not a
        shaped one. */
    static float applyCurve (float value, choices::ModCurve curve) noexcept
    {
        const auto sign = value < 0.0f ? -1.0f : 1.0f;
        const auto magnitude = juce::jlimit (0.0f, 1.0f, std::abs (value));

        switch (curve)
        {
            case choices::ModCurve::linear:
                return value;

            case choices::ModCurve::exponential:
                return sign * magnitude * magnitude;

            case choices::ModCurve::logarithmic:
                return sign * std::sqrt (magnitude);

            case choices::ModCurve::sCurve:
                return sign * magnitude * magnitude * (3.0f - 2.0f * magnitude);

            case choices::ModCurve::quantize:
                // Eight steps: enough to be obviously stepped, few enough to
                // be musically useful as a sequencer-like effect.
                return sign * std::floor (magnitude * 8.0f) / 8.0f;

            case choices::ModCurve::count:
            default:
                return value;
        }
    }

    std::array<Slot, pid::kNumModSlots> slots {};
};

} // namespace gnarl::dsp
