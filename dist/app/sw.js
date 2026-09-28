/*  The service worker.
 *
 *  WHAT IT IS FOR HERE IS OFFLINE, not speed. A synthesizer you opened on a
 *  train has to still open in a tunnel, and an installed app that shows the
 *  browser's dinosaur is not an app. Everything this interface needs is
 *  static and embedded - no API, no database - so the whole thing can be
 *  cached and served from the cache.
 *
 *  CACHE-FIRST, with the network as the fallback, and NOT the other way
 *  round. The bundle is versioned by the cache name below, so a cached copy
 *  is never stale in a way that matters: a new build changes the name, the
 *  old cache is deleted on activate, and the next load fetches everything
 *  fresh. Network-first would pay a round trip on every load to learn what
 *  the version already told us.
 *
 *  THE VERSION MUST CHANGE WHEN THE BUILD DOES or people keep the old app
 *  forever. It is written by the build, not by hand - a number somebody has
 *  to remember to bump is a number that does not get bumped.
 */
const VERSION = '3e9d05f48de6';
const CACHE = `gnarl-${VERSION}`;

/*  The shell: what has to be present for the app to start at all. Everything
    else is cached as it is asked for, because the wavetable and spectrum data
    is large and most of it is not needed on a first paint. */
const SHELL = [
  './',
  './index.html',
  './manifest.webmanifest',
  './assets/index.js',
  './assets/index.css',
  './icons/icon-192.png',
  './icons/icon-512.png',
];

self.addEventListener('install', (event) => {
  event.waitUntil(
    (async () => {
      const cache = await caches.open(CACHE);
      /*  addAll is atomic - one 404 throws the whole install away, and a
          half-installed worker that serves a broken shell is worse than no
          worker. Each file is added separately so a missing optional asset
          cannot stop the app being installable. */
      await Promise.all(
        SHELL.map((url) => cache.add(url).catch(() => undefined)),
      );
      /*  Take over immediately rather than waiting for every tab to close.
          Paired with clients.claim() below: without both, a fresh install
          sits idle until the user quits the app, which on a phone is rare. */
      await self.skipWaiting();
    })(),
  );
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    (async () => {
      const names = await caches.keys();
      await Promise.all(
        names.filter((n) => n.startsWith('gnarl-') && n !== CACHE).map((n) => caches.delete(n)),
      );
      await self.clients.claim();
    })(),
  );
});

self.addEventListener('fetch', (event) => {
  const { request } = event;

  //  Only GET, and only our own origin. A POST is not cacheable and another
  //  origin's response is not ours to keep.
  if (request.method !== 'GET') return;
  if (new URL(request.url).origin !== self.location.origin) return;

  event.respondWith(
    (async () => {
      const cached = await caches.match(request, { ignoreSearch: true });

      if (cached) return cached;

      try {
        const response = await fetch(request);

        /*  Only cache a real success. An opaque or error response cached here
            would be served forever by the branch above - the failure mode
            where an app is permanently broken until its storage is cleared. */
        if (response.ok && response.type === 'basic') {
          const cache = await caches.open(CACHE);
          cache.put(request, response.clone());
        }

        return response;
      } catch {
        /*  Offline and not cached. For a navigation, the shell is the right
            answer - a single-page app can route itself from there. For
            anything else there is nothing honest to return. */
        if (request.mode === 'navigate') {
          const shell = await caches.match('./index.html');
          if (shell) return shell;
        }

        throw new Error('offline and not cached');
      }
    })(),
  );
});
