#!/usr/bin/env python3
"""Emits plugin/source/params/ParameterIDs.h.

Run once, by hand, and commit the result. Keeping the emitter out of the build
means the header stays a plain, greppable file.
"""

from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

# (C++ member name, id suffix)
OSC = [
    ("enabled",         "enabled"),
    ("mode",            "mode"),
    ("wavetable",       "wavetable"),
    ("tablePos",        "table_pos"),
    ("pitchSemi",       "pitch_semi"),
    ("pitchFine",       "pitch_fine"),
    ("phase",           "phase"),
    ("phaseRandom",     "phase_random"),
    ("pan",             "pan"),
    ("level",           "level"),
    ("unisonVoices",    "uni_voices"),
    ("unisonDetune",    "uni_detune"),
    ("unisonBlend",     "uni_blend"),
    ("unisonSpread",    "uni_spread"),
    ("warpMode",        "warp_mode"),
    ("warpAmount",      "warp_amount"),
    ("grainSize",       "grain_size"),
    ("grainDensity",    "grain_density"),
    ("grainPosJitter",  "grain_pos_jitter"),
    ("grainPitchJitter","grain_pitch_jitter"),
    ("sendFilter1",     "send_f1"),
    ("sendFilter2",     "send_f2"),
    ("sendDirect",      "send_direct"),
]

SUB = [
    ("enabled",     "enabled"),
    ("waveform",    "waveform"),
    ("octave",      "octave"),
    ("pitchFine",   "pitch_fine"),
    ("phase",       "phase"),
    ("pan",         "pan"),
    ("level",       "level"),
    ("sendFilter1", "send_f1"),
    ("sendFilter2", "send_f2"),
    ("sendDirect",  "send_direct"),
]

NOISE = [
    ("enabled",     "enabled"),
    ("type",        "type"),
    ("level",       "level"),
    ("pan",         "pan"),
    ("pitchSemi",   "pitch_semi"),
    ("pitchFine",   "pitch_fine"),
    ("phaseRandom", "phase_random"),
    ("sendFilter1", "send_f1"),
    ("sendFilter2", "send_f2"),
    ("sendDirect",  "send_direct"),
]

FILTER = [
    ("enabled",       "enabled"),
    ("type",          "type"),
    ("cutoff",        "cutoff"),
    ("resonance",     "resonance"),
    ("drive",         "drive"),
    ("driveCurve",    "drive_curve"),
    ("mix",           "mix"),
    ("keyTrack",      "key_track"),
    ("formantX",      "formant_x"),
    ("formantY",      "formant_y"),
    ("formantThroat", "formant_throat"),
    ("combFeedback",  "comb_feedback"),
    ("combDamping",   "comb_damping"),
]

ENV = [
    ("mode",           "mode"),
    ("delay",          "delay"),
    ("attack",         "attack"),
    ("hold",           "hold"),
    ("decay",          "decay"),
    ("sustain",        "sustain"),
    ("release",        "release"),
    ("attackCurve",    "attack_curve"),
    ("decayCurve",     "decay_curve"),
    ("releaseCurve",   "release_curve"),
    ("velocityAmount", "velocity_amount"),
]

LFO = [
    ("shape",        "shape"),
    ("syncEnabled",  "sync_enabled"),
    ("rateHz",       "rate_hz"),
    ("rateDivision", "rate_division"),
    ("mode",         "mode"),
    ("phase",        "phase"),
    ("smooth",       "smooth"),
    ("gridDivision", "grid_division"),
    ("bipolar",      "bipolar"),
]

MOD = [
    ("enabled",     "enabled"),
    ("source",      "source"),
    ("depth",       "depth"),
    ("curve",       "curve"),
    ("auxSource",   "aux_source"),
    ("auxAmount",   "aux_amount"),
    ("bipolar",     "bipolar"),
]

