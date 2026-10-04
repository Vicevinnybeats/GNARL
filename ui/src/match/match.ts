/*
 * MATCH A SOUND (docs/design/phase4-04-match-in-app.md): the sound matcher
 * of tools/match.py inside the app. The producer drops in a short audio file
 * of one wob; it is cut to the wob, its note found, and GNARL's engine - a
 * copy in each Web Worker - renders candidate patches and keeps the ones
 * whose log-mel picture is closest. Offline: the file never leaves the
 * device. The best four go to the AI sheet, where PICK carries on by ear.
 */

import coreSource from '../web/engine-core.js?raw';
import matchCoreSource from './match-core.js?raw';
import workerSource from './match-worker.js?raw';
import { TABLE_NAMES, tableJson } from '../wavetables';
import { LEVEL_NOTES, LEVEL_SECONDS, levelVolume } from '../generate';
import { searchRecipe } from './recipe-search';
import { engineWasm } from '../data';

type Features = Float64Array[];
interface Scored {
  d: number;
  peak: number;
}
interface Core {
  SR: number;
  setTables(byName: Record<string, string>): void;
  features(x: Float32Array, length: number): Features;
}
const core = new Function(`${matchCoreSource}; return createMatcherCore;`)()() as Core;
// The wavetables a candidate may use, as .vital JSON. Built on first use,
// not at load: eighteen tables of sixteen keyframes is a second of work, and
// the panel opening is no time to do it (the producer: "very slow to open").
// Only the workers build patches from them; the page's own core only measures.
let tableTexts: Record<string, string> | null = null;
const TABLES = (): Record<string, string> =>
  (tableTexts ??= Object.fromEntries(TABLE_NAMES.map((n) => [n, tableJson(n) ?? ''])));

/**
 * The recipe every candidate is made in: Vinny Bass 2, as the AI's
 * (recipe-search.ts). Until 2026-10-02 it was the styles' voice, Yoi Talk,
 * with match.py's one-wob genes - and every match was a static screech.
 */
export const MATCH_BASE = 'Vinny Bass 2';
/** 160 random + 12 generations of 24: 3.86 dB on a known patch (phase4-04). */
export const MATCH_BUDGET = { randomCount: 160, generations: 12, children: 24 };

export interface Matched {
  name: string;
  about: string;
  seed: number;
  patch: string;
  /** On the matcher's measure, dB per cell: lower is closer. */
  distance: number;
}

/** Decode any audio file the browser can, as 44.1 kHz mono. */
export async function decodeAudio(data: ArrayBuffer): Promise<Float32Array> {
  const ctx = new OfflineAudioContext(1, 1, core.SR);
  const buffer = await ctx.decodeAudioData(data);
  const n = buffer.length;
  const out = new Float32Array(n);
  for (let c = 0; c < buffer.numberOfChannels; c += 1) {
    const ch = buffer.getChannelData(c);
    for (let i = 0; i < n; i += 1) out[i] = (out[i] ?? 0) + (ch[i] ?? 0) / buffer.numberOfChannels;
  }
  return out;
}

/** From the first sound (-26 dB under the peak), at most 1.25 s: one wob. */
export function trimToWob(x: Float32Array): Float32Array {
  let peak = 0;
  for (const v of x) peak = Math.max(peak, Math.abs(v));
  const gate = peak * 0.05;
  let start = 0;
  while (start < x.length && Math.abs(x[start] ?? 0) < gate) start += 1;
  return x.slice(start, Math.min(x.length, start + Math.round(1.25 * core.SR)));
}

/**
 * The wob's note: YIN on the bass under about 300 Hz, 30-200 Hz, over the
 * middle of the wob. A riddim wob's fundamental is its sub, so this finds
 * the note it was played at.
 */
