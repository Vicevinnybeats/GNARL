/**
 * Screenshots the boot screen.
 *
 * It needs its own script because it is over in well under a second on a
 * fast local connection - the thing worth photographing is the state it is
 * in while it WAITS, which barely exists unless the network is slowed on
 * purpose. CDP throttling does that, so the picture is of the screen a
 * visitor on a phone connection actually sees rather than of the last frame
 * before it dismissed itself.
 *
 *   cd site && npm run build
 *   (cd dist && python3 -m http.server 4174 --bind 127.0.0.1 &)
 *   node ../tools/screenshot_loader.mjs <output-directory>
 */
import { chromium } from 'playwright';

const OUT = process.argv[2] ?? '.';
const URL = process.env.GNARL_SITE_URL ?? 'http://127.0.0.1:4174/index.html';

const browser = await chromium.launch({
  executablePath:
    process.env.GNARL_CHROMIUM ?? '/opt/pw-browsers/chromium-1194/chrome-linux/chrome',
  args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
});

const DEVICES = [
  { name: 'desktop', viewport: { width: 1440, height: 900 } },
  { name: 'mobile', viewport: { width: 390, height: 844 }, isMobile: true, hasTouch: true },
];

const errors = [];

for (const device of DEVICES) {
  const context = await browser.newContext({ ...device, deviceScaleFactor: 2 });
  const page = await context.newPage();

  page.on('pageerror', (e) => errors.push(`${device.name}: ${e}`));
  page.on('console', (m) => { if (m.type() === 'error') errors.push(`${device.name}: ${m.text()}`); });

  // Slow enough that the dial has something to report.
  const cdp = await context.newCDPSession(page);
  await cdp.send('Network.emulateNetworkConditions', {
    offline: false,
    downloadThroughput: (220 * 1024) / 8,
    uploadThroughput: (220 * 1024) / 8,
    latency: 300,
  });

  void page.goto(URL).catch(() => {});

  // Mid-load: the dial part-filled, the readout counting.
  await page.waitForTimeout(3200);
  await page.mouse.move(device.viewport.width * 0.62, device.viewport.height * 0.42);
  await page.waitForTimeout(600);
  await page.screenshot({ path: `${OUT}/${device.name}-boot-loading.png` });
  console.log(`  ${device.name}-boot-loading.png`);

  // Ready: violet, prompting.
  await page
    .waitForFunction(() => window.__gnarlJourney !== undefined, null, { timeout: 120000 })
    .catch(() => errors.push(`${device.name}: never finished loading`));
  await page.waitForTimeout(1600);
  await page.screenshot({ path: `${OUT}/${device.name}-boot-ready.png` });
  console.log(`  ${device.name}-boot-ready.png`);

  /*  THE ZOOM THROUGH, in three frames. One picture of a transition says
      nothing about whether it is a move or a cut - what matters is that the
      dial is still recognisably leaving while the page is already arriving,
      and that only shows in a sequence.

      The waits are wall-clock while the build is frame-driven with a clamped
      step, so under software GL the build stretches and these land earlier
      in it than they would on real hardware. That is fine here - the
      question is what the frames look like, not exactly when. */
  await page.mouse.click(device.viewport.width / 2, device.viewport.height / 2);

  for (const [index, wait] of [420, 500, 600, 700].entries()) {
    await page.waitForTimeout(wait);
    const label = `${device.name}-zoom-${index + 1}`;
    await page.screenshot({ path: `${OUT}/${label}.png` });
    console.log(`  ${label}.png`);
  }

  // And where it lands.
  await page.waitForTimeout(4500);
  await page.screenshot({ path: `${OUT}/${device.name}-zoom-landed.png` });
  console.log(`  ${device.name}-zoom-landed.png`);

  await context.close();
}

await browser.close();

if (errors.length > 0) {
  console.log('\nCONSOLE ERRORS:');
  for (const e of errors) console.log('  ' + e);
  process.exit(1);
}

console.log('\nno console errors');
