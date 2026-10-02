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

/*
 * The patch every sound starts from: Vinny Bass 2, the producer's own, and
 * the one they call good (2026-10-02). Every sound before this was built
 * from the matcher's patches (Sig Wob, Ref Wob), and the producer heard
 * "all screech instead of a wob": measured, those barely move - tools/
 * measure.py finds no beat-locked movement in Sig Wob 1 or 3 or Ref Wob 3,
 * and Ref Wob 5's growl moves 0.07 where Vinny Bass 2's moves 0.72. The
 * matcher copied a wob's tone frozen in time, not its rhythm.
 *
 * So the generator now works INSIDE Vinny Bass 2's recipe, which is where
 * the wob lives: LFO 1 gates the level and the comb filter (the rhythm),
 * LFO 2 sweeps a spectral low pass (the wah), LFO 4 changes LFO 1's speed
 * within the bar, LFO 3 pushes the drive. A variation keeps all four routes
 * and moves their timing, depth and tone (riddimVary below);
 * tests/test_generate.py renders forty and requires each to keep a wob.
 */
export const GENERATOR_BASES = ['Vinny Bass 2'] as const;

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

/**
 * A new sound from a seed. `rhythm` ('1/4', '1/8', '1/8T', '1/16', '1/4T')
 * fixes LFO 1's speed; otherwise the seed picks one.
 */
export function generatePatch(bases: Readonly<Record<string, string>>, seed: number, rhythm?: string): Generated {
  const r = random(seed);
  const choose = <T>(items: readonly T[]): T => items[Math.floor(r() * items.length)] ?? (items[0] as T);
  const available = GENERATOR_BASES.filter((b) => bases[b] !== undefined);
  if (available.length === 0) throw new Error('the generator has none of its base patches');
  const baseName = choose(available);
  return vary(bases[baseName] as string, r, seed, 1, `from ${baseName}`, rhythm);
}

type Mod = { source: string; destination: string };

/** Vinny Bass 2's rhythm engine: LFO 1 on the level. */
function isRiddim(s: Settings): boolean {
  return ((s.modulations as Mod[] | undefined) ?? []).some((m) => m.source === 'lfo_1' && m.destination === 'osc_1_level');
}

/*
 * The rhythms LFO 1 may take, as (lfo_1_sync, lfo_1_tempo): sync 1 is
 * straight tempo, 3 triplets; tempo 8 is 1/4, 9 is 1/8, 10 is 1/16 (Vital's
 * kSyncedFrequencyNames). 1/8 is Vinny Bass 2's; 1/8T is riddim's triplet;
 * 1/4 the slow wob, one a beat (asked for by the producer, and every first
 * round of the AI sheet has one); 1/16 the stutter; 1/4T the half-time
 * triplet. Weighted toward 1/8 and 1/8T.
 */
export const RHYTHMS: readonly { sync: number; tempo: number; label: string; weight: number }[] = [
  { sync: 1, tempo: 9, label: '1/8', weight: 3 },
  { sync: 1, tempo: 8, label: '1/4', weight: 2 },
  { sync: 3, tempo: 9, label: '1/8T', weight: 3 },
  { sync: 1, tempo: 10, label: '1/16', weight: 1 },
  { sync: 3, tempo: 8, label: '1/4T', weight: 1 },
];
/** The spectral morphs LFO 2 may sweep (Vital's kSpectralMorphNames) and their amount ranges. */
const SWEEPS = [
  { type: 7, label: 'low pass', lo: 0.7, hi: 0.95 },
  { type: 2, label: 'formant', lo: 0.35, hi: 0.7 },
  { type: 3, label: 'harmonic stretch', lo: 0.3, hi: 0.6 },
  { type: 5, label: 'smear', lo: 0.4, hi: 0.8 },
] as const;
/** Oscillator warps (Vital's kPhaseDistortionNames). */
const WARPS = [
  { type: 2, label: 'formant' }, { type: 4, label: 'bend' }, { type: 5, label: 'squeeze' }, { type: 1, label: 'sync' },
] as const;
/** Distortion kinds (kDistortionTypeNames). Not bit crush: the producer's "screech". */
const DRIVES = [
  { type: 5, label: 'down-sample' }, { type: 0, label: 'soft clip' }, { type: 1, label: 'hard clip' },
  { type: 3, label: 'sine fold' },
] as const;
/*
 * LFO 1 shapes, as Vital stores them: x,y pairs with y = 0 at the TOP (full
 * level), one power per point. Vinny Bass 2's rises and falls once a cycle.
 */
