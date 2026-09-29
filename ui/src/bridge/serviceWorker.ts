import { getPluginInfo } from './pluginInfo';

/**
 * Retires the service worker, and makes sure it stays retired.
 *
 * THIS USED TO REGISTER A CACHE-FIRST WORKER for offline support. That
 * worker, combined with a year-long `immutable` header on filenames that
 * never change, made the app impossible to update: the worker filled each
 * new versioned cache by fetching URLs the HTTP cache had been told would
 * never change, so every "new" cache was a copy of the old bundle. Reloading
 * could not help, because the reload was answered from the cache that was
 * the problem.
 *
 * `public/sw.js` is now a worker that deletes every cache, unregisters
 * itself and reloads open pages. That handles clients which already have the
 * old worker — they fetch sw.js on navigation, see it differs, and install
 * the replacement.
 *
 * This function handles the other direction: a page that is already running
 * clean code must not put a worker back, and should clear up anything left
 * behind if the worker's own activate step did not get to finish. Belt and
 * braces, because the failure it guards against cost a working day.
 *
 * Offline support is deliberately gone for now. An instrument that opens
 * offline is a feature; an instrument that cannot be updated is a defect,
 * and the defect goes first. A correct worker can return once the delivery
 * path is proven clean — caching files whose URLs actually change.
 */
export function registerServiceWorker(): void {
  //  Inside the plugin there is no network and never was a worker.
  if (!getPluginInfo().isMock) return;
  if (!('serviceWorker' in navigator)) return;

  window.addEventListener('load', () => {
    void (async () => {
      try {
        const registrations = await navigator.serviceWorker.getRegistrations();

        await Promise.all(registrations.map((registration) => registration.unregister()));

        //  A registration can be gone while its caches remain — unregister
        //  does not empty the Cache API.
        if ('caches' in window) {
          const names = await caches.keys();
          await Promise.all(names.map((name) => caches.delete(name)));
        }
      } catch {
        //  Blocked site data, a private window, an embedded webview that
        //  refuses: none of these should stop the app running, and none of
        //  them can be doing the caching we are trying to undo either.
      }
    })();
  });
}
