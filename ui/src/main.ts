/*
 * GNARL's panel. The layout is the producer's mock-up (one page, no tabs);
 * the surfaces, glow and knob readouts are the retired engine's "Dream" UI.
 *
 * The desktop panel is laid out at one design size and SCALED to the window,
 * never reflowed - a plugin window resizes by aspect ratio, and a synth whose
 * knobs move when you resize it is a synth you cannot learn.
 *
 * A phone gets its own layout (phone(), below) built from the same panels:
 * one section at a time, controls at finger size, keys always in reach.
 */

import './fonts.css';
import './styles.css';
import { connect, isPlugin, showClassic } from './bridge';
import { drawEnvelope, drawOsc, drawScope, drawSub, drawVowel, drawWobble, setDrawnPoint, DRAWN_STEPS } from './draw';
import { engine, engineViews } from './engine';
import { headerMark, mountLogo } from './logo';
import { MOD_DESTINATIONS, MOD_SOURCES, PRESET_NAMES, WOBBLE_DESTINATIONS } from './params';
import { get, resetAll, subscribe } from './store';
import { chipButton, choiceRow, display, el, knob, panel, resizeDisplay, toggle } from './widgets';
import type { Display } from './widgets';
import { currentNote, noteName, noteOff, noteOn } from './voice';

const DESIGN_W = 1280;
const DESIGN_H = 720;

const displays: { d: Display; paint: (t: number) => void }[] = [];
function addDisplay(d: Display, paint: (t: number) => void): HTMLElement {
  displays.push({ d, paint });
  return d.root;
}

const knobs = (...nodes: HTMLElement[]): HTMLElement => el('div', 'knobs', ...nodes);

/* ------------------------------------------------------------------ header */

// The preset index is shared: the desktop and phone layouts both show it.
let presetIndex = 0;
const presetViews = new Set<() => void>();

function presetPicker(): HTMLElement {
  const name = el('span', 'preset__name');
  // In the plugin the name is the engine's loaded preset; there is no preset
  // browser behind the arrows yet, so they are off rather than lying.
  const show = (): void => {
    name.textContent = engine.connected ? (engine.preset ?? '') : (PRESET_NAMES[presetIndex] ?? '');
    prev.disabled = next.disabled = engine.connected;
  };
  const step = (by: number): void => {
    presetIndex = (presetIndex + by + PRESET_NAMES.length) % PRESET_NAMES.length;
    for (const v of presetViews) v();
  };
  const prev = el('button', 'preset__step', '‹');
  const next = el('button', 'preset__step', '›');
  prev.type = next.type = 'button';
  prev.setAttribute('aria-label', 'Previous preset');
  next.setAttribute('aria-label', 'Next preset');
  prev.addEventListener('click', () => step(-1));
  next.addEventListener('click', () => step(1));
  presetViews.add(show);
  engineViews.add(show);
  show();
  return el('div', 'preset', prev, name, next);
}

function aiButton(label: string): HTMLButtonElement {
  const ai = el('button', 'ai', el('span', 'ai__spark', '✦'), label);
  ai.type = 'button';
  ai.addEventListener('click', () =>
    toast('AI presets are Phase 4, not built yet. The button is where they will live.'),
  );
  return ai;
}

/**
 * The live waveform beside MASTER. Pressing and holding it plays a note, so
 * the patch can be seen moving without a keyboard.
 */
function scope(width: number, height: number): HTMLElement {
  const d = display(width, height, 'scope');
  const root = addDisplay(d, (t) => drawScope(d, t, t));
  root.title = 'Hold to play a note (or use the keys A to K)';
  const PREVIEW_NOTE = 36; // C2: low enough for riddim, high enough to see.
  root.addEventListener('pointerdown', (e) => {
    root.setPointerCapture(e.pointerId);
    noteOn(PREVIEW_NOTE, clock());
  });
  const release = (): void => noteOff(PREVIEW_NOTE, clock());
  root.addEventListener('pointerup', release);
  root.addEventListener('pointercancel', release);
  return root;
}

function masterKnob(): HTMLElement {
  const master = knob('master', { size: 28 });
  master.classList.add('knob--inline');
  return master;
}

/** Inside the plugin: Vital's full editor, for everything this panel lacks. */
function advancedButton(): HTMLButtonElement {
  const b = chipButton('ADVANCED');
  b.classList.add('advanced');
  b.title = 'The full editor: every parameter, the modulation matrix, wavetables';
  b.hidden = !isPlugin();
  b.addEventListener('click', showClassic);
  return b;
}

