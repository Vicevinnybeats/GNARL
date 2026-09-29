import { createHash } from 'node:crypto';
import { readFileSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { defineConfig, type Plugin } from 'vite';
import react from '@vitejs/plugin-react';

/**
 * Stamps the service worker's cache name with the build's own content.
 *
 * THE VERSION HAS TO CHANGE WHEN THE BUILD DOES, or an installed app serves
 * the old interface from its cache forever - the worker is cache-first, which
 * is right, and is only safe because the cache NAME carries the version.
 *
 * A number somebody has to remember to bump is a number that does not get
 * bumped, so it is a hash of the built bundle instead: identical output keeps
 * the cache (no pointless re-download), and any change to a byte invalidates
 * it. `writeBundle` rather than `transform` because public/ is copied
 * verbatim and never passes through the transform pipeline at all.
 */
function stampServiceWorker(): Plugin {
  return {
    name: 'gnarl-stamp-service-worker',
    apply: 'build',
    writeBundle(options, bundle) {
      const outDir = options.dir ?? 'dist';
      const swPath = resolve(outDir, 'sw.js');

      const hash = createHash('sha256');

      //  Sorted, because a rollup bundle's key order is not guaranteed and a
      //  version that changed on rebuild-with-no-changes would throw away a
      //  working cache for nothing.
      for (const name of Object.keys(bundle).sort()) {
        const chunk = bundle[name];
        hash.update(name);
        hash.update('code' in chunk ? chunk.code : String(chunk.source));
      }

      const id = hash.digest('hex').slice(0, 12);

      try {
        const source = readFileSync(swPath, 'utf8');
        writeFileSync(swPath, source.replace('__GNARL_BUILD_ID__', id));
      } catch {
        //  No service worker in this build; nothing to stamp.
      }

      /*  AND THE SAME ID ON THE ASSET URLS, which is the difference between
          an app that can be updated and one that cannot.

          The output filenames are deliberately stable and unhashed, because
          the plugin's C++ resource provider resolves them by name at compile
          time. On the WEB that same property is a trap: a URL that never
          changes its name was being served with `immutable`, so browsers
          were told the bytes behind `assets/index.js` would never change and
          correctly stopped asking. The header is fixed, but a response
          already cached as immutable is never revalidated - the browser does
          not ask, so there is nothing for a new header to answer.

          A query string makes it a DIFFERENT URL, which is a cache miss by
          construction, and it does not rename the file - so CMake's
          GNARL_UI_FILES list and the binary-data mangling in
          WebUIResourceProvider are both untouched, and the plugin build is
          unaffected. The query is only in the HTML the web deploy serves.

          This also repairs the service worker, which was defeated by the
          same header: a new worker's `cache.add('./assets/index.js')` is an
          ordinary fetch, so it was filling its brand-new versioned cache
          with the year-old immutable copy. Versioning the cache name could
          never have worked while the contents came from the HTTP cache. */
      const htmlPath = resolve(outDir, 'index.html');

      try {
        const html = readFileSync(htmlPath, 'utf8');

        writeFileSync(
          htmlPath,
          html.replace(
            /(src|href)="(\.?\/?assets\/[^"?]+)"/g,
            (_match, attr: string, url: string) => `${attr}="${url}?v=${id}"`,
          ),
        );
      } catch {
        //  No HTML in this build.
      }
    },
  };
}

/**
 * The bundle is embedded into the plugin binary as JUCE BinaryData, so output
 * filenames must be STABLE and UNHASHED — the C++ resource provider resolves
 * them by name at compile time and cannot follow a content hash.
 *
 * Keep this list in sync with GNARL_UI_FILES in cmake/WebUI.cmake. That list
 * also carries ui/public/assets/backdrop.png, which Vite copies through
 * untouched - anything added to public/ that the plugin must serve has to be
 * named there too, or it is built and then never embedded.
 */
export default defineConfig({
  plugins: [react(), stampServiceWorker()],
  base: './',
  build: {
    outDir: 'dist',
    // One chunk: the C++ resource provider serves a fixed file list, so code
    // splitting would produce files nothing can resolve.
    codeSplitting: false,
    emptyOutDir: true,
    assetsInlineLimit: 0,
    // No sourcemaps in the shipped binary; they would double its size.
    sourcemap: process.env.NODE_ENV !== 'production',
    target: 'es2022',
    rollupOptions: {
      output: {
        entryFileNames: 'assets/index.js',
        chunkFileNames: 'assets/index.js',
        assetFileNames: 'assets/index.[ext]',
        manualChunks: undefined,
      },
    },
  },
  server: {
    port: 5173,
    strictPort: true,
  },
});
