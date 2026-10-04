// The mobile version in a real browser: dist/gnarl-web.html, with GNARL's
// engine compiled to WebAssembly running in an AudioWorklet
// (docs/design/phase2-09-mobile.md).
//
//   node ui/tests/web.test.mjs        (after wasm/build.sh and `npm run build`)
//
// Opens the page at phone size, taps to start, and checks the whole path -
// page -> host.ts -> worklet -> engine and back - by listening to the same
// events the page listens to. The engine's SOUND is tests/test_web.py's job;
// this is the plumbing. Exit status: failures.

import { createRequire } from 'node:module';
import { execSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { loadEngine, render as renderNote, writeWav } from '../../tools/web_render.mjs';
import { createServer } from 'node:http';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const require = createRequire(import.meta.url);
function loadPlaywright() {
  try {
    return require('playwright');
  } catch {
    const root = execSync('npm root -g').toString().trim();
    return require(path.join(root, 'playwright'));
  }
}
const { chromium } = loadPlaywright();

const here = path.dirname(fileURLToPath(import.meta.url));
const pagePath = path.join(here, '..', 'dist', 'gnarl-web.html');
if (!existsSync(pagePath)) {
  console.log('FAIL dist/gnarl-web.html missing: run wasm/build.sh, then npm run build');
  process.exit(1);
}

const failures = [];
function check(ok, message) {
  console.log(`${ok ? 'PASS' : 'FAIL'} ${message}`);
  if (!ok) failures.push(message);
}

const browser = await chromium.launch({
  ...(process.env.GNARL_CHROMIUM ? { executablePath: process.env.GNARL_CHROMIUM } : {}),
  args: ['--autoplay-policy=no-user-gesture-required'],
});
const page = await browser.newPage({ viewport: { width: 390, height: 844 }, deviceScaleFactor: 2, isMobile: true, hasTouch: true, acceptDownloads: true });
const errors = [];
page.on('pageerror', (e) => errors.push(String(e)));
if (process.env.DEBUG) page.on('console', (m) => console.log('  page:', m.text().slice(0, 400)));
// Over http, as a phone opens it: Chrome refuses a worklet module from a
// blob: URL on a file:// page, whose origin is opaque.
const server = createServer((_req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
  res.end(readFileSync(pagePath));
});
await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
await page.goto(`http://127.0.0.1:${server.address().port}/`);

// Before the tap: the preview, with the start layer over it.
check(await page.locator('.webstart').isVisible(), 'the start layer is shown');
check(!(await page.evaluate(() => document.body.dataset.plugin === 'true')), 'nothing is connected before the tap');

await page.evaluate(() => {
  window.__startSets = 0;
  const post = MessagePort.prototype.postMessage;
  MessagePort.prototype.postMessage = function (m, ...rest) {
    if (m && m.type === 'set') window.__startSets += 1;
    return post.call(this, m, ...rest);
  };
});
await page.locator('.webstart__button').tap();
await page.waitForFunction(() => document.body.dataset.plugin === 'true', null, { timeout: 15000 });
await page.waitForFunction(() => !document.querySelector('.webstart'), null, { timeout: 5000 });
check(true, 'tapped: the engine started and the panel connected to it');

// Listen where the page listens.
await page.evaluate(() => {
  const w = window;
  w.__seen = { frames: 0, peak: 0, wobble: -1, routes: null, values: {}, tables: null, scope: null };
  const backend = w.__JUCE__.backend;
  backend.addEventListener('gnarlFrame', (f) => {
    w.__seen.frames += 1;
    for (const s of f.scope ?? []) w.__seen.peak = Math.max(w.__seen.peak, Math.abs(s));
    if (typeof f.wobblePhase === 'number') w.__seen.wobble = f.wobblePhase;
    if (f.tables) w.__seen.tables = f.tables;
    if (f.scope) w.__seen.scope = f.scope;
  });
  backend.addEventListener('gnarlRoutes', (r) => (w.__seen.routes = r));
  backend.addEventListener('gnarlValues', (v) => Object.assign(w.__seen.values, v));
});
const seen = () => page.evaluate(() => ({ ...window.__seen }));
const resetPeak = () => page.evaluate(() => (window.__seen.peak = 0));

// No control is dimmed: every engine name the page asked for exists in the
// browser build too.
const dimmed = await page.evaluate(() =>
  [...document.querySelectorAll('.m [data-unbound="true"]')].map((n) => n.dataset.param ?? n.textContent));
// Nothing is dimmed: the engine opens on Vital's init patch, whose filter
// is not the formant model (that one has no DRIVE).
check(dimmed.length === 0, `no dimmed controls on the init patch (${JSON.stringify(dimmed)})`);
check(await page.evaluate(() => document.querySelector('.advanced')?.hidden !== false), 'no ADVANCED button: there is no classic editor here');
// What the panel shows, which is the engine's current state: the lit wobble
// rate and whether the sub's switch is on.
const panelState = () => page.evaluate(() => ({
  rate: document.querySelector('.m [data-param="wobble.rate"] .chip[data-on="true"]')?.textContent ?? null,
  sub: document.querySelector('.m [data-param="sub.on"]')?.getAttribute('aria-pressed') === 'true',
  filter: document.querySelector('.m [data-param="vowel.on"]')?.getAttribute('aria-pressed') === 'true',
}));
// The page opens on Vinny Bass 2 (factory.json's first, a full patch from
// presets/): loaded whole by the worklet, so the page sent no values.
await page.waitForFunction(() => document.querySelector('.m .preset__name')?.textContent === 'Vinny Bass 2', null, { timeout: 5000 });
await page.waitForTimeout(300);
const startSets = await page.evaluate(() => window.__startSets);
const presetShown = await page.evaluate(() => document.querySelector('.m .preset__name')?.textContent);
const opening = await panelState();
check(startSets === 0 && presetShown === 'Vinny Bass 2' && opening.filter && !opening.sub,
  `opens on Vinny Bass 2: preset "${presetShown}", engine ${JSON.stringify(opening)}, ${startSets} values sent by the page`);
// On a patch that moves with LFO 1 (Vinny Bass 2: the wobble at zero), the
// wobble's RATE is LFO 1's: lit at its rate, and a tap changes LFO 1.
await page.locator('.m__tabs .chip', { hasText: 'WOBBLE' }).tap();
const lfoState = () => page.evaluate(() => ({
  aside: document.querySelector('.m [data-wobble-source]')?.textContent,
  lit: document.querySelector('.m [data-param="wobble.rate"] .chip[data-on="true"]')?.textContent ?? null,
}));
let lfo = await lfoState();
check(lfo.aside === 'RATE MOVES LFO 1' && lfo.lit === '1/8', `on Vinny Bass 2 the wobble's RATE is LFO 1's: ${JSON.stringify(lfo)}`);
await page.locator('.m [data-param="wobble.rate"] .chip', { hasText: '1/4' }).tap();
await page.waitForTimeout(300);
const lfoTempo = await page.evaluate(() => window.__seen.values.lfo_1_tempo);
lfo = await lfoState();
check(lfo.lit === '1/4' && lfoTempo?.[1] === '1/4', `a tap on 1/4 sets LFO 1 to 1/4 in the engine: ${JSON.stringify(lfoTempo)}, lit ${lfo.lit}`);
await page.locator('.m__tabs .chip', { hasText: 'OSC' }).tap();

await page.locator('.m .preset__step').last().tap();
await page.waitForFunction(() => document.querySelector('.m .preset__name')?.textContent === 'Riddim Sub', null, { timeout: 5000 })
  .catch(() => {});
const arrowed = await page.evaluate(() => document.querySelector('.m .preset__name')?.textContent);
check(arrowed === 'Riddim Sub', `the arrow steps through the starting sounds: Vinny Bass 2 -> ${arrowed}`);

// What follows was written for the init patch: load it.
await page.evaluate(() => window.__JUCE__.backend.emitEvent('gnarlPresetInit', { name: 'Init' }));
await page.waitForFunction(() => document.querySelector('.m .preset__name')?.textContent === 'Init', null, { timeout: 5000 });
await page.waitForTimeout(300);

// Silence before a note.
await page.waitForTimeout(400);
let s = await seen();
check(s.frames > 5, `the worklet sends frames (${s.frames} in about 0.4 s)`);
check(s.peak < 1e-3, `silent before a note (scope peak ${s.peak.toExponential(2)})`);

// A key: sound, and the wobble running.
const key = page.locator('.m .key[data-note="36"]');
const box = await key.boundingBox();
await page.mouse.move(box.x + box.width / 2, box.y + box.height * 0.8);
await page.mouse.down();
await page.waitForTimeout(600);
s = await seen();
check(s.peak > 0.02, `a held key sounds (scope peak ${s.peak.toFixed(3)})`);
check(s.wobble >= 0 && s.wobble <= 1, `the wobble is running (phase ${s.wobble.toFixed(3)})`);

// A value from the page reaches the engine: MASTER to zero silences it, and
// the engine answers with its own text for it.
await page.evaluate(() => window.__JUCE__.backend.emitEvent('gnarlSet', { name: 'volume', value: 0 }));
await page.waitForTimeout(300);
await resetPeak();
await page.waitForTimeout(300);
s = await seen();
check(s.peak < 1e-3, `MASTER at zero silences the held note (peak ${s.peak.toExponential(2)})`);
check(Array.isArray(s.values.volume) && /dB/.test(s.values.volume[1] ?? ''), `the engine's text came back: ${JSON.stringify(s.values.volume)}`);
await page.evaluate(() => window.__JUCE__.backend.emitEvent('gnarlSet', { name: 'volume', value: 0.7 }));
await page.mouse.up();

// The matrix: a route set from the page is reported back by the engine.
await page.evaluate(() =>
  window.__JUCE__.backend.emitEvent('gnarlRoute', { source: 'lfo_1', destination: 'filter_1_cutoff', amount: 0.4 }));
await page.waitForTimeout(300);
s = await seen();
const routes = (s.routes ?? []).map((r) => `${r.source}>${r.destination}:${r.amount.toFixed(2)}`);
check(routes.includes('lfo_1>filter_1_cutoff:0.40'), `the engine reports the new route: ${JSON.stringify(routes)}`);
await page.evaluate(() =>
  window.__JUCE__.backend.emitEvent('gnarlRoute', { source: 'lfo_1', destination: 'filter_1_cutoff', remove: true }));
await page.waitForTimeout(300);
s = await seen();
check(!(s.routes ?? []).some((r) => r.source === 'lfo_1'), 'and removes it');

// A wobble on an oscillator's level (the matrix's OSC1 LEVEL): a tremolo.
await page.evaluate(() =>
  window.__JUCE__.backend.emitEvent('gnarlRoute', { source: 'wobble', destination: 'osc_1_level', amount: 0.5 }));
await page.waitForTimeout(300);
s = await seen();
check((s.routes ?? []).some((r) => r.source === 'wobble' && r.destination === 'osc_1_level'),
  `the engine takes wobble -> osc 1 level: ${JSON.stringify((s.routes ?? []).map((r) => `${r.source}>${r.destination}`))}`);
await page.evaluate(() =>
  window.__JUCE__.backend.emitEvent('gnarlRoute', { source: 'wobble', destination: 'osc_1_level', remove: true }));
await page.waitForTimeout(300);
// VOLUME (the wobble panel): the gate wob in one tap - osc 1's LEVEL to 0,
// the wobble on it at +1.0. Measured on the scope, frame by frame, over a
// held note: the init patch without it holds steady; with it, the sound
// opens and shuts.
{
  await page.evaluate(() => {
    window.__frames = [];
    window.__JUCE__.backend.addEventListener('gnarlFrame', (f) => {
      if (f.scope) window.__frames.push(Math.max(...f.scope.map(Math.abs)));
    });
  });
  const swing = async () => {
    await page.mouse.move(box.x + box.width / 2, box.y + box.height * 0.8);
    await page.mouse.down();
    await page.waitForTimeout(300);
    await page.evaluate(() => (window.__frames = []));
    await page.waitForTimeout(1500);
    const peaks = await page.evaluate(() => window.__frames);
    await page.mouse.up();
    await page.waitForTimeout(400);
    const sorted = [...peaks].sort((a, b) => a - b);
    const lo = sorted[Math.floor(sorted.length * 0.1)] ?? 0;
    const hi = sorted[Math.floor(sorted.length * 0.9)] ?? 0;
    return { hi, lo, db: 20 * Math.log10((hi + 1e-6) / (lo + 1e-6)), n: peaks.length };
  };
  const before = await swing();
  await page.locator('.m__tabs .chip', { hasText: 'WOBBLE' }).tap();
  const volume = page.locator('.m .wobble .chip', { hasText: 'VOLUME' });
  check(await volume.isVisible() && (await volume.getAttribute('data-on')) === 'false', 'the wobble panel has VOLUME, off');
  await volume.tap();
  await page.waitForTimeout(300);
  s = await seen();
  const gate = (s.routes ?? []).find((r) => r.source === 'wobble' && r.destination === 'osc_1_level');
  check(gate?.amount === 1 && Number(s.values.osc_1_level?.[0]) === 0 && (await volume.getAttribute('data-on')) === 'true',
    `VOLUME sets osc 1 LEVEL to 0 and the wobble on it at 1.0: ${JSON.stringify(gate)}, level ${JSON.stringify(s.values.osc_1_level)}`);
  await page.locator('.m__tabs .chip', { hasText: 'OSC' }).tap();
  const after = await swing();
  check(before.db < 6 && after.db > 20,
    `and the sound opens and shuts: 10th-90th percentile swing ${before.db.toFixed(1)} dB without, ${after.db.toFixed(1)} dB with (${after.n} frames)`);
  await page.locator('.m__tabs .chip', { hasText: 'WOBBLE' }).tap();
  await volume.tap();
  await page.waitForTimeout(300);
  s = await seen();
  check(!(s.routes ?? []).some((r) => r.source === 'wobble' && r.destination === 'osc_1_level') &&
    Math.abs(Number(s.values.osc_1_level?.[0]) - 0.7071) < 0.01 && (await volume.getAttribute('data-on')) === 'false',
    `a second tap takes it off and gives the level back: ${JSON.stringify(s.values.osc_1_level)}`);
  await page.locator('.m__tabs .chip', { hasText: 'OSC' }).tap();
}
// The same from the matrix itself: a route turned to OSC1 LEVEL, its amount
// bar pulled left of centre - a negative amount, which cuts the level.
await page.locator('.m__tabs .chip', { hasText: 'MOD' }).tap();
await page.locator('.m .route__add').tap();
const lastRoute = page.locator('.m .route').last();
for (let k = 0; k < 12 && (await lastRoute.locator('.route__dest').textContent()) !== 'OSC1 LEVEL'; k += 1) {
  await lastRoute.locator('.route__dest').tap();
}
// The engine echoes each change and the matrix redraws: let it settle.
await page.waitForTimeout(400);
const amountBox = await page.locator('.m .route').last().locator('.amount').boundingBox();
await page.mouse.click(amountBox.x + amountBox.width * 0.25, amountBox.y + amountBox.height / 2);
await page.waitForTimeout(300);
s = await seen();
const level = (s.routes ?? []).find((r) => r.destination === 'osc_1_level');
check(level !== undefined && Math.abs(level.amount + 0.5) < 0.05,
  `the matrix routes to OSC1 LEVEL with a negative amount: ${JSON.stringify(level)}`);
await page.locator('.m .route').last().locator('.route__dest').click({ button: 'right' });
await page.waitForTimeout(300);
await page.locator('.m__tabs .chip', { hasText: 'OSC' }).tap();

// OSC 1's table (src/wavetables.ts): the init patch's is Vital's "Init";
// a pick reaches the engine, which names it back, changes the sound, and
// saves it inside the patch.
s = await seen();
const picker = page.locator('.m select[aria-label="OSC 1 wavetable"]');
check(s.tables?.[0] === 'Init' && (await picker.locator('option:checked').textContent()) === 'INIT',
  `the init patch's OSC 1 table is reported and shown: ${JSON.stringify(s.tables)}`);
const scopeOf = async () => {
  await page.evaluate(() => window.__JUCE__.backend.emitEvent('gnarlNote', { note: 48, on: true }));
  await page.waitForTimeout(500);
  const scope = (await seen()).scope;
  await page.evaluate(() => window.__JUCE__.backend.emitEvent('gnarlNote', { note: 48, on: false }));
  await page.waitForTimeout(400);
  return scope;
};
const before = await scopeOf();
await picker.selectOption('Pulse');
await page.waitForFunction(() => window.__seen.tables?.[0] === 'Pulse', null, { timeout: 3000 }).catch(() => {});
s = await seen();
check(s.tables?.[0] === 'Pulse' && s.tables?.[1] === 'Init', `picking PULSE loads it into OSC 1 only: ${JSON.stringify(s.tables)}`);
const after = await scopeOf();
const correlation = (a, b) => {
  let ab = 0, aa = 0, bb = 0;
  for (let i = 0; i < a.length; i += 1) { ab += a[i] * b[i]; aa += a[i] * a[i]; bb += b[i] * b[i]; }
  return ab / Math.sqrt(aa * bb + 1e-30);
};
const r = correlation(before, after);
check(r < 0.8, `and the output changes shape: correlation with the init table's ${r.toFixed(3)}`);
await page.locator('.m .table-pick__arrow').nth(1).tap();
await page.waitForFunction(() => window.__seen.tables?.[0] === 'Steps', null, { timeout: 3000 }).catch(() => {});
check((await seen()).tables?.[0] === 'Steps', `the next arrow steps PULSE -> ${(await seen()).tables?.[0]}`);
const saved = await page.evaluate(() => new Promise((resolve) => {
  window.__JUCE__.backend.addEventListener('gnarlPresetSaved', (reply) => resolve(reply.json));
  window.__JUCE__.backend.emitEvent('gnarlPresetSave', { name: 'Table Test' });
}));
const savedTables = JSON.parse(saved).settings.wavetables.map((t) => t.name);
check(savedTables[0] === 'Steps' && savedTables[1] === 'Init', `the table is saved in the patch: ${JSON.stringify(savedTables)}`);
await page.evaluate(() => window.__JUCE__.backend.emitEvent('gnarlPresetInit', { name: 'Init' }));
await page.waitForFunction(() => window.__seen.tables?.[0] === 'Init', null, { timeout: 3000 }).catch(() => {});
check((await seen()).tables?.[0] === 'Init', 'INIT puts the init table back');

// The AI button (pick-the-best): a tapped sound loads in the engine, which
// names it and shows its table, and plays; PICK grows round 2. Each round is
// levelled by the engine in workers (generate.ts levelVolume): the page asks
// for the four sounds' peaks and gets them back.
await page.evaluate(() => {
  const Native = window.Worker;
  window.__peaks = { asked: 0, answered: 0 };
  window.Worker = class extends Native {
    constructor(...args) {
      super(...args);
      this.addEventListener('message', (e) => { if (e.data?.type === 'peaks') window.__peaks.answered += 1; });
    }
    postMessage(m, ...rest) {
      if (m?.type === 'peaks') window.__peaks.asked += 1;
      return super.postMessage(m, ...rest);
    }
  };
});
await page.locator('.m .ai').tap();
await page.waitForFunction(() => window.__peaks.asked > 0 && window.__peaks.answered >= window.__peaks.asked, null,
  { timeout: 20000 }).catch(() => {});
const peaksSeen = await page.evaluate(() => window.__peaks);
check(peaksSeen.asked > 0 && peaksSeen.answered === peaksSeen.asked,
  `the AI's four are levelled by the engine in workers: ${JSON.stringify(peaksSeen)}`);
await page.locator('.evolve:not([hidden]) .evolve__card').first().waitFor({ timeout: 3000 }).catch(() => {});
await resetPeak();
await page.locator('.evolve .evolve__card').nth(0).locator('.evolve__play').tap();
await page.waitForFunction(() => /^[A-Z][a-z]+ [A-Za-z]+ \d+$/.test(document.querySelector('.m .preset__name')?.textContent ?? ''),
  null, { timeout: 5000 }).catch(() => {});
await page.waitForTimeout(600);
const madeName = await page.evaluate(() => document.querySelector('.m .preset__name')?.textContent ?? '');
const madeTable = (await seen()).tables?.[0];
const madePeak = (await seen()).peak;
check(/^[A-Z][a-z]+ [A-Za-z]+ \d+$/.test(madeName) && madeTable !== 'Init',
  `a tapped AI sound loads: "${madeName}", OSC 1 table ${madeTable}`);
check(madePeak > 0.05 && madePeak < 1, `and plays its note: scope peak ${madePeak.toFixed(3)}`);
await page.locator('.evolve .evolve__card').nth(0).locator('.evolve__pick').tap();
await page.waitForTimeout(200);
check((await page.locator('.evolve__title').textContent())?.endsWith('ROUND 2'), 'PICK starts round 2');
// FROM MY SOUND: the four grow from the patch now loaded (the pick, here),
// read back from the engine as .vital JSON and levelled like any round.
{
  const asked = (await page.evaluate(() => window.__peaks)).asked;
  await page.locator('.evolve .evolve__mine').tap();
  await page.waitForFunction(() => document.querySelector('.evolve__title')?.textContent?.endsWith('FROM YOUR SOUND'), null,
    { timeout: 5000 }).catch(() => {});
  const mine = await page.evaluate(() => ({
    title: document.querySelector('.evolve__title')?.textContent,
    cards: [...document.querySelectorAll('.evolve__card .evolve__name')].map((n) => n.textContent),
  }));
  check(mine.title?.endsWith('FROM YOUR SOUND') && mine.cards.length === 4,
    `FROM MY SOUND makes four from the loaded patch: ${JSON.stringify(mine.cards)}`);
  await page.waitForFunction((n) => window.__peaks.asked > n && window.__peaks.answered >= window.__peaks.asked, asked,
    { timeout: 20000 }).catch(() => {});
  check((await page.evaluate(() => window.__peaks)).asked > asked, 'and levels them in the workers');
  await page.locator('.evolve .evolve__card').nth(3).locator('.evolve__play').tap();
  await page.waitForTimeout(300);
  check(await page.locator('.evolve .evolve__card').nth(3).getAttribute('data-on') === 'true', 'its wild step plays');
}
await page.locator('.evolve .chip', { hasText: 'KEEP' }).tap();
await page.waitForTimeout(2000); // the audition note's own release
await page.evaluate(() => window.__JUCE__.backend.emitEvent('gnarlPresetInit', { name: 'Init' }));
await page.waitForTimeout(300);

// MATCH A SOUND (phase4-04-match-in-app.md): a known wob as the file - Wob
// Triplet Dry at the producer's D#3, rendered here with the same engine -
// goes in through the file picker; the matcher's workers search inside the
// riddim recipe and the four closest come back as the sheet's cards. On the
// matcher's measure the patch itself scores 3.78 against this file (its note
// is held longer than the matcher's render), thirty random sounds of the
// recipe 7.40-14.73 (median 9.75); the closest found must beat 7.0.
{
  const engineForFile = loadEngine();
  if (engineForFile.load(readFileSync(path.join(here, '..', '..', 'presets', 'Wob Triplet Dry.vital'), 'utf8')) !== 0) {
    throw new Error('web.test: Wob Triplet Dry did not load');
  }
  const wavPath = path.join(mkdtempSync(path.join(tmpdir(), 'gnarl-match-')), 'one-wob.wav');
  writeWav(wavPath, renderNote(engineForFile, { seconds: 1.25, bpm: 140, notes: [39] }));
  await page.locator('.m .ai').tap();
  await page.locator('.evolve:not([hidden]) .evolve__match').waitFor({ timeout: 3000 });
  const started = Date.now();
  // Seed 1: the search varies with its seed (phase4-04), and this checks the
  // plumbing, not the luck.
  await page.evaluate(() => (window.__GNARL_MATCH_SEED__ = 1));
  await page.locator('.evolve__file').setInputFiles(wavPath);
  await page.waitForFunction(() => document.querySelector('.evolve__title')?.textContent?.endsWith('MATCHED') ||
    /Could not match/.test(document.querySelector('.evolve__status')?.textContent ?? ''), null, { timeout: 600_000 });
  const matched = await page.evaluate(() => ({
    title: document.querySelector('.evolve__title')?.textContent,
    status: document.querySelector('.evolve__status')?.textContent,
    cards: [...document.querySelectorAll('.evolve__card .evolve__name')].map((n) => n.textContent),
  }));
  const seconds = (Date.now() - started) / 1000;
  check(matched.title?.endsWith('MATCHED') && matched.cards.length === 4 && matched.cards[0] === 'Match 1',
    `MATCH A SOUND returns four sounds in ${seconds.toFixed(0)} s: ${JSON.stringify(matched.cards)} - ${matched.status}`);
  // MIDI 39: D#2 in the renderer's naming, D#3 in FL Studio's (MIDI 60 = C5).
  check(/note D#3/.test(matched.status ?? ''), `it hears the file's note as MIDI 39, D#3 in FL Studio's naming: ${matched.status}`);
  await page.locator('.evolve .evolve__card').nth(0).locator('.evolve__play').tap();
  await page.waitForFunction(() => document.querySelector('.m .preset__name')?.textContent === 'Match 1', null, { timeout: 5000 })
    .catch(() => {});
  check((await page.evaluate(() => document.querySelector('.m .preset__name')?.textContent)) === 'Match 1',
    'tapping Match 1 loads it in the engine');
  const best = Number(/closest ([\d.]+) dB/.exec(matched.status ?? '')?.[1] ?? NaN);
  check(best < 7, `the closest found is ${best} dB from the file (the patch itself 3.78; random sounds of the recipe 7.40-14.73)`);
  await page.locator('.evolve .chip', { hasText: 'KEEP' }).tap();
  await page.waitForTimeout(2000);
  await page.evaluate(() => window.__JUCE__.backend.emitEvent('gnarlPresetInit', { name: 'Init' }));
  await page.waitForTimeout(300);
}

// The page's tempo (TEMPO button): 140 to start; a tap sends 145, and a
// preset load - here the init, Vital's 120 - keeps it, as a DAW's tempo does.
const savedBpm = () => page.evaluate(() => new Promise((resolve) => {
  const backend = window.__JUCE__.backend;
  backend.addEventListener('gnarlPresetSaved', (reply) => resolve(JSON.parse(reply.json).settings.beats_per_minute * 60));
  backend.emitEvent('gnarlPresetSave', { name: 'Tempo Test' });
}));
const tempoButton = page.locator('.m .tempo');
let bpm = await savedBpm();
check((await tempoButton.textContent()) === '140 BPM' && Math.abs(bpm - 140) < 0.01, `the page starts at 140 BPM (engine ${bpm.toFixed(2)})`);
await tempoButton.tap();
await page.waitForTimeout(150);
bpm = await savedBpm();
check((await tempoButton.textContent()) === '145 BPM' && Math.abs(bpm - 145) < 0.01, `a tap: 145 BPM (engine ${bpm.toFixed(2)})`);
await page.evaluate(() => window.__JUCE__.backend.emitEvent('gnarlPresetInit', { name: 'Init' }));
await page.waitForTimeout(300);
bpm = await savedBpm();
check(Math.abs(bpm - 145) < 0.01, `a preset load keeps the page's tempo: ${bpm.toFixed(2)} BPM`);
for (let k = 0; k < 2; k += 1) await tempoButton.tap();
await page.waitForTimeout(150);
check((await tempoButton.textContent()) === '140 BPM', `150, then round to 140: ${await tempoButton.textContent()}`);

// The effects rack (docs/design/phase2-10-fx.md): each page, each switch
// reaches the engine, and a mode button steps to the next mode.
await page.locator('.m__tabs .chip', { hasText: 'FX' }).tap();
const engineValue = (name) => page.evaluate((n) => window.__seen.values[n], name);
const FX_ON = { MOD: ['chorus', 'flanger', 'phaser', 'eq'], SPACE: ['delay', 'reverb'] };
for (const [pageName, slots] of Object.entries(FX_ON)) {
  await page.locator('.m .fxrack__nav .chip', { hasText: pageName }).tap();
  for (const slot of slots) {
    await page.locator(`.m .fx--${slot} .panel__dot`).tap();
    await page.waitForTimeout(150);
    const v = await engineValue(`${slot}_on`);
    check(v?.[0] === 1, `${pageName}: the ${slot.toUpperCase()} switch turns ${slot}_on on in the engine (${JSON.stringify(v)})`);
  }
}
// Each effect's PRESET button (src/fxpresets.ts) applies its next preset to
// the engine and names it.
await page.locator('.m .fxrack__nav .chip', { hasText: 'DRIVE' }).tap();
if ((await engineValue('distortion_on'))?.[0] !== 0) await page.locator('.m .fx--dist .panel__dot').tap();
await page.waitForTimeout(150);
check((await engineValue('distortion_on'))?.[0] === 0, 'DIST is off before its preset');
const distPreset = page.locator('.m .fx--dist .fx__preset');
await distPreset.tap();
await page.waitForTimeout(200);
const warmType = await engineValue('distortion_type');
check((await distPreset.textContent()) === 'WARM' && (await engineValue('distortion_on'))?.[0] === 1,
  `DIST's first preset is WARM and switches it on (${JSON.stringify(await engineValue('distortion_on'))})`);
await distPreset.tap();
await page.waitForTimeout(200);
const gritType = await engineValue('distortion_type');
const gritDrive = await engineValue('distortion_drive');
check((await distPreset.textContent()) === 'GRIT' && gritType?.[1] !== warmType?.[1],
  `the next is GRIT, a different distortion (${warmType?.[1]} -> ${gritType?.[1]}, drive ${gritDrive?.[1]})`);
await page.locator('.m .fxrack__nav .chip', { hasText: 'SPACE' }).tap();
const delayPreset = page.locator('.m .fx--delay .fx__preset');
for (let k = 0; k < 4; k += 1) await delayPreset.tap();
await page.waitForTimeout(250);
const dubSteps = await engineValue('delay_steps');
const dubStyle = await engineValue('delay_style');
check((await delayPreset.textContent()) === 'DUB' && dubSteps?.[1] === '3' && dubStyle?.[1] === 'Ping Pong',
  `the delay's fourth preset, DUB: 3 steps, ping-pong (${JSON.stringify(dubSteps)}, ${JSON.stringify(dubStyle)})`);
// Seven presets: from DUB (the 4th) five more taps go round to PING (the 2nd).
for (let k = 0; k < 5; k += 1) await delayPreset.tap();
await page.waitForTimeout(250);
check((await delayPreset.textContent()) === 'PING' && (await engineValue('delay_steps'))?.[1] === '1',
  `and round again to PING: ${await delayPreset.textContent()}, ${JSON.stringify(await engineValue('delay_steps'))} step`);
// Back to the init state the delay checks below expect.
await page.evaluate(() => window.__JUCE__.backend.emitEvent('gnarlPresetInit', { name: 'Init' }));
await page.waitForTimeout(400);
for (const slot of ['delay', 'reverb']) {
  if ((await engineValue(`${slot}_on`))?.[0] !== 1) await page.locator(`.m .fx--${slot} .panel__dot`).tap();
}
await page.waitForTimeout(200);

// The delay line (docs/design/phase2-11-ddl.md): the LED counts steps of
// the step length, or ms; both taps follow.
const led = () => page.locator('.m .ddl__led').getAttribute('aria-label');
const lengthShown = () => page.locator('.m .chips--cycle[data-param="delay.length"] .chip[data-on="true"]').textContent();
check((await led()) === '1 steps' && (await lengthShown()) === '1/8',
  `the init patch's delay reads as it is: ${await led()} of ${await lengthShown()}`);
for (let k = 0; k < 2; k += 1) await page.locator('.m .ddl__step').first().tap();
await page.waitForTimeout(200);
let v = await engineValue('delay_steps');
check((await led()) === '3 steps' && v?.[1] === '3', `two taps on up: the LED says ${await led()}, the engine ${JSON.stringify(v)}`);
// The step length cycles 1/16, 1/8T, 1/8, 1/4, 1/2: from 1/8, on to the
// two long ones, then round to 1/16.
let tempos;
for (const want of ['1/4', '1/2', '1/16']) {
  await page.locator('.m .chips--cycle[data-param="delay.length"] .chip[data-on="true"]').tap();
  await page.waitForTimeout(200);
  tempos = [await engineValue('delay_tempo'), await engineValue('delay_aux_tempo')];
  const syncsNow = [await engineValue('delay_sync'), await engineValue('delay_aux_sync')];
  check(tempos.every((x) => x?.[1] === want) && syncsNow.every((x) => !/trip/i.test(x?.[1] ?? '')) &&
    (await lengthShown()) === want, `STEP LENGTH -> ${want}, both taps: ${JSON.stringify(tempos)}`);
}
await page.locator('.m .chips--cycle[data-param="delay.length"] .chip[data-on="true"]').tap();
await page.waitForTimeout(200);
let syncs = [await engineValue('delay_sync'), await engineValue('delay_aux_sync')];
tempos = [await engineValue('delay_tempo'), await engineValue('delay_aux_tempo')];
check(syncs.every((x) => /trip/i.test(x?.[1] ?? '')) && tempos.every((x) => x?.[1] === '1/8'),
  `-> 1/8T: ${JSON.stringify(syncs)} ${JSON.stringify(tempos)}`);
await page.locator('.m .chips--cycle[data-param="delay.unit"] .chip[data-on="true"]').tap();
await page.waitForTimeout(200);
syncs = [await engineValue('delay_sync'), await engineValue('delay_aux_sync')];
check((await led()) === '250 milliseconds' && syncs.every((x) => x?.[0] === 0),
  `UNIT -> MS: the LED says ${await led()}, free time on both taps ${JSON.stringify(syncs)}`);
await page.locator('.m .ddl__step').first().tap();
await page.waitForTimeout(200);
const freqs = [await engineValue('delay_frequency'), await engineValue('delay_aux_frequency')];
const msOf = (host) => 1000 / 2 ** (-2 + host * 11);
check((await led()) === '260 milliseconds' && freqs.every((x) => Math.abs(msOf(x?.[0] ?? 0) - 260) < 0.01),
  `up in MS: a tap is 10 ms (250 -> 260), and the engine's frequencies are ${freqs.map((x) => msOf(x?.[0] ?? 0).toFixed(3)).join(', ')} ms`);
await page.locator('.m .chips--cycle[data-param="delay.unit"] .chip[data-on="true"]').tap();
await page.waitForTimeout(200);
syncs = [await engineValue('delay_sync')];
check((await led()) === '3 steps' && /trip/i.test(syncs[0]?.[1] ?? ''), `back to STEPS: ${await led()} of 1/8T`);
await page.locator('.m .fx--delay .chips--cycle[data-param="delay.style"] .chip[data-on="true"]').tap();
await page.waitForTimeout(200);
v = await engineValue('delay_style');
check(/stereo/i.test(v?.[1] ?? ''), `DELAY's style button steps MONO -> STEREO: ${JSON.stringify(v)}`);
await page.locator('.m .fxrack__nav .chip', { hasText: 'DRIVE' }).tap();
// Vital gives distortion_mix and compressor_mix as bare 0..1 numbers; the
// panel shows them as its other % knobs do.
const readouts = await page.evaluate(() => ['dist.mix', 'ott.depth'].map((id) =>
  document.querySelector(`.m [data-param="${id}"] .knob__readout`)?.textContent));
check(readouts.every((r) => /^\d+ %$/.test(r ?? '')), `DIST MIX and OTT DEPTH read as percentages: ${JSON.stringify(readouts)}`);
const distBefore = await page.locator('.m .fx--dist .chips--cycle .chip[data-on="true"]').textContent();
await page.locator('.m .fx--dist .chips--cycle .chip[data-on="true"]').tap();
await page.waitForTimeout(200);
const distAfter = await page.locator('.m .fx--dist .chips--cycle .chip[data-on="true"]').textContent();
check(distBefore !== distAfter, `DIST's mode button steps: ${distBefore} -> ${distAfter}`);
// Back off, so what follows hears the patch it expects.
for (const [pageName, slots] of Object.entries(FX_ON)) {
  await page.locator('.m .fxrack__nav .chip', { hasText: pageName }).tap();
  for (const slot of slots) await page.locator(`.m .fx--${slot} .panel__dot`).tap();
}
await page.locator('.m__tabs .chip', { hasText: 'OSC' }).tap();

// Presets (web/presets.ts): save under a name, change something, load it
// back; an old-version file is refused with its reason; EXPORT hands back a
// .vital file; the arrows step through what is saved. The sheet closes
// after each choice, on x and on a tap outside (a producer's phone showed it
// stuck open: display:flex outranked the hidden attribute).
const toastText = () => page.evaluate(() => document.querySelector('.toast')?.textContent ?? '');
const volumeNow = () => page.evaluate(() => window.__seen.values.volume?.[0]);
const sheetOpen = () => page.locator('.m .presets').isVisible();
const SAVED = '.m .presets__list:not(.presets__list--factory) .presets__load';
const openSheet = async () => {
  if (!(await sheetOpen())) await page.locator('.m .preset__name').tap();
  check(await sheetOpen(), 'tapping the name opens the sheet');
};
check(!(await sheetOpen()), 'the preset sheet starts closed');
await openSheet();
await page.locator('.m .presets__close').tap();
check(!(await sheetOpen()), 'x closes the sheet');
await openSheet();
await page.mouse.click(200, 600);
check(!(await sheetOpen()), 'a tap outside closes the sheet');
await openSheet();
await page.locator('.m .preset__name').tap();
check(!(await sheetOpen()), 'tapping the name again closes it');

await openSheet();
await page.fill('.m #preset-name', 'Test Wub');
await page.locator('.m .presets__form button[type="submit"]').tap();
await page.waitForFunction(() => document.querySelector('.toast')?.textContent?.startsWith('Saved Test Wub'), null, { timeout: 5000 });
check(!(await sheetOpen()), `SAVE closes the sheet: "${await toastText()}"`);
await page.evaluate(() => window.__JUCE__.backend.emitEvent('gnarlSet', { name: 'volume', value: 0.2 }));
await page.waitForTimeout(300);
const changed = await volumeNow();
await openSheet();
await page.locator(SAVED, { hasText: 'Test Wub' }).tap();
await page.waitForFunction(() => document.querySelector('.m .preset__name')?.textContent === 'Test Wub', null, { timeout: 5000 });
await page.waitForTimeout(300);
const restored = await volumeNow();
check(Math.abs(changed - 0.2) < 1e-6 && Math.abs(restored - 0.2) > 0.1 && !(await sheetOpen()),
  `loading "Test Wub" restores MASTER: ${changed.toFixed(3)} -> ${restored.toFixed(3)}, shows its name, closes the sheet`);

// The starting sounds (factory.json): Vinny Bass 2, Riddim Sub, sixteen wobs. One
// loads, names the bar, sets its values, and closes the sheet.
await openSheet();
const factoryNames = await page.locator('.m .presets__list--factory .presets__load').allTextContents();
check(factoryNames.length === 18 && factoryNames[0] === 'Vinny Bass 2' && factoryNames[1] === 'Riddim Sub' &&
  factoryNames[2] === 'Wob Eighth Dry' && factoryNames[17] === 'Wob Stutter Space',
  `eighteen factory sounds, Vinny Bass 2 first, then Riddim Sub and the sixteen wobs: ${JSON.stringify(factoryNames)}`);
await page.locator('.m .presets__list--factory .presets__load', { hasText: 'Riddim Sub' }).tap();
await page.waitForFunction(() => document.querySelector('.m .preset__name')?.textContent === 'Riddim Sub', null, { timeout: 5000 });
await page.waitForTimeout(300);
check(!(await sheetOpen()), 'Riddim Sub loads, names the bar and closes the sheet');
await page.locator('.m .preset__name').tap();
await page.locator('.m .presets__actions button', { hasText: 'INIT' }).tap();
await page.waitForFunction(() => document.querySelector('.m .preset__name')?.textContent === 'Init', null, { timeout: 5000 });
check(!(await sheetOpen()), 'INIT loads the init patch and closes the sheet');

await openSheet();
await page.locator('.m .presets__file').setInputFiles({
  name: 'old.vital', mimeType: 'application/json',
  buffer: Buffer.from(JSON.stringify({ synth_version: '0.9.0', settings: {} })),
});
await page.waitForTimeout(500);
check((await toastText()).includes('older version') && (await sheetOpen()),
  `an old-version file is refused, saying why, and the sheet stays open: "${await toastText()}"`);

await page.locator(SAVED, { hasText: 'Test Wub' }).tap();
await page.waitForFunction(() => document.querySelector('.m .preset__name')?.textContent === 'Test Wub', null, { timeout: 5000 });
await openSheet();
const [download] = await Promise.all([page.waitForEvent('download'), page.locator('.m .presets__actions button', { hasText: 'EXPORT' }).tap()]);
const exported = JSON.parse(readFileSync(await download.path(), 'utf8'));
check(download.suggestedFilename() === 'Test Wub.vital' && exported.preset_name === 'Test Wub' && exported.settings.wavetables.length === 3,
  `EXPORT hands back ${download.suggestedFilename()} (${exported.synth_version}, ${Object.keys(exported.settings).length} settings)`);

await page.fill('.m #preset-name', 'Another');
await page.locator('.m .presets__form button[type="submit"]').tap();
await page.waitForFunction(() => document.querySelector('.toast')?.textContent?.startsWith('Saved Another'), null, { timeout: 5000 });
await page.locator('.m .preset__step').last().tap();
await page.waitForTimeout(500);
const stepped = await page.evaluate(() => document.querySelector('.m .preset__name')?.textContent);
check(stepped === 'Test Wub', `the arrow steps from "Another" to the next saved patch: "${stepped}"`);

// Idle, the page sends the engine nothing (the echo rule, CLAUDE.md §5).
const sent = await page.evaluate(() => {
  const backend = window.__JUCE__.backend;
  const original = backend.emitEvent.bind(backend);
  window.__sent = 0;
  backend.emitEvent = (id, payload) => {
    if (id === 'gnarlSet') window.__sent += 1;
    original(id, payload);
  };
  return new Promise((resolve) => setTimeout(() => resolve(window.__sent), 600));
});
check(sent === 0, `idle for 0.6 s, the page sent ${sent} values`);

check(errors.length === 0, `no page errors ${JSON.stringify(errors)}`);
await browser.close();
server.close();
console.log(`\n${failures.length} failure(s)`);
process.exit(failures.length);
