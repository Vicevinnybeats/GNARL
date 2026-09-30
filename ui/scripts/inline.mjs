// Folds dist/app.js and dist/app.css into dist/gnarl-ui.html.
import { readFileSync, writeFileSync } from 'node:fs';

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
