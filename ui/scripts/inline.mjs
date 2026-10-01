// Folds dist/app.js and dist/app.css into dist/gnarl-ui.html, the plugin's
// page. Then, if the engine has been built for the browser (wasm/build.sh),
// the same page with the engine inside: dist/gnarl-web.html, the mobile
// version (docs/design/phase2-09-mobile.md).
import { existsSync, readFileSync, writeFileSync } from 'node:fs';

const dist = new URL('../dist/', import.meta.url);
let html = readFileSync(new URL('index.html', dist), 'utf8');
const js = readFileSync(new URL('app.js', dist), 'utf8');
const css = readFileSync(new URL('app.css', dist), 'utf8');

html = html
  .replace(/<script[^>]*src="\.\/app\.js"[^>]*><\/script>/, () => `<script type="module">${js}</script>`)
  .replace(/<link[^>]*href="\.\/app\.css"[^>]*>/, () => `<style>${css}</style>`);

if (html.includes('app.js') || html.includes('app.css')) {
  throw new Error('inline.mjs: an asset reference survived; the single file would be broken');
}
// See vite.config.ts: the page must survive being decoded as any charset.
const nonAscii = [...html].findIndex((c) => c.charCodeAt(0) > 0x7e);
if (nonAscii >= 0) {
  throw new Error(`inline.mjs: non-ASCII text survived near "${html.slice(nonAscii - 40, nonAscii + 10)}"`);
}
writeFileSync(new URL('gnarl-ui.html', dist), html);
console.log(`dist/gnarl-ui.html ${(html.length / 1024).toFixed(1)} kB`);

const wasm = new URL('../../wasm/build/gnarl.wasm', import.meta.url);
if (existsSync(wasm)) {
  // Base64 is ASCII, so the page stays ASCII. The engine is read before the
  // app's module script runs: a classic script, ahead of it.
  const engine = readFileSync(wasm).toString('base64');
  const web = html
    .replace(/<title>[^<]*<\/title>/, '<title>GNARL</title>')
    .replace('<script type="module">', () => `<script>window.__GNARL_WASM__="${engine}";</script><script type="module">`);
  if (!web.includes('__GNARL_WASM__')) throw new Error('inline.mjs: the engine was not embedded');
  writeFileSync(new URL('gnarl-web.html', dist), web);
  console.log(`dist/gnarl-web.html ${(web.length / 1024).toFixed(1)} kB (with the engine)`);
} else {
  console.log('dist/gnarl-web.html skipped: no wasm/build/gnarl.wasm (wasm/build.sh)');
}
