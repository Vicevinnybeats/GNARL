/*
 * The panel's building blocks: panels, knobs, button rows and displays.
 * Plain DOM, no framework - the whole UI is a few hundred elements and the
 * plugin's webview is sharing a machine with the audio thread.
 */

import { CHOICES, PARAM_BY_ID, formatValue } from './params';
import { engine } from './engine';
import { gesture, get, set, subscribe } from './store';

export type Accent = 'blue' | 'violet';

export function el<K extends keyof HTMLElementTagNameMap>(
  tag: K,
  cls = '',
  ...children: (Node | string)[]
): HTMLElementTagNameMap[K] {
  const node = document.createElement(tag);
  if (cls) node.className = cls;
  node.append(...children);
  return node;
}

/* ------------------------------------------------------------------ panel */

export interface PanelOptions {
  title: string;
  /** Right-hand header text, e.g. the loaded wavetable's name. */
  aside?: string | HTMLElement;
  accent?: Accent;
  /** Store id toggled by the header dot; the panel dims when it is off. */
  power?: string;
  cls?: string;
}

export function panel(opts: PanelOptions, ...body: Node[]): HTMLElement {
  const dot = el('button', 'panel__dot');
  dot.type = 'button';
  const title = el('span', 'panel__title', opts.title);
  const head = el('div', 'panel__head', el('div', 'panel__name', dot, title));
  if (opts.aside !== undefined) {
    head.append(typeof opts.aside === 'string' ? el('span', 'panel__aside', opts.aside) : opts.aside);
  }
  const root = el('section', `panel ${opts.cls ?? ''}`, head, el('div', 'panel__body', ...body));
  root.dataset.accent = opts.accent ?? 'blue';

  if (opts.power) {
    const id = opts.power;
    dot.dataset.param = id;
    dot.title = 'Switch this section on or off';
    dot.setAttribute('aria-label', `${opts.title} on/off`);
    dot.addEventListener('click', () => set(id, get(id) ? 0 : 1));
    const sync = (): void => {
      root.dataset.off = get(id) ? 'false' : 'true';
      dot.setAttribute('aria-pressed', get(id) ? 'true' : 'false');
    };
    subscribe((changed) => changed === id && sync());
    sync();
  } else {
    dot.disabled = true;
  }
  return root;
}

/* ------------------------------------------------------------------- knob */

// 270 degrees of travel, the gap at the bottom, as on every hardware knob.
const SWEEP = 270;
const START = -135;

function polar(cx: number, cy: number, r: number, deg: number): [number, number] {
  const a = ((deg - 90) * Math.PI) / 180;
  return [cx + r * Math.cos(a), cy + r * Math.sin(a)];
}

function arc(cx: number, cy: number, r: number, from: number, to: number): string {
  if (Math.abs(to - from) < 0.01) return '';
  const [x0, y0] = polar(cx, cy, r, from);
  const [x1, y1] = polar(cx, cy, r, to);
  const large = Math.abs(to - from) > 180 ? 1 : 0;
  const sweep = to > from ? 1 : 0;
  return `M${x0.toFixed(2)} ${y0.toFixed(2)}A${r} ${r} 0 ${large} ${sweep} ${x1.toFixed(2)} ${y1.toFixed(2)}`;
}

const SVG = 'http://www.w3.org/2000/svg';

