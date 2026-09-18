#pragma once

#include "../params/ParameterChoices.h"

namespace gnarl::dsp
{

/**
    Tempo-synced rate divisions, in beats per LFO cycle.

    TRIPLETS AND DOTTED RATES ARE FIRST-CLASS entries interleaved with the
    straight ones, not a separate mode behind a toggle. Riddim is built on the
    1/3 and 1/6 grids, so reaching a triplet rate has to cost exactly as much
    as reaching a straight one - this is the single most genre-specific
    decision in the whole parameter layout.

    A "beat" is a quarter note, which is what a host reports in its PPQ
    position, so these numbers go straight into the transport phase
    calculation with no conversion.
*/
namespace sync
{
    using Division = choices::LfoRateDivision;

    /** Beats per cycle for a division. A larger number is a slower LFO. */
    inline constexpr double getBeatsPerCycle (Division division) noexcept
    {
        switch (division)
        {
            case Division::bars8:               return 32.0;
            case Division::bars4:               return 16.0;
            case Division::bars2:               return 8.0;
            case Division::whole:               return 4.0;

            // Dotted is one and a half times the straight value; a triplet is
            // two thirds of it.
            case Division::halfDotted:          return 3.0;
            case Division::half:                return 2.0;
            case Division::halfTriplet:         return 4.0 / 3.0;

            case Division::quarterDotted:       return 1.5;
            case Division::quarter:             return 1.0;
            case Division::quarterTriplet:      return 2.0 / 3.0;

            case Division::eighthDotted:        return 0.75;
            case Division::eighth:              return 0.5;
            case Division::eighthTriplet:       return 1.0 / 3.0;

            case Division::sixteenthDotted:     return 0.375;
            case Division::sixteenth:           return 0.25;
            case Division::sixteenthTriplet:    return 1.0 / 6.0;

            case Division::thirtySecondDotted:  return 0.1875;
            case Division::thirtySecond:        return 0.125;
            case Division::thirtySecondTriplet: return 1.0 / 12.0;

            case Division::sixtyFourth:         return 0.0625;

            case Division::count:
            default:                            return 0.5;
        }
    }

    /** True for the triplet divisions. The editor draws their grid
        differently, because mistaking a triplet grid for a straight one
        produces a wobble that is subtly out of time in a way that is very
        hard to diagnose by ear. */
    inline constexpr bool isTriplet (Division division) noexcept
    {
        return division == Division::halfTriplet
            || division == Division::quarterTriplet
            || division == Division::eighthTriplet
            || division == Division::sixteenthTriplet
            || division == Division::thirtySecondTriplet;
    }

    inline constexpr bool isDotted (Division division) noexcept
    {
        return division == Division::halfDotted
            || division == Division::quarterDotted
            || division == Division::eighthDotted
            || division == Division::sixteenthDotted
            || division == Division::thirtySecondDotted;
    }

    /** Cycles per second for a division at a tempo. */
    inline double getFrequencyHz (Division division, double bpm) noexcept
    {
        const auto beats = getBeatsPerCycle (division);

        if (beats <= 0.0 || bpm <= 0.0)
            return 0.0;

        return (bpm / 60.0) / beats;
    }

    /**
        Steps the grid divides ONE LFO CYCLE into.

        THE GRID DIVIDES THE CYCLE, NOT THE BAR. "1/16" means sixteen steps
        across the LFO's own cycle, whatever rate that cycle is set to - which
        is how Serum and Vital both read their LFO grids, and the only reading
        that makes the editor drawable.

        The tempo-relative reading is defensible on paper and useless in
        practice: with the default 1/8 cycle, a "1/16" grid measured in beats
        is two steps, so a drag snaps to the start, the middle and the end and
        no shape can be drawn at all. The cycle is itself tempo-synced, so
        dividing it still lands every step on a musical subdivision.

        1/12 and 1/24 are the triplet grids and give twelve and twenty-four
        steps, which is the triplet feel this genre is built on.
    */
    inline constexpr int getGridStepsPerCycle (choices::GridDivision grid) noexcept
    {
        switch (grid)
        {
            case choices::GridDivision::quarter:      return 4;
            case choices::GridDivision::eighth:       return 8;
            case choices::GridDivision::twelfth:      return 12;
            case choices::GridDivision::sixteenth:    return 16;
            case choices::GridDivision::twentyFourth: return 24;
            case choices::GridDivision::thirtySecond: return 32;

            case choices::GridDivision::off:
            case choices::GridDivision::count:
            default:                                  return 0;
        }
    }

    /** The grid step as a fraction of one cycle, or 0 when the grid is off. */
    inline constexpr double getGridFraction (choices::GridDivision grid) noexcept
    {
        const auto steps = getGridStepsPerCycle (grid);
        return steps > 0 ? 1.0 / static_cast<double> (steps) : 0.0;
    }

    inline constexpr bool isTripletGrid (choices::GridDivision grid) noexcept
    {
        return grid == choices::GridDivision::twelfth
            || grid == choices::GridDivision::twentyFourth;
    }
}

} // namespace gnarl::dsp