function header(): HTMLElement {
  return el(
    'header',
    'top',
    el('div', 'brand', headerMark(), el('span', 'brand__word', 'GNARL')),
    presetPicker(),
    aiButton('AI PRESET'),
    advancedButton(),
    el('div', 'top__spacer'),
    scope(250, 34),
    masterKnob(),
  );
}

/* ------------------------------------------------------------ oscillators */

function oscPanel(n: 1 | 2, table: string): HTMLElement {
  const d = display(280, 110);
  return panel(
    { title: `OSC ${n}`, aside: table, power: `osc${n}.on` },
    addDisplay(d, (t) => drawOsc(d, n, t)),
    knobs(knob(`osc${n}.wtpos`), knob(`osc${n}.warp`), knob(`osc${n}.fm`), knob(`osc${n}.unison`), knob(`osc${n}.detune`)),
    choiceRow(`osc${n}.mode`),
  );
}

function subPanel(): HTMLElement {
  const d = display(170, 78);
  return panel(
    { title: 'SUB', power: 'sub.on' },
    addDisplay(d, () => drawSub(d)),
    knobs(knob('sub.level'), knob('sub.drive')),
    el('div', 'chips', toggle('sub.mono', 'MONO'), toggle('sub.oct', '-1 OCT')),
  );
}

/** Filter 1's model, which the bridge writes in once it knows (bridge.ts). */
function filterModelAside(): HTMLElement {
  const aside = el('span', 'panel__aside', 'FORMANT');
  aside.dataset.filterModel = 'true';
  return aside;
}

function vowelPanel(): HTMLElement {
  const d = display(360, 110);
  return panel(
    { title: 'VOWEL FILTER', aside: filterModelAside(), power: 'vowel.on' },
    addDisplay(d, (t) => drawVowel(d, t)),
    choiceRow('vowel.vowel', { cls: 'chips--vowels' }),
    knobs(knob('vowel.cutoff'), knob('vowel.res'), knob('vowel.morph'), knob('vowel.drive')),
  );
}

/* ------------------------------------------------------------------ wobble */

function wobblePanel(): HTMLElement {
  const d = display(210, 96);
  // DRAW mode: drag across the display to set the steps.
  let drawing = false;
  const paintAt = (e: PointerEvent): void => {
    const r = d.canvas.getBoundingClientRect();
    const x = (e.clientX - r.left) / r.width;
    const y = (e.clientY - r.top) / r.height;
    const step = Math.floor(((x * 2) % 1) * DRAWN_STEPS);
    setDrawnPoint(step, 1 - y);
  };
  d.canvas.addEventListener('pointerdown', (e) => {
    if (get('wobble.shape') !== 2) {
      toast('Pick DRAW under SHAPE to draw the wobble.');
      return;
    }
    drawing = true;
    d.canvas.setPointerCapture(e.pointerId);
    paintAt(e);
  });
  d.canvas.addEventListener('pointermove', (e) => drawing && paintAt(e));
  d.canvas.addEventListener('pointerup', () => (drawing = false));

  const destinations = el(
    'div',
    'chips',
    ...WOBBLE_DESTINATIONS.map((dest) => toggle(dest.id, dest.label, 'violet')),
  );

  return panel(
    { title: 'WOBBLE LFO', aside: 'SYNC TO HOST', accent: 'violet', power: 'wobble.on', cls: 'wobble' },
    el(
      'div',
      'wobble__grid',
      el(
        'div',
        'wobble__left',
        addDisplay(d, (t) => drawWobble(d, t)),
        knobs(
          knob('wobble.depth', { accent: 'violet' }),
          knob('wobble.smooth', { accent: 'violet' }),
          knob('wobble.phase', { accent: 'violet' }),
        ),
      ),
      el(
        'div',
        'wobble__right',
        el('span', 'field', 'RATE'),
        choiceRow('wobble.rate', { accent: 'violet' }),
        el('span', 'field', 'DESTINATION'),
        destinations,
        el('span', 'field', 'SHAPE'),
        choiceRow('wobble.shape'),
      ),
    ),
  );
}

/* ---------------------------------------------------------------- envelope */

function envelopePanel(): HTMLElement {
  const d = display(220, 96);
  const row = el('div', 'knobs');
  const build = (): void => {
    const page = get('env.page') ? 'filter' : 'amp';
    row.replaceChildren(
      knob(`env.${page}.att`),
      knob(`env.${page}.dec`),
      knob(`env.${page}.sus`),
      knob(`env.${page}.rel`),
    );
  };
  subscribe((id) => id === 'env.page' && build());
  build();
  return panel(
    { title: 'ENVELOPE', aside: choiceRow('env.page', { cls: 'chips--tabs' }) },
    addDisplay(d, () => drawEnvelope(d)),
    row,
  );
}

