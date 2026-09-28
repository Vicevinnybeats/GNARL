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

  //  After load, not during it. Registration competes for bandwidth with the
  //  bundle itself, and the worker is for the SECOND visit — winning a race
  //  on the first one at the cost of that first paint is backwards.
  window.addEventListener('load', () => {
    void navigator.serviceWorker.register('./sw.js', { scope: './' }).catch(() => {
      //  Deliberately swallowed: see the note above.
    });
  });
}
