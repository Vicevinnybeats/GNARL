/*
 * The page's half of the plugin bridge; src/plugin/web_panel.cpp is the other
 * half and lists the messages. Inside the plugin JUCE's web view defines
 * window.__JUCE__; in a browser it does not and none of this runs.
 *
 * Every control with an engine name (params.ts) is bound to that host
 * parameter. The page sends the HOST's 0..1 value - linear in the engine's
 * value, as ValueBridge defines it - and the engine answers with its own
 * readout text, since only it knows its units and skews. A control with no
 * engine parameter behind it yet is dimmed and says so.
 */

import { drawnListeners, drawnSteps } from './draw';
import { engine, engineViews } from './engine';
import type { EngineRoute } from './engine';
import { CHOICES, PARAMS, POWER, WOBBLE_DESTINATIONS } from './params';
import { apply, get, onGesture, refresh, subscribe } from './store';
import { setNoteSink } from './voice';
import vowelMap from './vowels.json';

interface JuceBackend {
  emitEvent(id: string, payload: unknown): void;
  addEventListener(id: string, fn: (payload: unknown) => void): unknown;
}

declare global {
  interface Window {
    __JUCE__?: { backend: JuceBackend; initialisationData: Record<string, unknown> };
  }
}

/** [host value 0..1, engine readout] */
type Entry = [number, string];
interface ConnectResult {
  version: string;
  values: Record<string, Entry>;
  steps: Record<string, number>;
  routes?: EngineRoute[];
}
interface Frame {
  scope?: number[];
  wobblePhase?: number;
  preset?: string;
  curve?: number[];
}

/** How one page control maps to one host parameter. */
interface Binding {
  id: string;
  name: string;
  toHost(value: number): number | null;
  fromHost(host: number): number;
  /** The engine's readout replaces the page's own under this control. */
  text: boolean;
  /** Only while filter 1 is (true) or is not (false) in the formant model. */
  formant?: boolean;
}

// Controls that are the page's own business, not an engine parameter:
// the envelope page tab, and the wobble's shape and on/off, which the bridge
// turns into a LineGenerator shape and three route depths.
const PAGE_ONLY = new Set(['env.page', 'wobble.shape', 'wobble.on', 'sub.mono']);

/* --------------------------------------------------------------- filter 1 */

// Vital's filter_1_model index of the formant filter, and its model names.
const FORMANT_MODEL = 5;
const MODEL_NAMES = ['ANALOG', 'DIRTY', 'LADDER', 'DIGITAL', 'DIODE', 'FORMANT', 'COMB', 'PHASER'];
// Formant style labels as MEASURED (tests/test_vowel.py): Vital's own labels
// for these two are swapped.
const FORMANT_STYLE_NAMES = ['AIUO', 'AOIE', 'MOUTH'];
// Where each vowel button puts the formant filter, and the names that
// setting lives in.
interface VowelPosition { style: number; x: number; y: number }
const VOWELS: Readonly<Record<string, VowelPosition>> = Object.fromEntries(
  Object.entries(vowelMap).filter(([k]) => k.length === 1) as [string, VowelPosition][],
);
const FILTER_NAMES = ['filter_1_model', 'filter_1_style', 'filter_1_formant_x', 'filter_1_formant_y'];
const FILTER_IDS = ['vowel.cutoff', 'vowel.res', 'vowel.morph', 'vowel.drive'];
// Choices with a GNARL switch as one option (osc FOLD, DIST TUBE).
const FLAG_CHOICES = CHOICES.filter((c) => c.flag && c.vital && c.values);
const FLAG_NAMES = FLAG_CHOICES.flatMap((c) => [c.vital ?? '', c.flag?.name ?? '']);
// The engine's last host value and text per name, for the binding a model
// switch makes active, and for the vowel buttons.
const lastEntry = new Map<string, Entry>();
let filterModel = -1;