# OTT: a dedicated effect with named parameters rather than a generic FX slot.
# Riddim producers put OTT on everything, so it ships inside the synth. Its
# controls are named because they mean specific things - a generic "FX 1 Param
# 3" would be useless in a host automation lane.
OTT = [
    ("enabled",         "enabled"),
    ("depth",           "depth"),
    ("time",            "time"),
    ("mix",             "mix"),
    ("inputGain",       "in_gain"),
    ("outputGain",      "out_gain"),
    ("crossoverLow",    "xover_low"),
    ("crossoverHigh",   "xover_high"),
    ("lowGain",         "low_gain"),
    ("midGain",         "mid_gain"),
    ("highGain",        "high_gain"),
    ("lowUpward",       "low_up"),
    ("midUpward",       "mid_up"),
    ("highUpward",      "high_up"),
    ("lowDownward",     "low_down"),
    ("midDownward",     "mid_down"),
    ("highDownward",    "high_down"),
]

FAMILIES = [
    ("OscillatorIDs", "osc",     OSC,    2,  "osc{i}_"),
    ("FilterIDs",     "filter",  FILTER, 2,  "filter{i}_"),
    ("EnvelopeIDs",   "envelope", ENV,   4,  "env{i}_"),
    ("LfoIDs",        "lfo",     LFO,    4,  "lfo{i}_"),
    ("ModSlotIDs",    "modSlot", MOD,    16, "mod{i}_"),
]

SINGLETONS = [
    ("SubOscillatorIDs", "sub",   SUB,   "sub_"),
    ("NoiseIDs",         "noise", NOISE, "noise_"),
    ("OttIDs",           "ott",   OTT,   "ott_"),
]

GLOBALS = [
    ("masterGain",      "master_gain"),
    ("bypass",          "bypass"),
    ("maxVoices",       "max_voices"),
    ("polyMode",        "poly_mode"),
    ("glideTime",       "glide_time"),
    ("glideAlways",     "glide_always"),
    ("pitchBendRange",  "pitch_bend_range"),
    ("oversampling",    "oversampling"),
    ("velocityCurve",   "velocity_curve"),
    ("analogDrift",     "analog_drift"),
    ("filterRouting",   "filter_routing"),
]

MACRO_COUNT = 4

def struct(name, fields):
    lines = [f"    struct {name}", "    {"]
    for member, _ in fields:
        lines.append(f"        const char* {member};")
    lines.append("    };")
    return "\n".join(lines)

def designated(fields, prefix, indent):
    """Designated initializers: self-documenting, and the compiler rejects a
    field written out of order, which a positional list would accept."""
    w = max(len(m) for m, _ in fields)
    pad = " " * indent
    return "\n".join(
        f'{pad}.{member.ljust(w)} = "{prefix}{suffix}",' for member, suffix in fields)

def instances(cname, var, fields, count, pattern):
    rows = []
    for i in range(1, count + 1):
        prefix = pattern.format(i=i)
        rows.append("        {\n" + designated(fields, prefix, 12) + "\n        },")
    body = "\n".join(rows)
    return (f"    /** Index 0 is \"{pattern.format(i=1)}...\": the UI's 1-based numbering minus one. */\n"
            f"    inline constexpr {cname} {var}[{count}] = {{\n{body}\n    }};")

def singleton(cname, var, fields, prefix):
    return (f"    inline constexpr {cname} {var} {{\n"
            + designated(fields, prefix, 8) + "\n    };")

