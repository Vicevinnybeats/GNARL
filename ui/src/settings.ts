import { useSyncExternalStore } from 'react';

/**
 * View settings: the things that change how the interface behaves rather than
 * how the instrument sounds.
 *
 * WHY THESE ARE NOT PARAMETERS, and not in the plugin's ValueTree either.
 * Every knob in this UI is an AudioProcessorValueTreeState parameter, which is
 * what gives it automation, undo and a place in a preset. These are the
 * opposite: they must NOT travel with a patch. If "animations off" lived in
 * the ValueTree then loading somebody else's preset would turn your animations
 * back on, and sharing yours would push your accessibility preference onto
 * them. They belong to the person at the machine, not to the sound.
 *
 * So they live in the webview's own localStorage. That folder is per-user and
 * WebView2 keeps it for the life of the view (see getWebViewDataFolder in
 * WebUIEditor.cpp), so the settings survive closing the plugin - and nothing
 * about them reaches the audio thread or a saved preset.
 *
 * HOW THEY REACH THE CSS. Each one is published as an attribute on <html>, and
 * the stylesheet gates on it by redefining TOKENS rather than by overriding
 * rules. Turning glow off sets --gn-glow-* to `none` in one place; every rule
 * that draws a glow keeps reading the same token and simply stops drawing one.
 * The alternative - a `[data-glow='off']` override beside every glow rule -
 * would have to be maintained in step with every new one, and would silently
 * miss the next one added.
 */

const STORAGE_KEY = 'gnarl.settings.v1';

export const THEMES = ['acid', 'ember', 'dream'] as const;
export type Theme = (typeof THEMES)[number];

export interface Settings {
  /** Which palette. Dream is the default, on the client's direction. */
  theme: Theme;
  /** Transitions and easing on hover, selection and panel changes. */
  motion: boolean;
  /** The hover and focus glow. State glow - what is enabled, what is active -
      is NOT covered by this: that glow is information, not decoration, and a
      UI that stops telling you which effects are on is a broken UI, not a
      calmer one. */
  glow: boolean;
  /** The one-line explanations under a panel's controls. On by default because
      a dense synth has to be learnable; off for someone who knows it and wants
      the density back. */
  hints: boolean;
  /** Pixels of vertical drag for a knob's full sweep. Higher is finer. */
  knobDragPx: number;
}

export const DEFAULT_KNOB_DRAG_PX = 200;
export const MIN_KNOB_DRAG_PX = 80;
export const MAX_KNOB_DRAG_PX = 600;

/**
 * The defaults, which are not all constants.
 *
 * prefers-reduced-motion is an OS-level accessibility setting, and a person who
 * has set it has already said what they want - so motion and the hover glow
 * start off for them rather than making them find this panel. The explicit
 * toggles still win once used, because the stored value is what loads.
 */
function defaults(): Settings {
  const reduced =
    typeof window !== 'undefined' &&
    typeof window.matchMedia === 'function' &&
    window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  return {
    theme: 'dream',
    motion: !reduced,
    glow: !reduced,
    hints: true,
    knobDragPx: DEFAULT_KNOB_DRAG_PX,
  };
}

function clampDrag(value: unknown): number {
  const number = typeof value === 'number' ? value : Number(value);

  if (!Number.isFinite(number)) return DEFAULT_KNOB_DRAG_PX;

  return Math.min(MAX_KNOB_DRAG_PX, Math.max(MIN_KNOB_DRAG_PX, Math.round(number)));
}

function parse(raw: string | null): Settings {
  const base = defaults();

  if (!raw) return base;

  try {
    const stored = JSON.parse(raw) as Partial<Settings>;

    return {
      // Each field validated on its own, so a hand-edited or
      // partially-written value falls back for that field alone rather than
      // throwing the whole lot away.
      theme: THEMES.includes(stored.theme as Theme) ? (stored.theme as Theme) : base.theme,
      motion: typeof stored.motion === 'boolean' ? stored.motion : base.motion,
      glow: typeof stored.glow === 'boolean' ? stored.glow : base.glow,
      hints: typeof stored.hints === 'boolean' ? stored.hints : base.hints,
      knobDragPx:
        stored.knobDragPx === undefined ? base.knobDragPx : clampDrag(stored.knobDragPx),
    };
  } catch {
    return base;
  }
}

function load(): Settings {
  // localStorage throws rather than returning null when it is blocked, which
  // it is in some embedded webview configurations. Defaults are a perfectly
  // good answer there; failing to start is not.
  try {
    return parse(window.localStorage.getItem(STORAGE_KEY));
  } catch {
    return defaults();
  }
}

function save(settings: Settings): void {
  try {
    window.localStorage.setItem(STORAGE_KEY, JSON.stringify(settings));
  } catch {
    // A setting that does not persist is a smaller problem than a UI that
    // cannot change one.
  }
}

let current: Settings = typeof window === 'undefined' ? defaults() : load();

const listeners = new Set<() => void>();

/** Publishes to the document, which is what the stylesheet reads. */
function apply(settings: Settings): void {
  if (typeof document === 'undefined') return;

  const root = document.documentElement;

  root.dataset.theme = settings.theme;

  // 'on'/'off' rather than presence-of-attribute: a selector on a value reads
  // the same way round as the setting does, and an attribute that is sometimes
  // absent is easy to get backwards.
  root.dataset.motion = settings.motion ? 'on' : 'off';
  root.dataset.glow = settings.glow ? 'on' : 'off';
  root.dataset.hints = settings.hints ? 'on' : 'off';
}

function emit(): void {
  for (const listener of listeners) listener();
}

function subscribe(onChange: () => void): () => void {
  listeners.add(onChange);
  return () => listeners.delete(onChange);
}

function getSnapshot(): Settings {
  return current;
}

/** Re-renders the caller whenever any setting changes. */
export function useSettings(): Settings {
  return useSyncExternalStore(subscribe, getSnapshot, getSnapshot);
}

export function updateSettings(change: Partial<Settings>): void {
  const next: Settings = {
    ...current,
    ...change,
    ...(change.knobDragPx === undefined ? {} : { knobDragPx: clampDrag(change.knobDragPx) }),
  };

  current = next;
  apply(next);
  save(next);
  emit();
}

export function resetSettings(): void {
  // Clears the stored copy as well, so the OS accessibility default applies
  // again on the next load rather than a snapshot of it being frozen in.
  try {
    window.localStorage.removeItem(STORAGE_KEY);
  } catch {
    // Nothing to clear.
  }

  current = defaults();
  apply(current);
  emit();
}

/**
 * Whether canvas controls should draw their glow.
 *
 * Read per FRAME by the two controls that redraw continuously - the wavetable
 * display and the LFO editor - which is why it is a plain attribute read
 * rather than a hook: a dataset lookup costs nothing, where getComputedStyle
 * forces style resolution. Those two re-read their colours every frame anyway
 * and so need no render dependency; the controls that redraw on DEMAND take
 * useSettings() instead, for the same reason they already take useTheme()
 * (CLAUDE.md section 6 - a canvas does not repaint when a custom property
 * changes).
 */
export function isCanvasGlowEnabled(): boolean {
  if (typeof document === 'undefined') return true;

  return document.documentElement.dataset.glow !== 'off';
}

/** Applies the loaded settings before React's first paint. Called from main. */
export function initialiseSettings(): void {
  apply(current);
}
