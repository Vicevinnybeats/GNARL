/**
 * Mirror of plugin/source/params/ParameterIDs.h.
 *
 * GENERATED — regenerate rather than editing by hand, and commit both files in
 * the same commit as the C++ change.
 *
 * These strings are the contract between the C++ parameter tree and this UI.
 * A mismatch fails SILENTLY: JUCE's relay never connects, so the control
 * renders, moves, and changes nothing. ParameterMirrorTests (C++) fails the
 * build if this file and the header disagree.
 *
 * Indexed families are arrays. Index 0 is oscillator "1" as the user sees it;
 * the IDs keep 1-based numbering because that is what a preset records.
 */

/** Bump only when the MEANING of an existing parameter changes. */
export const STATE_VERSION = 1;

export const COUNTS = {
  oscillators: 2,
  filters: 2,
  envelopes: 4,
  lfos: 4,
  modSlots: 16,
  macros: 4,
} as const;

export const MAX_VOICES = 16;
export const MAX_UNISON_VOICES = 16;

/** Main oscillators. OSC[0] is "Osc 1". */
export const OSC = [
  {
    enabled:          'osc1_enabled',
    mode:             'osc1_mode',
    wavetable:        'osc1_wavetable',
    tablePos:         'osc1_table_pos',
    pitchSemi:        'osc1_pitch_semi',
    pitchFine:        'osc1_pitch_fine',
    phase:            'osc1_phase',
    phaseRandom:      'osc1_phase_random',
    pan:              'osc1_pan',
    level:            'osc1_level',
    unisonVoices:     'osc1_uni_voices',
    unisonDetune:     'osc1_uni_detune',
    unisonBlend:      'osc1_uni_blend',
    unisonSpread:     'osc1_uni_spread',
    warpMode:         'osc1_warp_mode',
    warpAmount:       'osc1_warp_amount',
    grainSize:        'osc1_grain_size',
    grainDensity:     'osc1_grain_density',
    grainPosJitter:   'osc1_grain_pos_jitter',
    grainPitchJitter: 'osc1_grain_pitch_jitter',
    sendFilter1:      'osc1_send_f1',
    sendFilter2:      'osc1_send_f2',
    sendDirect:       'osc1_send_direct',
  },
  {
    enabled:          'osc2_enabled',
    mode:             'osc2_mode',
    wavetable:        'osc2_wavetable',
    tablePos:         'osc2_table_pos',
    pitchSemi:        'osc2_pitch_semi',
    pitchFine:        'osc2_pitch_fine',
    phase:            'osc2_phase',
    phaseRandom:      'osc2_phase_random',
    pan:              'osc2_pan',
    level:            'osc2_level',
    unisonVoices:     'osc2_uni_voices',
    unisonDetune:     'osc2_uni_detune',
    unisonBlend:      'osc2_uni_blend',
    unisonSpread:     'osc2_uni_spread',
    warpMode:         'osc2_warp_mode',
    warpAmount:       'osc2_warp_amount',
    grainSize:        'osc2_grain_size',
    grainDensity:     'osc2_grain_density',
    grainPosJitter:   'osc2_grain_pos_jitter',
    grainPitchJitter: 'osc2_grain_pitch_jitter',
    sendFilter1:      'osc2_send_f1',
    sendFilter2:      'osc2_send_f2',
    sendDirect:       'osc2_send_direct',
  },
] as const;

/** Filter slots. FILTER[0] is "Filter 1". */
export const FILTER = [
  {
    enabled:       'filter1_enabled',
    type:          'filter1_type',
    cutoff:        'filter1_cutoff',
    resonance:     'filter1_resonance',
    drive:         'filter1_drive',
    driveCurve:    'filter1_drive_curve',
    mix:           'filter1_mix',
    keyTrack:      'filter1_key_track',
    formantX:      'filter1_formant_x',
    formantY:      'filter1_formant_y',
    formantThroat: 'filter1_formant_throat',
    combFeedback:  'filter1_comb_feedback',
    combDamping:   'filter1_comb_damping',
  },
  {
    enabled:       'filter2_enabled',
    type:          'filter2_type',
    cutoff:        'filter2_cutoff',
    resonance:     'filter2_resonance',
    drive:         'filter2_drive',
    driveCurve:    'filter2_drive_curve',
    mix:           'filter2_mix',
    keyTrack:      'filter2_key_track',
    formantX:      'filter2_formant_x',
    formantY:      'filter2_formant_y',
    formantThroat: 'filter2_formant_throat',
    combFeedback:  'filter2_comb_feedback',
    combDamping:   'filter2_comb_damping',
  },
] as const;

