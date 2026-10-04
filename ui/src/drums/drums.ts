/*
 * DRUMS: riddim drum loops built from the producer's reference tracks
 * (docs/design/phase4-06-drums-riddimize.md).
 *
 * Every sound is synthesized here - no sample from anyone - and every
 * pattern starts from a template measured off the tracks the producer
 * uploaded (tools/measure_drums.py -> references/drums.json ->
 * tools/drum_templates.py -> templates.json): where kick, snare and hats land
 * in a bar, and how long and how bright each one is. Pure, no DOM, so Node
 * tests run it (ui/tests/drums.test.mjs).
 */

import templateFile from './templates.json' with { type: 'json' };

export type Row = 'kick' | 'snare' | 'hat' | 'open';
export const ROWS: readonly Row[] = ['kick', 'snare', 'hat', 'open'];
export const ROW_NAMES: Record<Row, string> = { kick: 'KICK', snare: 'SNARE', hat: 'HAT', open: 'OPEN HAT' };
export const STEPS = 16; // per bar, straight 16ths
export const BARS = 4;

interface MeasuredRow {
  grid: string;
  steps: number[];
}
interface Template {
  name: string;
  bpm: number;
  rows: Record<'kick' | 'snare' | 'hat', MeasuredRow>;
  sound: {
    kick: { decay_ms: number | null; pitch_start_hz?: number | null };
    snare: { decay_ms: number | null; centroid_hz?: number | null };
    hat: { decay_ms: number | null; centroid_hz?: number | null };
  };
}
export const TEMPLATES = (templateFile as unknown as { templates: Template[] }).templates;

export interface DrumSound {
  kick: { pitchStart: number; pitchEnd: number; sweepMs: number; decayMs: number; click: number; drive: number };
  snare: { tone: number; noiseHz: number; decayMs: number; body: number; drive: number };
  hat: { hz: number; decayMs: number };
  open: { hz: number; decayMs: number };
}

