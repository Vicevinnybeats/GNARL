/*
 * The AI button's generator (docs/design/phase4-01-generator.md): a new
 * patch from a seed, without a network or a model. It runs in the page
 * (main.ts) and in Node (tools/generate_patches.mjs, which
 * tests/test_generate.py renders and measures), so what the button makes is
 * what was measured.
 *
 * It makes VARIATIONS of patches that already sound right: the ones the
 * sound matcher found against the references' wobs (tools/match.py,
 * phase4-02-matcher.md). A first
 * version built every sound from one voice with two measured numbers as its
 * target, and the producer called the results "very bad … not even close".
 * A variation moves where the wob starts and how far it goes, the LFO's
 * timing, the drive and the shelf, and now and then swaps the table or adds
 * an effect. A seed always makes the same patch; the patch's name carries it.
 */

import { TABLE_NAMES, tableJson } from './wavetables.ts';

export interface Generated {
  name: string;
  about: string;
  seed: number;
  /** The patch as .vital JSON text. */
  patch: string;
}

/** The patches a variation starts from: presets/*.vital, by name. */
// Not Vinny Bass 2: built differently (four LFOs, its own table), its
// variations measured 9 dB from it - a different sound, not a neighbour.
export const GENERATOR_BASES = ['Ref Wob 1', 'Ref Wob 2', 'Ref Wob 3', 'Ref Wob 4', 'Ref Wob 5', 'Ref Wob 6'] as const;

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
const ADJECTIVES = ['Gnarly', 'Wet', 'Rusty', 'Toxic', 'Swamp', 'Feral', 'Mutant', 'Sludge', 'Acid', 'Rabid',
  'Filthy', 'Hungry', 'Broken', 'Molten', 'Sick', 'Grimy'] as const;

type Settings = Record<string, unknown>;

const num = (s: Settings, key: string, fallback = 0): number => {
  const v = s[key];
  return typeof v === 'number' ? v : fallback;
};
const clamp = (v: number, lo: number, hi: number): number => Math.min(hi, Math.max(lo, v));

export function generatePatch(bases: Readonly<Record<string, string>>, seed: number): Generated {
  const r = random(seed);
  const choose = <T>(items: readonly T[]): T => items[Math.floor(r() * items.length)] ?? (items[0] as T);
  const available = GENERATOR_BASES.filter((b) => bases[b] !== undefined);
  if (available.length === 0) throw new Error('the generator has none of its base patches');
  const baseName = choose(available);
  return vary(bases[baseName] as string, r, seed, 1, `from ${baseName}`);
}

/*
 * The AI button's pick-the-best mode (main.ts evolveSheet): a child of the
 * patch the producer picked. `strength` scales every step (1 = a generator
 * variation; less as the picks go on, so the search settles where the ears
 * lead it).
 */
export function evolvePatch(parentText: string, seed: number, strength: number, label: string): Generated {
  return vary(parentText, random(seed), seed, strength, label);
}