/* -------------------------------------------------------------- mod matrix */

interface Route {
  source: number;
  dest: number;
  amount: number;
}

// Shared by both layouts, so a route added on the phone shows on the desktop.
const routes: Route[] = [
  { source: 0, dest: 0, amount: 0.72 },
  { source: 0, dest: 2, amount: 0.45 },
  { source: 1, dest: 3, amount: 0.55 },
];
const MAX_ROUTES = 4;
const routeViews = new Set<() => void>();
const rerenderRoutes = (): void => {
  for (const v of routeViews) v();
};

function modPanel(): HTMLElement {
  const list = el('div', 'routes');
  const count = el('span', 'panel__aside');

  const render = (): void => {
    list.replaceChildren(
      ...routes.map((route, i) => {
        const src = chipButton(MOD_SOURCES[route.source] ?? '', 'violet');
        src.dataset.on = 'true';
        src.title = 'Click to change the source';
        src.addEventListener('click', () => {
          route.source = (route.source + 1) % MOD_SOURCES.length;
          rerenderRoutes();
        });

        const dest = el('button', 'route__dest', MOD_DESTINATIONS[route.dest] ?? '');
        dest.type = 'button';
        dest.title = 'Click to change the destination; right-click to remove';
        dest.addEventListener('click', () => {
          route.dest = (route.dest + 1) % MOD_DESTINATIONS.length;
          rerenderRoutes();
        });
        dest.addEventListener('contextmenu', (e) => {
          e.preventDefault();
          routes.splice(i, 1);
          rerenderRoutes();
        });

        const fill = el('span', 'amount__fill');
        const bar = el('div', 'amount', fill);
        bar.title = `${Math.round(route.amount * 100)} %`;
        fill.style.width = `${route.amount * 100}%`;
        const setFrom = (e: PointerEvent): void => {
          const r = bar.getBoundingClientRect();
          route.amount = Math.min(1, Math.max(0, (e.clientX - r.left) / r.width));
          fill.style.width = `${route.amount * 100}%`;
          bar.title = `${Math.round(route.amount * 100)} %`;
        };
        bar.addEventListener('pointerup', rerenderRoutes);
        bar.addEventListener('pointerdown', (e) => {
          bar.setPointerCapture(e.pointerId);
          setFrom(e);
        });
        bar.addEventListener('pointermove', (e) => bar.hasPointerCapture(e.pointerId) && setFrom(e));
        return el('div', 'route', src, dest, bar);
      }),
    );
    const add = el('button', 'route__add', '+ ADD ROUTE');
    add.type = 'button';
    add.disabled = routes.length >= MAX_ROUTES;
    add.addEventListener('click', () => {
      routes.push({ source: 2, dest: 4, amount: 0.5 });
      rerenderRoutes();
    });
    list.append(add);
    count.textContent = `${routes.length}/${MAX_ROUTES}`;
  };
  routeViews.add(render);
  render();

  return panel({ title: 'MOD MATRIX', aside: count, accent: 'violet' }, list);
}

/* ---------------------------------------------------------------------- fx */

function fxPanel(index: number, slot: string, title: string, a: string, b: string): HTMLElement {
  return panel(
    { title: `0${index} ${title}`, power: `${slot}.on`, cls: 'fx' },
    el('div', 'fx__row', knob(a), knob(b), choiceRow(`${slot}.mode`, { cls: 'chips--cycle' })),
  );
}

const logos: { canvas: HTMLCanvasElement; paint: (t: number) => void }[] = [];
function logoCanvas(cls: string): HTMLCanvasElement {
  const canvas = el('canvas', cls);
  logos.push({ canvas, paint: mountLogo(canvas) });
  return canvas;
}

function logoPanel(): HTMLElement {
  return el('section', 'panel logo', logoCanvas('logo__canvas'));
}

/* ------------------------------------------------------------------ layout */

