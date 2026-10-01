/*  Copies the built marketing site into dist/ for Vercel, and puts the
 *  phone version at /app (below).
 *
 *  THIS USED TO ASSEMBLE TWO THINGS. The site was at /, and the plugin's
 *  React interface was published as an installable web app at /app -
 *  under a path rather than a subdomain, because a service worker may only
 *  claim a scope at or below its own path.
 *
 *  That interface is gone: GNARL's UI is now Vital's OpenGL one, inside the
 *  plugin, and there is no web build of it to publish. What remains at /app
 *  is a tombstone page shipped from site/public/app/, because the demo was
 *  installed to at least one home screen and deleting the path outright
 *  turns a real icon into an unexplained 404.
 *
 *  The step is kept rather than folded into `outputDirectory: site/dist`
 *  so the checks below still run. A deploy that silently produces an empty
 *  directory looks exactly like a deploy that worked.
 */
import { cpSync, existsSync, mkdirSync, rmSync, readdirSync, statSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const out = join(root, 'dist');
const siteDist = join(root, 'site', 'dist');

if (!existsSync(siteDist)) {
  console.error(`assemble_deploy: site was not built (${siteDist} missing)`);
  process.exit(1);
}

rmSync(out, { recursive: true, force: true });
mkdirSync(out, { recursive: true });
cpSync(siteDist, out, { recursive: true });

function count(dir) {
  let n = 0;
  for (const e of readdirSync(dir)) {
    const p = join(dir, e);
    n += statSync(p).isDirectory() ? count(p) : 1;
  }
  return n;
}

console.log(`assemble_deploy: ${count(out)} files -> dist/`);

//  The pages the site is actually made of. An empty or half-copied dist
//  deploys green and serves nothing, which is the failure worth catching.
for (const required of ['index.html', 'checkout.html', 'thanks.html', 'app/index.html', 'app/manifest.webmanifest', 'app/sw.js']) {
  if (!existsSync(join(out, required))) {
    console.error(`assemble_deploy: missing ${required}`);
    process.exit(1);
  }
}

console.log('assemble_deploy: site pages present');

//  THE PHONE VERSION AT /app (docs/design/phase2-09-mobile.md, docs/release.md).
//  It is built in CI - the engine needs Emscripten, which this build does not
//  have - and published as gnarl-web.html on the latest GitHub release, so
//  the deploy fetches it from there. No release yet, or GitHub unreachable:
//  the tombstone page from site/public/app stays, and the log says so. A page
//  that does not carry the engine is refused rather than served.
const PHONE_URL = 'https://github.com/Vicevinnybeats/GNARL/releases/latest/download/gnarl-web.html';
try {
  const response = await fetch(PHONE_URL, { redirect: 'follow' });
  const page = response.ok ? await response.text() : '';
  if (page.includes('__GNARL_WASM__') && page.length > 500_000) {
    //  What makes it an installable app (site/public/app: the manifest, the
    //  icons, the worker): linked here rather than in ui/, because the same
    //  page is also embedded in the plugin and published elsewhere, where
    //  none of these files exist.
    const install = [
      '<link rel="manifest" href="manifest.webmanifest">',
      '<link rel="icon" type="image/png" sizes="192x192" href="icons/icon-192.png">',
      '<link rel="apple-touch-icon" href="icons/icon-192.png">',
      "<script>if ('serviceWorker' in navigator) addEventListener('load', () => navigator.serviceWorker.register('sw.js').catch(() => {}));</script>",
    ].join('');
    if (!page.includes('</head>')) throw new Error('assemble_deploy: the phone page has no </head>');
    writeFileSync(join(out, 'app', 'index.html'), page.replace('</head>', () => `${install}</head>`));
    console.log(`assemble_deploy: phone version at /app (${(page.length / 1024).toFixed(0)} kB)`);
  } else {
    console.warn(`assemble_deploy: WARNING no phone version (${response.status}); /app stays the tombstone`);
  }
} catch (error) {
  console.warn(`assemble_deploy: WARNING phone version not fetched (${error}); /app stays the tombstone`);
}
