/**
 * Mirror of plugin/source/params/ParameterChoices.h.
 *
 * ORDER IS FROZEN on the C++ side, because a preset stores the chosen index
 * rather than the name. These arrays must stay in the same order for the same
 * reason: a dropdown whose order drifts shows the wrong label for every saved
 * patch.
 */
export const OSC_MODE = ['Wavetable', 'Graintable'] as const;

export const WARP_MODE = [
  'Off', 'Sync', 'Bend +', 'Bend -', 'PWM', 'Asym',
  'Mirror', 'Quantize', 'FM', 'Ring Mod', 'Phase Dist', 'Remap',
] as const;

export const SUB_WAVEFORM = ['Sine', 'Triangle', 'Saw', 'Square', 'Pulse'] as const;

export const NOISE_TYPE = ['White', 'Pink', 'Brown', 'Blue', 'Vinyl'] as const;

export const FILTER_TYPE = [
  'LP 12', 'LP 24', 'HP 12', 'HP 24', 'BP 12', 'BP 24',
  'Notch 12', 'Notch 24', 'Ladder LP', 'Ladder HP', 'Comb', 'Formant',
] as const;

export const FILTER_ROUTING = ['Series', 'Parallel', 'Split'] as const;

export const DRIVE_CURVE = ['Tanh', 'Tube', 'Hard Clip', 'Fold', 'Rectify'] as const;

export const ENVELOPE_MODE = ['ADSR', 'DAHDSR'] as const;

/** Index 0 is the drawn curve: the drawable LFO is the feature, not an option
    buried behind the built-in shapes. */
export const LFO_SHAPE = [
  'Custom', 'Sine', 'Triangle', 'Saw Up', 'Saw Down', 'Square',
  'Random Step', 'Random Smooth',
] as const;

export const LFO_MODE = ['Trigger', 'Envelope', 'Free Run', 'S&H'] as const;

/** Triplet and dotted rates are FIRST-CLASS entries interleaved with the
    straight ones, not a separate mode behind a toggle. Riddim is built on the
    1/3 and 1/6 grids, so reaching a triplet rate must cost exactly as much as
    reaching a straight one. Slow to fast. */
export const LFO_RATE_DIVISION = [
  '8 Bars', '4 Bars', '2 Bars',
  '1/1',
  '1/2 D', '1/2', '1/2 T',
  '1/4 D', '1/4', '1/4 T',
  '1/8 D', '1/8', '1/8 T',
  '1/16 D', '1/16', '1/16 T',
  '1/32 D', '1/32', '1/32 T',
  '1/64',
] as const;

/** 1/12 and 1/24 are the triplet grids, drawn distinctly in the editor. */
export const GRID_DIVISION = [
  'Off', '1/4', '1/8', '1/12', '1/16', '1/24', '1/32',
] as const;

export const MOD_SOURCE = [
  'None',
  'Env 1', 'Env 2', 'Env 3', 'Env 4',
  'LFO 1', 'LFO 2', 'LFO 3', 'LFO 4',
  'Velocity', 'Note', 'Random', 'Uni Voice',
  'Mod Wheel', 'Pitch Bend', 'Aftertouch',
  'Macro 1', 'Macro 2', 'Macro 3', 'Macro 4',
] as const;

export const MOD_CURVE = ['Linear', 'Exp', 'Log', 'S-Curve', 'Quantize'] as const;

/** Macro 1 is GROWL, the one macro with a fixed identity. */
export const MACRO_NAMES = ['GROWL', 'Macro 2', 'Macro 3', 'Macro 4'] as const;

/** FX distortion curves. A superset of DRIVE_CURVE: bitcrush and downsample only make sense in the rack, where aliasing is the effect rather than something to remove. */
export const FX_DISTORTION_TYPE = [
  'Tanh', 'Tube', 'Hard Clip', 'Fold',
  'Rectify', 'Bitcrush', 'Downsample',
] as const;

/** FX filter types. A subset of FILTER_TYPE - no formant and no comb, because those get their character from tracking the note and the FX filter runs on the summed signal. */
export const FX_FILTER_TYPE = [
  'LP 12', 'LP 24', 'HP 12', 'HP 24',
  'BP 12', 'Notch 12',
] as const;

/** The rack's fourteen instances, in their DEFAULT chain order. The order the user sets is stored separately, as ValueTree state - see docs/fx-architecture.md. This array's own order is frozen like every other choice list. */
export const FX_SLOT_NAME = [
  'Distortion 1', 'EQ 1', 'Filter 1',
  'Distortion 2', 'EQ 2', 'Filter 2',
  'Chorus', 'Flanger', 'Phaser',
  'Hyper', 'Dimension', 'Delay',
  'Reverb', 'Limiter',
] as const;

export const OVERSAMPLING = ['Off', '2x', '4x'] as const;

export const POLY_MODE = ['Poly', 'Mono', 'Legato'] as const;

export const WAVETABLE_NAMES = [
  'Basic Shapes', 'Growl Vowels', 'Metallic FM', 'Hollow Comb', 'Odd Screech',
  'Sub Sine', 'Reese Detune', 'Formant Sweep', 'Bitcrush Steps', 'Pulse Width',
  'Additive Stack', 'Ring Mod Bell', 'Dirty Saw', 'Talk Box', 'Wobble Bass',
  'Glass Harmonics', 'Phase Distortion', 'Noise Bed', 'Growl Morph', 'Init',
] as const;

/** The formant filter's five vowel anchors, matching FormantFilter.h. */
export const VOWEL_ANCHORS = [
  { label: 'A', x: 0.5, y: 1.0 },
  { label: 'E', x: 0.0, y: 0.5 },
  { label: 'I', x: 0.15, y: 0.0 },
  { label: 'O', x: 1.0, y: 0.5 },
  { label: 'U', x: 0.85, y: 0.0 },
] as const;
