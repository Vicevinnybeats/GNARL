// Patches from the AI button's generator (ui/src/generate.ts), as files, so
// tests/test_generate.py can render and measure what the button makes.
//
//   node tools/generate_patches.mjs <out-dir> <first-seed> <count>   (Node 22.18+)
//   node tools/generate_patches.mjs --tables <dir>   GNARL's tables as JSON, for tools/match.py

import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { GENERATOR_BASES, generatePatch } from '../ui/src/generate.ts';
import { TABLE_NAMES, tableJson } from '../ui/src/wavetables.ts';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
if (process.argv[2] === '--tables') {
  const dir = process.argv[3];
  if (!dir) throw new Error('usage: generate_patches.mjs --tables <dir>');
  mkdirSync(dir, { recursive: true });
  for (const name of TABLE_NAMES) writeFileSync(join(dir, `${name}.json`), tableJson(name) ?? '');
  process.exit(0);
}
const [out, first = '1', count = '20'] = process.argv.slice(2);
if (!out) throw new Error('usage: generate_patches.mjs <out-dir> [first-seed] [count]');
const bases = {};
for (const name of GENERATOR_BASES) {
  const file = join(root, 'presets', `${name}.vital`);
  if (existsSync(file)) bases[name] = readFileSync(file, 'utf8');
}
mkdirSync(out, { recursive: true });
for (let seed = Number(first); seed < Number(first) + Number(count); seed += 1) {
  const g = generatePatch(bases, seed);
  writeFileSync(join(out, `${seed}.vital`), g.patch);
  console.log(JSON.stringify({ seed, name: g.name, about: g.about }));
}
