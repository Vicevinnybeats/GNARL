/**
 * Screenshots the marketing site at desktop and phone sizes, at each of its
 * scroll positions.
 *
 * The scroll IS the design here - the panel's zoom, its parking to the side
 * and the text decoding are all functions of scroll progress - so a picture
 * of the top of the page says almost nothing. This drives the page to each
 * section and waits for the scene's easing AND the decode to finish before
 * shooting; the first version shot immediately and photographed four cards
 * of static, which looked like a broken font rather than a working effect.
 *
 *   cd site && npm run build
 *   (cd dist && python3 -m http.server 4174 --bind 127.0.0.1 &)
 *   node ../tools/screenshot_site.mjs <output-directory>
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

/*  FIVE PAGES NOW, each shot at the top and again at the foot. The scroll
    is what the scenes are driven by, so a picture of only the top of a page
    says nothing about the state it ends in - which is exactly where the
    dissolve has finished burning and where the interface resolves. */
const PAGES = ['index', 'engine', 'presets', 'fx', 'download'];
const STOPS = [0, 1];

const errors = [];

for (const device of DEVICES) {
  const page = await browser.newPage({ ...device, deviceScaleFactor: 2 });

  page.on('console', (m) => { if (m.type() === 'error') errors.push(`${device.name}: ${m.text()}`); });
  page.on('pageerror', (e) => errors.push(`${device.name}: ${e}`));

  await page.goto(URL, { waitUntil: 'networkidle' });
  // The pointer parked in a corner leaves the lattice unlit; put it where a
  // reader's would be. Same lesson as the plugin's hover-glow shot.
  await page.mouse.move(device.viewport.width * 0.62, device.viewport.height * 0.45);
  await page.waitForTimeout(1800);

  for (const name of PAGES) {
    await page.goto(URL.replace('index.html', `${name}.html`), { waitUntil: 'networkidle' });
    await page.mouse.move(device.viewport.width * 0.62, device.viewport.height * 0.45);
    await page.waitForTimeout(1800);

    for (const stop of STOPS) {
      /*  Driven by absolute scroll rather than by scrolling an element into
          view: these pages are short enough that "the bottom" is the state
          worth photographing, and it is the one an element query cannot
          name. Read back in a SEPARATE evaluate, because reading scrollY in
          the same synchronous block reports the value that was ASKED for,
          not the one the browser settled on - the first version of this
          check passed every time while the page sat nowhere near it. */
      let landed = -1;

      for (let attempt = 0; attempt < 8; attempt += 1) {
        const target = await page.evaluate((fraction) => {
          const max = document.body.scrollHeight - window.innerHeight;
          const top = Math.round(max * fraction);
          document.documentElement.style.scrollBehavior = 'auto';
          window.scrollTo(0, top);
          return top;
        }, stop);

        await page.waitForTimeout(300);
        landed = await page.evaluate((top) => top - Math.round(window.scrollY), target);

        if (landed === 0) break;
      }

      if (landed !== 0) {
        errors.push(`${device.name}: could not scroll ${name} to ${stop} (off by ${landed}px)`);
      }

      /*  Long enough for the camera's easing AND the longest decoding line.
          The dissolve needs it most: its threshold is eased like everything
          else, so a shot taken early is a picture of a half-finished burn
          that looks like a bug in the shader. */
      await page.waitForTimeout(2600);

      const label = `${device.name}-${name}-${stop === 0 ? 'top' : 'end'}`;
      await page.screenshot({ path: `${OUT}/${label}.png` });
      console.log(`  ${label}.png`);
    }
  }

  await page.close();
}

await browser.close();

if (errors.length > 0) {
  console.log('\nCONSOLE ERRORS:');
  for (const e of errors) console.log('  ' + e);
  process.exit(1);
}

console.log('\nno console errors');