const SHAPES = [
  { label: 'rise-and-fall', points: [0, 1, 0.5, 0, 1, 1], powers: [0.16, 0, 0] },
  { label: 'hit-and-fall', points: [0, 0, 1, 1], powers: [-3, 0] },
  { label: 'open-and-shut', points: [0, 1, 0.08, 0, 0.55, 0, 0.65, 1, 1, 1], powers: [0, 0, 0, 0, 0] },
  { label: 'swell', points: [0, 1, 0.85, 0, 1, 1], powers: [2.5, -2, 0] },
] as const;
/** Tables that keep a wob warm. Not Screech, Metal, Tear, Sync, PD or FM. */
const WARM_TABLES = ['Growl', 'Vowel', 'Wub', 'Yoi', 'Hollow', 'Harmonic', 'Croak', 'Comb', 'Pulse'] as const;

function riddimVary(patch: Record<string, unknown> & { settings: Settings }, r: () => number, strength: number,
  changes: string[], rhythmLabel?: string): string {
  const s = patch.settings;
  const between = (lo: number, hi: number): number => lo + (hi - lo) * r();
  const nudge = (v: number, by: number, lo: number, hi: number): number => clamp(v + between(-by, by) * strength, lo, hi);
  const chance = (p: number): boolean => r() < p * strength;
  const mods = (s.modulations as Mod[] | undefined) ?? [];
  const amount = (source: string, destination: string): string | null => {
    const i = mods.findIndex((m) => m.source === source && m.destination === destination);
    return i < 0 ? null : `modulation_${i + 1}_amount`;
  };
  const scale = (key: string | null, lo: number, hi: number): void => {
    if (key) s[key] = clamp(num(s, key) * (1 + between(lo, hi) * strength), -1, 1);
  };

  // The rhythm: LFO 1's speed, and how much LFO 4 bends it within the bar.
  const fixed = RHYTHMS.find((x) => x.label === rhythmLabel);
  const rolled = chance(0.5);
  if (fixed || rolled) {
    const total = RHYTHMS.reduce((t, x) => t + x.weight, 0);
    let pick = r() * total;
    const rhythm = fixed ?? RHYTHMS.find((x) => (pick -= x.weight) < 0) ?? RHYTHMS[0];
    if (rhythm && (rhythm.sync !== num(s, 'lfo_1_sync') || rhythm.tempo !== num(s, 'lfo_1_tempo'))) {
      s.lfo_1_sync = rhythm.sync;
      s.lfo_1_tempo = rhythm.tempo;
      changes.push(`${rhythm.label} wob`);
    }
  }
  scale(amount('lfo_4', 'lfo_1_tempo'), -0.6, 0.6);
  // A 1/4 wob is a steady one a beat: LFO 4's bend, which keeps 1/8 on
  // the grid, pulled 1/4 to 1.25 a beat (measured), so it is off for 1/4.
  const bend = amount('lfo_4', 'lfo_1_tempo');
  if (bend && num(s, 'lfo_1_sync') === 1 && num(s, 'lfo_1_tempo') === 8) s[bend] = 0;

  // The sweep: LFO 2 on the spectral low pass - how far, and how slow.
  scale(amount('lfo_2', 'osc_1_spectral_morph_amount'), -0.5, 0.7);
  if (chance(0.3)) {
    s.lfo_2_tempo = [6, 7, 8][Math.floor(r() * 3)] ?? 6;
    s.lfo_2_sync = r() < 0.3 ? 3 : 1;
  }
  s.osc_1_spectral_morph_amount = nudge(num(s, 'osc_1_spectral_morph_amount'), 0.12, 0.6, 0.97);

  // The tone: Vinny Bass 2's table is an audio file cut into a few slices,
  // so the frame picks a slice rather than morphing - measured at D#3 FL:
  // frames 60, 110-130 (Vinny Bass 2's own, 110), 160 and 200 are each a
  // full-level sound of their own; 80-108 is one slice 9 dB quieter, and
  // nudging into it made half of forty variations 12-17 dB down. Now and
  // then a warm table instead.
  if (chance(0.5)) s.osc_1_wave_frame = [60, 110, 110, 160, 200][Math.floor(r() * 5)] ?? 110;
  let tableName = (s.wavetables as { name?: string }[] | undefined)?.[0]?.name ?? '';
  if (chance(0.45)) {
    tableName = WARM_TABLES[Math.floor(r() * WARM_TABLES.length)] ?? 'Growl';
    const table = tableJson(tableName);
    if (table) (s.wavetables as unknown[])[0] = JSON.parse(table);
    s.osc_1_wave_frame = between(105, 236);
    changes.push(`the ${tableName} table`);
  }

  // FAR FROM VINNY BASS 2 (the producer: "the AI sounds good, but it should
  // change it completely"). Each move below is common on a new sound
  // (strength 1) and rarer as picks narrow in. None touches the four LFO
  // routes, so every result still wobs; tests/test_generate.py holds that.

  // The sweep's kind: LFO 2 moves whichever spectral morph is chosen - a
  // low pass (Vinny Bass 2's), a formant shift (talking), a harmonic stretch
  // (metallic yoi) or a smear (wet).
  if (chance(0.4)) {
    const kind = SWEEPS[Math.floor(r() * SWEEPS.length)] ?? SWEEPS[0];
    if (kind && kind.type !== num(s, 'osc_1_spectral_morph_type')) {
      s.osc_1_spectral_morph_type = kind.type;
      s.osc_1_spectral_morph_amount = between(kind.lo, kind.hi);
      changes.push(`a ${kind.label} sweep`);
    }
  }
  // A warp on the oscillator: Vital's formant, bend, squeeze or sync.
  if (chance(0.35)) {
    const warp = WARPS[Math.floor(r() * WARPS.length)] ?? WARPS[0];
    if (warp) {
      s.osc_1_distortion_type = warp.type;
      s.osc_1_distortion_amount = between(0.3, 0.7);
      changes.push(`${warp.label} warp`);
    }
  }
  // Width: unison voices, 2-5.
  if (chance(0.35)) {
    s.osc_1_unison_voices = 2 + Math.floor(r() * 4);
    s.osc_1_unison_detune = between(2, 8);
    changes.push('unison');
  }
  // A second oscillator under it, a warm table an octave down or at pitch,
  // through the same filter and gated by the same LFO 1, so it wobs too.
  const free = mods.findIndex((m) => m.source === '' && m.destination === '');
  if (!num(s, 'osc_2_on') && free >= 0 && chance(0.3)) {
    const layer = WARM_TABLES[Math.floor(r() * WARM_TABLES.length)] ?? 'Growl';
    const json = tableJson(layer);
    const tables = s.wavetables as unknown[] | undefined;
    if (json && tables && tables.length > 1) {
      tables[1] = JSON.parse(json);
      Object.assign(s, { osc_2_on: 1, osc_2_destination: 0, osc_2_level: between(0.35, 0.6),
        osc_2_transpose: r() < 0.6 ? -12 : 0, osc_2_wave_frame: between(40, 220) });
      mods[free] = { source: 'lfo_1', destination: 'osc_2_level' };
      s[`modulation_${free + 1}_amount`] = 1;
      changes.push(`a ${layer} layer`);
    }
  }
  // The distortion's kind: down-sample (Vinny Bass 2's), soft or hard clip,
  // or sine fold - its drive is held as below.
  if (chance(0.35)) {
    const kind = DRIVES[Math.floor(r() * DRIVES.length)] ?? DRIVES[0];
    if (kind && kind.type !== num(s, 'distortion_type')) {
      s.distortion_type = kind.type;
      changes.push(`${kind.label} drive`);
    }
  }
  // LFO 1's shape: the wob's own curve. Vinny Bass 2's rises and falls once
  // a cycle; the others hit and fall, open and shut, or swell. Same speed,
  // so the rate on the grid is kept.
  const lfos = s.lfos as { points: number[]; powers: number[]; num_points: number; smooth?: boolean }[] | undefined;
  if (lfos?.[0] && chance(0.3)) {
    const shape = SHAPES[Math.floor(r() * SHAPES.length)] ?? SHAPES[0];
    if (shape) {
      lfos[0] = { ...lfos[0], points: [...shape.points], powers: [...shape.powers], num_points: shape.powers.length, smooth: false };
      if (shape.label !== 'rise-and-fall') changes.push(`a ${shape.label} shape`);
    }
  }

  // The filter LFO 1 also opens: the comb's pitch and ring, or now and then
  // the formant filter, which makes the wob talk.
  if (num(s, 'filter_1_model') === 6) {
    s.filter_1_cutoff = nudge(num(s, 'filter_1_cutoff'), 10, 50, 90);
    s.filter_1_resonance = nudge(num(s, 'filter_1_resonance'), 0.12, 0.25, 0.65);
    if (chance(0.15)) {
      Object.assign(s, { filter_1_model: 5, filter_1_style: 0, filter_1_formant_x: between(0.2, 0.8),
        filter_1_formant_y: between(0.2, 0.8), filter_1_formant_resonance: between(0.6, 0.85) });
      changes.push('a talking filter');
    }
  } else if (num(s, 'filter_1_model') === 5) {
    s.filter_1_formant_x = nudge(num(s, 'filter_1_formant_x'), 0.15, 0, 1);
    s.filter_1_formant_y = nudge(num(s, 'filter_1_formant_y'), 0.15, 0, 1);
  }
  scale(amount('lfo_1', 'filter_fx_cutoff'), -0.4, 0.4);

  // Space: Vinny Bass 2 carries a little delay (13%, unsynced) and reverb
  // (15%). The producer wants them on SOME wobs, not all: a sound is dry,
  // delayed, reverbed, or keeps both as Vinny Bass 2 has them. A pick's
  // children usually keep its choice (re-rolled at chance 0.6 x
  // strength; a new sound always rolls). Both are kept off the low end: the delay's filter is a band
  // around the growl, the reverb's input is cut below about 150-250 Hz, so
  // the sub stays dry.
  if (r() < (strength >= 1 ? 1 : 0.6 * strength)) {
    const roll = r();
    if (roll < 0.4) {
      Object.assign(s, { delay_on: 0, reverb_on: 0 });
      patch.gnarl_delay_synced = 0;
      changes.push('dry');
    } else if (roll < 0.65) {
      patch.gnarl_delay_synced = 1;
      Object.assign(s, { delay_on: 1, reverb_on: 0, delay_style: 0,
        delay_dry_wet: between(0.12, 0.22), delay_feedback: between(0.25, 0.45),
        delay_filter_cutoff: between(70, 90), delay_filter_spread: between(0.6, 1) });
      changes.push('a delay');
    } else if (roll < 0.85) {
      patch.gnarl_delay_synced = 0;
      Object.assign(s, { delay_on: 0, reverb_on: 1, reverb_dry_wet: between(0.18, 0.3), reverb_size: between(0.5, 0.9),
        reverb_decay_time: between(0.5, 2), reverb_pre_low_cutoff: between(52, 60) });
      changes.push('reverb');
    }
    // else: both, as Vinny Bass 2 has them.
  }
  // An echo lands ON the wob's grid: the wob's own rhythm (straight or
  // triplet), at its speed or one slower. Off it - a dotted echo, or a
  // triplet one over a straight wob - it filled the gaps between the hits:
  // eleven delayed variations measured movement down to 0.33 and one at
  // 2.75 a beat. Re-locked every time, since a pick may change the rhythm.
  // (patch.gnarl_delay_synced, which the engine ignores, as gnarl_level.)
  if (patch.gnarl_delay_synced === 1) {
    s.delay_sync = num(s, 'lfo_1_sync') === 3 ? 3 : 1;
    s.delay_tempo = r() < 0.7 ? num(s, 'lfo_1_tempo', 9) : Math.max(4, num(s, 'lfo_1_tempo', 9) - 1);
  }

  // Grit: the drive LFO 3 pushes, never past where Vinny Bass 2 sits by
  // more than a little - added drive is added screech.
  const driveBefore = num(s, 'distortion_drive');
  s.distortion_drive = nudge(driveBefore, 3, 0, 9);
  scale(amount('lfo_3', 'distortion_drive'), -0.5, 0.5);
  return tableName;
}