const WOBBLE_NAMES: readonly string[] = WOBBLE_DESTINATIONS.flatMap((d) => (d.vital ? [d.vital] : []));

let backend: JuceBackend | null = null;
let steps: Record<string, number> = {};
const bindings = new Map<string, Binding[]>(); // by page id; two for filter 1's knobs
const byName = new Map<string, Binding>(); // by engine name
const wobbleAmounts = new Map<string, number>(); // engine value, -1..1

function send(event: string, payload: unknown): void {
  backend?.emitEvent(event, payload);
}

/** A native function registered with withNativeFunction, as JUCE's own JS calls it. */
let nextCall = 0;
const pending = new Map<number, (result: unknown) => void>();
function call(name: string, ...params: unknown[]): Promise<unknown> {
  const resultId = nextCall++;
  return new Promise((resolve) => {
    pending.set(resultId, resolve);
    send('__juce__invoke', { name, params, resultId });
  });
}

function buildBindings(): void {
  const clamp = (v: number): number => Math.min(1, Math.max(0, v));
  for (const p of PARAMS) {
    const span = p.max - p.min;
    const knob = (name: string, formant?: boolean): Binding => ({
      id: p.id,
      name,
      toHost: (v) => clamp((v - p.min) / span),
      fromHost: (h) => {
        const v = p.min + h * span;
        return p.step ? Math.round(v / p.step) * p.step : v;
      },
      text: true,
      formant,
    });
    if (p.formant === undefined) {
      if (p.vital) add(knob(p.vital));
      continue;
    }
    if (p.vital) add(knob(p.vital, false));
    if (p.formant) add(knob(p.formant, true));
  }
  for (const c of CHOICES) {
    if (!c.vital || !c.values || c.flag) continue;
    const values = c.values;
    const name = c.vital;
    // Every choice the page binds is an indexed parameter starting at 0, so
    // its host value is index / (steps - 1).
    const last = (): number => Math.max(1, (steps[name] ?? values.length) - 1);
    add({
      id: c.id,
      name,
      toHost: (option) => {
        const v = values[option];
        return v === null || v === undefined ? null : v / last();
      },
      // An engine value this panel has no button for lights none of them.
      fromHost: (h) => values.indexOf(Math.round(h * last())),
      text: false,
    });
  }
  for (const [id, name] of Object.entries(POWER)) {
    if (!name) continue;
    add({ id, name, toHost: (v) => (v ? 1 : 0), fromHost: (h) => (h >= 0.5 ? 1 : 0), text: false });
  }
}

function add(b: Binding): void {
  bindings.set(b.id, [...(bindings.get(b.id) ?? []), b]);
  byName.set(b.name, b);
}

function isActive(b: Binding): boolean {
  return b.formant === undefined || b.formant === (filterModel === FORMANT_MODEL);
}

/** The binding a control sends through now: filter 1's depend on its model. */
function activeBinding(id: string): Binding | undefined {
  return bindings.get(id)?.find(isActive);
}

/** Engine value of an indexed parameter (all start at 0), from its host value. */
function indexOf(name: string): number {
  const entry = lastEntry.get(name);
  const last = Math.max(1, (steps[name] ?? 2) - 1);
  return entry ? Math.round(entry[0] * last) : -1;
}

function hostOfIndex(name: string, index: number): number {
  return index / Math.max(1, (steps[name] ?? 2) - 1);
}

/** The vowel button lit by filter 1's current setting, or -1 for none. */
function currentVowel(options: readonly string[]): number {
  if (filterModel !== FORMANT_MODEL) return -1;
  const style = indexOf('filter_1_style');
  const x = lastEntry.get('filter_1_formant_x')?.[0] ?? -1;
  const y = lastEntry.get('filter_1_formant_y')?.[0] ?? -1;
  return options.findIndex((v) => {
    const at = VOWELS[v];
    return at !== undefined && at.style === style && Math.abs(at.x - x) < 1e-3 && Math.abs(at.y - y) < 1e-3;
  });
}