export interface DrumLoop {
  name: string;
  /** BARS bars of STEPS steps per row. */
  hits: Record<Row, boolean[][]>;
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

/** A measured row on the 16-step grid (templates are straight 16ths). */
function row16(row: MeasuredRow): number[] {
  if (row.grid === 'straight' && row.steps.length === STEPS) return row.steps;
  // A triplet row (12 per bar) placed on the nearest 16th.
  const out = new Array<number>(STEPS).fill(0);
  row.steps.forEach((p, i) => {
    const s = Math.round((i * STEPS) / row.steps.length) % STEPS;
    out[s] = Math.max(out[s] ?? 0, p);
  });
  return out;
}

/*
 * A loop from a template. A step the template hits in 60% of its bars or
 * more is in every bar; 30-60% steps come and go with their probability; the
 * fourth bar may roll the snare or the hats into the next loop. Beat 1's kick
 * and beat 3's snare are always there (every template has them). The open
 * hat takes an offbeat 8th the closed hat leaves free, in the bars it is
 * drawn for. The sound is the template's measured numbers, each moved up to
 * 12% either way.
 */
export function generateLoop(seed: number, templateIndex?: number, options: { fill?: boolean } = {}): DrumLoop {
  const r = random(seed);
  const t = TEMPLATES[templateIndex ?? Math.floor(r() * TEMPLATES.length)] ?? TEMPLATES[0];
  if (!t) throw new Error('no drum templates');
  const hits: Record<Row, boolean[][]> = { kick: empty(), snare: empty(), hat: empty(), open: empty() };
  const kick = row16(t.rows.kick);
  const snare = row16(t.rows.snare);
  const hat = row16(t.rows.hat);
  const pick = (p: number): boolean => p >= 0.6 || (p >= 0.3 && r() < p);
  for (let b = 0; b < BARS; b += 1) {
    for (let s = 0; s < STEPS; s += 1) {
      hits.kick[b]![s] = pick(kick[s] ?? 0);
      hits.snare[b]![s] = pick(snare[s] ?? 0);
      hits.hat[b]![s] = pick(hat[s] ?? 0);
    }
    hits.kick[b]![0] = true;
    hits.snare[b]![8] = true;
    // The open hat: an offbeat 8th (steps 2, 6, 10, 14) the closed hat leaves.
    const free = [2, 6, 10, 14].filter((s) => !hits.hat[b]![s] && !hits.snare[b]![s]);
    if (free.length && r() < 0.6) hits.open[b]![free[Math.floor(r() * free.length)] ?? 2] = true;
  }
  // The fourth bar's last beat: a snare or hat roll, half the time.
  const last = hits.snare[BARS - 1]!;
  const roll = options.fill === false ? 1 : r();
  if (roll < 0.25) for (const s of [12, 13, 14, 15]) last[s] = true;
  else if (roll < 0.5) for (const s of [12, 13, 14, 15]) hits.hat[BARS - 1]![s] = true;

  const vary = (v: number): number => v * (0.88 + 0.24 * r());
  const kickPitch = clamp(t.sound.kick.pitch_start_hz ?? 160, 90, 220);
  const snareDecay = clamp(t.sound.snare.decay_ms ?? 140, 90, 220);
  const snareHz = clamp(t.sound.snare.centroid_hz ?? 3000, 1500, 5000);
  // The measured centroid (3-8 kHz) is the whole mix's at the hat's moment,
  // kick and bass included, so it reads low; a riddim hat is bright. Mapped
  // 3-8 kHz -> 7-11 kHz, keeping the tracks' order of brightness.
  const hatHz = 7000 + 4000 * clamp(((t.sound.hat.centroid_hz ?? 5500) - 3000) / 5000, 0, 1);
  const hatDecay = clamp(t.sound.hat.decay_ms ?? 60, 30, 160);
  const sound: DrumSound = {
    kick: {
      pitchStart: vary(kickPitch),
      pitchEnd: vary(48),
      sweepMs: vary(35),
      // The measured fall (on the percussive part, HPSS) is the punch's; a
      // kick's tail is tonal and HPSS strips it, so the body is set here.
      decayMs: vary(clamp((t.sound.kick.decay_ms ?? 60) * 4, 140, 320)),
      click: vary(0.35),
      drive: vary(0.5),
    },
    snare: { tone: vary(200), noiseHz: vary(snareHz), decayMs: vary(snareDecay), body: vary(0.45), drive: vary(0.4) },
    hat: { hz: vary(hatHz), decayMs: vary(Math.min(hatDecay, 70)) },
    open: { hz: vary(hatHz * 0.9), decayMs: vary(clamp(hatDecay * 2.2, 160, 320)) },
  };
  return { name: t.name, hits, sound };
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

function kickVoice(s: DrumSound['kick'], sr: number, r: () => number): Float32Array {
  const n = Math.round(sr * (s.decayMs / 1000) * 3);
  const out = new Float32Array(n);
  const tauA = fall(s.decayMs);
  const tauP = s.sweepMs / 1000;
  let phase = 0;
  for (let i = 0; i < n; i += 1) {
    const t = i / sr;
    const f = s.pitchEnd + (s.pitchStart - s.pitchEnd) * Math.exp(-t / tauP);
    phase += (2 * Math.PI * f) / sr;
    const click = t < 0.004 ? s.click * (r() * 2 - 1) * (1 - t / 0.004) : 0;
    out[i] = drive(Math.sin(phase) * Math.exp(-t / tauA) + click, s.drive);
  }
  return out;
}

function snareVoice(s: DrumSound['snare'], sr: number, r: () => number): Float32Array {
  const n = Math.round(sr * (s.decayMs / 1000) * 3);
  const noise = new Float32Array(n);
  for (let i = 0; i < n; i += 1) noise[i] = r() * 2 - 1;
  biquad(noise, 'bandpass', s.noiseHz, 0.7, sr);
  biquad(noise, 'highpass', 400, 0.7, sr);
  const out = new Float32Array(n);
  const tau = fall(s.decayMs);
  const tauBody = fall(s.decayMs * 0.6);
  let phase = 0;
  for (let i = 0; i < n; i += 1) {
    const t = i / sr;
    phase += (2 * Math.PI * s.tone * (1 + 0.15 * Math.exp(-t / 0.01))) / sr;
    const body = s.body * Math.sin(phase) * Math.exp(-t / tauBody);
    out[i] = drive(body + 2.2 * (noise[i] ?? 0) * Math.exp(-t / tau), s.drive);
  }
  // Driving noise makes rumble a snare does not have: under a bare loop it
  // raised the sub by 18 dB and read as a kick (tools/measure_drums.py).
  biquad(out, 'highpass', 120, 0.7, sr);
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

/** The loop as two channels, exactly BARS bars long at `bpm`, peaking at -1 dBFS (or one row of it). */
export function renderLoop(loop: DrumLoop, bpm: number, sr = 44100, seed = 1, only?: Row): [Float32Array, Float32Array] {
  const r = random(seed);
  const barSamples = (4 * 60 / bpm) * sr;
  const length = Math.round(barSamples * BARS);
  const left = new Float32Array(length);
  const right = new Float32Array(length);
  const voices: Record<Row, Float32Array> = {
    kick: kickVoice(loop.sound.kick, sr, r),
    snare: snareVoice(loop.sound.snare, sr, r),
    hat: hatVoice(loop.sound.hat, sr, r),
    open: hatVoice(loop.sound.open, sr, r),
  };
  const starts = (row: Row): number[] => {
    const out: number[] = [];
    loop.hits[row].forEach((bar, b) => bar.forEach((on, s) => on && out.push(Math.round((b + s / STEPS) * barSamples))));
    return out;
  };
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