/*
 * The AI button's pick-the-best mode (main.ts evolveSheet): a child of the
 * patch the producer picked. `strength` scales every step (1 = a generator
 * variation; less as the picks go on, so the search settles where the ears
 * lead it).
 */
/*
 * LEVELLING BY MEASUREMENT. Forty variations and five chains of picks showed
 * that no formula holds the level: the distortion's kind alone moves it ~5 dB
 * (soft clip, hard clip and sine fold quieter than down-sample), and with
 * down-sample LESS drive is LOUDER - a chain crept to +2.3 dBFS as its drive
 * fell. So every AI sound is rendered (the WebAssembly engine, in the page's
 * workers and in tools/generate_patches.mjs alike) at LEVEL_NOTES, held for
 * LEVEL_SECONDS, and its volume set so that the loudest sample sits at
 * LEVEL_TARGET_DB. The formula above stays as the first guess and the
 * fallback where no engine is at hand.
 */
export const LEVEL_NOTES = [26, 39]; // D1 and D#2 (renderer naming): the low end, and the producer's D#3 in FL
export const LEVEL_SECONDS = 1.6; // two wobs at 1/4, four at 1/8, at 140 BPM
export const LEVEL_TARGET_DB = -5;

/** The patch with its volume moved so that a measured `peak` (linear) lands on LEVEL_TARGET_DB. */
export function levelVolume(patchText: string, peak: number): string {
  if (!(peak > 1e-5) || !Number.isFinite(peak)) return patchText;
  const patch = JSON.parse(patchText) as { settings: Settings };
  const volume = num(patch.settings, 'volume', 4600);
  const shift = LEVEL_TARGET_DB - 20 * Math.log10(peak);
  // The volume control reads sqrt(value) - 80 dB, up to 7399.44 (+6 dB).
  patch.settings.volume = Math.min(7399.44, Math.pow(Math.max(0, Math.sqrt(volume) + shift), 2));
  return JSON.stringify(patch);
}

