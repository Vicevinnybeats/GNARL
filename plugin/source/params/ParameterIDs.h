#pragma once

#include <juce_core/juce_core.h>

/**
    Compile-time parameter identifiers.

    HARD RULE for the whole codebase: a parameter ID is never written as a
    string literal outside this file. A typo in a literal is a silent bug that
    only shows up as a preset that no longer recalls, months later.

    IDs are frozen once shipped. Renaming one breaks every saved preset and
    every host automation lane pointing at it. To retire a parameter, leave the
    ID in place and stop reading it.

    Versioning: bump kStateVersion whenever the meaning (not the name) of an
    existing parameter changes, and migrate in PluginProcessor::setStateInformation.
*/
namespace gnarl::pid
{
    /** Version stamped into saved state. Read on load to drive migrations. */
    inline constexpr int kStateVersion = 1;

    // --- Global ------------------------------------------------------------
    inline constexpr auto masterGain = "master_gain";
    inline constexpr auto bypass     = "bypass";

    // Phase 1 fills in the full layout (oscillators, filters, envelopes, LFOs,
    // mod matrix, FX). Declare IDs here before using them anywhere.
}