function desktop(): HTMLElement {
  return el(
    'div',
    'app',
    header(),
    el('div', 'row row--1', oscPanel(1, 'GROWL_TABLE_01'), oscPanel(2, 'HARD_SQUARE'), subPanel(), vowelPanel()),
    el('div', 'row row--2', wobblePanel(), envelopePanel(), modPanel()),
    el(
      'div',
      'row row--3',
      fxPanel(1, 'dist', 'DIST', 'dist.drive', 'dist.mix'),
      fxPanel(2, 'fold', 'FOLD', 'fold.amount', 'fold.mix'),
      fxPanel(3, 'crush', 'CRUSH', 'crush.bits', 'crush.rate'),
      fxPanel(4, 'ott', 'OTT', 'ott.depth', 'ott.time'),
      logoPanel(),
    ),
  );
}

/* ---------------------------------------------------------------- keyboard */

// C1 to C3: where riddim basses live, and two octaves fit a phone's width.
const KEY_LOW = 24;
const KEY_HIGH = 48;
const BLACK = new Set([1, 3, 6, 8, 10]);

const keyViews = new Set<() => void>();
function keyboard(): HTMLElement {
  const root = el('div', 'keys');
  const whites = el('div', 'keys__whites');
  const blacks = el('div', 'keys__blacks');
  const byNote = new Map<number, HTMLElement>();
  let whiteCount = 0;
  for (let n = KEY_LOW; n <= KEY_HIGH; n += 1) if (!BLACK.has(n % 12)) whiteCount += 1;

  let w = 0;
  for (let n = KEY_LOW; n <= KEY_HIGH; n += 1) {
    const black = BLACK.has(n % 12);
    const key = el('span', black ? 'key key--black' : 'key key--white');
    if (black) {
      key.style.left = `${(w / whiteCount) * 100}%`;
      key.style.width = `${(0.62 / whiteCount) * 100}%`;
      blacks.append(key);
    } else {
      if (n % 12 === 0) key.append(el('span', 'key__label', noteName(n)));
      whites.append(key);
      w += 1;
    }
    key.dataset.note = `${n}`;
    byNote.set(n, key);
  }
  root.append(whites, blacks);

  // One finger, one note: a glide across the keys retriggers as it crosses.
  let held: number | null = null;
  const noteAt = (e: PointerEvent): number | null => {
    const target = document.elementFromPoint(e.clientX, e.clientY);
    const key = target instanceof HTMLElement ? target.closest<HTMLElement>('.key') : null;
    return key && root.contains(key) ? Number(key.dataset.note) : null;
  };
  const play = (n: number | null): void => {
    if (n === held) return;
    if (held !== null) noteOff(held, clock());
    held = n;
    if (n !== null) noteOn(n, clock());
    for (const v of keyViews) v();
  };
  root.addEventListener('pointerdown', (e) => {
    root.setPointerCapture(e.pointerId);
    play(noteAt(e));
  });
  root.addEventListener('pointermove', (e) => root.hasPointerCapture(e.pointerId) && play(noteAt(e)));
  const up = (): void => play(null);
  root.addEventListener('pointerup', up);
  root.addEventListener('pointercancel', up);

  keyViews.add(() => {
    const on = currentNote();
    for (const [n, key] of byNote) key.dataset.on = n === on ? 'true' : 'false';
  });
  return root;
}

/* ------------------------------------------------------------------- phone */

const TABS = [
  { label: 'OSC', build: (): HTMLElement[] => [oscPanel(1, 'GROWL_TABLE_01'), oscPanel(2, 'HARD_SQUARE'), subPanel()] },
  { label: 'FILTER', build: (): HTMLElement[] => [vowelPanel(), envelopePanel()] },
  { label: 'WOBBLE', build: (): HTMLElement[] => [wobblePanel()] },
  { label: 'MOD', build: (): HTMLElement[] => [modPanel()] },
  {
    label: 'FX',
    build: (): HTMLElement[] => [
      fxPanel(1, 'dist', 'DIST', 'dist.drive', 'dist.mix'),
      fxPanel(2, 'fold', 'FOLD', 'fold.amount', 'fold.mix'),
      fxPanel(3, 'crush', 'CRUSH', 'crush.bits', 'crush.rate'),
      fxPanel(4, 'ott', 'OTT', 'ott.depth', 'ott.time'),
    ],
  },
] as const;

/**
 * The phone layout: one section at a time behind tabs, controls at finger
 * size, the scope and a keyboard always in reach. Upright, everything stacks;
 * sideways, the scope and keys take the left and the section the right.
 */
