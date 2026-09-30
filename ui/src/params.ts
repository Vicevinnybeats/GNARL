/*
 * Every control on the panel, and what it drives in the engine.
 *
 * `vital` is the engine parameter the control will be bound to when this UI
 * runs inside the plugin. It is the contract with src/common/synth_parameters.cpp:
 * presets store parameters BY NAME, so these strings must match exactly.
 * `null` means the engine has nothing behind the control yet - the feature is
 * one of the Phase 2 items (docs/design/phase2-02-ui.md lists which).
 *
 * Values here are the CONTROL's own units (0..1 for a plain amount). The bridge
 * converts to the engine's range; that conversion belongs next to the binding,
 * not scattered through the widgets.
 */

export type Unit = '%' | 'st' | 'x' | 'db' | 'hz' | 'ms' | 'bits' | 'deg' | 'raw';

export interface Param {
  readonly id: string;
  readonly label: string;
  readonly min: number;
  readonly max: number;
  readonly def: number;
  readonly unit: Unit;
  /** Controls that are bipolar draw their arc from the centre. */
  readonly bipolar?: boolean;
  /** Integer steps (unison voices, bit depth). */
  readonly step?: number;
  readonly vital: string | null;
}

const P = (
  id: string,
  label: string,
  def: number,
  vital: string | null,
  unit: Unit = '%',
  min = 0,
  max = 1,
  extra: Partial<Pick<Param, 'bipolar' | 'step'>> = {},
): Param => ({ id, label, min, max, def, unit, vital, ...extra });

const osc = (n: 1 | 2, wt: number, warp: number, fm: number): Param[] => [
  P(`osc${n}.wtpos`, 'WT POS', wt, `osc_${n}_wave_frame`),
  P(`osc${n}.warp`, 'WARP', warp, `osc_${n}_distortion_amount`),
  // Vital's FM is a warp MODE (FM <- Osc), not an amount of its own, so a
  // separate FM knob needs engine work. See the design doc.
  P(`osc${n}.fm`, 'FM', fm, null),
  P(`osc${n}.unison`, 'UNISON', n === 1 ? 5 : 3, `osc_${n}_unison_voices`, 'x', 1, 16, { step: 1 }),
  P(`osc${n}.detune`, 'DETUNE', n === 1 ? 0.35 : 0.2, `osc_${n}_unison_detune`),
];

export const PARAMS: readonly Param[] = [
  P('master', 'MASTER', 0.72, 'volume', 'db'),

  ...osc(1, 0.3, 0.25, 0.35),
  ...osc(2, 0.2, 0.25, 0.2),

  // The clean mono sub (docs/design/phase2-05-mono-sub.md).
  P('sub.level', 'LEVEL', 0.71, 'mono_sub_level'),
  P('sub.drive', 'DRIVE', 0, 'mono_sub_drive'),

  P('vowel.cutoff', 'CUTOFF', 0.5, 'filter_1_cutoff', 'hz'),
  P('vowel.res', 'RES', 0.38, 'filter_1_resonance'),
  P('vowel.morph', 'MORPH', 0.62, 'filter_1_formant_x'),
  P('vowel.drive', 'DRIVE', 0.3, 'filter_1_drive'),

  P('wobble.depth', 'DEPTH', 0.7, null),
  P('wobble.smooth', 'SMOOTH', 0.35, null),
  P('wobble.phase', 'PHASE', 0.0, null, 'deg'),

  P('env.amp.att', 'ATT', 0.04, 'env_1_attack', 'ms'),
  P('env.amp.dec', 'DEC', 0.3, 'env_1_decay', 'ms'),
  P('env.amp.sus', 'SUS', 0.72, 'env_1_sustain'),
  P('env.amp.rel', 'REL', 0.22, 'env_1_release', 'ms'),
  P('env.filter.att', 'ATT', 0.01, 'env_2_attack', 'ms'),
  P('env.filter.dec', 'DEC', 0.45, 'env_2_decay', 'ms'),
  P('env.filter.sus', 'SUS', 0.3, 'env_2_sustain'),
  P('env.filter.rel', 'REL', 0.3, 'env_2_release', 'ms'),

  P('dist.drive', 'DRIVE', 0.55, 'distortion_drive', 'db'),
  P('dist.mix', 'MIX', 0.8, 'distortion_mix'),
  P('fold.amount', 'AMOUNT', 0.4, null),
  P('fold.mix', 'MIX', 0.5, null),
  P('crush.bits', 'BITS', 10, null, 'bits', 1, 16, { step: 1 }),
  P('crush.rate', 'RATE', 0.3, null),
  P('ott.depth', 'DEPTH', 0.45, 'compressor_mix'),
  P('ott.time', 'TIME', 0.5, 'compressor_attack'),
];

export const PARAM_BY_ID: ReadonlyMap<string, Param> = new Map(PARAMS.map((p) => [p.id, p]));

/** Choice controls (button rows). `vital` as above; values are option indices. */
export interface Choice {
  readonly id: string;
  readonly options: readonly string[];
  readonly def: number;
  readonly vital: string | null;
  /** The engine value each option sets; null for an option the engine lacks. */
  readonly values?: readonly (number | null)[];
}

