// GNARL's built-in presets, as full patches (presets/*.vital), from the
// specs below - settings by name in the engine's units, matrix routes, LFO
// shapes and oscillator waveforms (docs/design/phase2-12-presets.md).
//
//   node tools/build_presets.mjs        # writes presets/*.vital
//
// Built with the browser build of the engine, which saves exactly what the
// plugin saves, so a file opens in both. The specs were read off the
// producer's screenshots of two Vital patches: knob positions are estimates
// (a knob's travel is 7:30 to 4:30 on a clock face), and what the screenshots
// do not show - above all the MATRIX tab - is marked GUESS.

import { mkdirSync, writeFileSync } from 'node:fs';
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
    name: 'Alien Riddim',
    about: 'A square and a randomised sine an octave down, through two comb filters, chorus, soft clip, flanger and phaser; LFO 1 at 1/4 with two humps.',
    waves: ['square', 'sine'],
    settings: {
      volume: 5650,
      osc_1_transpose: -12, osc_1_level: 0.5, osc_1_unison_voices: 1, osc_1_unison_detune: 2,
      osc_1_phase: 0.5, osc_1_random_phase: 1, osc_1_destination: 2,
      osc_2_on: 1, osc_2_transpose: -12, osc_2_level: 0.5, osc_2_unison_voices: 1, osc_2_unison_detune: 2,
      osc_2_phase: 0.5, osc_2_random_phase: 1, osc_2_destination: 2,
      // "RAND AMP": the spectral morph Random Amplitudes (6), about 2/3.
      osc_2_spectral_morph_type: 6, osc_2_spectral_morph_amount: 0.67,
      // Filter 1: Comb, Low High Flange+; filter 2: Comb, Low High Comb.
      filter_1_on: 1, filter_1_model: 6, filter_1_style: 1, filter_1_cutoff: 66, filter_1_resonance: 0.6,
      filter_1_mix: 0.67, filter_1_blend: 1.5,
      filter_2_on: 1, filter_2_model: 6, filter_2_style: 0, filter_2_cutoff: 36, filter_2_resonance: 0.5,
      filter_2_mix: 0.61, filter_2_blend: 1.0,
      env_1_attack: 0, env_1_hold: 0.5, env_1_sustain: 1, env_1_release: 0.25,
      lfo_1_sync: 1, lfo_1_tempo: 8, lfo_1_sync_type: 0,
      polyphony: 8, pitch_bend_range: 12,
      // Effects, as the EFFECTS tab shows them.
      chorus_on: 1, chorus_voices: 4, chorus_sync: 1, chorus_tempo: 0, chorus_feedback: -0.1, chorus_dry_wet: 0.67,
      chorus_mod_depth: 0.33, chorus_delay_1: -8.8, chorus_delay_2: -8.5, chorus_cutoff: 100, chorus_spread: 0.4,
      distortion_on: 1, distortion_type: 0, distortion_drive: 6.7, distortion_mix: 0.78,
      flanger_on: 1, flanger_sync: 1, flanger_tempo: 4, flanger_feedback: 0.22, flanger_dry_wet: 0.31,
      flanger_center: 66, flanger_mod_depth: 0.28,
      phaser_on: 1, phaser_sync: 1, phaser_tempo: 0, phaser_feedback: 0.17, phaser_dry_wet: 0.72,
      phaser_center: 50, phaser_mod_depth: 19,
    },
    lfo1: shape('Alien Two Hump', [[0, 1], [0.12, 0.38], [0.25, 0], [0.37, 0.38], [0.5, 1], [0.55, 0.75],
      [0.62, 0], [0.8, 0.38], [1, 1]]),
    // GUESS: the rings in the screenshots sit on filter 1's cutoff and the
    // flanger and phaser centres; the MATRIX tab would say for sure.
    routes: [
      ['lfo_1', 'filter_1_cutoff', 0.5],
      ['lfo_1', 'filter_2_cutoff', 0.3],
      ['lfo_1', 'flanger_center', 0.3],
      ['lfo_1', 'phaser_center', 0.3],
    ],
  },
];

export function buildPatch(engine, spec) {
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
