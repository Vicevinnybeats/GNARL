/*
 * The AI button's generator (docs/design/phase4-01-generator.md): a new
 * patch from a seed, inside the ranges the references' wobs were measured
 * at, without a network or a model. It runs in the page (main.ts) and in
 * Node (tools/generate_patches.mjs, which tests/test_generate.py renders and
 * measures), so what the button makes is what was measured.
 *
 * It starts from one built-in patch (the styles' shared voice, Yoi Talk) and
 * sets every setting the styles vary, so nothing of the base leaks through
 * except what the styles share: osc 1 moved through one of GNARL's tables by
 * LFO 1, drive, fold, OTT and a high shelf. A seed always makes the same
 * patch; the patch's name carries it.
 */

import { TABLE_NAMES, tableJson } from './wavetables.ts';

export interface Generated {
  name: string;
  about: string;
  seed: number;
  /** The patch as .vital JSON text. */
  patch: string;
}

/** The base patch: one of presets/*.vital, by name. */
export const GENERATOR_BASE = 'Yoi Talk';

/** mulberry32: small, fast, and the same in every browser and in Node. */
function random(seed: number): () => number {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

// The noun a table gives a sound's name.
const NOUNS: Readonly<Record<string, string>> = {
  Basic: 'Wob', Growl: 'Growl', Vowel: 'Talk', Croak: 'Croak', Fold: 'Fold', Sync: 'Rip', FM: 'Zap',
  Pulse: 'Buzz', Steps: 'Bit', Wub: 'Wub', Yoi: 'Yoi', Screech: 'Screech', Hollow: 'Bark', PD: 'Zap',
  Comb: 'Squelch', Metal: 'Grind', Tear: 'Tear', Harmonic: 'Swell',
};
/*
 * Some tables are darker or brighter than the rest through the same voice
 * (tests/test_generate.py, 40 seeds at F1: PD centred at 0.65-1.2 kHz, Wub
 * and Harmonic at 1.3-1.5, Pulse, Hollow and Sync at 2.6-2.8, against the
 * references' 1.9-2.2). The high shelf and where in the table the wob starts
 * pull them back: dB on the shelf, and the lowest starting frame. The
 * shelf also moves the peak, so the volume gives back half of it.
 */
const TILT: Readonly<Record<string, { shelf: number; start: number }>> = {
  PD: { shelf: 4, start: 120 }, Wub: { shelf: 3, start: 70 }, Harmonic: { shelf: 3, start: 60 },
  Pulse: { shelf: -3, start: 0 }, Hollow: { shelf: -3, start: 0 }, Sync: { shelf: -2, start: 0 },
  Steps: { shelf: -2, start: 0 },
};

const ADJECTIVES = ['Gnarly', 'Wet', 'Rusty', 'Toxic', 'Swamp', 'Feral', 'Mutant', 'Sludge', 'Acid', 'Rabid',
  'Filthy', 'Hungry', 'Broken', 'Molten', 'Sick', 'Grimy'] as const;

/*
 * LFO 1's shape over one wob (Vital's points: x, then y with 0 at the TOP).
 * The references' wobs open and close once, most often (oneshot/wobs.txt:
 * one brightness turn in 38 of 113 wobs, two in 18); some peak early, some
 * late. Two bumps for a fifth of them.
 */
function lfoShape(r: () => number): { name: string; points: number[][] } {
  const pick = r();
  if (pick < 0.45) {
    const peak = 0.3 + 0.55 * r();
    return { name: 'Swell', points: [[0, 1], [peak, 0], [1, 1]] };
  }
  if (pick < 0.65) {
    const peak = 0.1 + 0.2 * r();
    return { name: 'Early', points: [[0, 0.2 * r()], [peak, 0], [1, 1]] };
  }
  if (pick < 0.8) {
    return { name: 'Late', points: [[0, 1], [0.75 + 0.15 * r(), 0], [1, 1]] };
  }
  const first = 0.15 + 0.15 * r();
  const second = 0.55 + 0.25 * r();
  const dip = 0.4 + 0.4 * r();
  return { name: 'Double', points: [[0, 1], [first, 0], [(first + second) / 2, dip], [second, 0], [1, 1]] };
}

/** One modulation slot (1-based), as LoadSave stores it. */
function route(settings: Record<string, unknown>, slot: number, source: string, destination: string, amount: number,
  like: Record<string, unknown>): void {
  const list = settings.modulations as { source: string; destination: string }[];
  list[slot - 1] = { source, destination };
  for (const key of ['bipolar', 'bypass', 'power', 'stereo']) {
    settings[`modulation_${slot}_${key}`] = like[`modulation_1_${key}`] ?? 0;
  }
  settings[`modulation_${slot}_amount`] = amount;
}

export function generatePatch(baseText: string, seed: number): Generated {
  const r = random(seed);
  const between = (lo: number, hi: number): number => lo + (hi - lo) * r();
  const chance = (p: number): boolean => r() < p;
  const choose = <T>(items: readonly T[]): T => items[Math.floor(r() * items.length)] ?? (items[0] as T);

  const patch = JSON.parse(baseText) as Record<string, unknown> & { settings: Record<string, unknown> };
  const s = patch.settings;
  const tableName = choose(TABLE_NAMES.filter((t) => t !== 'Basic'));
  const table = tableJson(tableName);
  if (table) (s.wavetables as unknown[])[0] = JSON.parse(table);

  // One wob per key (the references), or a wobble looping while held.
  const loop = chance(0.3);
  let length: string;
  if (loop) {
    const rate = choose([{ sync: 1, tempo: 8, text: '1/4' }, { sync: 3, tempo: 9, text: '1/8T' }, { sync: 1, tempo: 9, text: '1/8' }]);
    Object.assign(s, { lfo_1_sync: rate.sync, lfo_1_tempo: rate.tempo, lfo_1_sync_type: 0,
      env_1_attack: 0, env_1_hold: 0, env_1_sustain: 1, env_1_release: 0.25 });
    length = `looping at ${rate.text}`;
  } else {
    // 1/2 = two beats, 1/4 = one: the references' wobs are 0.8-2.1 beats.
    const twoBeats = chance(0.6);
    // Hold for the wob at 140 BPM, then a fast fall (quartic time: s = v^4).
    Object.assign(s, { lfo_1_sync: 1, lfo_1_tempo: twoBeats ? 7 : 8, lfo_1_sync_type: 2,
      env_1_attack: 0, env_1_hold: Math.pow(twoBeats ? 0.84 : 0.41, 0.25), env_1_decay: 0.45,
      env_1_sustain: 0, env_1_release: 0.3 });
    length = twoBeats ? 'one wob over two beats' : 'one wob in a beat';
  }
  const shape = lfoShape(r);
  (s.lfos as unknown[])[0] = {
    name: shape.name, num_points: shape.points.length, points: shape.points.flat(),
    powers: shape.points.map(() => 0), smooth: false,
  };

  // The voice: the styles' ranges either side of what they measured at.
  const tilt = TILT[tableName] ?? { shelf: 0, start: 0 };
  Object.assign(s, {
    osc_1_wave_frame: Math.round(tilt.start + between(0, 40)),
    osc_1_unison_voices: Math.round(between(2, 4)), osc_1_unison_detune: between(0.8, 1.6),
    osc_1_stereo_spread: between(0.3, 0.6),
    distortion_on: 1, distortion_type: 0, distortion_drive: between(22, 30), distortion_mix: 0.8,
    distortion_fold_on: 1, distortion_fold_drive: between(6, 10),
    distortion_crush_on: 0,
    compressor_on: 1, compressor_mix: between(0.4, 0.65),
    eq_on: 1, eq_low_gain: 0, eq_band_gain: 0, eq_high_gain: tilt.shelf + between(4, 7), eq_high_cutoff: 88,
    filter_1_on: 0, filter_2_on: 0,
    flanger_on: 0, phaser_on: 0, chorus_on: 0, reverb_on: 0, delay_on: 0,
  });
  const like = { ...s };
  route(s, 1, 'lfo_1', 'osc_1_wave_frame', between(0.6, 1), like);
  const extras: string[] = [];
  let slot = 2;
  if (chance(0.2)) {
    Object.assign(s, { distortion_crush_on: 1, distortion_crush_bits: Math.round(between(8, 12)) });
    extras.push('crushed');
  }
  // Vital's formant filter, its vowel moved by the same LFO: the froggy ones.
  // Not on PD, which it takes down to about 1 kHz (seed 20).
  if (tableName !== 'PD' && chance(0.25)) {
    Object.assign(s, { filter_1_on: 1, filter_1_model: 5, filter_1_style: 0, filter_1_formant_x: between(0.15, 0.45),
      filter_1_formant_y: between(0.4, 0.7), filter_1_formant_resonance: 0.85 });
    route(s, slot++, 'lfo_1', 'filter_1_formant_x', between(0.4, 0.7), like);
    extras.push('a talking formant');
  }
  if (chance(0.15)) {
    route(s, slot++, 'lfo_1', 'osc_1_tune', between(0.05, 0.12), like);
    extras.push('the pitch bending');
  }
  if (chance(0.2)) {
    Object.assign(s, { flanger_on: 1, flanger_tempo: 8, flanger_mod_depth: between(0.3, 0.6),
      flanger_feedback: between(0.1, 0.5), flanger_dry_wet: between(0.4, 0.6) });
    extras.push('flanger');
  }
  if (chance(0.2)) {
    Object.assign(s, { phaser_on: 1, phaser_tempo: 8, phaser_feedback: between(0.5, 0.75), phaser_dry_wet: between(0.4, 0.7) });
    extras.push('phaser');
  }
  if (chance(0.15)) {
    Object.assign(s, { chorus_on: 1, chorus_dry_wet: between(0.2, 0.35) });
    extras.push('chorus');
  }
  if (chance(0.2)) {
    Object.assign(s, { reverb_on: 1, reverb_dry_wet: between(0.1, 0.2), reverb_size: between(0.3, 0.6) });
    extras.push('reverb');
  }
  // Below the styles' volume: a generated patch is not measured before it
  // plays, so it gets room (tests/test_generate.py: the loudest of 40 seeds,
  // D1 to F2, peaks at -2.6 dBFS; the median at -5.7).
  s.volume = Math.pow(Math.sqrt(4600) - 2 - tilt.shelf / 2, 2);

  const name = `${choose(ADJECTIVES)} ${NOUNS[tableName] ?? 'Wob'} ${seed % 1000}`;
  const about = `Generated (seed ${seed}): the ${tableName} table, ${length}, LFO shape ${shape.name.toLowerCase()}` +
    (extras.length ? `, with ${extras.join(', ')}` : '') + '.';
  patch.preset_name = name;
  patch.comments = about;
  patch.author = 'GNARL generator';
  return { name, about, seed, patch: JSON.stringify(patch) };
}
