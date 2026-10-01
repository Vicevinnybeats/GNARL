// The phone version's starting sounds (ui/src/web/factory.json), built the
// way the page builds them - the engine's init, then each setting by name and
// the wobble shape - and rendered with the browser build of the engine.
//
//   node tools/factory_sounds.mjs <output-directory>
//
// Writes <name>.wav (F1, 4 s at 140 bpm, as Phase 3 measures a growl) and
// <name>.vital (the patch as the engine saves it, so gnarl-render can render
// the same sound) per sound, and prints peak and RMS. tests/test_web.py and
// docs/design/phase2-09-mobile.md hold the numbers; nobody here has heard them.

import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { loadEngine, render, writeWav } from './web_render.mjs';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
export const factory = JSON.parse(readFileSync(join(root, 'ui/src/web/factory.json'), 'utf8')).sounds;

// As the worklet's 'factory' message: the page and this tool must agree.
const SHAPES = { sine: 0, square: 1 };
export function applyFactory(engine, sound) {
  if (sound.patch) {
    const result = engine.load(readFileSync(join(root, 'presets', `${sound.patch}.vital`), 'utf8'));
    if (result !== 0) throw new Error(`${sound.name}: the engine refused presets/${sound.patch}.vital (${result})`);
    return;
  }
  engine.reset();
  for (const [name, value] of Object.entries(sound.settings)) {
    const index = engine.index(name);
    if (index < 0) throw new Error(`${sound.name}: the engine has no control ${name}`);
    engine.setValue(index, value);
  }
  if (sound.shape in SHAPES) engine.wobbleShape(SHAPES[sound.shape]);
}

function main(out) {
  mkdirSync(out, { recursive: true });
  const engine = loadEngine();
  for (const sound of factory) {
    applyFactory(engine, sound);
    const file = sound.name.replace(/\s+/g, '_');
    writeFileSync(join(out, `${file}.vital`), engine.save(sound.name));
    const audio = render(engine, { seconds: 4, bpm: 140, notes: [29], block: 128, start: 0 });
    writeWav(join(out, `${file}.wav`), audio);
    let peak = 0;
    let sum = 0;
    const on = 4 * 44100;
    for (let i = 0; i < 2 * on; i += 1) {
      peak = Math.max(peak, Math.abs(audio[i]));
      sum += audio[i] * audio[i];
    }
    const db = (x) => (20 * Math.log10(x)).toFixed(1);
    console.log(`${sound.name.padEnd(14)} peak ${db(peak)} dBFS (${peak.toFixed(3)})  rms ${db(Math.sqrt(sum / (2 * on)))} dBFS`);
  }
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) main(process.argv[2] ?? 'factory-renders');
