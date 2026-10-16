// The site's SOUNDS clips (site/public/sounds/): each a built-in preset
// playing a riddim phrase over a DRUMS loop, rendered by GNARL itself - the
// browser build of the engine (wasm/build/gnarl.wasm) for the bass, and the
// DRUMS tab's own synth (ui/src/drums) for the drums. Nothing here comes from
// a reference track (CLAUDE.md section 7): every sample is GNARL's output.
//
//   node tools/render_demos.mjs            (needs wasm/build/gnarl.wasm and ffmpeg)
//
// Writes site/public/sounds/<slug>.mp3 and site/public/sounds/sounds.json,
// the list the site's SOUNDS section plays. The producer's own sounds go in
// the same list: an MP3 in that folder and a line in OWN below.

import { execFileSync } from 'node:child_process';
import { mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { loadEngine } from './web_render.mjs';
import { generateLoop, renderLoop } from '../ui/src/drums/drums.ts';
import { encodeWav } from '../ui/src/audio/wav.ts';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const outDir = join(root, 'site', 'public', 'sounds');
const SR = 44100;
// Riddim's tempo, and the producer's note: D#3 in FL Studio is MIDI 39
// (CLAUDE.md section 7).
const BPM = 140;
const NOTE = 39;
const BARS = 4;

// The bass's rhythm, in sixteenths of one bar, repeated: [on, off) pairs.
// The gaps sit where the snare lands (step 8, beat 3) and before the bar
// turns, so the drums are heard between the notes - riddim's call and answer.
const PHRASE = [[0, 4], [6, 8], [10, 14]];

// The built-in presets shown, with a DRUMS seed each so the loops differ.
const DEMOS = [
  { preset: 'Vinny Bass 2', seed: 3 },
  { preset: 'Wob Triplet Dry', seed: 7 },
  { preset: 'Wob Stutter Delay', seed: 11 },
  { preset: 'Yoi Talk', seed: 5 },
  { preset: 'Tear Growl', seed: 9 },
  { preset: 'Wob Eighth Space', seed: 13 },
];

// The producer's own clips: { title, file } with the MP3 already in
// site/public/sounds/. Listed first on the site.
const OWN = [];

const slug = (name) => name.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');

function renderBass(patchText) {
  const engine = loadEngine();
  if (engine.load(patchText) !== 0) throw new Error('render_demos: the patch did not load');
  engine.bpm(BPM);
  const step = (60 / BPM / 4) * SR;
  const barSamples = step * 16;
  const total = Math.round(barSamples * BARS);
  // Note-on and note-off sample positions, each rounded once from its own time.
  const events = [];
  for (let bar = 0; bar < BARS; bar += 1) {
    for (const [on, off] of PHRASE) {
      events.push({ at: Math.round(bar * barSamples + on * step), on: true });
      events.push({ at: Math.round(bar * barSamples + off * step), on: false });
    }
  }
  events.sort((a, b) => a.at - b.at || Number(a.on) - Number(b.on));
  const left = new Float32Array(total), right = new Float32Array(total);
  let time = 0, done = 0, next = 0;
  while (done < total) {
    while (next < events.length && events[next].at <= done) {
      engine.note(NOTE, events[next].on);
      next += 1;
    }
    const until = next < events.length ? events[next].at : total;
    const n = Math.max(1, Math.min(128, until - done, total - done));
    // The transport runs from 0, so the wobble lands on the bar as in a DAW.
    engine.time(time);
    time += n / SR;
    const audio = engine.process(n);
    for (let i = 0; i < n; i += 1) {
      left[done + i] = audio[2 * i];
      right[done + i] = audio[2 * i + 1];
    }
    done += n;
  }
  return [left, right];
}

const rms = (chs) => Math.sqrt(chs.reduce((s, c) => s + c.reduce((a, v) => a + v * v, 0), 0) / (chs.length * chs[0].length));

// Not emptied first: the producer's own MP3s (OWN) live in the same folder.
mkdirSync(outDir, { recursive: true });
const list = [...OWN];
for (const { preset, seed } of DEMOS) {
  const bass = renderBass(readFileSync(join(root, 'presets', `${preset}.vital`), 'utf8'));
  const drums = renderLoop(generateLoop(seed), BPM, SR, seed);
  // The bass a little over the drums, by RMS: what is being shown is the sound.
  const gain = (rms(drums) / Math.max(rms(bass), 1e-9)) * 1.4;
  const mix = [0, 1].map((c) => {
    const out = new Float32Array(bass[c].length);
    for (let i = 0; i < out.length; i += 1) out[i] = bass[c][i] * gain + (drums[c][i] ?? 0);
    return out;
  });
  // One clip as loud as the next: peak to -1 dBFS.
  let peak = 0;
  for (const c of mix) for (const v of c) peak = Math.max(peak, Math.abs(v));
  const norm = Math.pow(10, -1 / 20) / Math.max(peak, 1e-9);
  for (const c of mix) for (let i = 0; i < c.length; i += 1) c[i] *= norm;
  const name = slug(preset);
  const wav = join(outDir, `${name}.wav`);
  writeFileSync(wav, encodeWav(mix, SR));
  execFileSync('ffmpeg', ['-v', 'error', '-y', '-i', wav, '-codec:a', 'libmp3lame', '-b:a', '160k', join(outDir, `${name}.mp3`)]);
  rmSync(wav);
  list.push({ title: preset.toUpperCase(), file: `${name}.mp3`, note: `${BPM} BPM · D#3` });
  console.log(`${name}.mp3: bass ${(20 * Math.log10(rms(bass))).toFixed(1)} dBFS RMS, drums ${(20 * Math.log10(rms(drums))).toFixed(1)}, bass x${gain.toFixed(2)}, peak ${(20 * Math.log10(peak)).toFixed(1)} dBFS before levelling`);
}
writeFileSync(join(outDir, 'sounds.json'), JSON.stringify({ sounds: list }, null, 1) + '\n');
