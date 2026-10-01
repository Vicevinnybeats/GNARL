// Render a note with the BROWSER build of the engine (wasm/build/gnarl.wasm),
// the way gnarl-render renders it with the desktop build, so the two can be
// compared sample for sample (tests/test_web.py).
//
//   node tools/web_render.mjs patch.vital -o out.wav -l 2 -m C2 -b 120 --block 128
//
// The patch is loaded by the browser build's own loader (gnarl_load: the
// current format, wavetables, LFOs, modulations, sample). A patch from an
// older version needs the plugin's migration, which the browser build does
// not have; for those, --by-name applies every numeric setting and
// modulation by name instead (wavetables and shapes stay the init's).
// --save F writes the patch back out as the browser build saves it.
//
// Output: 32-bit float WAV, 44.1 kHz, as gnarl-render --bits 32.

import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, '..');

export function loadEngine(wasmPath = join(root, 'wasm/build/gnarl.wasm'), sampleRate = 44100) {
  const source = readFileSync(join(root, 'ui/src/web/engine-core.js'), 'utf8');
  const createGnarlEngine = new Function(`${source}; return createGnarlEngine;`)();
  return createGnarlEngine(readFileSync(wasmPath), sampleRate);
}

const NOTE_NAMES = { C: 0, D: 2, E: 4, F: 5, G: 7, A: 9, B: 11 };
// As Tuning::noteToMidiKey: C2 = 36.
function noteToMidi(name) {
  const m = /^([A-Ga-g])(#|b)?(-?\d+)$/.exec(name);
  if (!m) throw new Error(`bad note ${name}`);
  const pc = NOTE_NAMES[m[1].toUpperCase()] + (m[2] === '#' ? 1 : m[2] === 'b' ? -1 : 0);
  return 12 * (Number(m[3]) + 1) + pc;
}

export function applyPatch(engine, patch) {
  const settings = patch.settings ?? {};
  for (const [name, value] of Object.entries(settings)) {
    if (typeof value !== 'number') continue;
    const index = engine.index(name);
    if (index < 0) throw new Error(`the engine has no control ${name}`);
    engine.setValue(index, value);
  }
  for (const [i, m] of (settings.modulations ?? []).entries()) {
    if (!m.source || !m.destination) continue;
    const amount = settings[`modulation_${i + 1}_amount`] ?? 0;
    if (engine.route(m.source, m.destination, amount, false) < 0) {
      throw new Error(`cannot connect ${m.source} -> ${m.destination}`);
    }
  }
}

// SynthBase::renderAudioToFile, step for step: a second of pre-roll with the
// transport running, note-on at `start` seconds, note-off after `seconds`,
// then 30% more with a 200-sample fade at the very end.
export function render(engine, { seconds, bpm, notes, block = 128, start = 0 }) {
  const sampleRate = 44100;
  const preProcess = 44100;
  const fadeSamples = 200;
  const blockSize = Math.max(1, Math.min(block, 128));
  engine.bpm(bpm);
  const sampleTime = 1 / sampleRate;
  let time = start - preProcess * sampleTime;
  for (let done = 0; done < preProcess;) {
    const n = Math.min(blockSize, preProcess - done);
    engine.time(time);
    time += n * sampleTime;
    engine.process(n);
    done += n;
  }
  for (const note of notes) engine.note(note, true);

  const onSamples = Math.trunc(seconds * sampleRate);
  const total = onSamples + Math.trunc(seconds * sampleRate * 0.3);
  const out = new Float32Array(2 * total);
  for (let done = 0; done < total;) {
    let n = Math.min(blockSize, total - done);
    if (done < onSamples) n = Math.min(n, onSamples - done);
    engine.time(time);
    time += n * sampleTime;
    const audio = engine.process(n);
    for (let i = 0; i < n; i += 1) {
      const t = Math.min((total - (done + i)) / fadeSamples, 1);
      out[2 * (done + i)] = Math.fround(t * audio[2 * i]);
      out[2 * (done + i) + 1] = Math.fround(t * audio[2 * i + 1]);
    }
    done += n;
    if (done === onSamples) for (const note of notes) engine.note(note, false);
  }
  return out;
}

export function writeWav(path, interleaved, sampleRate = 44100) {
  const dataBytes = interleaved.length * 4;
  const header = Buffer.alloc(44);
  header.write('RIFF', 0);
  header.writeUInt32LE(36 + dataBytes, 4);
  header.write('WAVE', 8);
  header.write('fmt ', 12);
  header.writeUInt32LE(16, 16);
  header.writeUInt16LE(3, 20); // IEEE float
  header.writeUInt16LE(2, 22);
  header.writeUInt32LE(sampleRate, 24);
  header.writeUInt32LE(sampleRate * 8, 28);
  header.writeUInt16LE(8, 32);
  header.writeUInt16LE(32, 34);
  header.write('data', 36);
  header.writeUInt32LE(dataBytes, 40);
  writeFileSync(path, Buffer.concat([header, Buffer.from(interleaved.buffer, interleaved.byteOffset, dataBytes)]));
}

function main(argv) {
  const args = { seconds: 2, bpm: 120, notes: [48], block: 128, start: 0, out: 'web.wav', patch: null, save: null, byName: false };
  for (let i = 0; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === '-o') args.out = argv[++i];
    else if (a === '-l') args.seconds = Number(argv[++i]);
    else if (a === '-b') args.bpm = Number(argv[++i]);
    else if (a === '-m') args.notes = argv[++i].split(/[ ,]+/).filter(Boolean).map(noteToMidi);
    else if (a === '--block') args.block = Number(argv[++i]);
    else if (a === '--start') args.start = Number(argv[++i]);
    else if (a === '--wasm') args.wasm = argv[++i];
    else if (a === '--save') args.save = argv[++i];
    else if (a === '--by-name') args.byName = true;
    else args.patch = a;
  }
  const engine = loadEngine(args.wasm);
  if (args.patch) {
    const text = readFileSync(args.patch, 'utf8');
    if (args.byName) applyPatch(engine, JSON.parse(text));
    else {
      const result = engine.load(text);
      const why = ['', 'not a patch', 'made by a newer GNARL', 'from an older version: use --by-name'][result];
      if (result !== 0) throw new Error(`${args.patch}: ${why}`);
    }
  }
  if (args.save) writeFileSync(args.save, engine.save(args.patch ? args.patch.replace(/^.*\//, '').replace(/\.vital$/, '') : 'Init'));
  const started = performance.now();
  const audio = render(engine, args);
  const ms = performance.now() - started;
  writeWav(args.out, audio);
  const renderedSeconds = audio.length / 2 / 44100 + 1;
  console.log(`${args.out}: ${(renderedSeconds / (ms / 1000)).toFixed(1)}x real time`);
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) main(process.argv.slice(2));
