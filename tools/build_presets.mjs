// GNARL's built-in presets, as full patches (presets/*.vital), from the
// specs below - settings by name in the engine's units, matrix routes, LFO
// shapes and oscillator waveforms - or from a patch made elsewhere
// (presets/source/) with a few settings changed (docs/design/phase2-12-presets.md).
//
//   node tools/build_presets.mjs        # writes presets/*.vital
//
// Built with the browser build of the engine, which saves exactly what the
// plugin saves, so a file opens in both. The specs were read off the
// producer's screenshots of two Vital patches: knob positions are estimates
// (a knob's travel is 7:30 to 4:30 on a clock face), and what the screenshots
// do not show - above all the MATRIX tab - is marked GUESS.

import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { loadEngine } from './web_render.mjs';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');

// One cycle of 2048 samples, as a .vital wavetable keyframe stores it.
const CYCLE = 2048;
function waveData(fn) {
  const samples = new Float32Array(CYCLE);
  for (let i = 0; i < CYCLE; i += 1) samples[i] = fn(i / CYCLE);
  return Buffer.from(samples.buffer).toString('base64');
}
const WAVES = {
  sine: (p) => Math.sin(2 * Math.PI * p),
  square: (p) => (p < 0.5 ? 1 : -1),
};

// LFO shapes as Vital stores them: x, y pairs with y = 0 at the TOP.
function shape(name, points) {
  return { name, num_points: points.length, points: points.flat(), powers: points.map(() => 0), smooth: false };
}

// Tempo indices (kSyncedFrequencyNames): 0 Freeze, 4 4/1, 7 1/2, 8 1/4.
export const PRESETS = [
  {
    name: 'Riddim Sub',
    about: 'A clean sine two octaves down, mono, with a hold-and-release envelope.',
    waves: ['sine'],
    settings: {
      // The master volume puts the peak near -3 dBFS on F3 (F1 sounding).
      volume: 7100,
      osc_1_transpose: -24, osc_1_level: 0.7071, osc_1_random_phase: 0, osc_1_phase: 0.5,
      osc_1_unison_voices: 1, osc_1_destination: 0,
      env_1_attack: 0.05, env_1_hold: 0.3, env_1_sustain: 1, env_1_release: 0.45,
      // LFO 1 as in the screenshot: 1/2, retriggered. GUESS: the screenshot
      // does not show what it modulates, so it is shaped but not routed.
      lfo_1_sync: 1, lfo_1_tempo: 7, lfo_1_sync_type: 0,
      polyphony: 1, pitch_bend_range: 2, portamento_time: -7,
    },
    lfo1: shape('Sub Pluck', [[0, 1], [0.06, 0.62], [0.12, 0], [0.25, 0.6], [0.4, 0.8], [0.6, 0.9], [1, 0.98]]),
    routes: [],
  },
  {
    // The producer's own patch, made in Vital 1.5.5 (presets/source/), which
    // GNARL reads as it is (LoadSave::readableNewerPatch). Kept exactly as
    // made, except the master volume: it peaked at +0.6 to +1.2 dBFS on
    // D1-D2 (+0.5 with random phase off), which a phone's output clips; 4.5 dB down.
    name: 'Vinny Bass 2',
    about: "The producer's own riddim bass, best on low notes (D1).",
    source: 'presets/source/Vinny Bass 2.vital',
    settings: { volume: 5172 },
  },
];

export function buildPatch(engine, spec) {
  if (spec.source) return fromSource(engine, spec);
  engine.reset();
  for (const [name, value] of Object.entries(spec.settings)) {
    const index = engine.index(name);
    if (index < 0) throw new Error(`${spec.name}: the engine has no control ${name}`);
    engine.setValue(index, value);
  }
  for (const [source, destination, amount] of spec.routes) {
    if (engine.route(source, destination, amount, false) < 0) throw new Error(`${spec.name}: no route ${source} -> ${destination}`);
  }
  const patch = JSON.parse(engine.save(spec.name));
  spec.waves.forEach((wave, i) => {
    const table = patch.settings.wavetables[i];
    table.name = wave === 'sine' ? 'Sine' : 'Square';
    table.groups[0].components[0].keyframes = [{ position: 0, wave_data: waveData(WAVES[wave]) }];
  });
  if (spec.lfo1) patch.settings.lfos[0] = spec.lfo1;
  patch.comments = spec.about;
  patch.author = 'GNARL';
  const text = JSON.stringify(patch);
  const result = engine.load(text);
  if (result !== 0) throw new Error(`${spec.name}: the engine refused the built patch (${result})`);
  return engine.save(spec.name);
}

// A patch made elsewhere, loaded as it is, with a few settings changed.
function fromSource(engine, spec) {
  const result = engine.load(readFileSync(join(root, spec.source), 'utf8'));
  if (result !== 0) throw new Error(`${spec.name}: the engine refused ${spec.source} (${result})`);
  for (const [name, value] of Object.entries(spec.settings ?? {})) {
    const index = engine.index(name);
    if (index < 0) throw new Error(`${spec.name}: the engine has no control ${name}`);
    engine.setValue(index, value);
  }
  return engine.save(spec.name);
}

function main() {
  const engine = loadEngine();
  mkdirSync(join(root, 'presets'), { recursive: true });
  for (const spec of PRESETS) {
    const file = join(root, 'presets', `${spec.name}.vital`);
    writeFileSync(file, buildPatch(engine, spec));
    console.log(`presets/${spec.name}.vital`);
  }
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) main();
