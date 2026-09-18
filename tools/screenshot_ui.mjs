/**
 * Screenshots the UI at its design size, from the real built bundle.
 *
 * Run from ui/ so Playwright resolves, against a static server on the built
 * dist. Exists because "is the UI usable" is not a question any test answers,
 * and because a layout bug that only shows at the design size (a panel
 * overflowing and drawing over the one below it, which happened here) is
 * invisible in code review.
 *
 *   cd ui && npm run build
 *   (cd dist && python3 -m http.server 4173 --bind 127.0.0.1 &)
 *   node ../tools/screenshot_ui.mjs <output-directory>
 */
import { chromium } from 'playwright';

const OUT = process.argv[2] ?? '.';
const URL = process.env.GNARL_UI_URL ?? 'http://127.0.0.1:4173/index.html';

const browser = await chromium.launch({
  // The environment's preinstalled Chromium is a different build number from
  // the npm package's expectation, so point at it rather than downloading.
  executablePath:
    process.env.GNARL_CHROMIUM ?? '/opt/pw-browsers/chromium-1194/chrome-linux/chrome',
});

const page = await browser.newPage({
  // The design size. Everything must fit here without scrolling; the plugin
  // is resizable 70%-200% from this.
  viewport: { width: 1180, height: 720 },
  deviceScaleFactor: 2,
});

const errors = [];
page.on('console', (m) => {
  if (m.type() === 'error') errors.push(m.text());
});
page.on('pageerror', (e) => errors.push(String(e)));

await page.goto(URL, { waitUntil: 'networkidle' });
await page.waitForSelector('.gn-app', { timeout: 10000 });
await page.waitForTimeout(400);

const shoot = async (name) => {
  await page.screenshot({ path: `${OUT}/ui-${name}.png` });
  console.log(`  ui-${name}.png`);
};

for (const tab of ['OSC', 'MOD', 'FX', 'AI']) {
  await page.click(`.gn-tab:text-is("${tab}")`);
  await page.waitForTimeout(250);
  await shoot(tab.toLowerCase());
}

// The formant filter's vowel pad is the headline control.
await page.click('.gn-tab:text-is("OSC")');
await page.waitForTimeout(150);

const filterSelects = await page.$$(
  '.gn-panel:has(.gn-panel__title:text-is("Filter 1")) select',
);
if (filterSelects.length > 0) {
  await filterSelects[0].selectOption({ label: 'Formant' });
  await page.waitForTimeout(250);
  await shoot('osc-formant');
}

// Graintable mode, showing the grain controls appear only there.
const oscSelects = await page.$$('.gn-panel:has(.gn-panel__title:text-is("Osc 1")) select');
if (oscSelects.length > 1) {
  await oscSelects[1].selectOption({ label: 'Graintable' });
  await page.waitForTimeout(250);
  await shoot('osc-graintable');
}

// The theme button cycles dream -> acid -> ember (Dream is the default). Each
// one is shot, because
// the canvas controls read their colours from CSS custom properties and a
// canvas does not repaint when one changes - a theme that looks right in the
// DOM and wrong on the knobs is exactly the bug this catches.
await page.click('.gn-theme');
await page.waitForTimeout(250);
await shoot('theme-acid');

await page.click('.gn-theme');
await page.waitForTimeout(250);
await shoot('theme-ember');

await page.click('.gn-theme');
await page.waitForTimeout(350);
await shoot('theme-dream');

await page.click('.gn-tab:text-is("MOD")');
await page.waitForTimeout(400);
await shoot('theme-dream-mod');
await page.click('.gn-tab:text-is("OSC")');
await page.waitForTimeout(200);

await page.click('.gn-tab:text-is("FX")');
await page.waitForTimeout(150);
await page.click('.gn-panel:has(.gn-panel__title) .gn-toggle');
await page.waitForTimeout(250);
await shoot('fx-ott-on');

await browser.close();

if (errors.length > 0) {
  console.log('\nCONSOLE ERRORS:');
  for (const e of errors) console.log('  ' + e);
  process.exit(1);
}

console.log('\nno console errors');