export function detectMidi(x: Float32Array): number | null {
  // One-pole low pass at ~300 Hz, then down to 5.5 kHz.
  const a = Math.exp((-2 * Math.PI * 300) / core.SR);
  const step = 8;
  const y: number[] = [];
  let z = 0;
  for (let i = 0; i < x.length; i += 1) {
    z = (1 - a) * (x[i] ?? 0) + a * z;
    if (i % step === 0) y.push(z);
  }
  const sr = core.SR / step;
  const minLag = Math.floor(sr / 200);
  const maxLag = Math.ceil(sr / 30);
  const win = Math.min(2048, y.length - maxLag - 1);
  if (win < 256) return null;
  const start = Math.max(0, Math.floor((y.length - win - maxLag) / 2));
  const d = new Float64Array(maxLag + 1);
  for (let lag = 1; lag <= maxLag; lag += 1) {
    let sum = 0;
    for (let i = 0; i < win; i += 1) {
      const v = (y[start + i] ?? 0) - (y[start + i + lag] ?? 0);
      sum += v * v;
    }
    d[lag] = sum;
  }
  // Cumulative mean normalised difference; first dip under 0.15, else the minimum.
  let running = 0;
  let best = -1;
  let bestValue = Infinity;
  for (let lag = 1; lag <= maxLag; lag += 1) {
    running += d[lag] ?? 0;
    const cmnd = ((d[lag] ?? 0) * lag) / (running || 1);
    if (lag < minLag) continue;
    if (cmnd < 0.15) {
      best = lag;
      break;
    }
    if (cmnd < bestValue) {
      bestValue = cmnd;
      best = lag;
    }
  }
  if (best < 0) return null;
  const hz = sr / best;
  return Math.round(69 + 12 * Math.log2(hz / 440));
}

const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
/** FL Studio's naming (MIDI 60 = C5), the producer's. */
export function flNoteName(midi: number): string {
  return `${NOTE_NAMES[midi % 12] ?? ''}${Math.floor(midi / 12)}`;
}

function base64Bytes(text: string): Uint8Array {
  const binary = atob(text);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i += 1) bytes[i] = binary.charCodeAt(i);
  return bytes;
}

/** Score batches on several workers, each with its own engine. */
function workerPool(count: number, init: Record<string, unknown>): {
  scoreTexts(patches: string[]): Promise<Scored[]>;
  peaks(patches: string[]): Promise<number[]>;
  render(patch: string, midi: number, seconds: number): Promise<Float32Array | null>;
  close(): void;
} {
  const source = `${coreSource}\n${matchCoreSource}\n${workerSource}`;
  const url = URL.createObjectURL(new Blob([source], { type: 'text/javascript' }));
  const workers: Worker[] = [];
  const ready: Promise<void>[] = [];
  try {
    for (let i = 0; i < count; i += 1) {
      const w = new Worker(url);
      workers.push(w);
      ready.push(
        new Promise<void>((resolve, reject) => {
          w.onmessage = (e: MessageEvent<{ type: string; message?: string }>): void => {
            if (e.data.type === 'ready') resolve();
            else if (e.data.type === 'error') reject(new Error(e.data.message));
          };
          w.onerror = (e): void => reject(new Error(e.message));
        }),
      );
      w.postMessage(init);
    }
  } finally {
    URL.revokeObjectURL(url);
  }
  let nextId = 0;
  const runPeaks = (w: Worker, patches: string[]): Promise<number[]> =>
    new Promise((resolve, reject) => {
      const id = nextId++;
      w.onmessage = (e: MessageEvent<{ type: string; id: number; results?: number[]; message?: string }>): void => {
        if (e.data.id !== id) return;
        if (e.data.type === 'peaks') resolve(e.data.results ?? []);
        else reject(new Error(e.data.message));
      };
      w.postMessage({ type: 'peaks', id, patches, midis: LEVEL_NOTES, seconds: LEVEL_SECONDS });
    });
  const runTexts = (w: Worker, patches: string[]): Promise<Scored[]> =>
    new Promise((resolve, reject) => {
      const id = nextId++;
      w.onmessage = (e: MessageEvent<{ type: string; id: number; results?: Scored[]; message?: string }>): void => {
        if (e.data.id !== id) return;
        if (e.data.type === 'scored') resolve(e.data.results ?? []);
        else reject(new Error(e.data.message));
      };
      w.postMessage({ type: 'scoreText', id, patches });
    });
  return {
    async scoreTexts(patches) {
      await Promise.all(ready);
      const per = Math.ceil(patches.length / workers.length);
      const parts = await Promise.all(
        workers.map((w, i) => {
          const slice = patches.slice(i * per, (i + 1) * per);
          return slice.length ? runTexts(w, slice) : Promise.resolve([]);
        }),
      );
      return parts.flat();
    },
    async peaks(patches) {
      await Promise.all(ready);
      const per = Math.ceil(patches.length / workers.length);
      const parts = await Promise.all(
        workers.map((w, i) => {
          const slice = patches.slice(i * per, (i + 1) * per);
          return slice.length ? runPeaks(w, slice) : Promise.resolve([]);
        }),
      );
      return parts.flat();
    },
    async render(patch, midi, seconds) {
      await Promise.all(ready);
      const w = workers[0];
      if (!w) return null;
      return new Promise((resolve, reject) => {
        const id = nextId++;
        w.onmessage = (e: MessageEvent<{ type: string; id: number; audio?: Float32Array | null; message?: string }>): void => {
          if (e.data.id !== id) return;
          if (e.data.type === 'rendered') resolve(e.data.audio ?? null);
          else reject(new Error(e.data.message));
        };
        w.postMessage({ type: 'render', id, patch, midi, seconds });
      });
    },
    close() {
      for (const w of workers) w.terminate();
    },
  };
}

