// Patches from the AI button's generator (ui/src/generate.ts), as files, so
// tests/test_generate.py can render and measure what the button makes.
//
//   node tools/generate_patches.mjs <out-dir> <first-seed> <count>   (Node 22.18+)

import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { GENERATOR_BASE, generatePatch } from '../ui/src/generate.ts';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const [out, first = '1', count = '20'] = process.argv.slice(2);
if (!out) throw new Error('usage: generate_patches.mjs <out-dir> [first-seed] [count]');
const base = readFileSync(join(root, 'presets', `${GENERATOR_BASE}.vital`), 'utf8');
mkdirSync(out, { recursive: true });
for (let seed = Number(first); seed < Number(first) + Number(count); seed += 1) {
  const g = generatePatch(base, seed);
  writeFileSync(join(out, `${seed}.vital`), g.patch);
  console.log(JSON.stringify({ seed, name: g.name, about: g.about }));
}