/** Envelopes. ENV[0] is the amp envelope. */
export const ENV = [
  {
    mode:           'env1_mode',
    delay:          'env1_delay',
    attack:         'env1_attack',
    hold:           'env1_hold',
    decay:          'env1_decay',
    sustain:        'env1_sustain',
    release:        'env1_release',
    attackCurve:    'env1_attack_curve',
    decayCurve:     'env1_decay_curve',
    releaseCurve:   'env1_release_curve',
    velocityAmount: 'env1_velocity_amount',
  },
  {
    mode:           'env2_mode',
    delay:          'env2_delay',
    attack:         'env2_attack',
    hold:           'env2_hold',
    decay:          'env2_decay',
    sustain:        'env2_sustain',
    release:        'env2_release',
    attackCurve:    'env2_attack_curve',
    decayCurve:     'env2_decay_curve',
    releaseCurve:   'env2_release_curve',
    velocityAmount: 'env2_velocity_amount',
  },
  {
    mode:           'env3_mode',
    delay:          'env3_delay',
    attack:         'env3_attack',
    hold:           'env3_hold',
    decay:          'env3_decay',
    sustain:        'env3_sustain',
    release:        'env3_release',
    attackCurve:    'env3_attack_curve',
    decayCurve:     'env3_decay_curve',
    releaseCurve:   'env3_release_curve',
    velocityAmount: 'env3_velocity_amount',
  },
  {
    mode:           'env4_mode',
    delay:          'env4_delay',
    attack:         'env4_attack',
    hold:           'env4_hold',
    decay:          'env4_decay',
    sustain:        'env4_sustain',
    release:        'env4_release',
    attackCurve:    'env4_attack_curve',
    decayCurve:     'env4_decay_curve',
    releaseCurve:   'env4_release_curve',
    velocityAmount: 'env4_velocity_amount',
  },
] as const;

/** LFOs. LFO[0] is "LFO 1". */
export const LFO = [
  {
    shape:        'lfo1_shape',
    syncEnabled:  'lfo1_sync_enabled',
    rateHz:       'lfo1_rate_hz',
    rateDivision: 'lfo1_rate_division',
    mode:         'lfo1_mode',
    phase:        'lfo1_phase',
    smooth:       'lfo1_smooth',
    gridDivision: 'lfo1_grid_division',
    bipolar:      'lfo1_bipolar',
  },
  {
    shape:        'lfo2_shape',
    syncEnabled:  'lfo2_sync_enabled',
    rateHz:       'lfo2_rate_hz',
    rateDivision: 'lfo2_rate_division',
    mode:         'lfo2_mode',
    phase:        'lfo2_phase',
    smooth:       'lfo2_smooth',
    gridDivision: 'lfo2_grid_division',
    bipolar:      'lfo2_bipolar',
  },
  {
    shape:        'lfo3_shape',
    syncEnabled:  'lfo3_sync_enabled',
    rateHz:       'lfo3_rate_hz',
    rateDivision: 'lfo3_rate_division',
    mode:         'lfo3_mode',
    phase:        'lfo3_phase',
    smooth:       'lfo3_smooth',
    gridDivision: 'lfo3_grid_division',
    bipolar:      'lfo3_bipolar',
  },
  {
    shape:        'lfo4_shape',
    syncEnabled:  'lfo4_sync_enabled',
    rateHz:       'lfo4_rate_hz',
    rateDivision: 'lfo4_rate_division',
    mode:         'lfo4_mode',
    phase:        'lfo4_phase',
    smooth:       'lfo4_smooth',
    gridDivision: 'lfo4_grid_division',
    bipolar:      'lfo4_bipolar',
  },
] as const;