export interface MatchJob {
  audio: Float32Array;
  midi: number;
  baseText: string;
  seed: number;
  onProgress(done: number, total: number, best: number): void;
  cancelled(): boolean;
}

/** The four closest patches, distinct in table or filter, ready to load. */
export async function matchSound(job: MatchJob): Promise<Matched[]> {
  const wasm = engineWasm();
  if (!wasm) throw new Error('this build carries no engine to match with');
  const length = Math.min(job.audio.length, Math.round(1.25 * core.SR));
  const target = core.features(job.audio, length);
  const count = Math.max(1, Math.min(4, (navigator.hardwareConcurrency || 2) - 1));
  const pool = workerPool(count, {
    type: 'init', wasm: base64Bytes(wasm), tables: TABLES(), baseText: job.baseText, target, length, midi: job.midi,
  });
  try {
    const ranked = await searchRecipe({
      ...MATCH_BUDGET, bases: { [MATCH_BASE]: job.baseText }, seed: job.seed,
      score: (patches) => pool.scoreTexts(patches), onProgress: job.onProgress, cancelled: job.cancelled,
    });
    // Four that differ: by rhythm and OSC 1's table.
    const table = (p: string): string =>
      (JSON.parse(p) as { settings: { wavetables?: { name?: string }[] } }).settings.wavetables?.[0]?.name ?? '';
    const chosen: typeof ranked = [];
    for (const r of ranked) {
      const key = (c: (typeof ranked)[number]): string => `${c.rhythm}/${table(c.patch)}`;
      if (!chosen.some((c) => key(c) === key(r))) chosen.push(r);
      if (chosen.length === 4) break;
    }
    // Levelled as the AI's sounds are (generate.ts levelVolume).
    const peaks = await pool.peaks(chosen.map((c) => c.patch));
    return chosen.map((r, i) => {
      const patch = JSON.parse(levelVolume(r.patch, peaks[i] ?? 0)) as Record<string, unknown>;
      const name = `Match ${i + 1}`;
      const about = `Matched to your sound (${r.d.toFixed(2)} dB on the matcher's measure): a ${r.rhythm || 'riddim'} wob ` +
        `on the ${table(r.patch)} table, in Vinny Bass 2's recipe.`;
      Object.assign(patch, { preset_name: name, comments: about, author: 'GNARL matcher' });
      return { name, about, seed: job.seed * 10 + i, patch: JSON.stringify(patch), distance: r.d };
    });
  } finally {
    pool.close();
  }
}

/*
 * The AI's levelling (generate.ts levelVolume): each sound rendered by the
 * engine in workers, its volume set from its measured peak. One pool, made
 * on first use and kept, so a round of four costs renders, not start-ups.
 * Without an engine (a build with no wasm) the sounds come back as they
 * went in, with the generator's own estimate of their level.
 */
let levelPool: ReturnType<typeof workerPool> | null = null;

function enginePool(wasm: string): ReturnType<typeof workerPool> {
  levelPool ??= workerPool(Math.max(1, Math.min(4, (navigator.hardwareConcurrency || 2) - 1)), {
    type: 'init', wasm: base64Bytes(wasm), tables: {}, baseText: '', target: [], length: 0, midi: 39,
  });
  return levelPool;
}

/**
 * A patch's note held for `seconds` at `midi`, mono at 44.1 kHz, rendered by
 * the page's engine (RIDDIMIZE's USE MY SOUND); null without an engine.
 */
export async function renderPatch(patch: string, midi: number, seconds: number): Promise<Float32Array | null> {
  const wasm = engineWasm();
  if (!wasm || typeof Worker === 'undefined') return null;
  return enginePool(wasm).render(patch, midi, seconds);
}

export async function levelPatches(patches: string[]): Promise<string[]> {
  const wasm = engineWasm();
  if (!wasm || typeof Worker === 'undefined') return patches;
  try {
    enginePool(wasm);
    if (!levelPool) return patches;
    const peaks = await levelPool.peaks(patches);
    return patches.map((p, i) => levelVolume(p, peaks[i] ?? 0));
  } catch {
    levelPool?.close();
    levelPool = null;
    return patches;
  }
}