export const CHOICES: readonly Choice[] = [
  // FORMANT / SYNC / BEND are Vital's warp modes 2 / 1 / 4. FOLD is not one.
  { id: 'osc1.mode', options: ['FORMANT', 'SYNC', 'BEND', 'FOLD'], def: 0, vital: 'osc_1_distortion_type', values: [2, 1, 4, null] },
  { id: 'osc2.mode', options: ['FORMANT', 'SYNC', 'BEND', 'FOLD'], def: 1, vital: 'osc_2_distortion_type', values: [2, 1, 4, null] },
  { id: 'sub.mono', options: ['MONO'], def: 1, vital: null },
  // A one-button toggle: off is mono_sub_octave 0, on is 1 (-1 octave).
  // The engine's -2 octaves lights neither.
  { id: 'sub.oct', options: ['-1 OCT'], def: 1, vital: 'mono_sub_octave', values: [0, 1] },
  { id: 'vowel.vowel', options: ['A', 'E', 'I', 'O', 'U'], def: 3, vital: null },
  // The one wobble parameter that already exists in the engine (Phase 2 #1).
  { id: 'wobble.rate', options: ['1/4', '1/8', '1/8T', '1/16'], def: 1, vital: 'wobble_rate', values: [0, 1, 2, 3] },
  { id: 'wobble.shape', options: ['SINE', 'SOFT SQR', 'DRAW'], def: 1, vital: null },
  { id: 'env.page', options: ['AMP', 'FILTER'], def: 0, vital: null },
  // Vital's distortion types: 0 Soft Clip, 1 Hard Clip. No tube model yet.
  { id: 'dist.mode', options: ['TUBE', 'HARD', 'SOFT'], def: 0, vital: 'distortion_type', values: [null, 1, 0] },
  // Folding is a Vital distortion TYPE, and the DIST tile owns that
  // parameter: a separate FOLD stage is the Phase 2 distortion chain.
  { id: 'fold.mode', options: ['SINE', 'LINEAR'], def: 0, vital: null },
  { id: 'crush.mode', options: ['HARD', 'SOFT'], def: 0, vital: null },
  // compressor_enabled_bands: 0 Multiband (three bands). No two-band mode.
  { id: 'ott.mode', options: ['3-BAND', '2-BAND'], def: 0, vital: 'compressor_enabled_bands', values: [0, null] },
];

/** Wobble destinations are independent toggles, one engine depth each. */
export const WOBBLE_DESTINATIONS = [
  { id: 'wobble.to.wtpos', label: 'WT POS', vital: 'wobble_amount_wave_frame', def: true },
  { id: 'wobble.to.cutoff', label: 'CUTOFF', vital: 'wobble_amount_cutoff', def: true },
  { id: 'wobble.to.fm', label: 'FM', vital: 'wobble_amount_fm', def: true },
  { id: 'wobble.to.vowel', label: 'VOWEL', vital: null, def: false },
] as const;

/** Section switches (the dot in a panel's header) and the engine's on/off. */
export const POWER: Readonly<Record<string, string | null>> = {
  'osc1.on': 'osc_1_on',
  'osc2.on': 'osc_2_on',
  'vowel.on': 'filter_1_on',
  'dist.on': 'distortion_on',
  'ott.on': 'compressor_on',
  'sub.on': 'mono_sub_on',
  'fold.on': null,
  'crush.on': null,
  'wobble.on': null,
};

export const FX_SLOTS = ['dist', 'fold', 'crush', 'ott'] as const;

export const MOD_SOURCES = ['WOBBLE', 'ENV 2', 'LFO 1', 'MACRO 1', 'VELOCITY'] as const;
export const MOD_DESTINATIONS = [
  'OSC1 WT POS',
  'OSC2 WT POS',
  'FILTER CUTOFF',
  'VOWEL MORPH',
  'OSC1 WARP',
  'SUB LEVEL',
  'FOLD AMOUNT',
] as const;

export const PRESET_NAMES = [
  'Init — Riddim Wub',
  'Triplet Growl',
  'Formant Yoy',
  'Tearout Saw',
  'Metal Screech',
  'Sub Wobble',
  'Vowel Chop',
  'Foldback Bass',
  'Crushed Reese',
  'Neuro Riddim',
] as const;

/** A value as the readout under a knob shows it. */
export function formatValue(p: Param, v: number): string {
  switch (p.unit) {
    case 'x':
      return `${Math.round(v)}`;
    case 'bits':
      return `${Math.round(v)} bit`;
    case 'deg':
      return `${Math.round(v * 360)}°`;
    case 'hz': {
      // Displayed on the same 8 Hz .. 20 kHz log skew the engine's cutoff uses.
      const hz = 8 * Math.pow(20000 / 8, v);
      return hz >= 1000 ? `${(hz / 1000).toFixed(2)} kHz` : `${hz.toFixed(0)} Hz`;
    }
    case 'ms': {
      // Time controls are skewed so the short end, where riddim lives, gets
      // most of the travel: v^3 over 0..8 s.
      const ms = Math.pow(v, 3) * 8000;
      return ms >= 1000 ? `${(ms / 1000).toFixed(2)} s` : `${ms.toFixed(ms < 10 ? 1 : 0)} ms`;
    }
    case 'db': {
      const db = v <= 0.0001 ? -Infinity : 40 * Math.log10(v / 0.72);
      return Number.isFinite(db) ? `${db >= 0 ? '+' : ''}${db.toFixed(1)} dB` : '-inf';
    }
    default:
      return `${Math.round(v * 100)} %`;
  }
}
