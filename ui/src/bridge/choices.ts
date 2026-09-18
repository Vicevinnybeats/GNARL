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
