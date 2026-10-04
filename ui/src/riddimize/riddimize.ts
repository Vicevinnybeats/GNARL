/*
 * RIDDIMIZE: any sound in, a riddim one-shot out
 * (docs/design/phase4-06-drums-riddimize.md). The producer's ask: put a
 * sample in - a Serum growl, a resampled bass - and get a riddim one-shot to
 * drop in the playlist, as Avant's RiddimSmith does on a mixer track.
 *
 * The chain, in order: the sample looped to the length and pitched; chopped
 * into the rhythm by a gate; a filter that opens on every hit (a wah, or a
 * vowel moving from O to A); drive, fold and crush; a three-band squash in
 * the way of OTT; a clean sine sub under it, gated with it; levelled to
 * peak at -1 dBFS. Pure, no DOM: ui/tests/riddimize.test.mjs runs it in Node
 * and measures what comes out.
 */

export const RHYTHMS = ['1/8', '1/8T', '1/16', '1/4', 'STUTTER'] as const;
export type Rhythm = (typeof RHYTHMS)[number];
export const LENGTHS = ['1/2 BAR', '1 BAR', '2 BARS'] as const;
export type Length = (typeof LENGTHS)[number];
export const FILTERS = ['WAH', 'VOWEL', 'NONE'] as const;
export type FilterKind = (typeof FILTERS)[number];

export interface Settings {
  bpm: number;
  length: Length;
  rhythm: Rhythm;
  /** 0 smooth (a wub), 1 a hard chop. */
  chop: number;
  /** Share of each hit that sounds, 0.3-0.9. */
  gate: number;
  filter: FilterKind;
  /** How far the filter moves on each hit, 0-1. */
  sweep: number;
  pitch: number; // semitones
  drive: number;
  fold: number;
  crush: number; // 0 off, 1 = 4 bits
  ott: number;
  sub: number;
}

export const DEFAULTS: Settings = {
  bpm: 140, length: '1 BAR', rhythm: '1/8T', chop: 0.7, gate: 0.7, filter: 'WAH', sweep: 0.7,
  pitch: 0, drive: 0.5, fold: 0.15, crush: 0, ott: 0.6, sub: 0.5,
};

const BEATS: Record<Length, number> = { '1/2 BAR': 2, '1 BAR': 4, '2 BARS': 8 };

/** Hit starts in beats, over `beats`. */
export function hitTimes(rhythm: Rhythm, beats: number): number[] {
  const every = (step: number): number[] => Array.from({ length: Math.round(beats / step) }, (_, i) => i * step);
  switch (rhythm) {
    case '1/8': return every(0.5);
    case '1/8T': return every(1 / 3);
    case '1/16': return every(0.25);
    case '1/4': return every(1);
    case 'STUTTER': {
      // Riddim's stutter: two 1/16s, a 1/8, then a triplet pair, per beat.
      const out: number[] = [];
      for (let b = 0; b < beats; b += 1) out.push(b, b + 0.25, b + 0.5, b + 2 / 3, b + 5 / 6);
      return out;
    }
  }
}

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

/** Settings for GENERATE: inside riddim's ranges, weighted to 1/8T and 1/8. */
export function randomSettings(seed: number, bpm: number): Settings {
  const r = random(seed);
  const pick = <T>(xs: readonly T[], weights: number[]): T => {
    let x = r() * weights.reduce((a, b) => a + b, 0);
    for (let i = 0; i < xs.length; i += 1) {
      x -= weights[i] ?? 0;
      if (x <= 0) return xs[i] as T;
    }
    return xs[0] as T;
  };
  const between = (lo: number, hi: number): number => lo + (hi - lo) * r();
  return {
    bpm,
    length: pick(LENGTHS, [1, 3, 1]),
    rhythm: pick(RHYTHMS, [3, 4, 1, 1, 2]),
    chop: between(0.4, 1),
    gate: between(0.45, 0.85),
    filter: pick(FILTERS, [3, 2, 1]),
    sweep: between(0.4, 1),
    pitch: pick([0, 0, 0, -12, -5, 7], [1, 1, 1, 1, 1, 1]),
    drive: between(0.2, 0.8),
    fold: r() < 0.4 ? between(0.1, 0.5) : 0,
    crush: r() < 0.25 ? between(0.2, 0.6) : 0,
    ott: between(0.3, 0.9),
    sub: r() < 0.7 ? between(0.3, 0.7) : 0,
  };
}

/* --------------------------------------------------------------- helpers */