/** The option a switch-or-type choice shows: the switch's, or the type's. */
function applyFlagChoices(): void {
  for (const c of FLAG_CHOICES) {
    const flag = c.flag;
    const values = c.values;
    if (!flag || !values || !c.vital) continue;
    const on = (lastEntry.get(flag.name)?.[0] ?? 0) >= 0.5;
    apply(c.id, on ? flag.option : values.indexOf(indexOf(c.vital)));
  }
}

function sendFlagChoice(id: string, option: number): void {
  const c = FLAG_CHOICES.find((x) => x.id === id);
  if (!c || !c.flag || !c.values || !c.vital) return;
  const on = option === c.flag.option;
  send('gnarlSet', { name: c.flag.name, value: on ? 1 : 0 });
  const v = c.values[option];
  if (!on && v !== null && v !== undefined) send('gnarlSet', { name: c.vital, value: hostOfIndex(c.vital, v) });
}

function sendVowel(vowel: string): void {
  const at = VOWELS[vowel];
  if (!at) return;
  // Pressing a vowel means hearing it: the section switches on too.
  send('gnarlSet', { name: 'filter_1_on', value: 1 });
  send('gnarlSet', { name: 'filter_1_model', value: hostOfIndex('filter_1_model', FORMANT_MODEL) });
  send('gnarlSet', { name: 'filter_1_style', value: hostOfIndex('filter_1_style', at.style) });
  send('gnarlSet', { name: 'filter_1_formant_x', value: at.x });
  send('gnarlSet', { name: 'filter_1_formant_y', value: at.y });
}

/**
 * Filter 1's model changed (or arrived): name it in the panel's corner, move
 * each knob to the engine name that model reads, and dim what it lacks.
 */
function applyFilterModel(bound: ReadonlySet<string>): void {
  const style = indexOf('filter_1_style');
  const label = MODEL_NAMES[filterModel] ?? '';
  const aside = filterModel === FORMANT_MODEL ? `${label} ${FORMANT_STYLE_NAMES[style] ?? ''}`.trim() : label;
  for (const node of document.querySelectorAll<HTMLElement>('[data-filter-model]')) node.textContent = aside;

  for (const id of FILTER_IDS) {
    const b = activeBinding(id);
    const entry = b ? lastEntry.get(b.name) : undefined;
    for (const node of document.querySelectorAll<HTMLElement>(`[data-param="${id}"]`)) {
      if (b && bound.has(b.name)) {
        delete node.dataset.unbound;
        node.title = '';
      } else {
        node.dataset.unbound = 'true';
        node.title = `Not in the ${label.toLowerCase()} filter`;
      }
    }
    if (b && entry) {
      engine.text.set(id, tidyText(entry[1]));
      apply(id, b.fromHost(entry[0]));
      refresh(id);
    }
  }
  const vowels = CHOICES.find((c) => c.id === 'vowel.vowel');
  if (vowels) apply('vowel.vowel', currentVowel(vowels.options));
}

/* ------------------------------------------------------------ the wobble */

// One DEPTH knob, three destination toggles and an on/off on the page; three
// signed route depths in the engine. A route is DEPTH when its toggle and the
// section are on, and 0 otherwise.
function sendWobbleRoutes(): void {
  const on = get('wobble.on') === 1;
  for (const dest of WOBBLE_DESTINATIONS) {
    if (!dest.vital) continue;
    const amount = on && get(dest.id) ? get('wobble.depth') : 0;
    wobbleAmounts.set(dest.vital, amount);
    send('gnarlSet', { name: dest.vital, value: (amount + 1) / 2 });
  }
}

function applyWobbleRoutes(): void {
  let depth = 0;
  for (const dest of WOBBLE_DESTINATIONS) {
    if (!dest.vital) continue;
    const amount = wobbleAmounts.get(dest.vital) ?? 0;
    apply(dest.id, amount !== 0 ? 1 : 0);
    depth = Math.max(depth, Math.abs(amount));
  }
  // All routes at zero says nothing about the knob: leave it where it is.
  if (depth > 0) {
    apply('wobble.depth', depth);
    apply('wobble.on', 1);
  }
}

