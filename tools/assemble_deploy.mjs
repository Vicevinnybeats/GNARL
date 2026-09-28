/*  Puts the marketing site and the installable app in one deployment.
 *
 *  ONE ORIGIN, TWO THINGS. The site is `site/`, the app is the plugin's own
 *  React interface out of `ui/`, and they are separate builds with separate
 *  dependencies. Deploying them as one origin means the app can be linked to
 *  from the page that sells it, and - the part that actually matters - a
 *  service worker may only claim a scope at or below its own path, so an app
 *  at /app can cache itself and nothing else on the domain.
 *
 *  THE SERVICE WORKER'S SCOPE IS WHY /app AND NOT A SUBDOMAIN. A subdomain
 *  would need its own certificate and its own Vercel project, and the site
 *  could not then link to an installable thing on the same origin.
 */
import { cpSync, existsSync, mkdirSync, rmSync, readdirSync, statSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const out = join(root, 'dist');

const siteDist = join(root, 'site', 'dist');
const uiDist = join(root, 'ui', 'dist');

for (const [label, path] of [['site', siteDist], ['ui', uiDist]]) {
  if (!existsSync(path)) {
    console.error(`assemble_deploy: ${label} was not built (${path} missing)`);
    process.exit(1);
  }
}

rmSync(out, { recursive: true, force: true });
mkdirSync(out, { recursive: true });

//  The site at the root.
cpSync(siteDist, out, { recursive: true });

//  The app under /app, where its service worker's scope can reach it.
cpSync(uiDist, join(out, 'app'), { recursive: true });

function count(dir) {
  let n = 0;
  for (const e of readdirSync(dir)) {
    const p = join(dir, e);
    n += statSync(p).isDirectory() ? count(p) : 1;
  }
  return n;
}

console.log(`assemble_deploy: ${count(out)} files -> dist/ (site at /, app at /app)`);

//  A missing worker or manifest means the app installs as a bookmark rather
//  than as an app, which looks like success until somebody tries it offline.
for (const required of ['app/index.html', 'app/sw.js', 'app/manifest.webmanifest']) {
  if (!existsSync(join(out, required))) {
    console.error(`assemble_deploy: missing ${required}`);
    process.exit(1);
  }
}

console.log('assemble_deploy: manifest and service worker present');
