#pragma once

#include "FactoryBank.h"

#include <vector>

namespace gnarl::preset
{

/**
    The generated half of the factory bank.

    WHY GENERATED AND NOT WRITTEN OUT. The hand-written presets in
    `FactoryBank.cpp` each exist to exercise a specific part of the
    architecture, and every one says in a comment what it is for. That is worth
    doing twenty-six times and worthless a hundred and twenty-four times: what
    a bank of that size actually needs is COVERAGE of the space the first
    twenty-six map out - the same growl at eight articulations, the same sub at
    six weights, the same screech through four filters - and a list of
    variations is a thing to describe once rather than to type out.

    So each archetype here declares its skeleton and the axes it varies along,
    and the variants are drawn from curated tables rather than from free
    randomness. "Random" over the whole parameter space produces patches that
    are silent, clipped, or the same patch twice; these are random only in
    which of several MUSICALLY VALID values each axis takes.

    DETERMINISTIC. The generator is a fixed arithmetic sequence with a fixed
    seed, so the bank is byte-identical from build to build. A preset stores an
    index into this list, so a bank that reshuffled itself between versions
    would silently repoint every saved reference to it.

    WHAT THE REFERENCE TRACKS CHANGED. Eight were measured (band balance,
    crest factor, and the modulation rate of the 220-1200 Hz band, which is
    where formant movement lives). The findings that moved numbers here:

      - The sub carries 50-81% of the total energy. Every bass archetype
        therefore ships with the sub oscillator ON and loud, and the "Sub"
        category exists at all because of it.
      - The growl band is 2-21%, so the formant content sits UNDER the sub
        rather than over it.
      - Articulation clusters at 1/8 and 1/8 triplet (4.7-8.2 Hz at the ~144
        BPM those tracks run at), so the rate tables are weighted there, with
        1/16 and 1/16 triplet as the faster minority.
      - Crest factors of 9-13 dB mean heavy limiting, so the OTT and the
        limiter are on in nearly everything.

    NOTHING WAS SAMPLED. The tracks were measured for characteristics; no
    audio, spectrum or wavetable is derived from them (CLAUDE.md section 9).
*/
std::vector<FactoryBank::Definition> generateVariations();

} // namespace gnarl::preset