/** Mod matrix slots. The DESTINATION is deliberately absent: it is a parameter-ID string in the plugin ValueTree, not a host parameter. See ParameterIDs.h. */
export const MOD = [
  {
    enabled:   'mod1_enabled',
    source:    'mod1_source',
    depth:     'mod1_depth',
    curve:     'mod1_curve',
    auxSource: 'mod1_aux_source',
    auxAmount: 'mod1_aux_amount',
    bipolar:   'mod1_bipolar',
  },
  {
    enabled:   'mod2_enabled',
    source:    'mod2_source',
    depth:     'mod2_depth',
    curve:     'mod2_curve',
    auxSource: 'mod2_aux_source',
    auxAmount: 'mod2_aux_amount',
    bipolar:   'mod2_bipolar',
  },
  {
    enabled:   'mod3_enabled',
    source:    'mod3_source',
    depth:     'mod3_depth',
    curve:     'mod3_curve',
    auxSource: 'mod3_aux_source',
    auxAmount: 'mod3_aux_amount',
    bipolar:   'mod3_bipolar',
  },
  {
    enabled:   'mod4_enabled',
    source:    'mod4_source',
    depth:     'mod4_depth',
    curve:     'mod4_curve',
    auxSource: 'mod4_aux_source',
    auxAmount: 'mod4_aux_amount',
    bipolar:   'mod4_bipolar',
  },
  {
    enabled:   'mod5_enabled',
    source:    'mod5_source',
    depth:     'mod5_depth',
    curve:     'mod5_curve',
    auxSource: 'mod5_aux_source',
    auxAmount: 'mod5_aux_amount',
    bipolar:   'mod5_bipolar',
  },
  {
    enabled:   'mod6_enabled',
    source:    'mod6_source',
    depth:     'mod6_depth',
    curve:     'mod6_curve',
    auxSource: 'mod6_aux_source',
    auxAmount: 'mod6_aux_amount',
    bipolar:   'mod6_bipolar',
  },
  {
    enabled:   'mod7_enabled',
    source:    'mod7_source',
    depth:     'mod7_depth',
    curve:     'mod7_curve',
    auxSource: 'mod7_aux_source',
    auxAmount: 'mod7_aux_amount',
    bipolar:   'mod7_bipolar',
  },
  {
    enabled:   'mod8_enabled',
    source:    'mod8_source',
    depth:     'mod8_depth',
    curve:     'mod8_curve',
    auxSource: 'mod8_aux_source',
    auxAmount: 'mod8_aux_amount',
    bipolar:   'mod8_bipolar',
  },
  {
    enabled:   'mod9_enabled',
    source:    'mod9_source',
    depth:     'mod9_depth',
    curve:     'mod9_curve',
    auxSource: 'mod9_aux_source',
    auxAmount: 'mod9_aux_amount',
    bipolar:   'mod9_bipolar',
  },
  {
    enabled:   'mod10_enabled',
    source:    'mod10_source',
    depth:     'mod10_depth',
    curve:     'mod10_curve',
    auxSource: 'mod10_aux_source',
    auxAmount: 'mod10_aux_amount',
    bipolar:   'mod10_bipolar',
  },
  {
    enabled:   'mod11_enabled',
    source:    'mod11_source',
    depth:     'mod11_depth',
    curve:     'mod11_curve',
    auxSource: 'mod11_aux_source',
    auxAmount: 'mod11_aux_amount',
    bipolar:   'mod11_bipolar',
  },
  {
    enabled:   'mod12_enabled',
    source:    'mod12_source',
    depth:     'mod12_depth',
    curve:     'mod12_curve',
    auxSource: 'mod12_aux_source',
    auxAmount: 'mod12_aux_amount',
    bipolar:   'mod12_bipolar',
  },
  {
    enabled:   'mod13_enabled',
    source:    'mod13_source',
    depth:     'mod13_depth',
    curve:     'mod13_curve',
    auxSource: 'mod13_aux_source',
    auxAmount: 'mod13_aux_amount',
    bipolar:   'mod13_bipolar',
  },
  {
    enabled:   'mod14_enabled',
    source:    'mod14_source',
    depth:     'mod14_depth',
    curve:     'mod14_curve',
    auxSource: 'mod14_aux_source',
    auxAmount: 'mod14_aux_amount',
    bipolar:   'mod14_bipolar',
  },
  {
    enabled:   'mod15_enabled',
    source:    'mod15_source',
    depth:     'mod15_depth',
    curve:     'mod15_curve',
    auxSource: 'mod15_aux_source',
    auxAmount: 'mod15_aux_amount',
    bipolar:   'mod15_bipolar',
  },
  {
    enabled:   'mod16_enabled',
    source:    'mod16_source',
    depth:     'mod16_depth',
    curve:     'mod16_curve',
    auxSource: 'mod16_aux_source',
    auxAmount: 'mod16_aux_amount',
    bipolar:   'mod16_bipolar',
  },
] as const;

