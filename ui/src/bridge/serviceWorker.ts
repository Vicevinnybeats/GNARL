import { getPluginInfo } from './pluginInfo';

/**
 * Registers the service worker, and only where one belongs.
 *
 * THE SAME BUNDLE IS THE PLUGIN'S INTERFACE AND THE INSTALLABLE APP. Inside
 * the plugin it is served by `WebUIResourceProvider` from a custom scheme
 * with no network behind it, where a service worker is at best useless and at
 * worst a second cache layer in front of resources that are already in the
 * binary — one that could serve a stale interface after an update, from
 * storage the user has no obvious way to clear.
 *
 * So registration is guarded on `isMock`, which is true exactly when JUCE's
 * native backend is absent: a browser. That is the same signal the rest of
 * the bridge uses to decide whether it is talking to an engine, so there is
 * one answer to "am I in the plugin" rather than two that can disagree.
 *
 * FAILURE IS SILENT AND NON-FATAL. A worker cannot register over plain HTTP
 * on a remote host, in a private window, or where the user has blocked site
 * data — none of which should stop the app running. It only ever adds
 * offline support; nothing depends on it.
 */
export function registerServiceWorker(): void {
  if (!getPluginInfo().isMock) return;
  if (!('serviceWorker' in navigator)) return;

  /*  A NEW BUILD TAKES OVER ON ITS OWN, and this is the difference between
   *  an app that updates and one somebody has to delete and re-add.
   *
   *  The worker is cache-first, so a launch after a new build is deployed
   *  paints the OLD bundle from the cache while the new worker installs
   *  behind it. `skipWaiting` and `clients.claim` in sw.js then hand control
   *  over - but the page already running is still the old one, and stays
   *  old until something reloads it. On a phone an installed app is rarely
   *  quit, so "something" was never happening and the fix looked like it had
   *  not shipped.
   *
   *  `controllerchange` fires exactly when the new worker takes the page, so
   *  that is the moment to reload. Guarded with a flag because the event can
   *  fire more than once, and a reload that triggers another reload is an
   *  app that never finishes starting.
   */
  /*  READ NOW, NOT IN THE HANDLER. By the time `controllerchange` fires,
      `navigator.serviceWorker.controller` is already the NEW worker, so it
      is non-null whether this is a first install or an update and testing it
      there cannot tell them apart. What distinguishes them is whether a
      controller existed BEFORE, which is only knowable here.

      It matters because a first visit takes control of a page the worker did
      not serve - that page is already current, and reloading it costs a
      pointless round trip on the one launch where the user is waiting. */
  const hadController = Boolean(navigator.serviceWorker.controller);
  let reloading = false;

  navigator.serviceWorker.addEventListener('controllerchange', () => {
    if (reloading || !hadController) return;

    reloading = true;
    window.location.reload();
  });

  //  After load, not during it. Registration competes for bandwidth with the
  //  bundle itself, and the worker is for the SECOND visit — winning a race
  //  on the first one at the cost of that first paint is backwards.
  window.addEventListener('load', () => {
    void navigator.serviceWorker
      .register('./sw.js', { scope: './' })
      .then((reg) => {
        //  Ask on every launch rather than trusting the browser's own
        //  schedule, which can be up to 24 hours.
        void reg.update().catch(() => undefined);
      })
      .catch(() => {
        //  Deliberately swallowed: see the note above.
      });
  });
}
