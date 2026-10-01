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
import { existsSync, readFileSync } from 'node:fs';
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
const page = await browser.newPage({ viewport: { width: 390, height: 844 }, deviceScaleFactor: 2, isMobile: true, hasTouch: true });
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

await page.locator('.webstart__button').tap();
await page.waitForFunction(() => document.body.dataset.plugin === 'true', null, { timeout: 15000 });
await page.waitForFunction(() => !document.querySelector('.webstart'), null, { timeout: 5000 });
check(true, 'tapped: the engine started and the panel connected to it');

// Listen where the page listens.
await page.evaluate(() => {
  const w = window;
  w.__seen = { frames: 0, peak: 0, wobble: -1, routes: null, values: {} };
  const backend = w.__JUCE__.backend;
  backend.addEventListener('gnarlFrame', (f) => {
    w.__seen.frames += 1;
    for (const s of f.scope ?? []) w.__seen.peak = Math.max(w.__seen.peak, Math.abs(s));
    if (typeof f.wobblePhase === 'number') w.__seen.wobble = f.wobblePhase;
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
// DRIVE alone is dimmed: the page's default vowel filter is Vital's formant
// model, which has no drive (bridge.test.mjs checks the same in the plugin).
check(JSON.stringify(dimmed) === '["vowel.drive"]', `only the formant model's DRIVE is dimmed (${JSON.stringify(dimmed)})`);
check(await page.evaluate(() => document.querySelector('.advanced')?.hidden !== false), 'no ADVANCED button: there is no classic editor here');
// The page's defaults became the patch: the wobble shape the page shows is
// the one the engine got, still lit after the engine's first frames.
await page.waitForTimeout(300);
const shape = await page.evaluate(() =>
  [...document.querySelectorAll('.m [data-param="wobble.shape"] [data-option]')].filter((b) => b.dataset.on === 'true').map((b) => b.textContent));
check(JSON.stringify(shape) === '["SOFT SQR"]', `SHAPE still shows the page's default after start: ${JSON.stringify(shape)}`);

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