function sendWobbleShape(): void {
  const shape = get('wobble.shape');
  const kind = ['sine', 'square', 'draw'][shape];
  if (!kind) return;
  send('gnarlWobbleShape', { kind, points: kind === 'draw' ? drawnSteps() : [] });
}

/* ---------------------------------------------------------- engine -> page */

let boundNames: ReadonlySet<string> = new Set();

function receiveValues(values: Record<string, Entry>): void {
  let wobble = false;
  let filter = false;
  let flags = false;
  for (const [name, [host, text]] of Object.entries(values)) {
    lastEntry.set(name, [host, text]);
    if (FLAG_NAMES.includes(name)) flags = true;
    if (FILTER_NAMES.includes(name)) {
      filter = true;
      if (name === 'filter_1_model') filterModel = indexOf(name);
    }
    if (WOBBLE_NAMES.includes(name)) {
      wobbleAmounts.set(name, host * 2 - 1);
      wobble = true;
      continue;
    }
    const b = byName.get(name);
    if (!b || !isActive(b)) continue;
    if (b.text) engine.text.set(b.id, tidyText(text));
    const before = get(b.id);
    apply(b.id, b.fromHost(host));
    // Same value, new text (the engine's rounding): redraw the readout.
    if (b.text && get(b.id) === before) refresh(b.id);
  }
  if (wobble) applyWobbleRoutes();
  if (filter) applyFilterModel(boundNames);
  if (flags) applyFlagChoices();
}

/**
 * The engine's readout, as a knob's label shows it. ValueBridge::getText is
 * the host's text - full float precision, units spelled out ("0.000499534
 * secs") - which is right for a DAW's automation lane and too long under a
 * knob. Three significant figures; seconds under one become ms.
 */
export function tidyText(text: string): string {
  const m = /^(-?\d+(?:\.\d+)?(?:e-?\d+)?)\s*(.*)$/.exec(text.trim());
  if (!m) return text;
  let value = Number(m[1]);
  let unit = m[2] ?? '';
  if (unit === 'secs') {
    unit = Math.abs(value) < 1 ? 'ms' : 's';
    if (unit === 'ms') value *= 1000;
  } else if (unit === 'semitones') {
    unit = 'st';
  }
  const shown = Number(value.toPrecision(3)).toString();
  return unit ? `${shown} ${unit}` : shown;
}

let lastPreset: string | null = null;
function receiveFrame(frame: Frame): void {
  if (frame.scope) engine.scope = Float32Array.from(frame.scope);
  if (typeof frame.wobblePhase === 'number') engine.wobblePhase = frame.wobblePhase;
  if (frame.curve) engine.curve = frame.curve;
  if (frame.preset !== undefined && frame.preset !== lastPreset) {
    lastPreset = frame.preset;
    engine.preset = frame.preset;
    // A preset brings its own wobble shape, which may be none of the three.
    apply('wobble.shape', -1);
    for (const v of engineViews) v();
  }
}

/* ------------------------------------------------------------ the markers */

/** Dim what the engine does not have yet, and say so on hover. */
function markUnbound(bound: ReadonlySet<string>): void {
  const idle = (id: string): boolean =>
    !PAGE_ONLY.has(id) && !bound.has(id) && !WOBBLE_BOUND.has(id) && !FILTER_IDS.includes(id) && id !== 'vowel.vowel' &&
    !FLAG_CHOICES.some((c) => c.id === id);
  for (const node of document.querySelectorAll<HTMLElement>('[data-param]')) {
    const id = node.dataset.param ?? '';
    if (idle(id)) unbound(node);
    const choice = CHOICES.find((c) => c.id === id);
    if (!choice?.values || !(bound.has(id) || choice.flag)) continue;
    for (const chip of node.querySelectorAll<HTMLElement>('[data-option]')) {
      const option = Number(chip.dataset.option);
      if (choice.values[option] === null && choice.flag?.option !== option) unbound(chip);
    }
  }
}

