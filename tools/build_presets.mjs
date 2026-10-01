// GNARL's built-in presets, as full patches (presets/*.vital), from the
// specs below - settings by name in the engine's units, matrix routes, LFO
// shapes and oscillator waveforms - or from a patch made elsewhere
// (presets/source/) with a few settings changed (docs/design/phase2-12-presets.md).
//
//   node tools/build_presets.mjs        # writes presets/*.vital (Node 22.18+:
//                                       # it imports ui/src/wavetables.ts)
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
// GNARL's generated wavetables, the page's own file (node strips its types).
import { tableJson } from '../ui/src/wavetables.ts';

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

const STYLE_VOLUMES = {
  'Yoi Talk': 4660,
  'Tear Growl': 4632,
  'Metal Grind': 4800,
  'Screech': 4408,
  'Old Wub': 4747,
  'Triplet Riddim': 4503,
  'PD Zap': 4786,
  'Hollow Bark': 4662,
  'Croak Table': 4582,
  'Comb Squelch': 4645,
};

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

  // ----- One-shot wobs (docs/design/phase2-12-presets.md): one wob per key,
  // timed to the tempo, from the reference drops' own wobs - three of five
  // tracks wob for about 2 beats (1.81-2.06 median), opening and closing
  // once, centred about 2 kHz; lily's and meta 800's are a beat or less.
  ...ONE_SHOTS(),

  // ----- Styles (docs/design/phase2-12-presets.md): the references' wob -
  // one or two beats, one turn of brightness, centred near 2 kHz - made with
  // each of GNARL's generated tables in a different style.
  ...STYLES(),
];

/*
 * The one-shot wobs share a voice: a saw (the init wavetable) with a narrow
 * second voice, soft clip and some OTT, through filter 1. LFO 1 plays ONCE
 * per key (Envelope mode, 2) at a tempo-synced length; the amp envelope
 * holds for the same length and falls fast, so a held key is one wob. No
 * sub: play Riddim Sub under them.
 */
function oneShot(name, about, { tempo, shape, filter, routes, extra = {} }) {
  // tempo: kSyncedFrequencyNames index - 7 = 1/2 (2 beats), 8 = 1/4 (1 beat).
  const holdSeconds = { 7: 0.84, 8: 0.41 }[tempo];
  return {
    name,
    about,
    waves: [],
    settings: {
      volume: 5300,
      osc_1_unison_voices: 2, osc_1_unison_detune: 0.8, osc_1_stereo_spread: 0.4,
      osc_1_random_phase: 0,
      // Hold (quartic time: seconds = value^4) for the wob, then a fast fall.
      env_1_attack: 0, env_1_hold: Math.pow(holdSeconds, 0.25), env_1_decay: 0.45, env_1_sustain: 0,
      env_1_release: 0.3,
      lfo_1_sync: 1, lfo_1_tempo: tempo, lfo_1_sync_type: 2,
      distortion_on: 1, distortion_type: 0, distortion_drive: 9, distortion_mix: 0.8,
      compressor_on: 1, compressor_mix: 0.45,
      filter_1_on: 1,
      ...filter,
      ...extra,
    },
    lfo1: shape,
    routes,
  };
}

