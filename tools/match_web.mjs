// The in-app sound matcher (ui/src/match/), run in Node with the browser
// build of the engine: the same core, search and engine as the page uses, on
// one thread (docs/design/phase4-04-match-in-app.md). ui/tests/web.test.mjs
// runs the same matcher through the page.
//
//   node tools/match_web.mjs target.wav --midi 39 --out p.vital [--random 160 --generations 12 --seed 1]

import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { loadEngine } from './web_render.mjs';
import { searchRecipe } from '../ui/src/match/recipe-search.ts';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const load = (file, name) => new Function(readFileSync(join(root, 'ui/src/match', file), 'utf8') + `; return ${name};`)();
const core = load('match-core.js', 'createMatcherCore')();

export function readWav(path) {
  const b = readFileSync(path);
  let p = 12;
  let fmt = null;
  let data = null;
  while (p < b.length) {
    const id = b.toString('ascii', p, p + 4);
    const n = b.readUInt32LE(p + 4);
    if (id === 'fmt ') fmt = { ch: b.readUInt16LE(p + 10), rate: b.readUInt32LE(p + 12), bits: b.readUInt16LE(p + 22) };
    if (id === 'data') data = b.subarray(p + 8, p + 8 + n);
    p += 8 + n + (n & 1);
  }
  const bytes = fmt.bits / 8;
  const frames = Math.floor(data.length / bytes / fmt.ch);
  const out = new Float32Array(frames);
  for (let i = 0; i < frames; i += 1) {
    for (let c = 0; c < fmt.ch; c += 1) {
      const o = (i * fmt.ch + c) * bytes;
      out[i] += (fmt.bits === 32 ? data.readFloatLE(o) : data.readInt16LE(o) / 32768) / fmt.ch;
    }
  }
  if (fmt.rate !== 44100) throw new Error('match_web.mjs: the target must be 44.1 kHz');
  return out;
}

async function main(argv) {
  const args = { midi: 39, random: 160, generations: 12, children: 24, seed: 1, out: null, target: null };
  for (let i = 0; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === '--midi') args.midi = Number(argv[++i]);
    else if (a === '--out') args.out = argv[++i];
    else if (a === '--random') args.random = Number(argv[++i]);
    else if (a === '--generations') args.generations = Number(argv[++i]);
    else if (a === '--seed') args.seed = Number(argv[++i]);
    else args.target = a;
  }
  const engine = loadEngine();
  const baseText = readFileSync(join(root, 'presets', 'Vinny Bass 2.vital'), 'utf8');
  const x = readWav(args.target);
  const length = Math.min(x.length, Math.round(1.25 * core.SR));
  const target = core.features(x, length);
  const t0 = Date.now();
  // As the page's workers do (match-worker.js 'scoreText').
  const ranked = await searchRecipe({
    bases: { 'Vinny Bass 2': baseText }, randomCount: args.random, generations: args.generations,
    children: args.children, seed: args.seed,
    score: async (patches) => patches.map((text) => {
      const audio = core.renderMono(engine, text, args.midi, length / core.SR);
      if (!audio) return { d: Infinity, peak: 0 };
      let peak = 0;
      for (const v of audio) peak = Math.max(peak, Math.abs(v));
      return { d: peak > 1e-4 ? core.distance(core.features(audio, length), target) : Infinity, peak };
    }),
  });
  const best = ranked[0];
  if (args.out) writeFileSync(args.out, best.patch);
  const table = JSON.parse(best.patch).settings.wavetables?.[0]?.name;
  console.log(JSON.stringify({ distance: best.d, tries: ranked.length, seconds: (Date.now() - t0) / 1000,
    rhythm: best.rhythm, table }));
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) main(process.argv.slice(2));
