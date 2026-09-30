/*
 * GNARL's panel. The layout is the producer's mock-up (one page, no tabs);
 * the surfaces, glow and knob readouts are the retired engine's "Dream" UI.
 *
 * The panel is laid out at one design size and SCALED to the window, never
 * reflowed - a plugin window resizes by aspect ratio, and a synth whose knobs
 * move when you resize it is a synth you cannot learn.
 */

import './styles.css';
import { drawEnvelope, drawOsc, drawSub, drawVowel, drawWobble, setDrawnPoint, DRAWN_STEPS } from './draw';
import { headerMark, mountLogo } from './logo';
import { MOD_DESTINATIONS, MOD_SOURCES, PRESET_NAMES, WOBBLE_DESTINATIONS } from './params';
import { get, resetAll, subscribe } from './store';
import { chipButton, choiceRow, display, el, knob, panel, resizeDisplay, toggle } from './widgets';
import type { Display } from './widgets';

const DESIGN_W = 1280;
const DESIGN_H = 720;

const displays: { d: Display; paint: (t: number) => void }[] = [];
function addDisplay(d: Display, paint: (t: number) => void): HTMLElement {
  displays.push({ d, paint });
  return d.root;
}

const knobs = (...nodes: HTMLElement[]): HTMLElement => el('div', 'knobs', ...nodes);

/* ------------------------------------------------------------------ header */

function header(): HTMLElement {
  const name = el('span', 'preset__name');
  let index = 0;
  const show = (): void => {
    name.textContent = PRESET_NAMES[index] ?? '';
  };
  const step = (by: number): void => {
    index = (index + by + PRESET_NAMES.length) % PRESET_NAMES.length;
    show();
  };
  const prev = el('button', 'preset__step', '‹');
  const next = el('button', 'preset__step', '›');
  prev.type = next.type = 'button';
  prev.setAttribute('aria-label', 'Previous preset');
  next.setAttribute('aria-label', 'Next preset');
  prev.addEventListener('click', () => step(-1));
  next.addEventListener('click', () => step(1));
  show();

  const ai = el('button', 'ai', el('span', 'ai__spark', '✦'), 'AI PRESET');
  ai.type = 'button';
  ai.addEventListener('click', () =>
    toast('AI presets are Phase 4, not built yet. The button is where they will live.'),
  );

  const meter = el('div', 'meter', el('span', 'meter__bar'), el('span', 'meter__bar'));
  const syncMeter = (): void => {
    meter.style.setProperty('--level', `${Math.round(get('master') * 100)}%`);
  };
  subscribe((id) => id === 'master' && syncMeter());
  syncMeter();

  const master = knob('master', { size: 28 });
  master.classList.add('knob--inline');

  return el(
    'header',
    'top',
    el('div', 'brand', headerMark(), el('span', 'brand__word', 'GNARL')),
    el('div', 'preset', prev, name, next),
    ai,
    el('div', 'top__spacer'),
    meter,
    master,
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

function vowelPanel(): HTMLElement {
  const d = display(360, 110);
  return panel(
    { title: 'VOWEL FILTER', aside: 'FORMANT ×3', power: 'vowel.on' },
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

function modPanel(): HTMLElement {
  const routes: Route[] = [
    { source: 0, dest: 0, amount: 0.72 },
    { source: 0, dest: 2, amount: 0.45 },
    { source: 1, dest: 3, amount: 0.55 },
  ];
  const MAX_ROUTES = 4;
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
          render();
        });

        const dest = el('button', 'route__dest', MOD_DESTINATIONS[route.dest] ?? '');
        dest.type = 'button';
        dest.title = 'Click to change the destination; right-click to remove';
        dest.addEventListener('click', () => {
          route.dest = (route.dest + 1) % MOD_DESTINATIONS.length;
          render();
        });
        dest.addEventListener('contextmenu', (e) => {
          e.preventDefault();
          routes.splice(i, 1);
          render();
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
      render();
    });
    list.append(add);
    count.textContent = `${routes.length}/${MAX_ROUTES}`;
  };
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

function logoPanel(): HTMLElement {
  const canvas = el('canvas', 'logo__canvas');
  const paint = mountLogo(canvas);
  logoPaint = paint;
  logoCanvas = canvas;
  return el('section', 'panel logo', canvas);
}
let logoPaint: ((t: number) => void) | null = null;
let logoCanvas: HTMLCanvasElement | null = null;

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

/* ------------------------------------------------------------------ layout */

function build(): HTMLElement {
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
    el('div', 'toast'),
  );
}

function fit(app: HTMLElement, stage: HTMLElement): void {
  const scale = Math.min(window.innerWidth / DESIGN_W, window.innerHeight / DESIGN_H);
  // On a phone held upright the panel is width-bound; centre it vertically.
  app.style.transform = `scale(${scale})`;
  app.dataset.scale = `${scale}`;
  stage.style.width = `${DESIGN_W * scale}px`;
  stage.style.height = `${DESIGN_H * scale}px`;
  for (const { d } of displays) resizeDisplay(d, scale);
  if (logoCanvas) {
    const density = Math.min(3, (window.devicePixelRatio || 1) * scale);
    logoCanvas.width = Math.round(logoCanvas.clientWidth * density);
    logoCanvas.height = Math.round(logoCanvas.clientHeight * density);
  }
}

function start(): void {
  const stage = el('div', 'stage');
  const app = build();
  stage.append(app);
  document.body.append(stage);

  fit(app, stage);
  window.addEventListener('resize', () => fit(app, stage));

  // Double-click the wordmark to reset the whole panel.
  app.querySelector('.brand')?.addEventListener('dblclick', () => resetAll());

  const t0 = performance.now();
  const frame = (now: number): void => {
    const t = (now - t0) / 1000;
    for (const { paint } of displays) paint(t);
    logoPaint?.(t);
    requestAnimationFrame(frame);
  };
  requestAnimationFrame(frame);
}

start();
