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

const SECTIONS = ['#top', '#engine', '#fx', '#download'];

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

  for (const section of SECTIONS) {
    /*  SCROLL, THEN CHECK WHERE IT LANDED. Under Playwright's phone
        emulation `innerHeight` is briefly reported as four times the
        viewport, and the browser clamps scrollTop to the document height
        minus that - so a single scrollIntoView silently stopped a section
        and a half short, and the shot labelled `fx` was a photograph of the
        engine card. Nothing to do with the site; real phones are fine. The
        loop just refuses to shoot a position it did not reach. */
    let landed = -1;

    for (let attempt = 0; attempt < 8; attempt += 1) {
      const target = await page.evaluate((selector) => {
        const element = document.querySelector(selector);
        if (!element) return -1;

        const top = Math.round(element.getBoundingClientRect().top + window.scrollY);
        document.documentElement.style.scrollBehavior = 'auto';
        window.scrollTo(0, top);
        return top;
      }, section);

      /*  Read back in a SEPARATE evaluate, after a wait. Reading scrollY in
          the same synchronous block reports the value that was asked for,
          not the one the browser settled on - so the first version of this
          check passed every time while the page sat a section and a half
          short of where it claimed to be. */
      await page.waitForTimeout(300);

      landed = await page.evaluate((top) => top - Math.round(window.scrollY), target);

      if (landed === 0) break;
    }

    if (landed !== 0) {
      errors.push(`${device.name}: could not scroll to ${section} (off by ${landed}px)`);
    }

    /*  Long enough for BOTH: the scene eases the scroll at 0.09 per frame,
        which needs about a second to settle, and the longest decoding line
        runs a character every 1.4 frames on top of that. */
    await page.waitForTimeout(2600);

    const name = `${device.name}-${section.slice(1)}`;
    await page.screenshot({ path: `${OUT}/${name}.png` });
    console.log(`  ${name}.png`);
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
