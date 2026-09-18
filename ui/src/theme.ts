import { useSyncExternalStore } from 'react';

/**
 * The active theme name, as a value React can depend on.
 *
 * WHY THIS EXISTS. Several controls draw themselves on a canvas and read their
 * colours out of the CSS custom properties with getComputedStyle. That is the
 * right way to get them - it keeps one palette in one file - but a canvas does
 * not repaint when a custom property changes the way a DOM node does. So every
 * canvas control kept whatever colour the theme had when it last drew, and the
 * theme switch left a UI half in one palette and half in the other: the knobs
 * stayed acid green on a violet instrument.
 *
 * Components that draw on a canvas therefore take the theme name as a render
 * dependency. Components that redraw every frame anyway - the wavetable
 * display, the LFO editor - re-read the properties each frame and do not need
 * it.
 */

const THEME_ATTRIBUTE = 'data-theme';

function subscribe(onChange: () => void): () => void {
  const observer = new MutationObserver(onChange);

  observer.observe(document.documentElement, {
    attributes: true,
    attributeFilter: [THEME_ATTRIBUTE],
  });

  return () => observer.disconnect();
}

function getSnapshot(): string {
  return document.documentElement.getAttribute(THEME_ATTRIBUTE) ?? 'acid';
}

/** Re-renders the caller whenever the theme changes. */
export function useTheme(): string {
  // The server snapshot is the same as the client one: this never renders
  // outside a browser, but useSyncExternalStore wants both.
  return useSyncExternalStore(subscribe, getSnapshot, getSnapshot);
}