export function knob(id: string, opts: { size?: number; accent?: Accent; label?: string } = {}): HTMLElement {
  const p = PARAM_BY_ID.get(id);
  if (!p) throw new Error(`knob: unknown parameter ${id}`);
  const size = opts.size ?? 34;
  const c = 20;
  const r = 15;

  const svg = document.createElementNS(SVG, 'svg');
  svg.setAttribute('viewBox', '0 0 40 40');
  svg.setAttribute('width', `${size}`);
  svg.setAttribute('height', `${size}`);
  svg.classList.add('knob__dial');

  const body = document.createElementNS(SVG, 'circle');
  body.setAttribute('cx', `${c}`);
  body.setAttribute('cy', `${c}`);
  body.setAttribute('r', '11');
  body.setAttribute('class', 'knob__body');

  const track = document.createElementNS(SVG, 'path');
  track.setAttribute('d', arc(c, c, r, START, START + SWEEP));
  track.setAttribute('class', 'knob__track');

  const value = document.createElementNS(SVG, 'path');
  value.setAttribute('class', 'knob__value');

  const pointer = document.createElementNS(SVG, 'line');
  pointer.setAttribute('class', 'knob__pointer');

  svg.append(body, track, value, pointer);

  const readout = el('span', 'knob__readout');
  const root = el('div', 'knob', svg, el('span', 'knob__label', opts.label ?? p.label), readout);
  root.dataset.accent = opts.accent ?? 'blue';
  root.dataset.param = id;
  root.tabIndex = 0;
  root.setAttribute('role', 'slider');
  root.setAttribute('aria-label', opts.label ?? p.label);

  const norm = (v: number): number => (v - p.min) / (p.max - p.min);
  const denorm = (n: number): number => {
    const v = p.min + Math.min(1, Math.max(0, n)) * (p.max - p.min);
    return p.step ? Math.round(v / p.step) * p.step : v;
  };

  const draw = (): void => {
    const v = get(id);
    const n = norm(v);
    const angle = START + n * SWEEP;
    const from = p.bipolar ? 0 : START;
    value.setAttribute('d', arc(c, c, r, Math.min(from, angle), Math.max(from, angle)));
    const [x0, y0] = polar(c, c, 4, angle);
    const [x1, y1] = polar(c, c, 10, angle);
    pointer.setAttribute('x1', x0.toFixed(2));
    pointer.setAttribute('y1', y0.toFixed(2));
    pointer.setAttribute('x2', x1.toFixed(2));
    pointer.setAttribute('y2', y1.toFixed(2));
    readout.textContent = engine.text.get(id) ?? formatValue(p, v);
    root.setAttribute('aria-valuetext', readout.textContent);
  };

  // Drag: 200 px for the full range, a tenth of that speed with shift - the
  // same feel as a DAW's own knobs, which is what a producer's hand expects.
  let dragging = false;
  let lastY = 0;
  let acc = 0;
  svg.addEventListener('pointerdown', (e) => {
    dragging = true;
    lastY = e.clientY;
    acc = norm(get(id));
    svg.setPointerCapture(e.pointerId);
    root.dataset.active = 'true';
    gesture(id, true);
    e.preventDefault();
  });
  svg.addEventListener('pointermove', (e) => {
    if (!dragging) return;
    const scale = scaleOf(svg);
    const dy = (lastY - e.clientY) / scale;
    lastY = e.clientY;
    acc = Math.min(1, Math.max(0, acc + dy / (e.shiftKey ? 2000 : 200)));
    set(id, denorm(acc));
  });
  const end = (): void => {
    if (dragging) gesture(id, false);
    dragging = false;
    delete root.dataset.active;
  };
  svg.addEventListener('pointerup', end);
  svg.addEventListener('pointercancel', end);
  svg.addEventListener('dblclick', () => set(id, p.def));
  root.addEventListener(
    'wheel',
    (e) => {
      e.preventDefault();
      const stepN = p.step ? p.step / (p.max - p.min) : 0.01;
      set(id, denorm(norm(get(id)) - Math.sign(e.deltaY) * stepN));
    },
    { passive: false },
  );
  root.addEventListener('keydown', (e) => {
    const stepN = p.step ? p.step / (p.max - p.min) : e.shiftKey ? 0.001 : 0.01;
    if (e.key === 'ArrowUp' || e.key === 'ArrowRight') set(id, denorm(norm(get(id)) + stepN));
    else if (e.key === 'ArrowDown' || e.key === 'ArrowLeft') set(id, denorm(norm(get(id)) - stepN));
    else return;
    e.preventDefault();
  });

  subscribe((changed) => changed === id && draw());
  draw();
  return root;
}

/*
 * A performance wheel: a vertical strip, dragged like a hardware wheel
 * (140 px for the full range, so a thumb can sweep it in one move). A
 * springing wheel - pitch - returns to its default when let go, inside the
 * same gesture, so a DAW records the return too.
 */
export function wheel(id: string, opts: { spring?: boolean; accent?: Accent } = {}): HTMLElement {
  const p = PARAM_BY_ID.get(id);
  if (!p) throw new Error(`wheel: unknown parameter ${id}`);
  const fill = el('span', 'wheel__fill');
  const thumb = el('span', 'wheel__thumb');
  const track = el('div', 'wheel__track', fill, thumb);
  const root = el('div', 'wheel', track, el('span', 'wheel__label', p.label));
  root.dataset.accent = opts.accent ?? 'blue';
  root.dataset.param = id;
  root.tabIndex = 0;
  root.setAttribute('role', 'slider');
  root.setAttribute('aria-label', p.label);
  root.title = opts.spring ? `${p.label}: drag, springs back to centre` : `${p.label}: drag`;

  const norm = (v: number): number => (v - p.min) / (p.max - p.min);
  const denorm = (n: number): number => p.min + Math.min(1, Math.max(0, n)) * (p.max - p.min);

  const draw = (): void => {
    const n = norm(get(id));
    const from = p.bipolar ? 0.5 : 0;
    thumb.style.bottom = `${n * 100}%`;
    fill.style.bottom = `${Math.min(from, n) * 100}%`;
    fill.style.height = `${Math.abs(n - from) * 100}%`;
    root.setAttribute('aria-valuetext', engine.text.get(id) ?? formatValue(p, get(id)));
  };

  let dragging = false;
  let lastY = 0;
  let acc = 0;
  track.addEventListener('pointerdown', (e) => {
    dragging = true;
    lastY = e.clientY;
    acc = norm(get(id));
    track.setPointerCapture(e.pointerId);
    root.dataset.active = 'true';
    gesture(id, true);
    e.preventDefault();
  });
  track.addEventListener('pointermove', (e) => {
    if (!dragging) return;
    const dy = (lastY - e.clientY) / scaleOf(track);
    lastY = e.clientY;
    acc = Math.min(1, Math.max(0, acc + dy / 140));
    set(id, denorm(acc));
  });
  const end = (): void => {
    if (!dragging) return;
    dragging = false;
    if (opts.spring) set(id, p.def);
    gesture(id, false);
    delete root.dataset.active;
  };
  track.addEventListener('pointerup', end);
  track.addEventListener('pointercancel', end);
  track.addEventListener('dblclick', () => set(id, p.def));
  root.addEventListener('keydown', (e) => {
    if (e.key === 'ArrowUp') set(id, denorm(norm(get(id)) + 0.05));
    else if (e.key === 'ArrowDown') set(id, denorm(norm(get(id)) - 0.05));
    else return;
    e.preventDefault();
  });
  if (opts.spring) root.addEventListener('keyup', () => set(id, p.def));

  subscribe((changed) => changed === id && draw());
  draw();
  return root;
}