/** Sub oscillator. */
export const SUB = {
  enabled:     'sub_enabled',
  waveform:    'sub_waveform',
  octave:      'sub_octave',
  pitchFine:   'sub_pitch_fine',
  phase:       'sub_phase',
  pan:         'sub_pan',
  level:       'sub_level',
  sendFilter1: 'sub_send_f1',
  sendFilter2: 'sub_send_f2',
  sendDirect:  'sub_send_direct',
} as const;

/** Noise generator. */
export const NOISE = {
  enabled:     'noise_enabled',
  type:        'noise_type',
  level:       'noise_level',
  pan:         'noise_pan',
  pitchSemi:   'noise_pitch_semi',
  pitchFine:   'noise_pitch_fine',
  phaseRandom: 'noise_phase_random',
  sendFilter1: 'noise_send_f1',
  sendFilter2: 'noise_send_f2',
  sendDirect:  'noise_send_direct',
} as const;

/** Macro knobs. MACRO[0] is GROWL. */
export const MACRO = [
  'macro1',
  'macro2',
  'macro3',
  'macro4',
] as const;

/** Global parameters. */
export const GLOBAL = {
  masterGain:     'master_gain',
  bypass:         'bypass',
  maxVoices:      'max_voices',
  polyMode:       'poly_mode',
  glideTime:      'glide_time',
  glideAlways:    'glide_always',
  pitchBendRange: 'pitch_bend_range',
  oversampling:   'oversampling',
  velocityCurve:  'velocity_curve',
  analogDrift:    'analog_drift',
  filterRouting:  'filter_routing',
} as const;

type ValuesOf<T> = T[keyof T];

/**
 * The union of every valid parameter ID. A typo in a call site is a compile
 * error rather than a control that binds to nothing.
 */
export type ParameterId =
  | ValuesOf<(typeof OSC)[number]>
  | ValuesOf<typeof SUB>
  | ValuesOf<typeof NOISE>
  | ValuesOf<(typeof FILTER)[number]>
  | ValuesOf<(typeof ENV)[number]>
  | ValuesOf<(typeof LFO)[number]>
  | ValuesOf<(typeof MOD)[number]>
  | (typeof MACRO)[number]
  | ValuesOf<typeof GLOBAL>;

/** Every parameter ID, flat. Useful for bulk relay setup and for tests. */
export const ALL_PARAMETER_IDS: readonly ParameterId[] = [
  ...OSC.flatMap((o) => Object.values(o)),
  ...Object.values(SUB),
  ...Object.values(NOISE),
  ...FILTER.flatMap((f) => Object.values(f)),
  ...ENV.flatMap((e) => Object.values(e)),
  ...LFO.flatMap((l) => Object.values(l)),
  ...MOD.flatMap((m) => Object.values(m)),
  ...MACRO,
  ...Object.values(GLOBAL),
];