/** Strongest period 30-200 Hz by autocorrelation: the note, for the sub. */
export function pitchOf(x: Float32Array, sr: number): number | null {
  const n = Math.min(x.length, Math.round(sr * 0.5));
  if (n < sr * 0.05) return null;
  const lo = Math.floor(sr / 200);
  const hi = Math.ceil(sr / 30);
  let best = 0;
  let at = 0;
  let energy = 0;
  for (let i = 0; i < n; i += 1) energy += (x[i] ?? 0) ** 2;
  if (energy <= 0) return null;
  for (let lag = lo; lag <= hi; lag += 1) {
    let s = 0;
    for (let i = 0; i + lag < n; i += 2) s += (x[i] ?? 0) * (x[i + lag] ?? 0);
    if (s > best) {
      best = s;
      at = lag;
    }
  }
  return at > 0 && best > energy * 0.1 ? sr / at : null;
}

/** A topology-preserving state variable filter: modulated per sample. */
class Svf {
  private ic1 = 0;
  private ic2 = 0;
  run(v: number, hz: number, q: number, sr: number): { low: number; band: number } {
    const g = Math.tan((Math.PI * Math.min(hz, sr * 0.45)) / sr);
    const k = 1 / q;
    const a1 = 1 / (1 + g * (g + k));
    const v3 = v - this.ic2;
    const v1 = a1 * this.ic1 + g * a1 * v3;
    const v2 = this.ic2 + g * v1;
    this.ic1 = 2 * v1 - this.ic1;
    this.ic2 = 2 * v2 - this.ic2;
    return { low: v2, band: v1 };
  }
}

/** One-pole split into three bands (low < 200 Hz < mid < 2.5 kHz < high). */
function split3(x: Float32Array, sr: number): [Float32Array, Float32Array, Float32Array] {
  const lp = (input: Float32Array, hz: number): Float32Array => {
    // Two one-poles in series: 12 dB per octave, and phase-simple.
    const a = Math.exp((-2 * Math.PI * hz) / sr);
    const out = new Float32Array(input.length);
    let s1 = 0, s2 = 0;
    for (let i = 0; i < input.length; i += 1) {
      s1 = (1 - a) * (input[i] ?? 0) + a * s1;
      s2 = (1 - a) * s1 + a * s2;
      out[i] = s2;
    }
    return out;
  };
  const low = lp(x, 200);
  const lowMid = lp(x, 2500);
  const mid = new Float32Array(x.length);
  const high = new Float32Array(x.length);
  for (let i = 0; i < x.length; i += 1) {
    mid[i] = (lowMid[i] ?? 0) - (low[i] ?? 0);
    high[i] = (x[i] ?? 0) - (lowMid[i] ?? 0);
  }
  return [low, mid, high];
}

/** Upward and downward compression towards a target, per band (OTT-like). */
function squash(x: Float32Array, amount: number, sr: number): Float32Array {
  if (amount <= 0) return x;
  const bands = split3(x, sr);
  const out = new Float32Array(x.length);
  const attack = Math.exp(-1 / (0.002 * sr));
  const release = Math.exp(-1 / (0.06 * sr));
  // The high band is pushed hardest, as OTT's are by default.
  const weight = [0.7, 1, 1.2];
  bands.forEach((band, bi) => {
    let env = 0;
    const target = 0.25;
    const a = amount * (weight[bi] ?? 1);
    for (let i = 0; i < band.length; i += 1) {
      const v = Math.abs(band[i] ?? 0);
      env = v > env ? attack * env + (1 - attack) * v : release * env + (1 - release) * v;
      // Ratio towards the target: 3:1 down, up to +18 dB up.
      const gain = env > 1e-5 ? Math.min(8, Math.pow(target / env, a * 0.66)) : 1;
      out[i] = (out[i] ?? 0) + (band[i] ?? 0) * gain;
    }
  });
  // Dry/wet: amount is also the mix, as OTT's DEPTH.
  for (let i = 0; i < x.length; i += 1) out[i] = (1 - amount) * (x[i] ?? 0) + amount * (out[i] ?? 0);
  return out;
}

/* --------------------------------------------------------------- the chain */

export interface Riddimized {
  /** Two channels (the same), peaking at -1 dBFS. */
  channels: [Float32Array, Float32Array];
  /** The note found for the sub, in Hz, or null. */
  note: number | null;
}

