#pragma once

#include <juce_core/juce_core.h>

#include <array>

/**
    Choice lists for GNARL's discrete parameters.

    HARD RULE: choice ORDER is frozen once shipped. A preset stores the chosen
    index, not the name, so inserting an entry in the middle silently changes
    every existing preset that used a later entry. Append only.

    The enums mirror the arrays. Keep them in step: the static_asserts at the
    bottom of this file will fail if they drift.
*/
namespace gnarl::choices
{

// --- Oscillator mode -------------------------------------------------------
enum class OscMode
{
    wavetable = 0,
    graintable,
    count
};

inline const juce::StringArray oscMode { "Wavetable", "Graintable" };

// --- Warp modes ------------------------------------------------------------
/** Applied to the phase or the table read before the table lookup. Eleven
    modes is the minimum that makes the oscillator section feel deep enough to
    compete; `off` is a twelfth, and the default. */
enum class WarpMode
{
    off = 0,
    sync,
    bendPlus,
    bendMinus,
    pwm,
    asymmetry,
    mirror,
    quantize,       // bitcrushes the waveform itself, not the output
    fmFromOther,    // FM from the other main oscillator
    ringMod,
    phaseDistortion,
    remap,
    count
};

inline const juce::StringArray warpMode {
    "Off", "Sync", "Bend +", "Bend -", "PWM", "Asym",
    "Mirror", "Quantize", "FM", "Ring Mod", "Phase Dist", "Remap"
};

// --- Sub oscillator --------------------------------------------------------
enum class SubWaveform
{
    sine = 0,
    triangle,
    saw,
    square,
    pulse,
    count
};

inline const juce::StringArray subWaveform { "Sine", "Triangle", "Saw", "Square", "Pulse" };

// --- Noise -----------------------------------------------------------------
enum class NoiseType
{
    white = 0,
    pink,
    brown,
    blue,
    vinyl,
    count
};

inline const juce::StringArray noiseType { "White", "Pink", "Brown", "Blue", "Vinyl" };

// --- Filters ---------------------------------------------------------------
/** One flat list rather than a type + slope pair: a single dropdown is fewer
    clicks, and it lets the ladder, comb and formant filters sit alongside the
    SVF modes instead of needing their own selector. */
enum class FilterType
{
    lowPass12 = 0,
    lowPass24,
    highPass12,
    highPass24,
    bandPass12,
    bandPass24,
    notch12,
    notch24,
    ladderLowPass,
    ladderHighPass,
    comb,
    formant,        // the vowel filter - the one riddim lives on
    count
};

inline const juce::StringArray filterType {
    "LP 12", "LP 24", "HP 12", "HP 24", "BP 12", "BP 24",
    "Notch 12", "Notch 24", "Ladder LP", "Ladder HP", "Comb", "Formant"
};

enum class FilterRouting
{
    series = 0,
    parallel,
    split,
    count
};

inline const juce::StringArray filterRouting { "Series", "Parallel", "Split" };

/** Saturation curve for a filter's drive stage. */
enum class DriveCurve
{
    tanh = 0,
    tube,
    hardClip,
    fold,
    rectify,
    count
};

inline const juce::StringArray driveCurve { "Tanh", "Tube", "Hard Clip", "Fold", "Rectify" };

// --- Envelopes -------------------------------------------------------------
enum class EnvelopeMode
{
    adsr = 0,
    dahdsr,
    count
};

inline const juce::StringArray envelopeMode { "ADSR", "DAHDSR" };

// --- LFOs ------------------------------------------------------------------
enum class LfoShape
{
    custom = 0,     // the drawn breakpoint curve - the default and the point
    sine,
    triangle,
    sawUp,
    sawDown,
    square,
    randomStep,
    randomSmooth,
    count
};

inline const juce::StringArray lfoShape {
    "Custom", "Sine", "Triangle", "Saw Up", "Saw Down", "Square",
    "Random Step", "Random Smooth"
};

enum class LfoMode
{
    trigger = 0,    // restarts on each note
    envelope,       // one-shot, does not loop
    freeRun,        // locked to the host timeline, not the note
    sampleAndHold,
    count
};

inline const juce::StringArray lfoMode { "Trigger", "Envelope", "Free Run", "S&H" };

/** Tempo-synced LFO rates.
    Triplet and dotted variants are FIRST-CLASS entries interleaved with the
    straight rates, not a separate mode behind a toggle. Riddim is built on the
    1/3 and 1/6 grids, so reaching a triplet rate must cost exactly as much as
    reaching a straight one. Order runs slow to fast. */
enum class LfoRateDivision
{
    bars8 = 0, bars4, bars2,
    whole,
    halfDotted, half, halfTriplet,
    quarterDotted, quarter, quarterTriplet,
    eighthDotted, eighth, eighthTriplet,
    sixteenthDotted, sixteenth, sixteenthTriplet,
    thirtySecondDotted, thirtySecond, thirtySecondTriplet,
    sixtyFourth,
    count
};

inline const juce::StringArray lfoRateDivision {
    "8 Bars", "4 Bars", "2 Bars",
    "1/1",
    "1/2 D", "1/2", "1/2 T",
    "1/4 D", "1/4", "1/4 T",
    "1/8 D", "1/8", "1/8 T",
    "1/16 D", "1/16", "1/16 T",
    "1/32 D", "1/32", "1/32 T",
    "1/64"
};

/** Editor snap grid for the drawn LFO shape.
    1/12 and 1/24 are the triplet grids; they are drawn visually distinct from
    the straight ones in the editor. */
enum class GridDivision
{
    off = 0,
    quarter,        // 1/4
    eighth,         // 1/8
    twelfth,        // 1/12 - triplet
    sixteenth,      // 1/16
    twentyFourth,   // 1/24 - triplet
    thirtySecond,   // 1/32
    count
};

inline const juce::StringArray gridDivision {
    "Off", "1/4", "1/8", "1/12", "1/16", "1/24", "1/32"
};

// --- Modulation ------------------------------------------------------------
/** Mod matrix sources. Append only.

    Note-rate sources are evaluated once when a voice starts; the rest are
    block-rate, except the oscillators used as audio-rate FM sources, which are
    sample-rate. See CLAUDE.md section 5. */
enum class ModSource
{
    none = 0,
    env1, env2, env3, env4,
    lfo1, lfo2, lfo3, lfo4,
    velocity,           // note-rate
    noteNumber,         // note-rate
    noteOnRandom,       // note-rate
    unisonVoiceIndex,   // note-rate
    modWheel,
    pitchBend,
    aftertouch,
    macro1, macro2, macro3, macro4,
    count
};

inline const juce::StringArray modSource {
    "None",
    "Env 1", "Env 2", "Env 3", "Env 4",
    "LFO 1", "LFO 2", "LFO 3", "LFO 4",
    "Velocity", "Note", "Random", "Uni Voice",
    "Mod Wheel", "Pitch Bend", "Aftertouch",
    "Macro 1", "Macro 2", "Macro 3", "Macro 4"
};

/** Shape applied to a mod slot's value before it scales the depth. */
enum class ModCurve
{
    linear = 0,
    exponential,
    logarithmic,
    sCurve,
    quantize,
    count
};

inline const juce::StringArray modCurve {
    "Linear", "Exp", "Log", "S-Curve", "Quantize"
};

// --- Global ----------------------------------------------------------------
enum class PolyMode
{
    poly = 0,
    mono,
    legato,
    count
};

inline const juce::StringArray polyMode { "Poly", "Mono", "Legato" };

enum class Oversampling
{
    off = 0,
    twoTimes,
    fourTimes,
    count
};

inline const juce::StringArray oversampling { "Off", "2x", "4x" };

enum class VelocityCurve
{
    linear = 0,
    soft,
    hard,
    fixed,      // ignore velocity entirely
    count
};

inline const juce::StringArray velocityCurve { "Linear", "Soft", "Hard", "Fixed" };

// --- Consistency guards ----------------------------------------------------
// A mismatch between an enum and its StringArray would produce a parameter
// whose index maps to a mode the engine does not have. These are runtime
// checks because juce::StringArray has no constexpr size.
inline bool choiceListsAreConsistent()
{
    const auto check = [] (const juce::StringArray& list, auto countEnum)
    {
        return list.size() == static_cast<int> (countEnum);
    };

    return check (oscMode,         OscMode::count)
        && check (warpMode,        WarpMode::count)
        && check (subWaveform,     SubWaveform::count)
        && check (noiseType,       NoiseType::count)
        && check (filterType,      FilterType::count)
        && check (filterRouting,   FilterRouting::count)
        && check (driveCurve,      DriveCurve::count)
        && check (envelopeMode,    EnvelopeMode::count)
        && check (lfoShape,        LfoShape::count)
        && check (lfoMode,         LfoMode::count)
        && check (lfoRateDivision, LfoRateDivision::count)
        && check (gridDivision,    GridDivision::count)
        && check (modSource,       ModSource::count)
        && check (modCurve,        ModCurve::count)
        && check (polyMode,        PolyMode::count)
        && check (oversampling,    Oversampling::count)
        && check (velocityCurve,   VelocityCurve::count);
}

} // namespace gnarl::choices
