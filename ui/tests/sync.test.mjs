// CLOUD, preset sync (src/sync.ts, docs/design/phase8-01-sync.md), end to
// end in Chromium: two phones, one list.
//
//   node ui/tests/sync.test.mjs              (after `npm run build` in ui/)
//
// Phone A makes a code and uploads its patch; phone B types the code, sees
// the patch and opens it. The server is a stand-in that speaks the
// protocol of backend/src/sync.ts (whose own tests run it in workerd), on a
// different port from the page, so every request is cross-origin as it is
// for the real phone page and the plugin. Exit status: failures.

import { createRequire } from 'node:module';
import { execSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
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

const failures = [];
function check(ok, message) {
  console.log(`${ok ? 'PASS' : 'FAIL'} ${message}`);
  if (!ok) failures.push(message);
}

// ---------------------------------------------------- the stand-in server
const spaces = new Map(); // code symbols -> Map(name -> {patch, updated_at})
const requests = [];
const symbolsOf = (header) => {
  let s = (header ?? '').replace(/^Bearer\s+/i, '').toUpperCase().replace(/[^0-9A-Z]/g, '');
  if (s.length === 20 && s.startsWith('SYNC')) s = s.slice(4);
  return s;
};
const cors = {
  'access-control-allow-origin': '*',
  'access-control-allow-methods': 'GET, PUT, POST, DELETE, OPTIONS',
  'access-control-allow-headers': 'authorization, content-type',
};
const sync = createServer(async (req, res) => {
  const send = (status, body) => {
    res.writeHead(status, { 'content-type': 'application/json', ...cors });
    res.end(body === undefined ? '' : JSON.stringify(body));
  };
  let body = '';
  for await (const chunk of req) body += chunk;
  const url = new URL(req.url, 'http://x');
  requests.push(`${req.method} ${url.pathname}`);
  if (req.method === 'OPTIONS') return send(204);
  if (url.pathname === '/sync/new' && req.method === 'POST') {
    const code = `SYNC-ABCD-EFGH-JKLM-${String(2000 + spaces.size).slice(-4).replace(/[01]/g, '9')}`;
    spaces.set(symbolsOf(code), new Map());
    return send(201, { code });
  }
  const space = spaces.get(symbolsOf(req.headers.authorization));
  if (!req.headers.authorization) return send(401, { error: 'a sync code is required' });
  if (!space) return send(404, { error: 'unknown code' });
  if (url.pathname === '/sync') {
    return send(200, { presets: [...space].map(([name, p]) => ({ name, updated_at: p.updated_at, size: p.patch.length })) });
  }
  const name = decodeURIComponent(url.pathname.replace(/^\/sync\/p\//, ''));
  if (req.method === 'PUT') {
    const { patch } = JSON.parse(body);
    if (typeof JSON.parse(patch).settings !== 'object') return send(400, { error: 'not a patch' });
    space.set(name, { patch, updated_at: new Date().toISOString() });
    return send(200, { name });
  }
  if (req.method === 'GET') return space.has(name) ? send(200, { name, ...space.get(name) }) : send(404, { error: 'no such patch' });
  if (req.method === 'DELETE') {
    space.delete(name);
    return send(204);
  }
  return send(405, {});
});
await new Promise((resolve) => sync.listen(0, '127.0.0.1', resolve));
const syncUrl = `http://127.0.0.1:${sync.address().port}`;

const pages = createServer((_req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
  res.end(readFileSync(pagePath));
});
await new Promise((resolve) => pages.listen(0, '127.0.0.1', resolve));
const pageUrl = `http://127.0.0.1:${pages.address().port}/`;

// ------------------------------------------------------------- two phones
const browser = await chromium.launch({
  ...(process.env.GNARL_CHROMIUM ? { executablePath: process.env.GNARL_CHROMIUM } : {}),
  args: ['--autoplay-policy=no-user-gesture-required'],
});
const errors = [];
async function phone(withServer = true) {
  // A context each: two devices, two localStorages.
  const context = await browser.newContext({ viewport: { width: 390, height: 844 }, isMobile: true, hasTouch: true });
  if (withServer) await context.addInitScript((u) => (window.__GNARL_SYNC_URL__ = u), syncUrl);
  const page = await context.newPage();
  page.on('pageerror', (e) => errors.push(String(e)));
  await page.goto(pageUrl);
  await page.locator('.webstart__button').tap();
  await page.waitForFunction(() => document.body.dataset.plugin === 'true', null, { timeout: 15000 });
  await page.waitForFunction(() => document.querySelector('.m .preset__name')?.textContent === 'Vinny Bass 2', null, { timeout: 5000 });
  await page.locator('.m .preset__name').tap();
  return page;
}
const status = (page) => page.locator('.m .cloud__status').textContent();

// A build or page with no server: no CLOUD, no request.
const plain = await phone(false);
check((await plain.locator('.m .cloud__section').count()) === 0, 'no server configured: no CLOUD section');
check(requests.length === 0, 'and no request was made');
await plain.context().close();

const a = await phone();
check(await a.locator('.m .cloud__section').isVisible(), 'phone A: CLOUD is in the preset sheet');
check(requests.length === 0, 'phone A: nothing is asked of the server before a code is made');
await a.locator('.m .cloud .chip', { hasText: 'NEW CODE' }).tap();
await a.waitForFunction(() => /Your code is SYNC-/.test(document.querySelector('.m .cloud__status')?.textContent ?? ''), null, { timeout: 5000 });
const code = (await status(a)).match(/SYNC-[0-9A-Z-]+/)?.[0] ?? '';
check(/^SYNC-(\w{4}-){3}\w{4}$/.test(code), `phone A: a code is shown (${code})`);
check((await a.locator('.m .cloud__code').textContent()) === code, 'phone A: the code stays on show, to copy');
check((await a.locator('.m .cloud__list .presets__empty').count()) === 1, 'phone A: the new list is empty');

await a.locator('.m .cloud .chip', { hasText: 'UPLOAD' }).tap();
await a.waitForFunction(() => /is in the cloud/.test(document.querySelector('.m .cloud__status')?.textContent ?? ''), null, { timeout: 5000 });
const uploaded = [...spaces.values()][0]?.get('Vinny Bass 2');
check(uploaded !== undefined, 'phone A: UPLOAD put the current patch (Vinny Bass 2) in the cloud');
check(uploaded !== undefined && typeof JSON.parse(uploaded.patch).settings === 'object', 'the upload is the engine\'s .vital JSON');
check((await a.locator('.m .cloud__list .presets__load').allTextContents()).includes('Vinny Bass 2'), 'phone A: and lists it');

// Phone B: a different device, typing the code sloppily.
const b = await phone();
await b.locator('.m .cloud .chip', { hasText: 'USE CODE' }).tap();
await b.locator('.m .cloud__input').fill(code.toLowerCase().replace(/-/g, ' '));
await b.locator('.m .cloud .chip', { hasText: 'USE CODE' }).tap();
await b.waitForSelector('.m .cloud__list .presets__load', { timeout: 5000 });
check((await b.locator('.m .cloud__list .presets__load').allTextContents()).join() === 'Vinny Bass 2', 'phone B: the code opens the same list');

// B loads something else first, then opens the cloud patch.
await b.locator('.m .presets__list--factory .presets__load', { hasText: 'Riddim Sub' }).tap();
await b.waitForFunction(() => document.querySelector('.m .preset__name')?.textContent === 'Riddim Sub', null, { timeout: 5000 });
await b.locator('.m .preset__name').tap();
await b.waitForSelector('.m .cloud__list .presets__load', { timeout: 5000 });
await b.locator('.m .cloud__list .presets__load', { hasText: 'Vinny Bass 2' }).tap();
await b.waitForFunction(() => document.querySelector('.m .preset__name')?.textContent === 'Vinny Bass 2', null, { timeout: 5000 });
check(true, 'phone B: tapping it loads Vinny Bass 2 from the cloud');
check(await b.locator('.m .presets').isHidden(), 'phone B: and the sheet closes, as choosing a patch does');

// A wrong code says so, in words, and keeps nothing.
await b.locator('.m .preset__name').tap();
await b.locator('.m .cloud .chip', { hasText: 'FORGET' }).tap();
check((await b.locator('.m .cloud .chip', { hasText: 'USE CODE' }).count()) === 1, 'phone B: FORGET returns to NEW CODE / USE CODE');
check(spaces.size === 1 && [...spaces.values()][0].size === 1, 'FORGET deleted nothing in the cloud');
await b.locator('.m .cloud .chip', { hasText: 'USE CODE' }).tap();
await b.locator('.m .cloud__input').fill('SYNC-2222-3333-4444-5555');
await b.locator('.m .cloud .chip', { hasText: 'USE CODE' }).tap();
await b.waitForFunction(() => /not known/.test(document.querySelector('.m .cloud__status')?.textContent ?? ''), null, { timeout: 5000 });
check(true, `phone B: an unknown code is refused: "${await status(b)}"`);
await b.locator('.m .cloud__input').fill('hello');
await b.locator('.m .cloud .chip', { hasText: 'USE CODE' }).tap();
await b.waitForFunction(() => /16 letters/.test(document.querySelector('.m .cloud__status')?.textContent ?? ''), null, { timeout: 5000 });
check(true, 'phone B: a malformed code is refused before any request');

// Delete from A; the server goes away; A says so and keeps working.
await a.locator('.m .cloud__list .presets__delete').first().tap();
await a.waitForSelector('.m .cloud__list .presets__empty', { timeout: 5000 });
check([...spaces.values()][0].size === 0, 'phone A: x deletes the patch from the cloud');
await new Promise((resolve) => sync.close(resolve));
sync.closeAllConnections?.();
await a.locator('.m .cloud .chip', { hasText: 'UPLOAD' }).tap();
await a.waitForFunction(() => /No connection/.test(document.querySelector('.m .cloud__status')?.textContent ?? ''), null, { timeout: 8000 });
check(true, `phone A: with the server gone: "${await status(a)}"`);

check(errors.length === 0, `no page errors (${errors.join(' | ')})`);
await browser.close();
pages.close();
console.log(`\n${failures.length} failure(s)`);
process.exit(failures.length);