function ONE_SHOTS() {
  // Analog low pass from about 370 Hz (cutoff 66); LFO 1 opens it by up to
  // 0.6 of the range. A plain saw at F1 centres at only 170 Hz even
  // unfiltered (its power is in the low harmonics), where the references'
  // wobs centre near 2 kHz: so the growl is made first - osc 1 in FORMANT
  // warp, harder soft clip and the FOLD stage - and the filter moves that.
  const lowpass = { filter_1_model: 0, filter_1_style: 1, filter_1_cutoff: 66, filter_1_resonance: 0.55,
    osc_1_distortion_type: 2, osc_1_distortion_amount: 0.7, distortion_drive: 18,
    distortion_fold_on: 1, distortion_fold_drive: 6 };
  // Vital's formant filter: the vowel moves along X with LFO 1.
  const formant = { filter_1_model: 5, filter_1_style: 0, filter_1_formant_x: 0.15, filter_1_formant_y: 0.6,
    filter_1_formant_resonance: 0.9 };
  return [
    oneShot('Wob Open', 'One wob that opens up through two beats, brightest at its end (as 6:25:300).', {
      tempo: 7, filter: lowpass, routes: [['lfo_1', 'filter_1_cutoff', 0.6]],
      shape: shape('Open', [[0, 1], [0.85, 0], [1, 1]]),
    }),
    oneShot('Wob Peak', 'One wob over two beats, brightest early, then closing (as drac07).', {
      tempo: 7, filter: lowpass, routes: [['lfo_1', 'filter_1_cutoff', 0.6]],
      shape: shape('Peak', [[0, 1], [0.2, 0], [0.6, 0.7], [1, 1]]),
    }),
    oneShot('Wob Close', 'A one-beat wob, bright at the hit and closing (as lily).', {
      tempo: 8, filter: lowpass, routes: [['lfo_1', 'filter_1_cutoff', 0.6]],
      shape: shape('Close', [[0, 0], [0.3, 0.55], [1, 1]]),
    }),
    oneShot('Frog Croak', 'A vowel that swells and sinks over two beats, with the pitch dropping a little: a croak.', {
      tempo: 7, filter: formant,
      routes: [['lfo_1', 'filter_1_formant_x', 0.7], ['lfo_1', 'osc_1_tune', 0.08]],
      shape: shape('Croak', [[0, 1], [0.35, 0], [1, 1]]),
    }),
    oneShot('Ribbit', 'Two quick vowel blips in one beat.', {
      tempo: 8, filter: formant, routes: [['lfo_1', 'filter_1_formant_x', 0.75]],
      shape: shape('Ribbit', [[0, 1], [0.2, 0], [0.45, 1], [0.65, 0], [1, 1]]),
    }),
    oneShot('Swamp Gurgle', 'Four fast vowel bubbles inside a two-beat wob.', {
      tempo: 7, filter: formant, routes: [['lfo_1', 'filter_1_formant_x', 0.6], ['lfo_1', 'filter_1_formant_y', 0.3]],
      shape: shape('Gurgle', [[0, 1], [0.12, 0.1], [0.25, 0.8], [0.37, 0.05], [0.5, 0.75], [0.62, 0.1], [0.75, 0.8],
        [0.87, 0.2], [1, 1]]),
    }),
  ];
}

/*
 * The styles move osc 1 through one of GNARL's own tables (ui/src/wavetables.ts)
 * with LFO 1 on WT POS - the way a wavetable growl is made - rather than with
 * a filter. `loop` plays LFO 1 as a repeating wobble (trigger mode) under a
 * held note instead of once.
 */
function style(name, about, { table, tempo, sync = 1, loop = false, shape, depth = 0.9, start = 0,
  routes = [], extra = {} }) {
  // Each sound's own master volume: its loudest note from D1 to F2 peaks
  // at -3 dBFS (32-bit float renders at 140 BPM).
  const volume = STYLE_VOLUMES[name];
  const holdSeconds = { 7: 0.84, 8: 0.41, 9: 0.2 }[tempo] ?? 0.84;
  return {
    name,
    about,
    waves: [],
    table: [0, table],
    settings: {
      volume,
      osc_1_wave_frame: start,
      osc_1_unison_voices: 3, osc_1_unison_detune: 1.2, osc_1_stereo_spread: 0.5,
      osc_1_random_phase: 0,
      ...(loop
        ? { env_1_attack: 0, env_1_sustain: 1, env_1_release: 0.25, lfo_1_sync_type: 0 }
        : { env_1_attack: 0, env_1_hold: Math.pow(holdSeconds, 0.25), env_1_decay: 0.45, env_1_sustain: 0,
            env_1_release: 0.3, lfo_1_sync_type: 2 }),
      lfo_1_sync: sync, lfo_1_tempo: tempo,
      // The references' wobs centre at 1.9-2.2 kHz in the growl band (150 Hz
      // to 6 kHz), from about 1.6 kHz at their darkest; a table at F1 alone
      // centres at 0.3-1.8 kHz. Hard drive, the fold and a 6 dB high shelf
      // bring them there (measured per preset in phase2-12-presets.md).
      distortion_on: 1, distortion_type: 0, distortion_drive: 28, distortion_mix: 0.8,
      distortion_fold_on: 1, distortion_fold_drive: 9,
      compressor_on: 1, compressor_mix: 0.5,
      eq_on: 1, eq_low_gain: 0, eq_band_gain: 0, eq_high_gain: 6, eq_high_cutoff: 88,
      ...extra,
    },
    lfo1: shape,
    routes: [['lfo_1', 'osc_1_wave_frame', depth], ...routes],
  };
}

