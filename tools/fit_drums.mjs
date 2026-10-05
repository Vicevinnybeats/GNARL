// Fit DRUMS' kick and snare to the producer's tracks
// (docs/design/phase4-06-drums-riddimize.md, "Sounding like the tracks").
//
//   node tools/fit_drums.mjs prints.json ui/src/drums/kits.json
//   node tools/fit_drums.mjs --stamp ui/src/drums/kits.json   (after a deliberate synth change)
//
// prints.json comes from `python3 tools/drum_prints.py tracks... --json` and
// holds, per track, its kick's and snare's fingerprint (drum_prints.py: power
// in 20 log bands x 8 time frames over the first 360 ms of a hit, averaged
// over the hits of a drop, the sound before each hit taken off). It is
// measurement of copyrighted tracks: it stays out of the repository. What
// this writes is GNARL's own synth settings (drums.ts kickVoice and
// snareVoice) whose fingerprint comes closest, and how close, in dB.
//
// The snare is fitted over the kick just fitted (its kickLayer: how much of
// that kick plays under it). The search: 400 random settings, then a (1+1) evolution strategy from the
// best, three times over; the closest of all. Seeded: the same prints give
// the same kits.

import { readFileSync, writeFileSync } from 'node:fs';
import { kickVoice, layeredSnare, random } from '../ui/src/drums/drums.ts';

const SR = 44100;
// As drum_prints.py: these must stay the same in both.
const EDGES = Array.from({ length: 21 }, (_, i) => 30 * Math.pow(16000 / 30, i / 20));
const TIMES = [0, 0.01, 0.025, 0.05, 0.08, 0.12, 0.18, 0.26, 0.36];
const NFFT = 8192;
const PRE = Math.trunc(0.045 * SR) - Math.trunc(0.005 * SR);

/** In-place radix-2 FFT of (re, im). */
function fft(re, im) {
  const n = re.length;
  for (let i = 1, j = 0; i < n; i += 1) {
    let bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) {
      [re[i], re[j]] = [re[j], re[i]];
      [im[i], im[j]] = [im[j], im[i]];
    }
  }
  for (let len = 2; len <= n; len <<= 1) {
    const a = (-2 * Math.PI) / len;
    const wr = Math.cos(a), wi = Math.sin(a);
    for (let i = 0; i < n; i += len) {
      let cr = 1, ci = 0;
      for (let k = 0; k < len / 2; k += 1) {
        const ur = re[i + k], ui = im[i + k];
        const vr = re[i + k + len / 2] * cr - im[i + k + len / 2] * ci;
        const vi = re[i + k + len / 2] * ci + im[i + k + len / 2] * cr;
        re[i + k] = ur + vr; im[i + k] = ui + vi;
        re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi;
        const t = cr * wr - ci * wi;
        ci = cr * wi + ci * wr;
        cr = t;
      }
    }
  }
}

function bands(seg) {
  const m = seg.length;
  const re = new Float64Array(NFFT), im = new Float64Array(NFFT);
  let w2 = 0;
  for (let i = 0; i < m; i += 1) {
    const w = m > 1 ? 0.5 - 0.5 * Math.cos((2 * Math.PI * i) / (m - 1)) : 1;
    re[i] = seg[i] * w;
    w2 += w * w;
  }
  fft(re, im);
  const out = new Array(20).fill(0);
  for (let k = 0; k <= NFFT / 2; k += 1) {
    const f = (k * SR) / NFFT;
    for (let b = 0; b < 20; b += 1) {
      if (f >= EDGES[b] && f < EDGES[b + 1]) {
        out[b] += (re[k] * re[k] + im[k] * im[k]) / w2;
        break;
      }
    }
  }
  return out;
}

/** A lone hit's fingerprint: 20 bands x 8 frames, as drum_prints.py (no sound before it to take off). */
export function fingerprint(x) {
  const F = Array.from({ length: 20 }, () => new Array(8).fill(0));
  for (let k = 0; k < 8; k += 1) {
    const a = Math.trunc(TIMES[k] * SR), b = Math.trunc(TIMES[k + 1] * SR);
    const seg = new Float64Array(b - a);
    for (let i = 0; i < seg.length; i += 1) seg[i] = x[a + i] ?? 0;
    const p = bands(seg);
    for (let band = 0; band < 20; band += 1) F[band][k] = Math.max(p[band] * (PRE / seg.length), 0);
  }
  return F;
}

/*
 * How much each cell counts. Under 105 Hz (bands 0-3) after 120 ms (frames
 * 5-7) a third: riddim's bass restarts with the kick, and the fingerprint's
 * take-off (the 40 ms before the hit) cannot remove what starts with it.
 * Everything else counts whole.
 */
const WEIGHT = Array.from({ length: 20 }, (_, b) => Array.from({ length: 8 }, (_, k) => (b <= 3 && k >= 5 ? 1 / 3 : 1)));

/**
 * The distance between two fingerprints, in dB: each as a share of its own
 * total (level does not count), floored 30 dB under its loudest cell (below
 * that the take-off leaves mostly noise), the weighted mean of the cells'
 * absolute differences.
 */
export function distance(A, B) {
  const norm = (F) => {
    const total = F.flat().reduce((s, v) => s + v, 0) || 1;
    const db = F.map((row) => row.map((v) => 10 * Math.log10(v / total + 1e-20)));
    const top = Math.max(...db.flat());
    return db.map((row) => row.map((v) => Math.max(v, top - 30)));
  };
  const a = norm(A), b = norm(B);
  let sum = 0, weights = 0;
  for (let i = 0; i < 20; i += 1) {
    for (let k = 0; k < 8; k += 1) {
      sum += WEIGHT[i][k] * Math.abs(a[i][k] - b[i][k]);
      weights += WEIGHT[i][k];
    }
  }
  return sum / weights;
}

