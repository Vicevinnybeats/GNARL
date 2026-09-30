/*  A SERVICE WORKER WHOSE ONLY JOB IS TO REMOVE ITSELF.
 *
 *  WHY THIS EXISTS. The previous worker was cache-first, which is correct
 *  for offline support and was fatal here: `/app/assets/(.*)` was served
 *  with `immutable` for a year while the filenames never change, so the
 *  worker's own `cache.add` - an ordinary fetch, subject to the HTTP cache -
 *  filled every new versioned cache with the SAME old bundle. Versioning the
 *  cache name cannot help when its contents come from a cache that has been
 *  told never to revalidate. Both layers agreed the file could not have
 *  changed, because one of them had been instructed to say so.
 *
 *  The header is fixed and the asset URLs now carry a build id, so the HTTP
 *  layer is clean. But a browser that already holds the old worker AND its
 *  old cache will keep serving that cache for navigations, and no amount of
 *  reloading gets past it - the reload is answered from the very cache that
 *  is the problem. Fixing the server cannot reach a client that never asks
 *  the server anything.
 *
 *  So this replaces it. `sw.js` is the one file that was always served with
 *  `max-age=0, must-revalidate`, and browsers byte-compare it on navigation,
 *  so a stale client fetches THIS, sees it differs, and installs it - which
 *  is the one door into a client that is otherwise sealed. It then deletes
 *  every cache, unregisters itself, and reloads whatever is open.
 *
 *  OFFLINE IS DELIBERATELY GIVEN UP FOR NOW. An instrument that opens
 *  offline is a feature; an instrument that cannot be updated is a defect,
 *  and between the two the defect has to go first. A correct cache-first
 *  worker can come back once the channel is proven clean - the difference
 *  then being that it will be caching files whose URLs actually change.
 */

self.addEventListener('install', () => {
  //  No waiting: the point is to replace the old worker immediately.
  void self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    (async () => {
      //  EVERY cache, not just the ones matching the old naming scheme. The
      //  names carried a build id, so there is no telling how many
      //  generations are sitting there, and this origin has nothing else
      //  using the Cache API.
      const names = await caches.keys();
      await Promise.all(names.map((name) => caches.delete(name)));

      //  Take control of pages that were loaded by the OLD worker, so the
      //  reload below reaches them.
      await self.clients.claim();

      await self.registration.unregister();

      /*  And reload, because the page currently on screen was served from
          the cache that has just been deleted - it is the stale build, and
          without this the person is looking at it until they navigate
          again. This is the moment the loop everybody has been stuck in
          actually breaks. */
      const clients = await self.clients.matchAll({ type: 'window' });

      for (const client of clients) {
        client.navigate(client.url);
      }
    })(),
  );
});

/*  No fetch handler at all. A worker without one is transparent: every
    request goes to the network exactly as if no worker were installed,
    which is the desired behaviour for the moments between this activating
    and it being gone. */