export function riddimize(input: Float32Array, sr: number, s: Settings): Riddimized {
  const beat = 60 / s.bpm;
  const beats = BEATS[s.length];
  const length = Math.round(beats * beat * sr);

  // 1. Start at the sound (26 dB under the peak), pitch it, loop it to length.
  let peak = 0;
  for (const v of input) peak = Math.max(peak, Math.abs(v));
  let first = 0;
  while (first < input.length && Math.abs(input[first] ?? 0) < peak * 0.05) first += 1;
  const src = input.subarray(first);
  const rate = Math.pow(2, s.pitch / 12);
  const usable = Math.max(1, Math.floor((src.length - 1) / rate));
  const fadeLoop = Math.min(Math.round(0.01 * sr), Math.floor(usable / 4));
  const x = new Float32Array(length);
  for (let i = 0; i < length; i += 1) {
    const j = i % usable;
    const pos = j * rate;
    const k = Math.floor(pos);
    const f = pos - k;
    let v = (src[k] ?? 0) * (1 - f) + (src[k + 1] ?? 0) * f;
    // Crossfade the loop's seam: the end fades into the start.
    if (usable < length && j >= usable - fadeLoop) {
      const w = (j - (usable - fadeLoop)) / fadeLoop;
      const p2 = (j - (usable - fadeLoop)) * rate;
      const k2 = Math.floor(p2);
      v = v * (1 - w) + (src[k2] ?? 0) * w;
    }
    x[i] = v;
  }
  const note = pitchOf(x, sr);

  // 2. The gate, and how far into its hit each sample is (for the filter).
  const hits = hitTimes(s.rhythm, beats).map((b) => Math.round(b * beat * sr));
  const gate = new Float32Array(length);
  const phase = new Float32Array(length);
  hits.forEach((start, h) => {
    const end = h + 1 < hits.length ? (hits[h + 1] ?? length) : length;
    const span = end - start;
    const on = Math.max(1, Math.round(span * s.gate));
    // Edges: a hard chop has 2 ms ramps, a smooth one a quarter of the hit.
    const edge = Math.max(Math.round(0.002 * sr), Math.round(on * 0.25 * (1 - s.chop)));
    for (let i = 0; i < span && start + i < length; i += 1) {
      let g = 0;
      if (i < on) g = Math.min(1, i / edge, (on - i) / edge);
      g = Math.max(0, g);
      // A smooth chop is a raised cosine, not a ramp.
      g = s.chop < 1 ? (1 - s.chop) * (0.5 - 0.5 * Math.cos(Math.PI * g)) + s.chop * g : g;
      gate[start + i] = g;
      phase[start + i] = i / span;
    }
  });

  // 3. The filter: opens on each hit and closes through it.
  const y = new Float32Array(length);
  if (s.filter === 'NONE') y.set(x);
  else {
    const f1 = new Svf();
    const f2 = new Svf();
    for (let i = 0; i < length; i += 1) {
      const open = Math.exp(-(phase[i] ?? 0) * 3) * s.sweep; // 1 at the hit, falling
      const v = x[i] ?? 0;
      if (s.filter === 'WAH') {
        const hz = 150 * Math.pow(2, 1 + 5.5 * open);
        y[i] = f1.run(v, hz, 2.2, sr).low;
      } else {
        // O (450, 800 Hz) opening to A (800, 1200 Hz) on each hit.
        const fa = 450 + 350 * open;
        const fb = 800 + 400 * open;
        y[i] = 1.6 * (f1.run(v, fa, 6, sr).band + 0.8 * f2.run(v, fb, 6, sr).band) + 0.25 * v;
      }
    }
  }

  // 4. Drive, fold, crush.
  const k = 1 + 9 * s.drive;
  const bits = 16 - 12 * s.crush;
  const levels = Math.pow(2, bits - 1);
  for (let i = 0; i < length; i += 1) {
    let v = Math.tanh(k * (y[i] ?? 0)) / Math.tanh(k);
    if (s.fold > 0) v = (1 - s.fold) * v + s.fold * Math.sin(v * Math.PI * (1 + 2 * s.fold));
    if (s.crush > 0) v = Math.round(v * levels) / levels;
    y[i] = v;
  }

  // 5. The squash, then the gate (after it, so OTT cannot lift the gaps).
  const z = squash(y, s.ott, sr);
  for (let i = 0; i < length; i += 1) z[i] = (z[i] ?? 0) * (gate[i] ?? 0);

  // 6. A clean sub at the note, an octave down if it is above 80 Hz.
  if (s.sub > 0 && note) {
    const subHz = note > 80 ? note / 2 : note;
    let p = 0;
    let level = 0;
    for (let i = 0; i < length; i += 1) level = Math.max(level, Math.abs(z[i] ?? 0));
    for (let i = 0; i < length; i += 1) {
      p += (2 * Math.PI * subHz) / sr;
      z[i] = (z[i] ?? 0) + s.sub * level * 0.8 * Math.sin(p) * (gate[i] ?? 0);
    }
  }

  // 7. Level: peak at -1 dBFS, a 5 ms fade at the very end.
  let top = 0;
  for (const v of z) top = Math.max(top, Math.abs(v));
  const gain = top > 0 ? Math.pow(10, -1 / 20) / top : 1;
  const fade = Math.round(0.005 * sr);
  for (let i = 0; i < length; i += 1) {
    const f = i > length - fade ? (length - i) / fade : 1;
    z[i] = (z[i] ?? 0) * gain * f;
  }
  return { channels: [z, Float32Array.from(z)], note };
}
