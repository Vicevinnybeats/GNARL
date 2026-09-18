#pragma once

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

    // ----------------------------------------------------------------------
    struct OscillatorIDs
    {
        const char* enabled;
        const char* mode;
        const char* wavetable;
        const char* tablePos;
        const char* pitchSemi;
        const char* pitchFine;
        const char* phase;
        const char* phaseRandom;
        const char* pan;
        const char* level;
        const char* unisonVoices;
        const char* unisonDetune;
        const char* unisonBlend;
        const char* unisonSpread;
        const char* warpMode;
        const char* warpAmount;
        const char* grainSize;
        const char* grainDensity;
        const char* grainPosJitter;
        const char* grainPitchJitter;
        const char* sendFilter1;
        const char* sendFilter2;
        const char* sendDirect;
    };

    /** Index 0 is "osc1_...": the UI's 1-based numbering minus one. */
    inline constexpr OscillatorIDs osc[2] = {
        {
            .enabled          = "osc1_enabled",
            .mode             = "osc1_mode",
            .wavetable        = "osc1_wavetable",
            .tablePos         = "osc1_table_pos",
            .pitchSemi        = "osc1_pitch_semi",
            .pitchFine        = "osc1_pitch_fine",
            .phase            = "osc1_phase",
            .phaseRandom      = "osc1_phase_random",
            .pan              = "osc1_pan",
            .level            = "osc1_level",
            .unisonVoices     = "osc1_uni_voices",
            .unisonDetune     = "osc1_uni_detune",
            .unisonBlend      = "osc1_uni_blend",
            .unisonSpread     = "osc1_uni_spread",
            .warpMode         = "osc1_warp_mode",
            .warpAmount       = "osc1_warp_amount",
            .grainSize        = "osc1_grain_size",
            .grainDensity     = "osc1_grain_density",
            .grainPosJitter   = "osc1_grain_pos_jitter",
            .grainPitchJitter = "osc1_grain_pitch_jitter",
            .sendFilter1      = "osc1_send_f1",
            .sendFilter2      = "osc1_send_f2",
            .sendDirect       = "osc1_send_direct",
        },
        {
            .enabled          = "osc2_enabled",
            .mode             = "osc2_mode",
            .wavetable        = "osc2_wavetable",
            .tablePos         = "osc2_table_pos",
            .pitchSemi        = "osc2_pitch_semi",
            .pitchFine        = "osc2_pitch_fine",
            .phase            = "osc2_phase",
            .phaseRandom      = "osc2_phase_random",
            .pan              = "osc2_pan",
            .level            = "osc2_level",
            .unisonVoices     = "osc2_uni_voices",
            .unisonDetune     = "osc2_uni_detune",
            .unisonBlend      = "osc2_uni_blend",
            .unisonSpread     = "osc2_uni_spread",
            .warpMode         = "osc2_warp_mode",
            .warpAmount       = "osc2_warp_amount",
            .grainSize        = "osc2_grain_size",
            .grainDensity     = "osc2_grain_density",
            .grainPosJitter   = "osc2_grain_pos_jitter",
            .grainPitchJitter = "osc2_grain_pitch_jitter",
            .sendFilter1      = "osc2_send_f1",
            .sendFilter2      = "osc2_send_f2",
            .sendDirect       = "osc2_send_direct",
        },
    };

    // ----------------------------------------------------------------------
    struct FilterIDs
    {
        const char* enabled;
        const char* type;
        const char* cutoff;
        const char* resonance;
        const char* drive;
        const char* driveCurve;
        const char* mix;
        const char* keyTrack;
        const char* formantX;
        const char* formantY;
        const char* formantThroat;
        const char* combFeedback;
        const char* combDamping;
    };

    /** Index 0 is "filter1_...": the UI's 1-based numbering minus one. */
    inline constexpr FilterIDs filter[2] = {
        {
            .enabled       = "filter1_enabled",
            .type          = "filter1_type",
            .cutoff        = "filter1_cutoff",
            .resonance     = "filter1_resonance",
            .drive         = "filter1_drive",
            .driveCurve    = "filter1_drive_curve",
            .mix           = "filter1_mix",
            .keyTrack      = "filter1_key_track",
            .formantX      = "filter1_formant_x",
            .formantY      = "filter1_formant_y",
            .formantThroat = "filter1_formant_throat",
            .combFeedback  = "filter1_comb_feedback",
            .combDamping   = "filter1_comb_damping",
        },
        {
            .enabled       = "filter2_enabled",
            .type          = "filter2_type",
            .cutoff        = "filter2_cutoff",
            .resonance     = "filter2_resonance",
            .drive         = "filter2_drive",
            .driveCurve    = "filter2_drive_curve",
            .mix           = "filter2_mix",
            .keyTrack      = "filter2_key_track",
            .formantX      = "filter2_formant_x",
            .formantY      = "filter2_formant_y",
            .formantThroat = "filter2_formant_throat",
            .combFeedback  = "filter2_comb_feedback",
            .combDamping   = "filter2_comb_damping",
        },
    };

    // ----------------------------------------------------------------------
    struct EnvelopeIDs
    {
        const char* mode;
        const char* delay;
        const char* attack;
        const char* hold;
        const char* decay;
        const char* sustain;
        const char* release;
        const char* attackCurve;
        const char* decayCurve;
        const char* releaseCurve;
        const char* velocityAmount;
    };

    /** Index 0 is "env1_...": the UI's 1-based numbering minus one. */
    inline constexpr EnvelopeIDs envelope[4] = {
        {
            .mode           = "env1_mode",
            .delay          = "env1_delay",
            .attack         = "env1_attack",
            .hold           = "env1_hold",
            .decay          = "env1_decay",
            .sustain        = "env1_sustain",
            .release        = "env1_release",
            .attackCurve    = "env1_attack_curve",
            .decayCurve     = "env1_decay_curve",
            .releaseCurve   = "env1_release_curve",
            .velocityAmount = "env1_velocity_amount",
        },
        {
            .mode           = "env2_mode",
            .delay          = "env2_delay",
            .attack         = "env2_attack",
            .hold           = "env2_hold",
            .decay          = "env2_decay",
            .sustain        = "env2_sustain",
            .release        = "env2_release",
            .attackCurve    = "env2_attack_curve",
            .decayCurve     = "env2_decay_curve",
            .releaseCurve   = "env2_release_curve",
            .velocityAmount = "env2_velocity_amount",
        },
        {
            .mode           = "env3_mode",
            .delay          = "env3_delay",
            .attack         = "env3_attack",
            .hold           = "env3_hold",
            .decay          = "env3_decay",
            .sustain        = "env3_sustain",
            .release        = "env3_release",
            .attackCurve    = "env3_attack_curve",
            .decayCurve     = "env3_decay_curve",
            .releaseCurve   = "env3_release_curve",
            .velocityAmount = "env3_velocity_amount",
        },
        {
            .mode           = "env4_mode",
            .delay          = "env4_delay",
            .attack         = "env4_attack",
            .hold           = "env4_hold",
            .decay          = "env4_decay",
            .sustain        = "env4_sustain",
            .release        = "env4_release",
            .attackCurve    = "env4_attack_curve",
            .decayCurve     = "env4_decay_curve",
            .releaseCurve   = "env4_release_curve",
            .velocityAmount = "env4_velocity_amount",
        },
    };

    // ----------------------------------------------------------------------
    struct LfoIDs
    {
        const char* shape;
        const char* syncEnabled;
        const char* rateHz;
        const char* rateDivision;
        const char* mode;
        const char* phase;
        const char* smooth;
        const char* gridDivision;
        const char* bipolar;
    };

    /** Index 0 is "lfo1_...": the UI's 1-based numbering minus one. */
    inline constexpr LfoIDs lfo[4] = {
        {
            .shape        = "lfo1_shape",
            .syncEnabled  = "lfo1_sync_enabled",
            .rateHz       = "lfo1_rate_hz",
            .rateDivision = "lfo1_rate_division",
            .mode         = "lfo1_mode",
            .phase        = "lfo1_phase",
            .smooth       = "lfo1_smooth",
            .gridDivision = "lfo1_grid_division",
            .bipolar      = "lfo1_bipolar",
        },
        {
            .shape        = "lfo2_shape",
            .syncEnabled  = "lfo2_sync_enabled",
            .rateHz       = "lfo2_rate_hz",
            .rateDivision = "lfo2_rate_division",
            .mode         = "lfo2_mode",
            .phase        = "lfo2_phase",
            .smooth       = "lfo2_smooth",
            .gridDivision = "lfo2_grid_division",
            .bipolar      = "lfo2_bipolar",
        },
        {
            .shape        = "lfo3_shape",
            .syncEnabled  = "lfo3_sync_enabled",
            .rateHz       = "lfo3_rate_hz",
            .rateDivision = "lfo3_rate_division",
            .mode         = "lfo3_mode",
            .phase        = "lfo3_phase",
            .smooth       = "lfo3_smooth",
            .gridDivision = "lfo3_grid_division",
            .bipolar      = "lfo3_bipolar",
        },
        {
            .shape        = "lfo4_shape",
            .syncEnabled  = "lfo4_sync_enabled",
            .rateHz       = "lfo4_rate_hz",
            .rateDivision = "lfo4_rate_division",
            .mode         = "lfo4_mode",
            .phase        = "lfo4_phase",
            .smooth       = "lfo4_smooth",
            .gridDivision = "lfo4_grid_division",
            .bipolar      = "lfo4_bipolar",
        },
    };

    // ----------------------------------------------------------------------
    struct ModSlotIDs
    {
        const char* enabled;
        const char* source;
        const char* depth;
        const char* curve;
        const char* auxSource;
        const char* auxAmount;
        const char* bipolar;
    };

    /** Index 0 is "mod1_...": the UI's 1-based numbering minus one. */
    inline constexpr ModSlotIDs modSlot[16] = {
        {
            .enabled   = "mod1_enabled",
            .source    = "mod1_source",
            .depth     = "mod1_depth",
            .curve     = "mod1_curve",
            .auxSource = "mod1_aux_source",
            .auxAmount = "mod1_aux_amount",
            .bipolar   = "mod1_bipolar",
        },
        {
            .enabled   = "mod2_enabled",
            .source    = "mod2_source",
            .depth     = "mod2_depth",
            .curve     = "mod2_curve",
            .auxSource = "mod2_aux_source",
            .auxAmount = "mod2_aux_amount",
            .bipolar   = "mod2_bipolar",
        },
        {
            .enabled   = "mod3_enabled",
            .source    = "mod3_source",
            .depth     = "mod3_depth",
            .curve     = "mod3_curve",
            .auxSource = "mod3_aux_source",
            .auxAmount = "mod3_aux_amount",
            .bipolar   = "mod3_bipolar",
        },
        {
            .enabled   = "mod4_enabled",
            .source    = "mod4_source",
            .depth     = "mod4_depth",
            .curve     = "mod4_curve",
            .auxSource = "mod4_aux_source",
            .auxAmount = "mod4_aux_amount",
            .bipolar   = "mod4_bipolar",
        },
        {
            .enabled   = "mod5_enabled",
            .source    = "mod5_source",
            .depth     = "mod5_depth",
            .curve     = "mod5_curve",
            .auxSource = "mod5_aux_source",
            .auxAmount = "mod5_aux_amount",
            .bipolar   = "mod5_bipolar",
        },
        {
            .enabled   = "mod6_enabled",
            .source    = "mod6_source",
            .depth     = "mod6_depth",
            .curve     = "mod6_curve",
            .auxSource = "mod6_aux_source",
            .auxAmount = "mod6_aux_amount",
            .bipolar   = "mod6_bipolar",
        },
        {
            .enabled   = "mod7_enabled",
            .source    = "mod7_source",
            .depth     = "mod7_depth",
            .curve     = "mod7_curve",
            .auxSource = "mod7_aux_source",
            .auxAmount = "mod7_aux_amount",
            .bipolar   = "mod7_bipolar",
        },
        {
            .enabled   = "mod8_enabled",
            .source    = "mod8_source",
            .depth     = "mod8_depth",
            .curve     = "mod8_curve",
            .auxSource = "mod8_aux_source",
            .auxAmount = "mod8_aux_amount",
            .bipolar   = "mod8_bipolar",
        },
        {
            .enabled   = "mod9_enabled",
            .source    = "mod9_source",
            .depth     = "mod9_depth",
            .curve     = "mod9_curve",
            .auxSource = "mod9_aux_source",
            .auxAmount = "mod9_aux_amount",
            .bipolar   = "mod9_bipolar",
        },
        {
            .enabled   = "mod10_enabled",
            .source    = "mod10_source",
            .depth     = "mod10_depth",
            .curve     = "mod10_curve",
            .auxSource = "mod10_aux_source",
            .auxAmount = "mod10_aux_amount",
            .bipolar   = "mod10_bipolar",
        },
        {
            .enabled   = "mod11_enabled",
            .source    = "mod11_source",
            .depth     = "mod11_depth",
            .curve     = "mod11_curve",
            .auxSource = "mod11_aux_source",
            .auxAmount = "mod11_aux_amount",
            .bipolar   = "mod11_bipolar",
        },
        {
            .enabled   = "mod12_enabled",
            .source    = "mod12_source",
            .depth     = "mod12_depth",
            .curve     = "mod12_curve",
            .auxSource = "mod12_aux_source",
            .auxAmount = "mod12_aux_amount",
            .bipolar   = "mod12_bipolar",
        },
        {
            .enabled   = "mod13_enabled",
            .source    = "mod13_source",
            .depth     = "mod13_depth",
            .curve     = "mod13_curve",
            .auxSource = "mod13_aux_source",
            .auxAmount = "mod13_aux_amount",
            .bipolar   = "mod13_bipolar",
        },
        {
            .enabled   = "mod14_enabled",
            .source    = "mod14_source",
            .depth     = "mod14_depth",
            .curve     = "mod14_curve",
            .auxSource = "mod14_aux_source",
            .auxAmount = "mod14_aux_amount",
            .bipolar   = "mod14_bipolar",
        },
        {
            .enabled   = "mod15_enabled",
            .source    = "mod15_source",
            .depth     = "mod15_depth",
            .curve     = "mod15_curve",
            .auxSource = "mod15_aux_source",
            .auxAmount = "mod15_aux_amount",
            .bipolar   = "mod15_bipolar",
        },
        {
            .enabled   = "mod16_enabled",
            .source    = "mod16_source",
            .depth     = "mod16_depth",
            .curve     = "mod16_curve",
            .auxSource = "mod16_aux_source",
            .auxAmount = "mod16_aux_amount",
            .bipolar   = "mod16_bipolar",
        },
    };

    // ----------------------------------------------------------------------
    struct SubOscillatorIDs
    {
        const char* enabled;
        const char* waveform;
        const char* octave;
        const char* pitchFine;
        const char* phase;
        const char* pan;
        const char* level;
        const char* sendFilter1;
        const char* sendFilter2;
        const char* sendDirect;
    };

    inline constexpr SubOscillatorIDs sub {
        .enabled     = "sub_enabled",
        .waveform    = "sub_waveform",
        .octave      = "sub_octave",
        .pitchFine   = "sub_pitch_fine",
        .phase       = "sub_phase",
        .pan         = "sub_pan",
        .level       = "sub_level",
        .sendFilter1 = "sub_send_f1",
        .sendFilter2 = "sub_send_f2",
        .sendDirect  = "sub_send_direct",
    };

    // ----------------------------------------------------------------------
    struct NoiseIDs
    {
        const char* enabled;
        const char* type;
        const char* level;
        const char* pan;
        const char* pitchSemi;
        const char* pitchFine;
        const char* phaseRandom;
        const char* sendFilter1;
        const char* sendFilter2;
        const char* sendDirect;
    };

    inline constexpr NoiseIDs noise {
        .enabled     = "noise_enabled",
        .type        = "noise_type",
        .level       = "noise_level",
        .pan         = "noise_pan",
        .pitchSemi   = "noise_pitch_semi",
        .pitchFine   = "noise_pitch_fine",
        .phaseRandom = "noise_phase_random",
        .sendFilter1 = "noise_send_f1",
        .sendFilter2 = "noise_send_f2",
        .sendDirect  = "noise_send_direct",
    };

    // ----------------------------------------------------------------------
    struct OttIDs
    {
        const char* enabled;
        const char* depth;
        const char* time;
        const char* mix;
        const char* inputGain;
        const char* outputGain;
        const char* crossoverLow;
        const char* crossoverHigh;
        const char* lowGain;
        const char* midGain;
        const char* highGain;
        const char* lowUpward;
        const char* midUpward;
        const char* highUpward;
        const char* lowDownward;
        const char* midDownward;
        const char* highDownward;
    };

    inline constexpr OttIDs ott {
        .enabled       = "ott_enabled",
        .depth         = "ott_depth",
        .time          = "ott_time",
        .mix           = "ott_mix",
        .inputGain     = "ott_in_gain",
        .outputGain    = "ott_out_gain",
        .crossoverLow  = "ott_xover_low",
        .crossoverHigh = "ott_xover_high",
        .lowGain       = "ott_low_gain",
        .midGain       = "ott_mid_gain",
        .highGain      = "ott_high_gain",
        .lowUpward     = "ott_low_up",
        .midUpward     = "ott_mid_up",
        .highUpward    = "ott_high_up",
        .lowDownward   = "ott_low_down",
        .midDownward   = "ott_mid_down",
        .highDownward  = "ott_high_down",
    };

    // ----------------------------------------------------------------------
    /** Macro knobs. Destinations are assigned through the mod matrix. */
    inline constexpr std::array<const char*, kNumMacros> macro {
        "macro1",
        "macro2",
        "macro3",
        "macro4"
    };

    // ----------------------------------------------------------------------
    // Global parameters.
    inline constexpr auto masterGain     = "master_gain";
    inline constexpr auto bypass         = "bypass";
    inline constexpr auto maxVoices      = "max_voices";
    inline constexpr auto polyMode       = "poly_mode";
    inline constexpr auto glideTime      = "glide_time";
    inline constexpr auto glideAlways    = "glide_always";
    inline constexpr auto pitchBendRange = "pitch_bend_range";
    inline constexpr auto oversampling   = "oversampling";
    inline constexpr auto velocityCurve  = "velocity_curve";
    inline constexpr auto analogDrift    = "analog_drift";
    inline constexpr auto filterRouting  = "filter_routing";
}
