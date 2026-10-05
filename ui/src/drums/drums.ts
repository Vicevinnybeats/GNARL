/*
 * DRUMS: riddim drum loops built from the producer's reference tracks
 * (docs/design/phase4-06-drums-riddimize.md).
 *
 * Every sound is synthesized here - no sample from anyone - from numbers
 * measured off the tracks the producer uploaded (tools/measure_drums.py ->
 * references/drums.json -> tools/drum_templates.py -> templates.json): how
 * long and how bright each drum is. The patterns are a library per row
 * (PATTERNS), riddim's kick-on-1, snare-on-3 in every one, worked like
 * DrumSmith's: a pattern and a sound per row, each re-rolled or locked on
 * its own. Pure, no DOM, so Node runs it (tools/render_tools.mjs).
 */

import templateFile from './templates.json' with { type: 'json' };
import kitFile from './kits.json' with { type: 'json' };

export type Row = 'kick' | 'snare' | 'hat' | 'open';
export const ROWS: readonly Row[] = ['kick', 'snare', 'hat', 'open'];
export const ROW_NAMES: Record<Row, string> = { kick: 'KICK', snare: 'SNARE', hat: 'HAT', open: 'OPEN HAT' };
export const STEPS = 16; // per bar, straight 16ths
export const BARS = 4;

interface Template {
  name: string;
  bpm: number;
  sound: {
    kick: { decay_ms: number | null; pitch_start_hz?: number | null };
    snare: { decay_ms: number | null; centroid_hz?: number | null };
    hat: { decay_ms: number | null; centroid_hz?: number | null };
  };
}
export const TEMPLATES = (templateFile as unknown as { templates: Template[] }).templates;

export interface DrumSound {
  /**
   * The kick: a sine falling from pitchStart to pitchEnd (sweepMs a time
   * constant), held for holdMs then falling 20 dB in decayMs; a noise click
   * of clickMs; tanh drive.
   */
  kick: { pitchStart: number; pitchEnd: number; sweepMs: number; decayMs: number; click: number; drive: number;
    holdMs?: number; clickMs?: number };
  /**
   * The snare: a tone (body, falling 20 dB in bodyMs) under band-passed
   * noise (noiseHz, noiseQ, falling in decayMs), a short bright crack, tanh
   * drive, high-passed at hpHz.
   */
  snare: { tone: number; noiseHz: number; decayMs: number; body: number; drive: number;
    noiseQ?: number; bodyMs?: number; crack?: number; hpHz?: number;
    /** How much of the kick plays under the snare: in several of the producer's tracks the beat-3 hit is both. */
    kickLayer?: number };
  hat: { hz: number; decayMs: number };
  open: { hz: number; decayMs: number };
}

/**
 * A row's pattern: the steps of one bar (0-15) it hits, and for the hat, a
 * roll on a step (2 = two 32nds, 3 = a triplet in that 16th).
 */
export interface RowPattern {
  name: string;
  steps: number[];
  rolls?: Record<number, number>;
}

/*
 * Each row's patterns, as DrumSmith keeps a library of flows per row and
 * picks one per row (the producer asked for DRUMS to work like it). Riddim's
 * skeleton is in every one: the kick on beat 1, the snare on beat 3 (half
 * time at 140) - all fifteen of the producer's tracks have both
 * (references/drums.json); the extra kicks sit where those tracks put theirs
 * (beat 2, the 16th after beat 3, beat 4). The hats run every 4th step or
 * trap-style most often ("hats are like 4 step or like hip hop trap").
 */
