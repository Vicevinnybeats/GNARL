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

/*  The rack's own panels, because each of the fourteen lays its controls out
    differently and the tallest is the one that can overflow. The EQ is the
    densest (three rows of four knobs) and the delay is the only one whose
    controls CHANGE with a setting, so both are worth a picture. A panel that
    overflows draws on top of its siblings, and that is invisible in code
    review (CLAUDE.md section 6). */
/*  The settings popover, open, and then the same view with animations and
    hover glow turned off. The second one is the check that matters: the off
    switch has to leave a UI that is still READABLE, not one that has lost the
    glow marking which effects are on - state glow is information, and only
    the hover glow is the decoration being switched off. */
await page.click('.gn-settings__button');
await page.waitForTimeout(250);
await shoot('settings-open');

/*  HOVER SOMETHING FIRST. The first version of this shot toggled the effects
    off and photographed the window with the pointer parked in a corner - and
    the two pictures came out all but identical, because a hover effect that
    nobody is hovering draws nothing either way. The only visible difference
    was two small LEDs, which is not what the shot is for. */
await page.click('.gn-settings__button');
await page.hover('.gn-fx-chain__row:has-text("Reverb")');
await page.waitForTimeout(250);
await shoot('hover-glow-on');

await page.click('.gn-settings__button');
await page.click('.gn-settings__row:has-text("Animations") .gn-toggle');
await page.click('.gn-settings__row:has-text("Hover glow") .gn-toggle');
await page.click('.gn-settings__button');
await page.hover('.gn-fx-chain__row:has-text("Reverb")');
await page.waitForTimeout(250);
await shoot('hover-glow-off');

await page.click('.gn-settings__button');
await shoot('settings-effects-off');

// Back on, so the remaining shots are of the shipped defaults.
await page.click('.gn-settings__row:has-text("Animations") .gn-toggle');
await page.click('.gn-settings__row:has-text("Hover glow") .gn-toggle');
await page.click('.gn-settings__button');
await page.mouse.move(600, 700);
await page.waitForTimeout(200);

/*  The preset browser, open. It hangs off the header like the settings
    popover does, which is exactly the arrangement that went wrong once
    already - a stacking context nobody declared put the settings panel behind
    the tab, and this one also has to escape an overflow:hidden ancestor. Both
    are invisible in code review and obvious here. */
await page.click('.gn-preset__name');
await page.waitForTimeout(350);
await shoot('preset-browser');

await page.click('.gn-browser__row:has-text("Dream Pad")');
await page.waitForTimeout(250);
await shoot('preset-browser-selected');

await page.keyboard.press('Escape');
await page.waitForTimeout(200);

for (const slot of ['EQ 1', 'Delay', 'Reverb']) {
  await page.click(`.gn-fx-chain__name:text-is("${slot}")`);
  await page.waitForTimeout(150);
  await shoot(`fx-${slot.toLowerCase().replace(/ /g, '')}`);
}

await browser.close();

if (errors.length > 0) {
  console.log('\nCONSOLE ERRORS:');
  for (const e of errors) console.log('  ' + e);
  process.exit(1);
}

console.log('\nno console errors');
