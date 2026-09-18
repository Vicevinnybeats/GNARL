/**
 * Records a video of the UI, to show motion a screenshot cannot.
 *
 * Every movement here is a REAL interaction - Playwright drags the actual
 * controls - so the waveform moves because the parameter moves, not because
 * something is animating for the camera.
 *
 *   cd ui && npm run build
 *   (cd dist && python3 -m http.server 4173 --bind 127.0.0.1 &)
 *   node ../tools/record_ui_motion.mjs <output-directory>
 */

const OUT = process.argv[2] ?? '.';
const URL = 'http://127.0.0.1:4173/index.html';

const browser = await chromium.launch({
  executablePath: '/opt/pw-browsers/chromium-1194/chrome-linux/chrome',
});

const context = await browser.newContext({
  viewport: { width: 1180, height: 720 },
  deviceScaleFactor: 1,
  recordVideo: { dir: OUT, size: { width: 1180, height: 720 } },
});

const page = await context.newPage();
await page.goto(URL, { waitUntil: 'networkidle' });
await page.waitForSelector('.gn-wavetable', { timeout: 10000 });
await page.waitForTimeout(600);

// Drag the position knob of oscillator 1 back and forth. This is a REAL
// interaction - the waveform moves because the parameter moves, not because
// anything is animating for the camera.
const posKnob = await page.$('.gn-panel:has(.gn-panel__title:text-is("Osc 1")) .gn-knob__dial');

if (posKnob) {
  const box = await posKnob.boundingBox();
  const cx = box.x + box.width / 2;
  const cy = box.y + box.height / 2;

  await page.mouse.move(cx, cy);
  await page.mouse.down();

  // Up (raising the value), then down, then up again, in small steps so the
  // display has frames to interpolate across.
  const sweep = async (from, to, steps) => {
    for (let i = 0; i <= steps; i += 1) {
      const t = i / steps;
      await page.mouse.move(cx, cy + (from + (to - from) * t));
      await page.waitForTimeout(16);
    }
  };

  await sweep(0, -170, 60);
  await sweep(-170, -20, 45);
  await sweep(-20, -150, 45);
  await page.mouse.up();
  await page.waitForTimeout(400);
}

// Sweep the warp amount, so the waveform visibly deforms.
const warpSelects = await page.$$('.gn-panel:has(.gn-panel__title:text-is("Osc 1")) select');
if (warpSelects.length > 2) {
  await warpSelects[2].selectOption({ label: 'Asym' });
  await page.waitForTimeout(300);
}

const knobs = await page.$$('.gn-panel:has(.gn-panel__title:text-is("Osc 1")) .gn-knob__dial');
// The warp amount knob follows the warp dropdown in the first row.
const amtKnob = knobs[5];

if (amtKnob) {
  const box = await amtKnob.boundingBox();
  const cx = box.x + box.width / 2;
  const cy = box.y + box.height / 2;

  await page.mouse.move(cx, cy);
  await page.mouse.down();

  for (let i = 0; i <= 50; i += 1) {
    await page.mouse.move(cx, cy - i * 2.2);
    await page.waitForTimeout(16);
  }
  for (let i = 50; i >= 0; i -= 1) {
    await page.mouse.move(cx, cy - i * 2.2);
    await page.waitForTimeout(16);
  }

  await page.mouse.up();
  await page.waitForTimeout(500);
}

// A different table, to show the stack change shape entirely.
const tableSelect = (await page.$$('.gn-panel:has(.gn-panel__title:text-is("Osc 1")) select'))[0];
if (tableSelect) {
  await tableSelect.selectOption({ label: 'Growl Morph' });
  await page.waitForTimeout(400);

  const again = await page.$('.gn-panel:has(.gn-panel__title:text-is("Osc 1")) .gn-knob__dial');
  if (again) {
    const box = await again.boundingBox();
    const cx = box.x + box.width / 2;
    const cy = box.y + box.height / 2;

    await page.mouse.move(cx, cy);
    await page.mouse.down();
    for (let i = 0; i <= 70; i += 1) {
      await page.mouse.move(cx, cy - i * 2.4);
      await page.waitForTimeout(16);
    }
    await page.mouse.up();
    await page.waitForTimeout(600);
  }
}

await context.close();
await browser.close();
console.log('video written to', OUT);
