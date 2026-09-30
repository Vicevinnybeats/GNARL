/*
 * The panel's state. One flat map of values plus change listeners.
 *
 * In the plugin, `set` is where a value is handed to the engine (bridge.ts
 * subscribes and sends it), and `apply` is how the engine's side -
 * automation, a preset load - comes back without being sent again. Here, in
 * the browser, both only redraw.
 */

import { CHOICES, PARAMS, WOBBLE_DESTINATIONS } from './params';

/** `fromEngine` is true for a value the engine sent: never send it back. */
type Listener = (id: string, value: number, fromEngine: boolean) => void;
type GestureListener = (id: string, begin: boolean) => void;

const values = new Map<string, number>();
const listeners = new Set<Listener>();
const gestureListeners = new Set<GestureListener>();
let applying = false;

export function resetAll(): void {
  for (const p of PARAMS) values.set(p.id, p.def);
  for (const c of CHOICES) values.set(c.id, c.def);
  for (const d of WOBBLE_DESTINATIONS) values.set(d.id, d.def ? 1 : 0);
  for (const slot of ['dist', 'fold', 'crush', 'ott', 'osc1', 'osc2', 'sub', 'vowel', 'wobble']) {
    values.set(`${slot}.on`, 1);
  }
  for (const id of values.keys()) emit(id);
}

export function get(id: string): number {
  return values.get(id) ?? 0;
}

export function set(id: string, value: number): void {
  if (values.get(id) === value) return;
  values.set(id, value);
  emit(id);
}

/** A value from the engine: shown, not sent back. */
export function apply(id: string, value: number): void {
  applying = true;
  try {
    set(id, value);
  } finally {
    applying = false;
  }
}

/**
 * Redraw a control whose value is unchanged but whose readout is not. It is
 * the engine's news, so listeners are told fromEngine: a refresh that looked
 * like a user's change was sent back to the engine, echoed, refreshed and
 * sent again - forever, and it overwrote a vowel with a stale X on the way
 * (ui/tests/bridge.test.mjs).
 */
export function refresh(id: string): void {
  applying = true;
  try {
    emit(id);
  } finally {
    applying = false;
  }
}

/**
 * A knob grabbed (begin) or let go. A DAW records automation between the
 * two, and treats everything in between as one undo step.
 */
export function gesture(id: string, begin: boolean): void {
  for (const l of gestureListeners) l(id, begin);
}

export function onGesture(listener: GestureListener): void {
  gestureListeners.add(listener);
}

export function subscribe(listener: Listener): () => void {
  listeners.add(listener);
  return () => listeners.delete(listener);
}

function emit(id: string): void {
  const v = values.get(id) ?? 0;
  for (const l of listeners) l(id, v, applying);
}

resetAll();