export const PATTERNS: Record<Row, readonly RowPattern[]> = {
  kick: [
    { name: 'ONE', steps: [0] },
    { name: 'ONE + 3E', steps: [0, 10] },
    { name: 'ONE + TWO', steps: [0, 4] },
    { name: 'ONE + FOUR', steps: [0, 12] },
    { name: 'ONE + 1A', steps: [0, 3] },
    { name: 'ONE + 4AND', steps: [0, 14] },
    { name: 'BOUNCE', steps: [0, 6, 10] },
    { name: 'PUSH', steps: [0, 11] },
    { name: 'DOUBLE', steps: [0, 2] },
  ],
  snare: [
    { name: 'THREE', steps: [8] },
    { name: 'THREE + GHOST', steps: [8, 15] },
    { name: 'THREE + DRAG', steps: [8, 14] },
    { name: 'THREE + PICKUP', steps: [8, 13, 15] },
  ],
  hat: [
    { name: '4 STEP', steps: [0, 4, 8, 12] },
    { name: 'OFFBEAT', steps: [2, 6, 10, 14] },
    { name: '1/8', steps: [0, 2, 4, 6, 8, 10, 12, 14] },
    { name: 'TRAP 1', steps: [0, 2, 4, 6, 7, 8, 10, 12, 14], rolls: { 14: 3 } },
    { name: 'TRAP 2', steps: [0, 2, 4, 6, 8, 10, 11, 12, 14, 15], rolls: { 12: 2, 15: 3 } },
    { name: 'TRAP 3', steps: [0, 2, 3, 4, 6, 8, 10, 12, 13, 14], rolls: { 6: 2, 14: 3 } },
    { name: 'TRAP 4', steps: [0, 2, 4, 6, 8, 10, 12, 14, 15], rolls: { 4: 3, 12: 2, 15: 2 } },
    { name: '1/16', steps: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15] },
    { name: 'SPARSE', steps: [4, 12] },
  ],
  open: [
    { name: 'NONE', steps: [] },
    { name: 'FOUR AND', steps: [14] },
    { name: 'TWO AND', steps: [6] },
    { name: 'BOTH ANDS', steps: [6, 14] },
    { name: 'ONE AND', steps: [2] },
  ],
};

export interface DrumLoop {
  /** The track the hat's sound was measured from, naming the loop. */
  name: string;
  /**
   * BARS bars of STEPS steps per row. The pattern is ONE bar, repeated as a
   * DAW's step sequencer repeats it: every bar holds the same steps, and an
   * edit is made to all of them (setStep). The producer: "the pattern is
   * always repeating even when I move a block" - when each bar was its own,
   * an edit changed one bar in four.
   */
  hits: Record<Row, boolean[][]>;
  /** Per step, how many hats the closed hat plays in it: 1, or a roll of 2 (32nds) or 3 (a triplet). */
  rolls: number[];
  /** Each row's pattern, an index into PATTERNS[row]. */
  picks: Record<Row, number>;
  /** Each row's sound: the track it was measured from. */
  sources: Record<Row, string>;
  sound: DrumSound;
}

