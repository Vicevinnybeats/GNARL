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
  /**
   * The engine name while filter 1 is in Vital's FORMANT model, which reads
   * its own controls instead of cutoff and resonance; null when that model
   * has nothing for the control. Absent: the same name in every model.
   */
  readonly formant?: string | null;
}

const P = (
  id: string,
  label: string,
  def: number,
  vital: string | null,
  unit: Unit = '%',
  min = 0,
  max = 1,
  extra: Partial<Pick<Param, 'bipolar' | 'step' | 'formant'>> = {},
): Param => ({ id, label, min, max, def, unit, vital, ...extra });

const osc = (n: 1 | 2, wt: number, warp: number, fm: number): Param[] => [
  P(`osc${n}.wtpos`, 'WT POS', wt, `osc_${n}_wave_frame`),
  P(`osc${n}.warp`, 'WARP', warp, `osc_${n}_distortion_amount`),
  // FM from the other oscillator, alongside the warp mode (docs/design/
  // phase2-08-panel-controls.md): the same law as Vital's FM warp.
  P(`osc${n}.fm`, 'FM', fm, `osc_${n}_fm_amount`),
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

  // Filter 1 (docs/design/phase2-06-vowel-filter.md). In the formant model
  // CUTOFF shifts the formants, RES is their resonance, MORPH slides between
  // the vowels on X, and there is no drive. In every other model MORPH is
  // Vital's blend: low pass -> band pass -> high pass.
  P('vowel.cutoff', 'CUTOFF', 0.5, 'filter_1_cutoff', 'hz', 0, 1, { formant: 'filter_1_formant_transpose' }),
  P('vowel.res', 'RES', 0.38, 'filter_1_resonance', '%', 0, 1, { formant: 'filter_1_formant_resonance' }),
  P('vowel.morph', 'MORPH', 0.62, 'filter_1_blend', '%', 0, 1, { formant: 'filter_1_formant_x' }),
  P('vowel.drive', 'DRIVE', 0.3, 'filter_1_drive', '%', 0, 1, { formant: null }),

  P('wobble.depth', 'DEPTH', 0.7, null),
  P('wobble.smooth', 'SMOOTH', 0.18, 'wobble_smooth_time'),
  P('wobble.phase', 'PHASE', 0.0, 'wobble_phase', 'deg'),

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
  // FOLD and CRUSH: the drive chain after DIST (docs/design/phase2-07-drive-chain.md).
  P('fold.amount', 'AMOUNT', 0.4, 'distortion_fold_drive', 'db'),
  P('fold.mix', 'MIX', 1, 'distortion_fold_mix'),
  P('crush.bits', 'BITS', 8, 'distortion_crush_bits', 'bits', 1, 16, { step: 1 }),
  P('crush.rate', 'RATE', 0, 'distortion_crush_rate'),
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
  /**
   * An option that is a GNARL switch rather than a value of `vital`: on, it
   * overrides the type (osc FOLD, DIST TUBE). Choosing any other option turns
   * it off and sets the type.
   */
  readonly flag?: { readonly name: string; readonly option: number };
}

export const CHOICES: readonly Choice[] = [
  // FORMANT / SYNC / BEND are Vital's warp modes 2 / 1 / 4. FOLD is not one.
  { id: 'osc1.mode', options: ['FORMANT', 'SYNC', 'BEND', 'FOLD'], def: 0, vital: 'osc_1_distortion_type',
    values: [2, 1, 4, null], flag: { name: 'osc_1_fold', option: 3 } },
  { id: 'osc2.mode', options: ['FORMANT', 'SYNC', 'BEND', 'FOLD'], def: 1, vital: 'osc_2_distortion_type',
    values: [2, 1, 4, null], flag: { name: 'osc_2_fold', option: 3 } },
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
  { id: 'dist.mode', options: ['TUBE', 'HARD', 'SOFT'], def: 0, vital: 'distortion_type', values: [null, 1, 0],
    flag: { name: 'distortion_tube', option: 0 } },
  // The FOLD stage's own type: Vital's sine or linear fold.
  { id: 'fold.mode', options: ['SINE', 'LINEAR'], def: 0, vital: 'distortion_fold_type', values: [0, 1] },
  { id: 'crush.mode', options: ['HARD', 'SOFT'], def: 0, vital: 'distortion_crush_mode', values: [0, 1] },
  // compressor_enabled_bands: 0 Multiband (three bands); 1 "Low Band", which
  // splits at the low crossover into two compressed bands - lows and the rest.
  { id: 'ott.mode', options: ['3-BAND', '2-BAND'], def: 0, vital: 'compressor_enabled_bands', values: [0, 1] },
];

/** Wobble destinations are independent toggles, one engine depth each. */
export const WOBBLE_DESTINATIONS = [
  { id: 'wobble.to.wtpos', label: 'WT POS', vital: 'wobble_amount_wave_frame', def: true },
  { id: 'wobble.to.cutoff', label: 'CUTOFF', vital: 'wobble_amount_cutoff', def: true },
  // The FM knob's depth. (wobble_amount_fm is osc 1's WARP, as presets saved it.)
  { id: 'wobble.to.fm', label: 'FM', vital: 'wobble_amount_osc_fm', def: true },
  { id: 'wobble.to.vowel', label: 'VOWEL', vital: 'wobble_amount_formant', def: false },
] as const;

/** Section switches (the dot in a panel's header) and the engine's on/off. */
export const POWER: Readonly<Record<string, string | null>> = {
  'osc1.on': 'osc_1_on',
  'osc2.on': 'osc_2_on',
  'vowel.on': 'filter_1_on',
  'dist.on': 'distortion_on',
  'ott.on': 'compressor_on',
  'sub.on': 'mono_sub_on',
  'fold.on': 'distortion_fold_on',
  'crush.on': 'distortion_crush_on',
  'wobble.on': null,
};

export const FX_SLOTS = ['dist', 'fold', 'crush', 'ott'] as const;

// The matrix's sources and destinations, and the engine names behind them
// (Vital's modulation sources; any modulatable parameter as a destination).
export const MOD_SOURCES = ['WOBBLE', 'ENV 2', 'LFO 1', 'MACRO 1', 'VELOCITY'] as const;
export const MOD_SOURCE_NAMES = ['wobble', 'env_2', 'lfo_1', 'macro_control_1', 'velocity'] as const;
export const MOD_DESTINATIONS = [
  'OSC1 WT POS',
  'OSC2 WT POS',
  'FILTER CUTOFF',
  'VOWEL MORPH',
  'OSC1 WARP',
  'SUB LEVEL',
  'FOLD AMOUNT',
] as const;
export const MOD_DESTINATION_NAMES = [
  'osc_1_wave_frame',
  'osc_2_wave_frame',
  'filter_1_cutoff',
  'filter_1_formant_x',
  'osc_1_distortion_amount',
  'mono_sub_level',
  'distortion_fold_drive',
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
