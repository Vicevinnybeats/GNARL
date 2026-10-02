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
import searchSource from './match-search.js?raw';
import workerSource from './match-worker.js?raw';
import { TABLE_NAMES, tableJson } from '../wavetables';

type Features = Float64Array[];
interface Scored {
  d: number;
  peak: number;
}
interface Core {
  SR: number;
  CHOICES: { table: string[]; filter: string[] };
  setTables(byName: Record<string, string>): void;
  build(genes: Genes, baseText: string, name: string): Record<string, unknown> & { settings: Record<string, unknown> };
  features(x: Float32Array, length: number): Features;
}
type Genes = Record<string, number>;
interface Ranked extends Scored {
  genes: Genes;
}
interface SearchApi {
  search(options: {
    randomCount: number;
    generations: number;
    children: number;
    seed: number;
    score(genes: Genes[]): Promise<Scored[]>;
    onProgress?(done: number, total: number, best: number): void;
    cancelled?(): boolean;
  }): Promise<Ranked[]>;
}

const core = new Function(`${matchCoreSource}; return createMatcherCore;`)()() as Core;
const searchApi = new Function(`${searchSource}; return createMatcherSearch;`)()(core) as SearchApi;
const TABLES: Record<string, string> = Object.fromEntries(TABLE_NAMES.map((n) => [n, tableJson(n) ?? '']));
core.setTables(TABLES);

/** The candidates' base patch: the styles' shared voice, as tools/match.py. */
export const MATCH_BASE = 'Yoi Talk';
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
  score(genes: Genes[]): Promise<Scored[]>;
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
  const run = (w: Worker, genes: Genes[]): Promise<Scored[]> =>
    new Promise((resolve, reject) => {
      const id = nextId++;
      w.onmessage = (e: MessageEvent<{ type: string; id: number; results?: Scored[]; message?: string }>): void => {
        if (e.data.id !== id) return;
        if (e.data.type === 'scored') resolve(e.data.results ?? []);
        else reject(new Error(e.data.message));
      };
      w.postMessage({ type: 'score', id, genes });
    });
  return {
    async score(genes) {
      await Promise.all(ready);
      const per = Math.ceil(genes.length / workers.length);
      const parts = await Promise.all(
        workers.map((w, i) => {
          const slice = genes.slice(i * per, (i + 1) * per);
          return slice.length ? run(w, slice) : Promise.resolve([]);
        }),
      );
      return parts.flat();
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
  const wasm = window.__GNARL_WASM__;
  if (!wasm) throw new Error('this build carries no engine to match with');
  const length = Math.min(job.audio.length, Math.round(1.25 * core.SR));
  const target = core.features(job.audio, length);
  const count = Math.max(1, Math.min(4, (navigator.hardwareConcurrency || 2) - 1));
  const pool = workerPool(count, {
    type: 'init', wasm: base64Bytes(wasm), tables: TABLES, baseText: job.baseText, target, length, midi: job.midi,
  });
  try {
    const ranked = await searchApi.search({
      ...MATCH_BUDGET, seed: job.seed, score: (g) => pool.score(g), onProgress: job.onProgress, cancelled: job.cancelled,
    });
    const chosen: Ranked[] = [];
    for (const r of ranked) {
      if (!Number.isFinite(r.d)) break;
      const key = (c: Ranked): string => `${c.genes.table}/${c.genes.filter}`;
      if (!chosen.some((c) => key(c) === key(r))) chosen.push(r);
      if (chosen.length === 4) break;
    }
    return chosen.map((r, i) => {
      const patch = core.build(r.genes, job.baseText, `Match ${i + 1}`);
      // Level: the candidate's peak at the target's note put at -4.5 dBFS
      // (the volume control reads sqrt(value) - 80 dB). Matched patches came
      // out from -14 to -2 dBFS at one volume (phase4-02-matcher.md).
      const peakDb = 20 * Math.log10(Math.max(1e-6, r.peak));
      const volume = Number(patch.settings.volume ?? 4300);
      patch.settings.volume = Math.pow(Math.max(0, Math.sqrt(volume) + (-4.5 - peakDb)), 2);
      const table = core.CHOICES.table[r.genes.table ?? 0] ?? '';
      const filter = core.CHOICES.filter[r.genes.filter ?? 0] ?? '';
      const about = `Matched to your sound (${r.d.toFixed(2)} dB on the matcher's measure): the ${table} table` +
        (filter === 'off' ? '' : `, ${filter} filter`) + '.';
      patch.comments = about;
      return { name: `Match ${i + 1}`, about, seed: job.seed * 10 + i, patch: JSON.stringify(patch), distance: r.d };
    });
  } finally {
    pool.close();
  }
}