function STYLES() {
  // LFO shapes, y = 0 at the TOP: one rise and fall, as most reference wobs.
  const swell = (n) => shape(n, [[0, 1], [0.5, 0], [1, 1]]);
  const late = (n) => shape(n, [[0, 1], [0.8, 0], [1, 1]]);
  const early = (n) => shape(n, [[0, 0.1], [0.2, 0], [1, 1]]);
  return [
    style('Yoi Talk', 'An I opening into an O over two beats: the talking yoi.', {
      table: 'Yoi', tempo: 7, shape: swell('Yoi'),
    }),
    style('Tear Growl', 'Tearout: the uneven fold table, folded again and crushed, opening late.', {
      table: 'Tear', tempo: 7, shape: late('Tear'),
      extra: { distortion_crush_on: 1, distortion_crush_bits: 10 },
    }),
    style('Metal Grind', 'Metallic clusters swelling in one beat, through a short flanger.', {
      table: 'Metal', tempo: 8, shape: swell('Grind'),
      extra: { flanger_on: 1, flanger_tempo: 8, flanger_mod_depth: 0.5, flanger_feedback: 0.1, flanger_dry_wet: 0.6 },
    }),
    style('Screech', 'A narrow peak screaming up and back in one beat, with a phaser.', {
      table: 'Screech', tempo: 8, shape: swell('Screech'), depth: 0.7,
      extra: { phaser_on: 1, phaser_tempo: 8, phaser_feedback: 0.7, phaser_dry_wet: 0.6 },
    }),
    style('Old Wub', 'The classic wub: a resonant low pass table, looping at 1/4 while the key is held.', {
      table: 'Wub', tempo: 8, loop: true, shape: swell('Wub'),
    }),
    style('Triplet Riddim', 'A growl looping in 1/8 triplets while the key is held: the riddim pattern.', {
      table: 'Growl', tempo: 9, sync: 3, loop: true, shape: early('Triplet'),
    }),
    style('PD Zap', 'A phase-distortion zap: bright at the hit, closing in an eighth, the pitch falling.', {
      table: 'PD', tempo: 9, shape: early('Zap'),
      routes: [['lfo_1', 'osc_1_tune', 0.1]],
    }),
    style('Hollow Bark', 'A hollow square with its notch sweeping, one beat.', {
      table: 'Hollow', tempo: 8, shape: swell('Bark'),
    }),
    style('Croak Table', 'The croak table across two beats: a ring rising and falling inside the note.', {
      table: 'Croak', tempo: 7, shape: swell('Croak T'),
    }),
    style('Comb Squelch', 'The comb table through the dirty filter, squelching once over two beats.', {
      table: 'Comb', tempo: 7, shape: late('Squelch'),
      extra: { filter_1_on: 1, filter_1_model: 1, filter_1_cutoff: 100, filter_1_resonance: 0.5 },
    }),
  ];
}

export function buildPatch(engine, spec) {
  if (spec.source) return fromSource(engine, spec);
  engine.reset();
  // Riddim's tempo, 140 BPM (in beats per second, as the engine keeps it),
  // not Vital's 120: the phone page plays a patch at its own tempo until the
  // TEMPO button says otherwise; in a DAW the host's tempo wins.
  engine.setValue(engine.index('beats_per_minute'), 140 / 60);
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
  if (spec.table) patch.settings.wavetables[spec.table[0]] = JSON.parse(tableJson(spec.table[1]));
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