function vary(text: string, r: () => number, seed: number, strength: number, origin: string): Generated {
  const between = (lo: number, hi: number): number => (lo + (hi - lo) * r()) * strength;
  const chance = (p: number): boolean => r() < p * strength;
  const choose = <T>(items: readonly T[]): T => items[Math.floor(r() * items.length)] ?? (items[0] as T);
  const patch = JSON.parse(text) as Record<string, unknown> & { settings: Settings };
  const s = patch.settings;
  const changes: string[] = [];

  // Where the wob starts in the table, and how far LFO 1 takes it.
  s.osc_1_wave_frame = clamp(num(s, 'osc_1_wave_frame') + between(-25, 25), 0, 256);
  const mods = s.modulations as { source: string; destination: string }[] | undefined;
  mods?.forEach((m, i) => {
    if (m.source === 'lfo_1' && m.destination !== '') {
      const key = `modulation_${i + 1}_amount`;
      s[key] = clamp(num(s, key) * (1 + between(-0.25, 0.25)), -1, 1);
    }
  });

  // LFO 1's timing: its turning points move a little; a one-shot sometimes
  // takes one beat instead of two, or the other way round.
  const lfos = s.lfos as { points: number[]; num_points: number }[] | undefined;
  const lfo = lfos?.[0];
  if (lfo && Array.isArray(lfo.points)) {
    const n = lfo.num_points;
    for (let i = 1; i < n - 1; i += 1) {
      const lo = (lfo.points[2 * (i - 1)] ?? 0) + 0.02;
      const hi = (lfo.points[2 * (i + 1)] ?? 1) - 0.02;
      lfo.points[2 * i] = clamp((lfo.points[2 * i] ?? 0.5) + between(-0.08, 0.08), lo, hi);
    }
  }
  if (num(s, 'lfo_1_sync_type') === 2 && num(s, 'lfo_1_sync') === 1 && chance(0.2)) {
    const tempo = num(s, 'lfo_1_tempo');
    if (tempo === 7 || tempo === 8) {
      s.lfo_1_tempo = tempo === 7 ? 8 : 7;
      changes.push(tempo === 7 ? 'one beat' : 'two beats');
    }
  }

  // Grit and brightness, either side of where the patch sits.
  const driveBefore = num(s, 'distortion_drive');
  s.distortion_drive = clamp(driveBefore + between(-4, 4), 0, 30);
  if (num(s, 'distortion_fold_on')) s.distortion_fold_drive = clamp(num(s, 'distortion_fold_drive') + between(-1.5, 1.5), 0, 12);
  if (num(s, 'eq_on')) s.eq_high_gain = clamp(num(s, 'eq_high_gain') + between(-2, 2), -12, 12);
  if (num(s, 'filter_1_on')) {
    if (num(s, 'filter_1_model') === 5) s.filter_1_formant_x = clamp(num(s, 'filter_1_formant_x') + between(-0.1, 0.1), 0, 1);
    else s.filter_1_cutoff = clamp(num(s, 'filter_1_cutoff') + between(-6, 6), 8, 136);
  }

  // Now and then a bigger step: another table, or an effect.
  let tableName = (s.wavetables as { name?: string }[] | undefined)?.[0]?.name ?? '';
  if (chance(0.15)) {
    tableName = choose(TABLE_NAMES.filter((t) => t !== tableName && t !== 'Basic'));
    const table = tableJson(tableName);
    if (table) (s.wavetables as unknown[])[0] = JSON.parse(table);
    changes.push(`the ${tableName} table`);
  }
  if (!num(s, 'flanger_on') && chance(0.2)) {
    Object.assign(s, { flanger_on: 1, flanger_tempo: 8, flanger_mod_depth: 0.3 + 0.3 * r(),
      flanger_feedback: 0.1 + 0.4 * r(), flanger_dry_wet: 0.3 + 0.2 * r() });
    changes.push('flanger');
  }
  if (!num(s, 'phaser_on') && chance(0.15)) {
    Object.assign(s, { phaser_on: 1, phaser_tempo: 8, phaser_feedback: 0.5 + 0.25 * r(), phaser_dry_wet: 0.3 + 0.3 * r() });
    changes.push('phaser');
  }
  if (!num(s, 'reverb_on') && chance(0.15)) {
    Object.assign(s, { reverb_on: 1, reverb_dry_wet: 0.08 + 0.1 * r(), reverb_size: 0.3 + 0.3 * r() });
    changes.push('reverb');
  }

  // A base's volume peaks at -3 dBFS (build_presets.mjs). Every variation's
  // volume is worked out from the BASE's level, carried along in the patch
  // (gnarl_level, which the engine ignores): 4.5 dB of room, less any drive
  // added since the base. Computed step by step instead, giving back drive
  // taken away, a chain of picks crept to +3.9 dBFS - less drive does not
  // make a saturated sound proportionally quieter. With 2.5 dB of room the
  // other steps (the wob's place in the table, the filter) still moved a
  // chain by up to 6 dB, to 0.0 dBFS (tests/test_generate.py).
  const level = (patch.gnarl_level as { volume: number; drive: number } | undefined) ??
    { volume: num(s, 'volume', 4600), drive: driveBefore };
  const added = Math.max(0, num(s, 'distortion_drive') - level.drive);
  s.volume = Math.pow(Math.max(0, Math.sqrt(level.volume) - 4.5 - added), 2);
  patch.gnarl_level = level;

  const noun = NOUNS[tableName] ?? 'Wob';
  const name = `${choose(ADJECTIVES)} ${noun} ${seed % 1000}`;
  const about = `Generated (seed ${seed}) ${origin}` + (changes.length ? `, with ${changes.join(', ')}` : '') + '.';
  patch.preset_name = name;
  patch.comments = about;
  patch.author = 'GNARL generator';
  return { name, about, seed, patch: JSON.stringify(patch) };
}
