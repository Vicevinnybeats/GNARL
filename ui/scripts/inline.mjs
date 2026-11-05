// Folds dist/app.js and dist/app.css into dist/gnarl-ui.html, the plugin's
// page. Then, if the engine has been built for the browser (wasm/build.sh),
// the same page with the engine inside: dist/gnarl-web.html, the mobile
// version (docs/design/phase2-09-mobile.md).
import { existsSync, readdirSync, readFileSync, writeFileSync } from 'node:fs';

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
// The large data goes in inert blocks at the end of the page, not in the
// app: as JavaScript string literals it was parsed as code before the panel
// could show (src/data.ts reads it on first use). Base64 and JSON are ASCII,
// and neither can contain "</script": base64 has no "<", and JSON.stringify
// leaves "<" as is, so it is escaped below.
const inert = (id, text) => `<script type="text/plain" id="${id}">${text.replace(/</g, '\\u003c')}</script>`;

// The built-in patches, each wavetable stored once: 45 patches repeat the
// same few tables, 12 MB of patch text but 4.6 MB of distinct strings.
const presetDir = new URL('../../presets/', import.meta.url);
const strings = [];
const index = new Map();
const patches = {};
for (const file of readdirSync(presetDir).filter((f) => f.endsWith('.vital')).sort()) {
  const text = readFileSync(new URL(file, presetDir), 'utf8');
  const packed = text.replace(/"([A-Za-z0-9+/=]{1000,})"/g, (_, long) => {
    if (!index.has(long)) index.set(long, strings.push(long) - 1);
    return `"@gnarl:${index.get(long)}"`;
  });
  // Unpacked as data.ts does, it must be the file, byte for byte.
  if (packed.replace(/"@gnarl:(\d+)"/g, (_, i) => `"${strings[Number(i)]}"`) !== text) {
    throw new Error(`inline.mjs: ${file} does not survive packing`);
  }
  patches[file.replace(/\.vital$/, '')] = packed;
}
const presetJson = JSON.stringify({ strings, patches });
// The PLUGIN's page names its data instead of carrying it: an empty block
// with a data-src the plugin serves (web_panel.cpp getResource), fetched once
// the panel is up (src/data.ts loadData). Inside the page it cost the window
// half its opening time. The phone page (below) carries it: it is one file.
const named = (id, src) => `<script type="text/plain" id="${id}" data-src="${src}"></script>`;
const plugin = html.replace('</body>', () => `${named('gnarl-presets', 'gnarl-presets.json')}</body>`);
html = html.replace('</body>', () => `${inert('gnarl-presets', presetJson)}</body>`);
writeFileSync(new URL('gnarl-presets.json', dist), presetJson);

const wasm = new URL('../../wasm/build/gnarl.wasm', import.meta.url);
// The plugin's page carries the engine too, for MATCH A SOUND (src/match/
// match.ts), which renders candidates with it in Web Workers; the plugin
// itself still plays through its own engine (hasWebEngine() is false where
// window.__JUCE__ exists).
const engine = existsSync(wasm) ? readFileSync(wasm).toString('base64') : null;
const withEngine = (page) => (engine ? page.replace('</body>', () => `${inert('gnarl-wasm', engine)}</body>`) : page);
const pluginPage = engine ? plugin.replace('</body>', () => `${named('gnarl-wasm', 'gnarl-engine.b64')}</body>`) : plugin;
writeFileSync(new URL('gnarl-ui.html', dist), pluginPage);
// Always written - a lone newline without an engine, as an empty file is no
// C++ array - since CMake embeds it either way. Only a page with the engine
// names it.
writeFileSync(new URL('gnarl-engine.b64', dist), engine ?? '\n');
console.log(`dist/gnarl-ui.html ${(pluginPage.length / 1024).toFixed(1)} kB, its data beside it: ` +
  `gnarl-presets.json ${(presetJson.length / 1024).toFixed(1)} kB${engine ? `, gnarl-engine.b64 ${(engine.length / 1024).toFixed(1)} kB (for MATCH)` : ''}; ` +
  `${Object.keys(patches).length} patches, ${strings.length} distinct tables`);

if (engine) {
  const web = withEngine(html.replace(/<title>[^<]*<\/title>/, '<title>GNARL</title>'));
  if (!web.includes('id="gnarl-wasm"')) throw new Error('inline.mjs: the engine was not embedded');
  writeFileSync(new URL('gnarl-web.html', dist), web);
  console.log(`dist/gnarl-web.html ${(web.length / 1024).toFixed(1)} kB (with the engine)`);
} else {
  console.log('dist/gnarl-web.html skipped: no wasm/build/gnarl.wasm (wasm/build.sh)');
}
