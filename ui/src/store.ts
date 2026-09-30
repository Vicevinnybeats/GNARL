/*
 * The panel's state. One flat map of values plus change listeners.
 *
 * In the plugin, `set` is where a value is handed to the engine (the same
 * path Vital's own UI uses: a control_change enqueued on SynthBase), and
 * `apply` is how the engine's side - automation, a preset load - comes back.
 * Here, in the browser, it only redraws.
 */

import { CHOICES, PARAMS, WOBBLE_DESTINATIONS } from './params';

type Listener = (id: string, value: number) => void;

const values = new Map<string, number>();
const listeners = new Set<Listener>();

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

export function subscribe(listener: Listener): () => void {
  listeners.add(listener);
  return () => listeners.delete(listener);
}

function emit(id: string): void {
  const v = values.get(id) ?? 0;
  for (const l of listeners) l(id, v);
}

resetAll();