/** Seeded random, so a loop can be made again from its seed. */
export function random(seed: number): () => number {
  let a = seed >>> 0 || 1;
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

const clamp = (v: number, lo: number, hi: number): number => Math.max(lo, Math.min(hi, v));
const empty = (): boolean[][] => Array.from({ length: BARS }, () => new Array<boolean>(STEPS).fill(false));

/** Sets one step of the pattern in every bar. */
export function setStep(loop: DrumLoop, row: Row, step: number, on: boolean): void {
  for (const bar of loop.hits[row]) bar[step] = on;
}

/** Puts a row's pattern number `index` (wrapping) in every bar. */
export function applyPattern(loop: DrumLoop, row: Row, index: number): void {
  const list = PATTERNS[row];
  const k = ((index % list.length) + list.length) % list.length;
  const p = list[k];
  if (!p) return;
  for (let s = 0; s < STEPS; s += 1) setStep(loop, row, s, p.steps.includes(s));
  if (row === 'hat') for (let s = 0; s < STEPS; s += 1) loop.rolls[s] = p.rolls?.[s] ?? 1;
  loop.picks[row] = k;
}

/** One row's sound, from a measured template, each number moved up to 12%. */
function rowSound(row: Row, t: Template, r: () => number): Partial<DrumSound> {
  const vary = (v: number): number => v * (0.88 + 0.24 * r());
  // The measured hat centroid (3-8 kHz) is the whole mix's at the hat's
  // moment, kick and bass included, so it reads low; a riddim hat is
  // bright. Mapped 3-8 kHz -> 7-11 kHz, keeping the tracks' order.
  const hatHz = 7000 + 4000 * clamp(((t.sound.hat.centroid_hz ?? 5500) - 3000) / 5000, 0, 1);
  const hatDecay = clamp(t.sound.hat.decay_ms ?? 60, 30, 160);
  switch (row) {
    case 'kick':
      return { kick: {
        pitchStart: vary(clamp(t.sound.kick.pitch_start_hz ?? 160, 90, 220)),
        pitchEnd: vary(48),
        sweepMs: vary(35),
        // The measured fall (on the percussive part, HPSS) is the punch's; a
        // kick's tail is tonal and HPSS strips it, so the body is set here.
        decayMs: vary(clamp((t.sound.kick.decay_ms ?? 60) * 4, 140, 320)),
        click: vary(0.35),
        drive: vary(0.5),
      } };
    case 'snare':
      return { snare: {
        tone: vary(200),
        noiseHz: vary(clamp(t.sound.snare.centroid_hz ?? 3000, 1500, 5000)),
        decayMs: vary(clamp(t.sound.snare.decay_ms ?? 140, 90, 220)),
        body: vary(0.45),
        drive: vary(0.4),
      } };
    case 'hat':
      return { hat: { hz: vary(hatHz), decayMs: vary(Math.min(hatDecay, 70)) } };
    case 'open':
      return { open: { hz: vary(hatHz * 0.9), decayMs: vary(clamp(hatDecay * 2.2, 160, 320)) } };
  }
}

/**
 * Kicks and snares fitted to the producer's tracks (tools/drum_prints.py ->
 * tools/fit_drums.mjs -> kits.json): GNARL's synth settings whose
 * fingerprint is closest to each track's, and how close (dB). Used as they
 * are - the producer asked for them to sound the same as the tracks.
 */
export interface Kit {
  name: string;
  kick: DrumSound['kick'];
  snare: DrumSound['snare'];
  kick_db: number;
  snare_db: number;
}
export const KITS = (kitFile as unknown as { kits: Kit[] }).kits.filter((k) => k.kick && k.snare);

/** One row's sound from template `t`, put in the loop: a kick or snare from its track's kit (or another's). */
function setSound(loop: DrumLoop, row: Row, t: Template, r: () => number): void {
  if ((row === 'kick' || row === 'snare') && KITS.length) {
    const kit = KITS.find((k) => t.name.startsWith(k.name)) ?? KITS[Math.floor(r() * KITS.length)]!;
    Object.assign(loop.sound, { [row]: { ...kit[row] } });
    loop.sources[row] = kit.name;
    return;
  }
  Object.assign(loop.sound, rowSound(row, t, r));
  loop.sources[row] = t.name;
  if (row === 'hat') loop.name = t.name;
}

/**
 * Re-rolls one row, as DrumSmith's dice does: a pattern from the row's list
 * and a sound from one of the producer's tracks.
 */
export function randomizeRow(loop: DrumLoop, row: Row, r: () => number = Math.random): void {
  applyPattern(loop, row, Math.floor(r() * PATTERNS[row].length));
  const t = TEMPLATES[Math.floor(r() * TEMPLATES.length)] ?? TEMPLATES[0];
  if (t) setSound(loop, row, t, r);
}

/*
 * A loop: every row re-rolled, except those in `keep` (DrumSmith's locks),
 * which come from `from`. With `templateIndex`, every sound is that
 * template's. The same bar plays four times, with no fills: until 2026-10-05
 * each bar drew its own steps from the templates' odds, and the producer
 * heard the pattern as "totally wrong".
 */
export function generateLoop(seed: number, templateIndex?: number,
  options: { fill?: boolean; keep?: readonly Row[]; from?: DrumLoop } = {}): DrumLoop {
  const r = random(seed);
  const loop: DrumLoop = {
    name: '',
    hits: { kick: empty(), snare: empty(), hat: empty(), open: empty() },
    rolls: new Array<number>(STEPS).fill(1),
    picks: { kick: 0, snare: 0, hat: 0, open: 0 },
    sources: { kick: '', snare: '', hat: '', open: '' },
    sound: {} as DrumSound,
  };
  const fixed = templateIndex === undefined ? undefined : TEMPLATES[templateIndex];
  for (const row of ROWS) {
    const from = options.from;
    if (from && options.keep?.includes(row)) {
      loop.hits[row] = from.hits[row].map((bar) => [...bar]);
      if (row === 'hat') {
        loop.rolls = [...from.rolls];
        loop.name = from.name;
      }
      loop.picks[row] = from.picks[row];
      loop.sources[row] = from.sources[row];
      Object.assign(loop.sound, { [row]: { ...from.sound[row] } });
    } else if (fixed) {
      applyPattern(loop, row, Math.floor(r() * PATTERNS[row].length));
      setSound(loop, row, fixed, r);
    } else {
      randomizeRow(loop, row, r);
    }
  }
  return loop;
}

/* ------------------------------------------------------------------ synthesis */

/** RBJ biquad, run over a buffer in place. */
function biquad(x: Float32Array, type: 'bandpass' | 'highpass', hz: number, q: number, sr: number): void {
  const w = (2 * Math.PI * Math.min(hz, sr * 0.45)) / sr;
  const alpha = Math.sin(w) / (2 * q);
  const cos = Math.cos(w);
  let b0: number, b1: number, b2: number;
  if (type === 'bandpass') {
    b0 = alpha;
    b1 = 0;
    b2 = -alpha;
  } else {
    b0 = (1 + cos) / 2;
    b1 = -(1 + cos);
    b2 = (1 + cos) / 2;
  }
  const a0 = 1 + alpha;
  const a1 = -2 * cos;
  const a2 = 1 - alpha;
  let x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  for (let i = 0; i < x.length; i += 1) {
    const v = x[i] ?? 0;
    const y = (b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0;
    x2 = x1;
    x1 = v;
    y2 = y1;
    y1 = y;
    x[i] = y;
  }
}

/** exp(-t / tau) with tau set so the level is 20 dB down at `ms`. */
const fall = (ms: number): number => ms / 1000 / Math.log(10);
const drive = (v: number, amount: number): number => {
  const k = 1 + 6 * amount;
  return Math.tanh(k * v) / Math.tanh(k);
};

export function kickVoice(s: DrumSound['kick'], sr: number, r: () => number): Float32Array {
  const hold = (s.holdMs ?? 0) / 1000;
  const clickS = (s.clickMs ?? 4) / 1000;
  const n = Math.round(sr * (hold + (s.decayMs / 1000) * 3));
  const out = new Float32Array(n);
  const tauA = fall(s.decayMs);
  const tauP = s.sweepMs / 1000;
  let phase = 0;
  for (let i = 0; i < n; i += 1) {
    const t = i / sr;
    const f = s.pitchEnd + (s.pitchStart - s.pitchEnd) * Math.exp(-t / tauP);
    phase += (2 * Math.PI * f) / sr;
    const click = t < clickS ? s.click * (r() * 2 - 1) * (1 - t / clickS) : 0;
    const amp = t < hold ? 1 : Math.exp(-(t - hold) / tauA);
    out[i] = drive(Math.sin(phase) * amp + click, s.drive);
  }
  return out;
}

export function snareVoice(s: DrumSound['snare'], sr: number, r: () => number): Float32Array {
  const bodyMs = s.bodyMs ?? s.decayMs * 0.6;
  const n = Math.round(sr * (Math.max(s.decayMs, bodyMs) / 1000) * 3);
  const noise = new Float32Array(n);
  for (let i = 0; i < n; i += 1) noise[i] = r() * 2 - 1;
  biquad(noise, 'bandpass', s.noiseHz, s.noiseQ ?? 0.7, sr);
  biquad(noise, 'highpass', 400, 0.7, sr);
  // The crack: a few milliseconds of bright noise, the stick's attack.
  const crackN = Math.round(0.012 * sr);
  const crack = new Float32Array(crackN);
  for (let i = 0; i < crackN; i += 1) crack[i] = r() * 2 - 1;
  biquad(crack, 'highpass', 3000, 0.7, sr);
  const out = new Float32Array(n);
  const tau = fall(s.decayMs);
  const tauBody = fall(bodyMs);
  const tauCrack = fall(8);
  let phase = 0;
  for (let i = 0; i < n; i += 1) {
    const t = i / sr;
    phase += (2 * Math.PI * s.tone * (1 + 0.15 * Math.exp(-t / 0.01))) / sr;
    const body = s.body * Math.sin(phase) * Math.exp(-t / tauBody);
    const c = i < crackN ? (s.crack ?? 0) * (crack[i] ?? 0) * Math.exp(-t / tauCrack) : 0;
    out[i] = drive(body + 2.2 * (noise[i] ?? 0) * Math.exp(-t / tau) + c, s.drive);
  }
  // Driving noise makes rumble a snare does not have: under a bare loop it
  // raised the sub by 18 dB and read as a kick (tools/measure_drums.py).
  biquad(out, 'highpass', s.hpHz ?? 120, 0.7, sr);
  return out;
}

/** The snare with `kickLayer` of the kick under it (no kick, or 0: the snare alone). */
export function layeredSnare(s: DrumSound['snare'], kick: DrumSound['kick'] | null, sr: number, r: () => number): Float32Array {
  const snare = snareVoice(s, sr, r);
  const layer = s.kickLayer ?? 0;
  if (!kick || layer <= 0) return snare;
  const k = kickVoice(kick, sr, r);
  const out = new Float32Array(Math.max(snare.length, k.length));
  for (let i = 0; i < out.length; i += 1) out[i] = (snare[i] ?? 0) + layer * (k[i] ?? 0);
  return out;
}

// A metallic hat: six square waves at the classic drum machine's ratios,
// plus noise, high passed.
const HAT_RATIOS = [1, 1.342, 1.2312, 1.6532, 1.9523, 2.1523];
function hatVoice(s: { hz: number; decayMs: number }, sr: number, r: () => number): Float32Array {
  const n = Math.round(sr * (s.decayMs / 1000) * 3);
  const out = new Float32Array(n);
  const base = s.hz / 14;
  for (let i = 0; i < n; i += 1) {
    const t = i / sr;
    let v = 0;
    for (const k of HAT_RATIOS) v += Math.sin(2 * Math.PI * base * k * t) >= 0 ? 1 : -1;
    out[i] = v / 6 * 0.6 + (r() * 2 - 1) * 0.5;
  }
  biquad(out, 'highpass', s.hz * 0.7, 0.8, sr);
  const tau = fall(s.decayMs);
  for (let i = 0; i < n; i += 1) out[i] = (out[i] ?? 0) * Math.exp(-i / sr / tau);
  return out;
}

const LEVEL: Record<Row, number> = { kick: 1.0, snare: 0.75, hat: 0.22, open: 0.2 };
// Hats a little apart, kick and snare in the middle.
const PAN: Record<Row, number> = { kick: 0, snare: 0, hat: -0.25, open: 0.25 };

/** A row's hits as (bar, place in the bar 0-1): each step, and a hat roll's 2 or 3 spread over its 16th. */
function hitPlaces(loop: DrumLoop, row: Row): [number, number][] {
  const out: [number, number][] = [];
  loop.hits[row].forEach((steps, b) => steps.forEach((on, s) => {
    if (!on) return;
    const n = row === 'hat' ? Math.max(1, loop.rolls[s] ?? 1) : 1;
    for (let k = 0; k < n; k += 1) out.push([b, (s + k / n) / STEPS]);
  }));
  return out;
}

/** Where a row's hits fall, in seconds from the loop's start. */
export function hitTimes(loop: DrumLoop, bpm: number, row: Row): number[] {
  return hitPlaces(loop, row).map(([b, place]) => (b + place) * ((4 * 60) / bpm));
}

/** The loop as two channels, exactly BARS bars long at `bpm`, peaking at -1 dBFS (or one row of it). */
export function renderLoop(loop: DrumLoop, bpm: number, sr = 44100, seed = 1, only?: Row): [Float32Array, Float32Array] {
  const r = random(seed);
  const barSamples = (4 * 60 / bpm) * sr;
  const length = Math.round(barSamples * BARS);
  const left = new Float32Array(length);
  const right = new Float32Array(length);
  const voices: Record<Row, Float32Array> = {
    kick: kickVoice(loop.sound.kick, sr, r),
    snare: layeredSnare(loop.sound.snare, loop.sound.kick, sr, r),
    hat: hatVoice(loop.sound.hat, sr, r),
    open: hatVoice(loop.sound.open, sr, r),
  };
  // In samples: each bar's start plus the hit's place in its bar, rounded
  // apart, so every bar is the same bar (a 32nd roll falls half a sample off
  // the grid; rounded as one time it moved a sample from bar to bar).
  const starts = (row: Row): number[] =>
    hitPlaces(loop, row).map(([b, place]) => Math.round(b * barSamples) + Math.round(place * barSamples));
  const closed = starts('hat');
  const kicks = starts('kick');
  // `only`: one row alone, a stem (the tests check each hit's time on it).
  for (const row of ROWS.filter((x) => !only || x === only)) {
    const v = voices[row];
    const gl = LEVEL[row] * Math.sqrt((1 - PAN[row]) / 2) * Math.SQRT2;
    const gr = LEVEL[row] * Math.sqrt((1 + PAN[row]) / 2) * Math.SQRT2;
    for (const at of starts(row)) {
      // Chokes, as on a drum machine, with a 5 ms fade: a closed hat cuts an
      // open one, and a kick cuts the kick before it. Without the kick's, a
      // kick a 16th after another started inside its still-full tail and
      // rose by under 6 dB - mush, not a second hit (tests/
      // test_drums_riddimize.py). The loop wraps, so the next hit after the
      // last is the first, a loop later.
      let end = v.length;
      const chokers = row === 'open' ? closed : row === 'kick' ? kicks : null;
      if (chokers && chokers.length) {
        const next = chokers.find((c) => c > at) ?? (chokers[0] ?? 0) + length;
        end = Math.min(end, next - at);
      }
      const fade = Math.round(0.005 * sr);
      for (let i = 0; i < end; i += 1) {
        // The loop wraps: a tail past the end plays at its start, so the
        // loop repeats without a cut.
        const j = (at + i) % length;
        const g = chokers && end < v.length && i > end - fade ? (end - i) / fade : 1;
        left[j] = (left[j] ?? 0) + (v[i] ?? 0) * gl * g;
        right[j] = (right[j] ?? 0) + (v[i] ?? 0) * gr * g;
      }
    }
  }
  let peak = 0;
  for (let i = 0; i < length; i += 1) peak = Math.max(peak, Math.abs(left[i] ?? 0), Math.abs(right[i] ?? 0));
  const gain = peak > 0 ? Math.pow(10, -1 / 20) / peak : 1;
  for (let i = 0; i < length; i += 1) {
    left[i] = (left[i] ?? 0) * gain;
    right[i] = (right[i] ?? 0) * gain;
  }
  return [left, right];
}