function phone(): HTMLElement {
  const pages = TABS.map((tab) => el('div', 'm__page', ...tab.build()));
  const tabs = el('nav', 'm__tabs chips');
  const buttons = TABS.map((tab, i) => {
    const b = chipButton(tab.label);
    b.addEventListener('click', () => select(i));
    tabs.append(b);
    return b;
  });
  const content = el('div', 'm__pages', ...pages);
  const select = (i: number): void => {
    pages.forEach((p, k) => (p.hidden = k !== i));
    buttons.forEach((b, k) => (b.dataset.on = k === i ? 'true' : 'false'));
    content.scrollTop = 0;
    // A page that was hidden has zero width; size its displays now.
    requestAnimationFrame(() => fit());
  };
  select(0);

  return el(
    'div',
    'm',
    el(
      'div',
      'm__side',
      el('header', 'm__top', logoCanvas('m__logo'), el('span', 'brand__word', 'GNARL'), el('div', 'top__spacer'), masterKnob()),
      el('div', 'm__preset', presetPicker(), aiButton('AI')),
      scope(320, 64),
      keyboard(),
    ),
    el('div', 'm__main', tabs, content),
  );
}

/* ------------------------------------------------------------------- toast */

let toastTimer = 0;
function toast(message: string): void {
  const node = document.querySelector<HTMLElement>('.toast');
  if (!node) return;
  node.textContent = message;
  node.dataset.show = 'true';
  window.clearTimeout(toastTimer);
  toastTimer = window.setTimeout(() => (node.dataset.show = 'false'), 2600);
}

/* -------------------------------------------------------------------- fit */

let desktopApp: HTMLElement | null = null;
let stage: HTMLElement | null = null;

/** Phone layout when the window is phone-sized in either orientation. */
function isPhone(): boolean {
  const w = window.innerWidth;
  const h = window.innerHeight;
  return w < 760 || (h < 500 && w < 1000);
}

function fit(): void {
  const phoneMode = isPhone();
  document.body.dataset.mode = phoneMode ? 'phone' : 'desktop';
  document.body.dataset.orient = window.innerHeight > window.innerWidth ? 'portrait' : 'landscape';

  let scale = 1;
  if (!phoneMode && desktopApp && stage) {
    scale = Math.min(window.innerWidth / DESIGN_W, window.innerHeight / DESIGN_H);
    desktopApp.style.transform = `scale(${scale})`;
    desktopApp.dataset.scale = `${scale}`;
    stage.style.width = `${DESIGN_W * scale}px`;
    stage.style.height = `${DESIGN_H * scale}px`;
  }
  for (const { d } of displays) if (isShown(d.root)) resizeDisplay(d, phoneMode ? 1 : scale);
  for (const { canvas } of logos) {
    if (!isShown(canvas)) continue;
    const density = Math.min(3, (window.devicePixelRatio || 1) * (phoneMode ? 1 : scale));
    canvas.width = Math.round(canvas.clientWidth * density);
    canvas.height = Math.round(canvas.clientHeight * density);
  }
}

function isShown(node: HTMLElement): boolean {
  return node.offsetParent !== null;
}

/* ------------------------------------------------------------------ start */

const t0 = performance.now();
function clock(): number {
  return (performance.now() - t0) / 1000;
}

// Computer keys for the desktop preview: A W S E D F T G Y H U J K = C2..C3.
const COMPUTER_KEYS = 'awsedftgyhujk';

function start(): void {
  stage = el('div', 'stage');
  desktopApp = desktop();
  stage.append(desktopApp);
  document.body.append(stage, phone(), el('div', 'toast'));

  fit();
  window.addEventListener('resize', fit);
  void connect().then(() => {
    for (const v of presetViews) v();
  });

  // Double-click the wordmark to reset the whole panel.
  // Not in the plugin: the page's defaults are not a patch, and the engine
  // would take every one of them.
  desktopApp.querySelector('.brand')?.addEventListener('dblclick', () => !engine.connected && resetAll());

  window.addEventListener('keydown', (e) => {
    const i = COMPUTER_KEYS.indexOf(e.key.toLowerCase());
    if (i < 0 || e.repeat || e.metaKey || e.ctrlKey) return;
    noteOn(36 + i, clock());
    for (const v of keyViews) v();
  });
  window.addEventListener('keyup', (e) => {
    const i = COMPUTER_KEYS.indexOf(e.key.toLowerCase());
    if (i < 0) return;
    noteOff(36 + i, clock());
    for (const v of keyViews) v();
  });

  const frame = (): void => {
    const t = clock();
    // Only what is on screen: the hidden layout costs nothing per frame.
    for (const { d, paint } of displays) if (isShown(d.root)) paint(t);
    for (const { canvas, paint } of logos) if (isShown(canvas)) paint(t);
    requestAnimationFrame(frame);
  };
  requestAnimationFrame(frame);
}

start();
