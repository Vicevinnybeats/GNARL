// The panel's bridge against a fake plugin, in a real browser.
//
//   node ui/tests/bridge.test.mjs            (after `npm run build` in ui/)
//
// Loads dist/gnarl-ui.html in Chromium with a window.__JUCE__ that behaves
// like src/plugin/web_panel.cpp: it answers gnarlConnect with host values,
// takes gnarlSet, stores it and echoes gnarlValues - asynchronously, as the
// real web view does. Then it clicks the panel and checks what the "engine"
// ended up holding, and what the panel shows. Exit status: failures.
//
// Why: the plugin's own web view under Xvfb is software-rendered and takes
// seconds per click, which made one wrong vowel indistinguishable from a
// slow one. This runs in about a second and says which.

import { createRequire } from 'node:module';
import { execSync } from 'node:child_process';
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
const page_url = 'file://' + path.join(here, '..', 'dist', 'gnarl-ui.html');
const vowels = JSON.parse((await import('node:fs')).readFileSync(path.join(here, '..', 'src', 'vowels.json'), 'utf8'));

// The fake engine: host values (0..1) and step counts, as web_panel.cpp
// reports them. Indexed parameters carry steps; every other name is linear.
const engineInit = {
  values: {
    filter_1_model: 0, // Analog
    filter_1_style: 0,
    filter_1_formant_x: 0.5,
    filter_1_formant_y: 0.5,
    filter_1_on: 0,
  },
  steps: { filter_1_model: 8, filter_1_style: 10, filter_1_on: 2, osc_1_on: 2, osc_2_on: 2 },
};

function fakeJuce(init) {
  const listeners = new Map();
  const values = { ...init.values };
  const log = [];
  const emitToPage = (id, payload) =>
    setTimeout(() => (listeners.get(id) ?? []).forEach((fn) => fn(payload)), 1);
  const entry = (name) => [values[name] ?? 0.5, `${(values[name] ?? 0.5).toFixed(3)}`];
  window.__fake = { values, log };
  window.__JUCE__ = {
    initialisationData: {},
    backend: {
      addEventListener(id, fn) {
        listeners.set(id, [...(listeners.get(id) ?? []), fn]);
      },
      emitEvent(id, payload) {
        setTimeout(() => {
          log.push([id, payload]);
          if (id === '__juce__invoke' && payload.name === 'gnarlConnect') {
            const names = payload.params[0];
            const result = { version: '1.0.6', values: {}, steps: {} };
            for (const n of names) {
              result.values[n] = entry(n);
              if (init.steps[n]) result.steps[n] = init.steps[n];
            }
            emitToPage('__juce__complete', { promiseId: payload.resultId, result });
          } else if (id === 'gnarlSet') {
            values[payload.name] = payload.value;
            emitToPage('gnarlValues', { [payload.name]: entry(payload.name) });
          }
        }, 1);
      },
    },
  };
}

const failures = [];
function check(ok, message) {
  console.log(`${ok ? 'PASS' : 'FAIL'} ${message}`);
  if (!ok) failures.push(message);
}

// GNARL_CHROMIUM names a browser binary; otherwise Playwright's own.
const browser = await chromium.launch(process.env.GNARL_CHROMIUM ? { executablePath: process.env.GNARL_CHROMIUM } : {});
const page = await browser.newPage({ viewport: { width: 1400, height: 820 } });
const errors = [];
page.on('pageerror', (e) => errors.push(String(e)));
if (process.env.DEBUG) page.on('console', (m) => console.log('  page:', m.text().slice(0, 400)));
await page.addInitScript(fakeJuce, engineInit);
await page.goto(page_url);
await page.waitForFunction(() => document.body.dataset.plugin === 'true');

const settle = () => page.waitForTimeout(100);
const engine = () => page.evaluate(() => ({ ...window.__fake.values }));
const lit = () =>
  page.evaluate(() =>
    [...document.querySelectorAll('.app [data-param="vowel.vowel"] [data-option]')]
      .filter((b) => b.dataset.on === 'true')
      .map((b) => b.textContent),
  );
const aside = () => page.evaluate(() => document.querySelector('.app [data-filter-model]')?.textContent);
const unbound = (id) =>
  page.evaluate((id) => document.querySelector(`.app [data-param="${id}"]`)?.dataset.unbound === 'true', id);
const near = (a, b) => Math.abs(a - b) < 1e-6;

await settle();
check((await aside()) === 'ANALOG', `connected on an analog filter: corner reads ${await aside()}`);
check((await lit()).length === 0, 'no vowel lit on an analog filter');
check(!(await unbound('vowel.drive')), 'DRIVE is live on an analog filter');

for (const vowel of ['O', 'U', 'E', 'A', 'I']) {
  await page.click(`.app [data-param="vowel.vowel"] [data-option="${'AEIOU'.indexOf(vowel)}"]`);
  await settle();
  const e = await engine();
  const want = vowels[vowel];
  const style = Math.round(e.filter_1_style * 9);
  check(
    near(e.filter_1_model * 7, 5) && style === want.style && near(e.filter_1_formant_x, want.x) &&
      near(e.filter_1_formant_y, want.y),
    `${vowel}: engine holds model ${e.filter_1_model * 7}, style ${style}, x ${e.filter_1_formant_x}, y ${e.filter_1_formant_y}`,
  );
  const shown = await lit();
  check(shown.length === 1 && shown[0] === vowel, `${vowel}: the panel lights ${JSON.stringify(shown)}`);
}
check((await engine()).filter_1_on === 1, 'pressing a vowel switched filter 1 on');
check((await aside())?.startsWith('FORMANT'), `in the formant model the corner reads ${await aside()}`);
check(await unbound('vowel.drive'), 'DRIVE is dimmed in the formant model');

// The engine switches filter 1 back (automation, Vital's editor): the panel follows.
await page.evaluate(() => window.__JUCE__.backend.emitEvent('gnarlSet', { name: 'filter_1_model', value: 3 / 7 }));
await settle();
check((await aside()) === 'DIGITAL', `after the engine picks Digital the corner reads ${await aside()}`);
check((await lit()).length === 0, 'and no vowel is lit');

// Left alone, the page sends nothing: an echo must never be sent back.
const sets = () => page.evaluate(() => window.__fake.log.filter(([id]) => id === 'gnarlSet').length);
const before = await sets();
await page.waitForTimeout(500);
const after = await sets();
check(after === before, `idle for 0.5 s, the page sent ${after - before} values (an echo loop sends forever)`);

check(errors.length === 0, `no page errors ${JSON.stringify(errors)}`);
await browser.close();
console.log(`\n${failures.length} failure(s)`);
process.exit(failures.length);
