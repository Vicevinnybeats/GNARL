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
// Nothing is dimmed: the engine opens on Vital's init patch, whose filter
// is not the formant model (that one has no DRIVE).
check(dimmed.length === 0, `no dimmed controls on the init patch (${JSON.stringify(dimmed)})`);
check(await page.evaluate(() => document.querySelector('.advanced')?.hidden !== false), 'no ADVANCED button: there is no classic editor here');
// The engine's own init patch, as Vital and the plugin open: the page sent
// it nothing at start, and SHAPE lights none of its three - the init
// wobble is the engine's triangle.
await page.waitForTimeout(300);
const shape = await page.evaluate(() =>
  [...document.querySelectorAll('.m [data-param="wobble.shape"] [data-option]')].filter((b) => b.dataset.on === 'true').map((b) => b.textContent));
const startSets = await page.evaluate(() => window.__startSets);
const presetShown = await page.evaluate(() => document.querySelector('.m .preset__name')?.textContent);
check(startSets === 0 && JSON.stringify(shape) === '[]' && presetShown === 'Init',
  `opens on the engine's init patch: ${startSets} values sent at start, SHAPE ${JSON.stringify(shape)}, preset "${presetShown}"`);

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

// The five starting sounds (factory.json): each loads, names the bar, sets
// its values, and closes the sheet.
await openSheet();
const factoryNames = await page.locator('.m .presets__list--factory .presets__load').allTextContents();
check(factoryNames.length === 5, `five factory sounds: ${JSON.stringify(factoryNames)}`);
await page.locator('.m .presets__list--factory .presets__load', { hasText: 'Triplet Growl' }).tap();
await page.waitForFunction(() => document.querySelector('.m .preset__name')?.textContent === 'Triplet Growl', null, { timeout: 5000 });
await page.waitForTimeout(300);
const growl = await page.evaluate(() => ({ rate: window.__seen.values.wobble_rate?.[1], sub: window.__seen.values.mono_sub_on?.[0] }));
check(!(await sheetOpen()) && growl.sub === 1, `Triplet Growl loads (wobble ${growl.rate}, sub on) and closes the sheet`);
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
