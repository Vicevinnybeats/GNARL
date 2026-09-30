/*  Copies the built marketing site into dist/ for Vercel.
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
import { cpSync, existsSync, mkdirSync, rmSync, readdirSync, statSync } from 'node:fs';
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
for (const required of ['index.html', 'checkout.html', 'app/index.html']) {
  if (!existsSync(join(out, required))) {
    console.error(`assemble_deploy: missing ${required}`);
    process.exit(1);
  }
}

console.log('assemble_deploy: site pages present');
