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
    distortion_type: 0,
    distortion_tube: 0,
    osc_1_distortion_type: 2 / 12, // FORMANT
    osc_1_fold: 0,
  },
  steps: {
    filter_1_model: 8, filter_1_style: 10, filter_1_on: 2, osc_1_on: 2, osc_2_on: 2,
    distortion_type: 6, distortion_tube: 2, osc_1_distortion_type: 13, osc_2_distortion_type: 13,
    osc_1_fold: 2, osc_2_fold: 2, distortion_crush_mode: 2, compressor_enabled_bands: 4,
  },
  // The matrix as web_panel.cpp reports it; the second has no panel names.
  routes: [
    { source: 'wobble', destination: 'filter_1_cutoff', amount: 0.45 },
    { source: 'lfo_3', destination: 'osc_1_pan', amount: 0.2 },
  ],
};

function fakeJuce(init) {
  const listeners = new Map();
  const values = { ...init.values };
  const log = [];
  const emitToPage = (id, payload) =>
    setTimeout(() => (listeners.get(id) ?? []).forEach((fn) => fn(payload)), 1);
  const entry = (name) => [values[name] ?? 0.5, `${(values[name] ?? 0.5).toFixed(3)}`];
  const routes = init.routes.map((r) => ({ ...r }));
  window.__fake = { values, log, routes };
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
            const result = { version: '1.0.6', values: {}, steps: {}, routes: routes.map((r) => ({ ...r })),
              licence: { status: 'expired', message: 'Audio still works. Connect to the internet to restore preset saving.', saving: false, hasKey: true } };
            for (const n of names) {
              result.values[n] = entry(n);
              if (init.steps[n]) result.steps[n] = init.steps[n];
            }
            emitToPage('__juce__complete', { promiseId: payload.resultId, result });
          } else if (id === 'gnarlSet') {
            values[payload.name] = payload.value;
            emitToPage('gnarlValues', { [payload.name]: entry(payload.name) });
          } else if (id === 'gnarlLicenceKey') {
            // As web_panel.cpp: the key is stored and checked; the answer
            // arrives as a new banner state.
            emitToPage('gnarlLicence', { status: 'licensed', message: '', saving: true, hasKey: true });
          } else if (id === 'gnarlRoute') {
            const i = routes.findIndex((r) => r.source === payload.source && r.destination === payload.destination);
            if (payload.remove) {
              if (i >= 0) routes.splice(i, 1);
            } else if (i >= 0) routes[i].amount = payload.amount;
            else routes.push({ source: payload.source, destination: payload.destination, amount: payload.amount });
            emitToPage('gnarlRoutes', routes.map((r) => ({ ...r })));
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

// Nothing on the panel is dimmed any more: every control has an engine path.
const dimmed = await page.evaluate(() =>
  [...document.querySelectorAll('.app [data-unbound="true"]')].map((n) => n.dataset.param ?? n.textContent));
check(dimmed.length === 0, `no dimmed controls in the desktop panel (${JSON.stringify(dimmed)})`);

// DIST TUBE / HARD: a switch, then a type.
const chip = (param, option) => `.app [data-param="${param}"] [data-option="${option}"]`;
await page.$eval(chip('dist.mode', 0), (b) => b.click());
await settle();
let e = await engine();
check(e.distortion_tube === 1, `TUBE switches distortion_tube on (${e.distortion_tube})`);
await page.$eval(chip('dist.mode', 1), (b) => b.click());
await settle();
e = await engine();
check(e.distortion_tube === 0 && near(e.distortion_type * 5, 1), `HARD: tube off, distortion_type ${e.distortion_type * 5}`);
// Osc FOLD / SYNC.
await page.$eval(chip('osc1.mode', 3), (b) => b.click());
await settle();
e = await engine();
check(e.osc_1_fold === 1, `osc 1 FOLD switches osc_1_fold on (${e.osc_1_fold})`);
await page.$eval(chip('osc1.mode', 1), (b) => b.click());
await settle();
e = await engine();
check(e.osc_1_fold === 0 && near(e.osc_1_distortion_type * 12, 1),
      `SYNC: fold off, osc_1_distortion_type ${e.osc_1_distortion_type * 12}`);
const litMode = await page.evaluate(() =>
  [...document.querySelectorAll('.app [data-param="osc1.mode"] [data-option]')].filter((b) => b.dataset.on === 'true')
    .map((b) => b.textContent));
check(JSON.stringify(litMode) === '["SYNC"]', `osc 1 lights ${JSON.stringify(litMode)}`);

// The matrix: engine routes in, edits out.
const rows = () => page.evaluate(() => [...document.querySelectorAll('.app .route')].map((r) =>
  [...r.querySelectorAll('button')].map((b) => b.textContent).join(' > ')));
const matrixAside = await page.evaluate(() => [...document.querySelectorAll('.app .panel')].find((p) =>
  p.textContent.includes('MOD MATRIX'))?.querySelector('.panel__aside')?.textContent);
check(JSON.stringify(await rows()) === '["WOBBLE > FILTER CUTOFF"]' && matrixAside === '1/4 +1 in ADVANCED',
      `matrix shows the engine's route ${JSON.stringify(await rows())}, counts the other: "${matrixAside}"`);
await page.click('.app .route .route__dest');
await settle();
let routes = await page.evaluate(() => window.__fake.routes.map((r) => `${r.source}>${r.destination}`));
check(JSON.stringify(routes) === '["lfo_3>osc_1_pan","wobble>filter_1_formant_x"]',
      `retargeting the destination disconnects and connects: ${JSON.stringify(routes)}`);
await page.click('.app .route__add');
await settle();
routes = await page.evaluate(() => window.__fake.routes.length);
check(routes === 3, `ADD ROUTE connects one in the engine (${routes} connections)`);
await page.click('.app .route .route__dest', { button: 'right' });
await settle();
routes = await page.evaluate(() => window.__fake.routes.map((r) => `${r.source}>${r.destination}`));
check(!routes.includes('wobble>filter_1_formant_x'), `right-click disconnects it: ${JSON.stringify(routes)}`);

// The wheels: a drag sends pitch_wheel / mod_wheel; pitch springs back to the
// centre before the gesture ends, so a DAW records the return; mod stays.
async function dragWheel(param, dy) {
  const box = await page.locator(`.app .wheel[data-param="${param}"] .wheel__track`).boundingBox();
  const x = box.x + box.width / 2;
  const y = box.y + box.height / 2;
  await page.mouse.move(x, y);
  await page.mouse.down();
  for (let i = 1; i <= 6; i += 1) await page.mouse.move(x, y + (dy * i) / 6);
  await settle();
  const held = (await engine())[param === 'pitch' ? 'pitch_wheel' : 'mod_wheel'];
  await page.mouse.up();
  await settle();
  return held;
}
const logFrom = await page.evaluate(() => window.__fake.log.length);
const bent = await dragWheel('pitch', -40);
e = await engine();
check(bent > 0.6 && near(e.pitch_wheel, 0.5), `PITCH: held at ${bent.toFixed(3)}, released to ${e.pitch_wheel}`);
const wheelLog = await page.evaluate((from) =>
  window.__fake.log.slice(from).filter(([id, p]) => (id === 'gnarlSet' || id === 'gnarlGesture') && p.name === 'pitch_wheel')
    .map(([id, p]) => (id === 'gnarlGesture' ? (p.begin ? 'begin' : 'end') : p.value)), logFrom);
check(wheelLog[0] === 'begin' && wheelLog.at(-1) === 'end' && wheelLog.at(-2) === 0.5,
  `PITCH: the return to centre is inside the gesture (${JSON.stringify(wheelLog.slice(-3))})`);
const modHeld = await dragWheel('modwheel', -40);
e = await engine();
check(modHeld > 0.1 && near(e.mod_wheel, modHeld), `MOD: stays where it is let go (${e.mod_wheel})`);

// The licence chip (Phase 7): shown for an expired licence, saying saving is
// off; a key typed into it goes to the plugin; a licensed answer hides it.
const chipState = () => page.evaluate(() => {
  const root = document.querySelector('.app .licence');
  return { shown: !root.hidden, text: root.querySelector('.licence__chip').textContent,
    status: root.dataset.status, message: root.querySelector('.licence__message').textContent };
});
let licenceChip = await chipState();
check(licenceChip.shown && licenceChip.text === 'NOT SAVING' && licenceChip.status === 'expired' && licenceChip.message.startsWith('Audio still works'),
  `an expired licence shows the chip: ${JSON.stringify(licenceChip)}`);
check(!(await page.locator('.app .licence__pop').isVisible()), 'the licence popover stays closed until the chip is tapped');
await page.click('.app .licence__chip');
check(await page.locator('.app .licence__pop').isVisible(), 'and opens when it is');
await page.fill('.app #licence-key', 'GNARL-TEST-0001');
const notesBefore = await page.evaluate(() => window.__fake.log.filter(([id]) => id === 'gnarlNote').length);
await page.click('.app .licence__form button[type="submit"]');
await settle();
const keySent = await page.evaluate(() => window.__fake.log.filter(([id]) => id === 'gnarlLicenceKey').map(([, p]) => p.key));
const notesAfter = await page.evaluate(() => window.__fake.log.filter(([id]) => id === 'gnarlNote').length);
check(JSON.stringify(keySent) === '["GNARL-TEST-0001"]', `the typed key is sent to the plugin: ${JSON.stringify(keySent)}`);
check(notesAfter === notesBefore, `typing a key plays no notes (${notesAfter - notesBefore})`);
licenceChip = await chipState();
check(!licenceChip.shown, 'once the plugin answers licensed, the chip is gone');

// The plugin's preset sheet: the starting sounds and INIT, without the web
// build's saving; a sound is sent whole as gnarlPresetFactory, and the
// arrows step through the starting sounds.
const presetLog = (id) => page.evaluate((i) => window.__fake.log.filter(([e]) => e === i).map(([, p]) => p), id);
await page.locator('.app .preset__name').click();
const sheet = await page.evaluate(() => {
  const root = document.querySelector('.app .presets');
  const visible = (s) => [...root.querySelectorAll(s)].filter((n) => n.offsetParent !== null).length;
  return { open: root && !root.hidden, factory: visible('.presets__list--factory .presets__load'),
           form: visible('.presets__form'), open_file: [...root.querySelectorAll('.presets__actions button')]
             .filter((n) => n.offsetParent !== null).map((n) => n.textContent) };
});
check(sheet.open && sheet.factory === 18 && sheet.form === 0 && JSON.stringify(sheet.open_file) === '["INIT"]',
  `in the plugin the name opens the starting sounds and INIT only: ${JSON.stringify(sheet)}`);
await page.locator('.app .presets__list--factory .presets__load', { hasText: 'Riddim Sub' }).click();
await page.waitForTimeout(100); // the fake plugin logs on a timer
const factorySent = await presetLog('gnarlPresetFactory');
let wobPatch = null;
try { wobPatch = JSON.parse(factorySent[0]?.patch ?? ''); } catch { /* checked below */ }
check(factorySent.length === 1 && factorySent[0].name === 'Riddim Sub' && typeof wobPatch?.settings === 'object' &&
  (await page.locator('.app .presets').isHidden()),
  `a starting sound goes to the plugin whole, and the sheet closes: ${JSON.stringify(factorySent.map((s) => s.name))}`);
await page.locator('.app .preset__step').last().click();
await page.waitForTimeout(100);
const stepped = (await presetLog('gnarlPresetFactory')).map((s) => s.name);
check(stepped.length === 2, `an arrow loads a starting sound in the plugin: ${JSON.stringify(stepped)}`);
// The AI button: pick-the-best (phase4-03-pick.md). It opens four sounds
// and loads nothing until one is tapped; a tap loads that patch whole and
// plays a note; PICK starts round 2 with four grown from the pick.
const factoryBefore = (await presetLog('gnarlPresetFactory')).length;
await page.locator('.app .ai').click();
await page.waitForTimeout(100);
const cards = page.locator('.evolve:not([hidden]) .evolve__card');
check((await cards.count()) === 4 && (await presetLog('gnarlPresetFactory')).length === factoryBefore,
  `the AI button opens four sounds and loads none: ${await cards.count()} cards`);
await cards.nth(1).locator('.evolve__play').click();
await page.waitForTimeout(150);
const tapped = (await presetLog('gnarlPresetFactory')).slice(factoryBefore);
let tappedPatch = null;
try { tappedPatch = JSON.parse(tapped[0]?.patch ?? ''); } catch { /* checked below */ }
check(tapped.length === 1 && /^[A-Z][a-z]+ [A-Za-z]+ \d+$/.test(tapped[0]?.name ?? '') &&
  tappedPatch?.author === 'GNARL generator',
  `a tap loads that sound whole: ${tapped[0]?.name}, ${tappedPatch?.comments}`);
await cards.nth(2).locator('.evolve__pick').click();
await page.waitForTimeout(150);
const round2 = await page.evaluate(() => ({
  title: document.querySelector('.evolve__title')?.textContent,
  cards: document.querySelectorAll('.evolve__card').length,
  wild: document.querySelectorAll('.evolve__badge').length,
}));
check(round2.title?.endsWith('ROUND 2') && round2.cards === 4 && round2.wild === 1,
  `PICK grows round 2 from the pick: ${JSON.stringify(round2)}`);
await cards.nth(0).locator('.evolve__play').click();
await page.waitForTimeout(150);
const child = (await presetLog('gnarlPresetFactory')).slice(-1)[0];
let childPatch = null;
try { childPatch = JSON.parse(child?.patch ?? ''); } catch { /* checked below */ }
check(/picked from /.test(childPatch?.comments ?? ''), `a round-2 sound names its parent: ${childPatch?.comments}`);
await page.locator('.evolve .chip', { hasText: 'BACK' }).click();
check((await page.locator('.evolve__title').textContent())?.endsWith('ROUND 1'), 'BACK returns to round 1');
await page.locator('.evolve .chip', { hasText: 'KEEP' }).click();
check(await page.locator('.evolve').isHidden(), 'KEEP closes the AI');

// A full patch goes to the plugin whole, as the .vital text.
await page.locator('.app .preset__name').click();
await page.locator('.app .presets__list--factory .presets__load', { hasText: 'Riddim Sub' }).click();
await page.waitForTimeout(100);
const sub = (await presetLog('gnarlPresetFactory')).at(-1);
let subPatch = null;
try { subPatch = JSON.parse(sub.patch); } catch { /* checked below */ }
check(sub.name === 'Riddim Sub' && subPatch?.settings?.osc_1_transpose === -24,
  `Riddim Sub goes to the plugin as a whole patch (osc 1 at ${subPatch?.settings?.osc_1_transpose})`);
await page.locator('.app .preset__name').click();
await page.locator('.app .presets__actions button', { hasText: 'INIT' }).click();
await page.waitForTimeout(100);
check((await presetLog('gnarlPresetInit')).length === 1, 'INIT asks the plugin for the init patch');

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
