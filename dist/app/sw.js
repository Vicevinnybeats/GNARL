/*  The phone version's service worker (docs/design/phase2-09-mobile.md).
 *
 *  NETWORK FIRST, cache as the fallback: online, every visit gets the
 *  current page from the server, so a new release is never hidden behind a
 *  stale copy (the trap the retired app's cache-first worker fell into -
 *  see its history on custom-engine-archive); offline, the last copy plays.
 *
 *  It also replaces the retired app's worker, which had the same URL: the
 *  browser byte-compares sw.js, installs this one, and the activate step
 *  below deletes every cache that is not this one's.
 */
const CACHE = 'gnarl-app-v1';
const SHELL = ['./', 'manifest.webmanifest', 'icons/icon-192.png', 'icons/icon-512.png'];

self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(CACHE).then((cache) => cache.addAll(SHELL.map((url) => new Request(url, { cache: 'reload' })))),
  );
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    (async () => {
      for (const name of await caches.keys()) if (name !== CACHE) await caches.delete(name);
      await self.clients.claim();
    })(),
  );
});

self.addEventListener('fetch', (event) => {
  const request = event.request;
  if (request.method !== 'GET' || new URL(request.url).origin !== self.location.origin) return;
  event.respondWith(
    (async () => {
      try {
        const response = await fetch(request, { cache: 'no-store' });
        if (response.ok) {
          const cache = await caches.open(CACHE);
          await cache.put(request.mode === 'navigate' ? './' : request, response.clone());
        }
        return response;
      } catch {
        const cached = await caches.match(request.mode === 'navigate' ? './' : request);
        return cached ?? Response.error();
      }
    })(),
  );
});
