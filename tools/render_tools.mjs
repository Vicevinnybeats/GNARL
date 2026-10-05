// Render the DRUMS and RIDDIMIZE tabs' sounds from the command line, exactly
// as the page does (ui/src/drums, ui/src/riddimize), for tests and listening.
//
//   node tools/render_tools.mjs drums SEED BPM out.wav [hits.json] [PATTERNS] [ROW]   (PATTERNS: JSON row -> pattern index, or 'fill'; ROW: one row alone)
//   node tools/render_tools.mjs riddimize in.wav out.wav [settings JSON]
//
// drums writes the loop and, if asked, its hits (bars x steps per row) as
// JSON; riddimize reads a 16/24-bit or float WAV, mono or stereo.
import { readFileSync, writeFileSync } from 'node:fs';
import { applyPattern, generateLoop, hitTimes, renderLoop, ROWS } from '../ui/src/drums/drums.ts';
import { DEFAULTS, riddimize } from '../ui/src/riddimize/riddimize.ts';
import { encodeWav } from '../ui/src/audio/wav.ts';

function readWav(path) {
  const b = readFileSync(path);
  const view = new DataView(b.buffer, b.byteOffset, b.byteLength);
  let at = 12, fmt = null, data = null;
  while (at + 8 <= b.length) {
    const id = b.toString('ascii', at, at + 4);
    const size = view.getUint32(at + 4, true);
    if (id === 'fmt ') fmt = { format: view.getUint16(at + 8, true), channels: view.getUint16(at + 10, true), rate: view.getUint32(at + 12, true), bits: view.getUint16(at + 22, true) };
    if (id === 'data') data = { at: at + 8, size };
    at += 8 + size + (size & 1);
  }
  const bytes = fmt.bits / 8, frames = Math.floor(data.size / (bytes * fmt.channels));
  const out = new Float32Array(frames);
  for (let i = 0; i < frames; i += 1) {
    let sum = 0;
    for (let c = 0; c < fmt.channels; c += 1) {
      const p = data.at + (i * fmt.channels + c) * bytes;
      if (fmt.format === 3) sum += view.getFloat32(p, true);
      else if (fmt.bits === 16) sum += view.getInt16(p, true) / 32768;
      else if (fmt.bits === 24) sum += ((view.getUint8(p) | (view.getUint8(p + 1) << 8) | (view.getInt8(p + 2) << 16)) / 8388608);
    }
    out[i] = sum / fmt.channels;
  }
  return { audio: out, rate: fmt.rate };
}

const [mode, ...args] = process.argv.slice(2);
if (mode === 'drums') {
  const [seed, bpm, out, hitsPath, fill, only] = args;
  // `fill` may name patterns instead: {"hat": 0, ...} (indexes into PATTERNS).
  const loop = generateLoop(Number(seed));
  if (fill?.startsWith('{')) for (const [row, k] of Object.entries(JSON.parse(fill))) applyPattern(loop, row, k);
  writeFileSync(out, encodeWav(renderLoop(loop, Number(bpm), 44100, Number(seed), only || undefined), 44100));
  // hits: bars x steps per row; times: every hit's onset in seconds, a hat
  // roll's 2 or 3 included (drums.ts hitTimes).
  const times = Object.fromEntries(ROWS.map((row) => [row, hitTimes(loop, Number(bpm), row)]));
  // layer: how much kick plays under each snare (drums.ts kickLayer).
  if (hitsPath) writeFileSync(hitsPath, JSON.stringify({ name: loop.name, hits: loop.hits, times, layer: loop.sound.snare.kickLayer ?? 0 }));
} else if (mode === 'riddimize') {
  const [input, out, settings] = args;
  const { audio, rate } = readWav(input);
  if (rate !== 44100) throw new Error('riddimize: 44.1 kHz input only');
  const r = riddimize(audio, 44100, { ...DEFAULTS, ...(settings ? JSON.parse(settings) : {}) });
  writeFileSync(out, encodeWav(r.channels, 44100));
  console.log(JSON.stringify({ note: r.note, samples: r.channels[0].length }));
} else {
  console.error('usage: render_tools.mjs drums SEED BPM out.wav [hits.json] | riddimize in.wav out.wav [settings]');
  process.exit(2);
}
