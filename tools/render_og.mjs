/*  Renders the social card.
 *
 *  THE CARD IS A SEPARATE DRAWING, NOT A SCREENSHOT OF THE PAGE. A crop of
 *  the hero would be mostly empty space with small type, because the page is
 *  composed for a 16:9 window read at arm's length and a social card is
 *  reproduced at a few hundred pixels wide inside somebody else's timeline.
 *  What has to survive that is the wordmark and one line, large.
 *
 *  It is a PNG, NOT a WebP, and not because of size: the page's artwork is
 *  WebP for exactly the reason given in CLAUDE.md, but several of the
 *  crawlers that read these tags do not decode WebP and show nothing at all
 *  rather than falling back. A card nobody can see is worse than no card.
 *
 *  The palette is duplicated from styles.css on purpose - this runs in a
 *  blank page with no stylesheet, and importing the site's CSS would pull in
 *  the whole layout to draw two lines of text.
 */
import { chromium } from 'playwright';
import { mkdirSync } from 'node:fs';
import { dirname, resolve } from 'node:path';

const out = resolve(process.argv[2] ?? 'site/public/og.png');

// Exactly the size the crawlers want: 1200x630 is what Open Graph, Twitter
// and Slack all crop to, so drawing at it means no crop happens at all.
const W = 1200;
const H = 630;

const html = `<!doctype html><html><head><meta charset="utf-8"><style>
  * { box-sizing: border-box; margin: 0; }
  body {
    width: ${W}px; height: ${H}px; overflow: hidden;
    background:
      radial-gradient(120% 95% at 16% 0%, #2a1360 0%, transparent 58%),
      radial-gradient(100% 85% at 90% 26%, #0d2a58 0%, transparent 60%),
      #05040f;
    color: #efeaff;
    font-family: ui-monospace, 'DejaVu Sans Mono', monospace;
    letter-spacing: 0.04em;
    padding: 72px 84px;
    display: flex; flex-direction: column; justify-content: space-between;
    position: relative;
  }
  /* The one cyan rule, top left, same role it has in the interface: it marks
     what is live. */
  .rule { width: 132px; height: 2px; background: #64e6ff; box-shadow: 0 0 18px rgba(100,230,255,0.75); }
  .eyebrow { margin-top: 26px; font-size: 19px; color: #8878b4; }
  .mark { font-size: 176px; line-height: 0.92; font-weight: 700; letter-spacing: 0.02em; }
  .lede { margin-top: 20px; font-size: 27px; line-height: 1.45; color: #b9aede; max-width: 20ch; }
  .foot { display: flex; gap: 34px; font-size: 17px; color: #8878b4; }
  .foot b { color: #64e6ff; font-weight: 400; }
  /* The waveform is drawn rather than placed: one polyline, the same shape
     the favicon carries, so the two read as the same instrument. */
  svg { position: absolute; right: 64px; bottom: 96px; }
</style></head><body>
  <div>
    <div class="rule"></div>
    <p class="eyebrow">VST3 &middot; AU &middot; STANDALONE</p>
  </div>
  <div>
    <h1 class="mark">GNARL</h1>
    <p class="lede">A wavetable synthesizer built for one sound.</p>
  </div>
  <div class="foot">
    <span><b>150</b> factory presets</span>
    <span><b>14</b> effects, any order</span>
    <span><b>&minus;65.6</b> dBc aliasing</span>
  </div>
  <svg width="420" height="200" viewBox="0 0 420 200" fill="none">
    <path d="M0 150 L52 44 L104 168 L156 30 L208 176 L260 52 L312 150 L364 88 L420 130"
          stroke="#a678ff" stroke-width="3" opacity="0.5"/>
    <path d="M0 150 L52 44 L104 168 L156 30 L208 176 L260 52 L312 150 L364 88 L420 130"
          stroke="#64e6ff" stroke-width="1.6"/>
  </svg>
</body></html>`;

const browser = await chromium.launch({
  // Same pinned binary the other screenshot tools use; the bundled headless
  // shell is not installed in this container.
  executablePath:
    process.env.GNARL_CHROMIUM ?? '/opt/pw-browsers/chromium-1194/chrome-linux/chrome',
});
const page = await browser.newPage({ viewport: { width: W, height: H }, deviceScaleFactor: 1 });
await page.setContent(html, { waitUntil: 'load' });
// Fonts settle a frame after load; without this the type can be measured with
// a fallback and photographed with the real one mid-swap.
await page.evaluate(() => document.fonts.ready);
mkdirSync(dirname(out), { recursive: true });
await page.screenshot({ path: out, type: 'png' });
await browser.close();
console.log('wrote', out, `${W}x${H}`);