/** The app is scaled with a transform; pointer deltas arrive in screen pixels. */
function scaleOf(node: Element): number {
  const app = node.closest<HTMLElement>('.app');
  return app ? Number(app.dataset.scale ?? '1') || 1 : 1;
}

/* ------------------------------------------------------------- button rows */

/** One-of-N buttons bound to a choice. */
export function choiceRow(id: string, opts: { accent?: Accent; cls?: string } = {}): HTMLElement {
  const choice = CHOICES.find((c) => c.id === id);
  if (!choice) throw new Error(`choiceRow: unknown choice ${id}`);
  const row = el('div', `chips ${opts.cls ?? ''}`);
  const cycle = (opts.cls ?? '').split(' ').includes('chips--cycle');
  row.dataset.param = id;
  const buttons = choice.options.map((label, i) => {
    const b = chipButton(label, opts.accent);
    b.dataset.option = `${i}`;
    b.addEventListener('click', () => {
      // A single-option row is a toggle (MONO, -1 OCT).
      if (choice.options.length === 1) set(id, get(id) ? 0 : 1);
      // A cycle row shows one button: each tap steps to the next option
      // (from none lit, to the first).
      else if (cycle) set(id, (Math.max(-1, get(id)) + 1) % choice.options.length);
      else set(id, i);
    });
    row.append(b);
    return b;
  });
  const sync = (): void => {
    const v = get(id);
    buttons.forEach((b, i) => {
      const on = choice.options.length === 1 ? v === 1 : v === i;
      b.dataset.on = on ? 'true' : 'false';
      b.setAttribute('aria-pressed', on ? 'true' : 'false');
    });
    // An engine value with no button (a preset's 2/1 rate) lights none.
    row.dataset.none = buttons.some((b) => b.dataset.on === 'true') ? 'false' : 'true';
  };
  subscribe((changed) => changed === id && sync());
  sync();
  return row;
}

/** An on/off button bound to a 0/1 store value. */
export function toggle(id: string, label: string, accent?: Accent): HTMLButtonElement {
  const b = chipButton(label, accent);
  b.dataset.param = id;
  b.addEventListener('click', () => set(id, get(id) ? 0 : 1));
  const sync = (): void => {
    b.dataset.on = get(id) ? 'true' : 'false';
    b.setAttribute('aria-pressed', get(id) ? 'true' : 'false');
  };
  subscribe((changed) => changed === id && sync());
  sync();
  return b;
}

export function chipButton(label: string, accent?: Accent): HTMLButtonElement {
  const b = el('button', 'chip', label);
  b.type = 'button';
  if (accent) b.dataset.accent = accent;
  return b;
}

/* --------------------------------------------------------------- displays */

export interface Display {
  root: HTMLElement;
  canvas: HTMLCanvasElement;
  ctx: CanvasRenderingContext2D;
  width: number;
  height: number;
}

/**
 * A canvas sized in CSS pixels at the design size and backed at the device's
 * pixel density times the app's scale, so a trace stays one sharp pixel wide
 * whether the window is small or full-screen.
 */
export function display(width: number, height: number, cls = ''): Display {
  const canvas = el('canvas', 'display__canvas');
  const root = el('div', `display ${cls}`, canvas);
  root.style.height = `${height}px`;
  const ctx = canvas.getContext('2d');
  if (!ctx) throw new Error('display: no 2D context');
  const d: Display = { root, canvas, ctx, width, height };
  resizeDisplay(d, 1);
  return d;
}

export function resizeDisplay(d: Display, appScale: number): void {
  const density = Math.min(3, (window.devicePixelRatio || 1) * appScale);
  d.width = d.root.clientWidth || d.width;
  d.canvas.width = Math.round(d.width * density);
  d.canvas.height = Math.round(d.height * density);
  d.canvas.style.width = `${d.width}px`;
  d.canvas.style.height = `${d.height}px`;
  d.ctx.setTransform(density, 0, 0, density, 0, 0);
}