export function evolvePatch(parentText: string, seed: number, strength: number, label: string): Generated {
  return vary(parentText, random(seed), seed, strength, label);
}

function vary(text: string, r: () => number, seed: number, strength: number, origin: string,
  rhythm?: string): Generated {
  const between = (lo: number, hi: number): number => (lo + (hi - lo) * r()) * strength;
  const chance = (p: number): boolean => r() < p * strength;
  const choose = <T>(items: readonly T[]): T => items[Math.floor(r() * items.length)] ?? (items[0] as T);
  const patch = JSON.parse(text) as Record<string, unknown> & { settings: Settings };
  const s = patch.settings;
  const changes: string[] = [];
  if (isRiddim(s)) {
    const levelBefore = (patch.gnarl_level as { volume: number; drive: number } | undefined) ??
      { volume: num(s, 'volume', 4600), drive: num(s, 'distortion_drive') };
    const tableName = riddimVary(patch, r, strength, changes, rhythm);
    const added = Math.max(0, num(s, 'distortion_drive') - levelBefore.drive);
    // Unison voices can line up: forty variations at D1-F2 put the loudest,
    // with two voices, at +0.3 dBFS, the next at -3.1; 3 dB more room for
    // any unison. Worked out from the patch's state, not its last step, so a
    // chain of picks cannot accumulate it.
    const unison = num(s, 'osc_1_unison_voices', 1) > 1 ? 3 : 0;
    s.volume = Math.pow(Math.max(0, Math.sqrt(levelBefore.volume) - 5 - added - unison), 2);
    patch.gnarl_level = levelBefore;
    return named(patch, NOUNS[tableName] ?? 'Wob', seed, origin, changes, choose);
  }

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
  // (gnarl_level, which the engine ignores): 5 dB of room, less any drive
  // added since the base. Computed step by step instead, giving back drive
  // taken away, a chain of picks crept to +3.9 dBFS - less drive does not
  // make a saturated sound proportionally quieter. With 2.5 dB of room the
  // other steps (the wob's place in the table, the filter) still moved a
  // chain by up to 6 dB, to 0.0 dBFS; with 4.5 dB and the Sig Wobs as bases,
  // to -1.4 dBFS (tests/test_generate.py).
  const level = (patch.gnarl_level as { volume: number; drive: number } | undefined) ??
    { volume: num(s, 'volume', 4600), drive: driveBefore };
  const added = Math.max(0, num(s, 'distortion_drive') - level.drive);
  s.volume = Math.pow(Math.max(0, Math.sqrt(level.volume) - 5 - added), 2);
  patch.gnarl_level = level;

  return named(patch, NOUNS[tableName] ?? 'Wob', seed, origin, changes, choose);
}

function named(patch: Record<string, unknown>, noun: string, seed: number, origin: string, changes: string[],
  choose: <T>(items: readonly T[]) => T): Generated {
  const name = `${choose(ADJECTIVES)} ${noun} ${seed % 1000}`;
  const about = `Generated (seed ${seed}) ${origin}` + (changes.length ? `, with ${changes.join(', ')}` : '') + '.';
  patch.preset_name = name;
  patch.comments = about;
  patch.author = 'GNARL generator';
  return { name, about, seed, patch: JSON.stringify(patch) };
}