out = []
out.append('''#pragma once

#include <array>
#include <cstddef>

/**
    Compile-time parameter identifiers for GNARL.

    HARD RULE: a parameter ID is never written as a string literal outside this
    file. A typo in a literal is a silent bug that surfaces months later as a
    preset that no longer recalls.

    IDs are FROZEN once shipped. Renaming one breaks every saved preset and
    every host automation lane pointing at it. To retire a parameter, leave the
    ID declared here and stop reading it.

    The full layout is declared up front, in Phase 1, even though most of it is
    not implemented until Phases 2-4. A parameter added after release cannot be
    inserted without breaking preset compatibility, so there is no later.

    The TypeScript mirror is ui/src/bridge/parameterIds.ts. Add a parameter to
    BOTH files in the same commit - a mismatch fails silently, because the relay
    simply never connects. ParameterMirrorTests guards this.

    NOT HERE: a mod slot's DESTINATION. Everything in this file is an
    AudioProcessorValueTreeState parameter, and a destination cannot safely be
    one. A host parameter is a number, so a destination parameter would have to
    be an index into an ordered list of targets - and that index shifts the
    moment the list changes, silently repointing every saved preset's
    modulation at the wrong parameter. Destinations are therefore stored as
    parameter-ID STRINGS in the plugin's ValueTree (see ModMatrixState), which
    is stable because the IDs above are frozen. A mod slot's depth, curve and
    enable ARE parameters, because those are worth automating.

    Indexed families are arrays, so DSP code can loop:

        for (size_t i = 0; i < pid::kNumOscillators; ++i)
            apvts.getRawParameterValue (pid::osc[i].tablePos);

    Array index 0 is oscillator "1" in the UI. The IDs keep 1-based numbering
    because that is what a user sees and what a preset file records.
*/
namespace gnarl::pid
{
    /** Version stamped into saved state. Bump only when the MEANING of an
        existing parameter changes, never when one is added. */
    inline constexpr int kStateVersion = 1;

    /** The juce::ParameterID version hint. This is NOT kStateVersion and must
        NEVER change: JUCE folds the hint into the AU parameter ID, so bumping
        it silently invalidates every AU automation lane a customer has drawn.
        It is a separate constant precisely so that bumping kStateVersion for a
        preset migration cannot take AU automation down with it. */
    inline constexpr int kParameterVersionHint = 1;

    // --- Counts ------------------------------------------------------------
    inline constexpr std::size_t kNumOscillators = 2;
    inline constexpr std::size_t kNumFilters     = 2;
    inline constexpr std::size_t kNumEnvelopes   = 4;
    inline constexpr std::size_t kNumLfos        = 4;
    inline constexpr std::size_t kNumModSlots    = 16;
    inline constexpr std::size_t kNumMacros      = 4;

    /** Hard ceiling on simultaneous voices, and on unison voices per osc. */
    inline constexpr int kMaxVoices       = 16;
    inline constexpr int kMaxUnisonVoices = 16;
''')

for cname, var, fields, count, pattern in FAMILIES:
    out.append("    // " + "-" * 70)
    out.append(struct(cname, fields))
    out.append("")
    out.append(instances(cname, var, fields, count, pattern))
    out.append("")

for cname, var, fields, prefix in SINGLETONS:
    out.append("    // " + "-" * 70)
    out.append(struct(cname, fields))
    out.append("")
    out.append(singleton(cname, var, fields, prefix))
    out.append("")

out.append("    // " + "-" * 70)
out.append("    /** Macro knobs. Destinations are assigned through the mod matrix. */")
macro_vals = ",\n".join(f'        "macro{i}"' for i in range(1, MACRO_COUNT + 1))
out.append(f"    inline constexpr std::array<const char*, kNumMacros> macro {{\n{macro_vals}\n    }};")
out.append("")

out.append("    // " + "-" * 70)
out.append("    // Global parameters.")
w = max(len(n) for n, _ in GLOBALS)
for name, sid in GLOBALS:
    out.append(f'    inline constexpr auto {name.ljust(w)} = "{sid}";')

out.append("}")
out.append("")

text = "\n".join(out)
# collapse any accidental triple blank lines
while "\n\n\n\n" in text:
    text = text.replace("\n\n\n\n", "\n\n\n")
open(REPO / "plugin/source/params/ParameterIDs.h", "w").write(text)

total = (len(OSC)*2 + len(SUB) + len(NOISE) + len(OTT) + len(FILTER)*2 + len(ENV)*4
         + len(LFO)*4 + len(MOD)*16 + MACRO_COUNT + len(GLOBALS))
print("declared parameter ids:", total)
for label, n in [("osc", len(OSC)*2), ("sub", len(SUB)), ("noise", len(NOISE)),
                 ("ott", len(OTT)),
                 ("filters", len(FILTER)*2), ("envelopes", len(ENV)*4),
                 ("lfos", len(LFO)*4), ("mod matrix", len(MOD)*16),
                 ("macros", MACRO_COUNT), ("global", len(GLOBALS))]:
    print(f"  {label:12} {n}")