const WOBBLE_BOUND = new Set(['wobble.depth', ...WOBBLE_DESTINATIONS.filter((d) => d.vital).map((d) => d.id)]);

function unbound(node: HTMLElement): void {
  node.dataset.unbound = 'true';
  node.title = 'Not in the engine yet';
}

/* ------------------------------------------------------------------ start */

export function isPlugin(): boolean {
  return window.__JUCE__?.backend !== undefined;
}

/** Set a matrix connection (amount -1..1), or remove it. */
export function sendRoute(source: string, destination: string, amount: number, remove = false): void {
  send('gnarlRoute', { source, destination, amount, remove });
}

function receiveRoutes(routes: EngineRoute[]): void {
  engine.routes = routes;
  for (const v of engineViews) v();
}

/** Show Vital's full editor in place of this panel. */
export function showClassic(): void {
  send('gnarlClassic', {});
}

export async function connect(): Promise<void> {
  const juce = window.__JUCE__;
  if (!juce) return;
  backend = juce.backend;
  backend.addEventListener('__juce__complete', (payload) => {
    const { promiseId, result } = payload as { promiseId: number; result: unknown };
    pending.get(promiseId)?.(result);
    pending.delete(promiseId);
  });

  buildBindings();
  const names = [...new Set([...byName.keys(), ...WOBBLE_NAMES, ...FILTER_NAMES, ...FLAG_NAMES])];
  const result = (await call('gnarlConnect', names)) as ConnectResult;
  steps = result.steps;
  engine.connected = true;
  document.body.dataset.plugin = 'true';

  // A name the engine did not answer for is as unbound as a null one.
  const bound = new Set<string>();
  for (const list of bindings.values()) for (const b of list) if (b.name in result.values) bound.add(b.id);
  boundNames = new Set(Object.keys(result.values));
  markUnbound(bound);
  // The sub is mono by construction, so MONO is a statement, not a switch.
  for (const node of document.querySelectorAll<HTMLButtonElement>('button[data-param="sub.mono"]')) {
    node.disabled = true;
    node.dataset.on = 'true';
    node.title = 'The sub is always mono: both channels carry the same samples';
  }
  receiveValues(result.values);

  backend.addEventListener('gnarlValues', (payload) => receiveValues(payload as Record<string, Entry>));
  backend.addEventListener('gnarlFrame', (payload) => receiveFrame(payload as Frame));
  backend.addEventListener('gnarlRoutes', (payload) => receiveRoutes(payload as EngineRoute[]));
  receiveRoutes(result.routes ?? []);

  subscribe((id, value, fromEngine) => {
    if (fromEngine) return;
    if (id === 'wobble.shape') sendWobbleShape();
    else if (WOBBLE_BOUND.has(id) || id === 'wobble.on') sendWobbleRoutes();
    else if (id === 'vowel.vowel') sendVowel(CHOICES.find((c) => c.id === id)?.options[value] ?? '');
    else if (FLAG_CHOICES.some((c) => c.id === id)) sendFlagChoice(id, value);
    const b = activeBinding(id);
    if (!b || !boundNames.has(b.name)) return;
    const host = b.toHost(value);
    if (host !== null) send('gnarlSet', { name: b.name, value: host });
  });
  onGesture((id, begin) => {
    const active = activeBinding(id);
    const names = id === 'wobble.depth' ? WOBBLE_NAMES : active && boundNames.has(active.name) ? [active.name] : [];
    for (const name of names) send('gnarlGesture', { name, begin });
  });
  drawnListeners.add(() => get('wobble.shape') === 2 && sendWobbleShape());
  setNoteSink((note, on) => send('gnarlNote', { note, on }));
}
