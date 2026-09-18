#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/**
    Shared ranges and value formatting for GNARL's parameters.

    Every range that is not linear says WHY its skew centre is where it is. A
    knob whose useful values are all crammed into the last 10% of its travel is
    a knob nobody can play, so the skew is a feature decision, not a detail.

    Ranges may be widened after release, but never narrowed: a saved preset can
    hold a value that a narrowed range would clamp.
*/
namespace gnarl::ranges
{

// --- Builders --------------------------------------------------------------

/** Linear range. */
inline juce::NormalisableRange<float> linear (float min, float max, float step = 0.0f)
{
    return { min, max, step };
}

/** Range whose midpoint lands on `centre`, so the knob spends half its travel
    below that value and half above. */
inline juce::NormalisableRange<float> skewed (float min, float max, float centre)
{
    auto range = juce::NormalisableRange<float> { min, max };
    range.setSkewForCentre (centre);
    return range;
}

// --- Frequency -------------------------------------------------------------

/** Filter cutoff. Centre at 1 kHz: below it is where bass and growl weight
    live, above it is where screech lives, and a producer wants equal
    resolution on both sides. */
inline juce::NormalisableRange<float> cutoff()      { return skewed (20.0f, 20000.0f, 1000.0f); }

/** Free-running LFO rate. Centre at 2 Hz because a wobble is a low-frequency
    event; the audio-rate end of the range is a special effect. */
inline juce::NormalisableRange<float> lfoRateHz()   { return skewed (0.01f, 200.0f, 2.0f); }

// --- Time ------------------------------------------------------------------

/** Envelope attack. Centre at 10 ms: percussive attacks are the common case,
    and the difference between 1 ms and 20 ms is audible in a way the
    difference between 8 s and 10 s is not. */
inline juce::NormalisableRange<float> attackTime()  { return skewed (0.0f, 16.0f, 0.01f); }

/** Envelope decay and release. Centre at 300 ms. */
inline juce::NormalisableRange<float> decayTime()   { return skewed (0.001f, 32.0f, 0.3f); }

/** Envelope pre-attack delay and post-attack hold. */
inline juce::NormalisableRange<float> holdTime()    { return skewed (0.0f, 8.0f, 0.1f); }

/** Portamento. Centre at 80 ms, roughly a 1/16 note at 170 BPM. */
inline juce::NormalisableRange<float> glideTime()   { return skewed (0.0f, 4.0f, 0.08f); }

/** Graintable grain length, in milliseconds. Centre at 40 ms: short enough to
    be pitched, long enough to carry the table's character. */
inline juce::NormalisableRange<float> grainSizeMs() { return skewed (1.0f, 500.0f, 40.0f); }

/** Grains per second. */
inline juce::NormalisableRange<float> grainDensity() { return skewed (1.0f, 200.0f, 20.0f); }

// --- Level and ratio -------------------------------------------------------

inline juce::NormalisableRange<float> unipolar()    { return linear (0.0f, 1.0f); }
inline juce::NormalisableRange<float> bipolar()     { return linear (-1.0f, 1.0f); }

/** Master output. Bottom of the range is treated as silence. */
inline juce::NormalisableRange<float> masterGainDb() { return linear (-60.0f, 12.0f, 0.01f); }

inline constexpr float kMasterGainMinDb = -60.0f;

// --- Pitch -----------------------------------------------------------------

/** Coarse pitch offset, in semitones. +/- 4 octaves. */
inline juce::NormalisableRange<float> pitchSemitones() { return linear (-48.0f, 48.0f, 1.0f); }

/** Fine pitch offset, in cents. */
inline juce::NormalisableRange<float> pitchCents()     { return linear (-100.0f, 100.0f, 0.01f); }

// --- FX --------------------------------------------------------------------

/** Delay time when not tempo-synced. Skewed low: the musically interesting
    settings are short, and a linear knob spends most of its travel between
    one and two seconds where nothing changes. Down to 1 ms so the delay
    doubles as a comb/flanger by hand. */
inline juce::NormalisableRange<float> delayTimeMs() { return skewed (1.0f, 4000.0f, 250.0f); }

/** Reverb pre-delay. Short: past about 200 ms it stops reading as a room and
    starts reading as a second, quieter note. */
inline juce::NormalisableRange<float> preDelayMs()  { return skewed (0.0f, 250.0f, 30.0f); }

/** Modulation rate for the chorus, flanger and phaser. Tops out well below
    the LFO's 200 Hz: past a few Hz these stop being modulation and become
    ring modulation, which the warp section already does properly. */
inline juce::NormalisableRange<float> fxModRate()   { return skewed (0.01f, 20.0f, 1.0f); }

/** A shelving or cut frequency inside an effect. Same shape as the voice
    filter's cutoff, so the two feel like the same control. */
inline juce::NormalisableRange<float> fxFrequency() { return skewed (20.0f, 20000.0f, 1000.0f); }

/** Band gain for the EQ, in dB. +-18 rather than +-24: past 18 dB a bell is
    doing damage rather than shaping, and the extra travel costs resolution
    across the range people use. */
inline juce::NormalisableRange<float> eqGainDb()    { return linear (-18.0f, 18.0f, 0.1f); }

/** EQ bell width. Q rather than bandwidth, skewed so the middle of the knob
    is the moderate Q people reach for. */
inline juce::NormalisableRange<float> eqQ()         { return skewed (0.2f, 18.0f, 1.0f); }

/** Limiter threshold and ceiling. The ceiling stops just below 0 dBFS by
    default because a true-peak of exactly 0 clips in a lossy encoder. */
inline juce::NormalisableRange<float> limiterDb()   { return linear (-40.0f, 0.0f, 0.1f); }

/** Limiter release. Fast enough to be transparent on a bass note, slow enough
    to be usable as a pumping effect. */
inline juce::NormalisableRange<float> limiterRelease() { return skewed (1.0f, 500.0f, 50.0f); }

/** Drive gain for the FX distortion, in dB of input gain rather than an
    abstract 0..1 - the amount of drive is a level decision and reads better
    as one. */
inline juce::NormalisableRange<float> driveDb()     { return linear (0.0f, 48.0f, 0.1f); }

/** Output trim on an effect. Narrower than the master: this is for making up
    what the effect took, not for mixing. */
inline juce::NormalisableRange<float> trimDb()      { return linear (-24.0f, 24.0f, 0.1f); }

// --- Formatting ------------------------------------------------------------
// Host automation panels and the plugin's own readouts both use these, so a
// value reads the same everywhere.

inline juce::String formatHertz (float value, int)
{
    if (value >= 1000.0f)
        return juce::String (value / 1000.0f, 2) + " kHz";

    return juce::String (value, value < 10.0f ? 2 : 1) + " Hz";
}

inline juce::String formatSeconds (float value, int)
{
    if (value < 1.0f)
        return juce::String (value * 1000.0f, value < 0.01f ? 2 : 1) + " ms";

    return juce::String (value, 2) + " s";
}

inline juce::String formatPercent (float value, int)
{
    return juce::String (juce::roundToInt (value * 100.0f)) + " %";
}

inline juce::String formatSignedPercent (float value, int)
{
    const auto percent = juce::roundToInt (value * 100.0f);
    return (percent > 0 ? "+" : "") + juce::String (percent) + " %";
}

inline juce::String formatDecibels (float value, int)
{
    if (value <= kMasterGainMinDb)
        return "-inf dB";

    return juce::String (value, 1) + " dB";
}

inline juce::String formatSemitones (float value, int)
{
    const auto semis = juce::roundToInt (value);
    return (semis > 0 ? "+" : "") + juce::String (semis) + " st";
}

inline juce::String formatCents (float value, int)
{
    return (value > 0.0f ? "+" : "") + juce::String (value, 1) + " ct";
}

inline juce::String formatMilliseconds (float value, int)
{
    return juce::String (value, value < 10.0f ? 2 : 1) + " ms";
}

/** Pan: "L50", "C", "R30" reads faster than "-0.50". */
inline juce::String formatPan (float value, int)
{
    const auto amount = juce::roundToInt (std::abs (value) * 100.0f);

    if (amount == 0)
        return "C";

    return (value < 0.0f ? "L" : "R") + juce::String (amount);
}

} // namespace gnarl::ranges