const RANGES = {
  kick: {
    pitchStart: [60, 400], pitchEnd: [30, 90], sweepMs: [4, 120], decayMs: [60, 600],
    click: [0, 1.5], drive: [0, 1.5], holdMs: [0, 200], clickMs: [1, 15],
  },
  snare: {
    tone: [120, 400], noiseHz: [800, 9000], decayMs: [40, 400], body: [0, 1.5], drive: [0, 1.5],
    noiseQ: [0.3, 3], bodyMs: [20, 300], crack: [0, 3], hpHz: [60, 400], kickLayer: [0, 1.5],
  },
};
// The snare is fitted with the track's fitted kick under it (kickLayer):
// the beat-3 hit in several of the tracks is a kick and a snare together.
let fittedKick = null;
const voice = {
  kick: kickVoice,
  snare: (s, sr, r) => layeredSnare(s, fittedKick, sr, r),
};

const fromUnit = (row, u) => Object.fromEntries(Object.entries(RANGES[row]).map(([k, [lo, hi]], i) =>
  [k, lo + (hi - lo) * Math.min(1, Math.max(0, u[i]))]));

/** The settings of `row` whose fingerprint is closest to `target`. */
export function fit(row, target, seed = 1) {
  const r = random(seed);
  const dims = Object.keys(RANGES[row]).length;
  const score = (u) => distance(fingerprint(voice[row](fromUnit(row, u), SR, random(7))), target);
  let best = null;
  for (let round = 0; round < 3; round += 1) {
    let u = null, d = Infinity;
    for (let i = 0; i < 400; i += 1) {
      const c = Array.from({ length: dims }, () => r());
      const e = score(c);
      if (e < d) [u, d] = [c, e];
    }
    let sigma = 0.15;
    for (let i = 0; i < 700; i += 1) {
      // A normal step per dimension (Box-Muller), kept inside the unit cube.
      const c = u.map((v) => Math.min(1, Math.max(0, v + sigma * Math.sqrt(-2 * Math.log(r() + 1e-12)) * Math.cos(2 * Math.PI * r()))));
      const e = score(c);
      if (e < d) {
        [u, d] = [c, e];
        sigma = Math.min(0.3, sigma * 1.5);
      } else {
        sigma = Math.max(0.005, sigma * 0.95);
      }
    }
    if (!best || d < best.d) best = { u, d };
  }
  const settings = Object.fromEntries(Object.entries(fromUnit(row, best.u)).map(([k, v]) => [k, Math.round(v * 1000) / 1000]));
  return { settings, db: Math.round(best.d * 100) / 100 };
}

/** Each kit's fitted kick and snare, fingerprinted: what tests/test_drum_prints.py holds them to. */
function stamp(kits) {
  for (const kit of kits) {
    if (!kit.kick || !kit.snare) continue;
    const round = (F) => F.map((row) => row.map((v) => Number(v.toPrecision(6))));
    kit.prints = {
      kick: round(fingerprint(kickVoice(kit.kick, SR, random(7)))),
      snare: round(fingerprint(layeredSnare(kit.snare, kit.kick, SR, random(7)))),
    };
  }
  return kits;
}

const COMMENT = 'Generated by tools/fit_drums.mjs from the producer\'s tracks\' measured drum fingerprints (not committed). ' +
  'GNARL synth settings, how close each came (dB), and the fitted sounds\' own fingerprints (prints); do not edit.';

if (process.argv[1]?.endsWith('fit_drums.mjs') && process.argv[2] === '--stamp') {
  // After a deliberate synth change: the kits' fingerprints taken again.
  const path = process.argv[3];
  const file = JSON.parse(readFileSync(path, 'utf8'));
  writeFileSync(path, JSON.stringify({ _comment: COMMENT, kits: stamp(file.kits) }, null, 1));
} else if (process.argv[1]?.endsWith('fit_drums.mjs')) {
  const [printsPath, outPath] = process.argv.slice(2);
  const prints = JSON.parse(readFileSync(printsPath, 'utf8'));
  // GNARL's sound before this fit, for the comparison (drums.ts defaults).
  const before = {
    kick: { pitchStart: 160, pitchEnd: 48, sweepMs: 35, decayMs: 240, click: 0.35, drive: 0.5 },
    snare: { tone: 200, noiseHz: 3000, decayMs: 140, body: 0.45, drive: 0.4 },
  };
  const kits = [];
  for (const t of prints.tracks) {
    const kit = { name: t.name, bpm: t.bpm };
    for (const row of ['kick', 'snare']) {
      const target = t[row];
      if (!target) continue;
      if (row === 'snare') fittedKick = kit.kick ?? before.kick;
      const was = distance(fingerprint((row === 'kick' ? kickVoice : (s, sr, r) => layeredSnare(s, null, sr, r))(before[row], SR, random(7))), target);
      const { settings, db } = fit(row, target, 1);
      kit[row] = settings;
      kit[`${row}_db`] = db;
      kit[`${row}_db_before`] = Math.round(was * 100) / 100;
      console.log(`${t.name.padEnd(34)} ${row.padEnd(5)} ${was.toFixed(2)} dB -> ${db.toFixed(2)} dB`);
    }
    kits.push(kit);
  }
  writeFileSync(outPath, JSON.stringify({ _comment: COMMENT, kits: stamp(kits) }, null, 1));
}
