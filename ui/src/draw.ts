/*
 * What the displays draw. Each function paints one display from the store and
 * the transport clock, and is cheap enough to run every frame.
 *
 * Glow is spent on ONE trace per display, as the old UI's rule had it: every
 * shadowBlur is compositing work inside a webview sharing the machine with
 * the audio thread.
 */

import type { Display } from './widgets';
import { get } from './store';

export const CYAN = '#5fe3ff';
export const BLUE = '#3d8bff';
export const VIOLET = '#b06bff';

/** The preview's tempo. In the plugin this is the host's. */
export const PREVIEW_BPM = 140;
const CYCLES_PER_BEAT = [1, 2, 3, 4] as const;

/* ------------------------------------------------------------------ wobble */

let drawnShape: number[] = [0.1, 0.9, 0.95, 0.6, 0.2, 0.05, 0.4, 0.85, 0.9, 0.3, 0.1, 0.7, 1, 0.5, 0.15, 0.05];

export function setDrawnPoint(i: number, v: number): void {
  if (i >= 0 && i < drawnShape.length) {
    drawnShape = drawnShape.slice();
    drawnShape[i] = Math.min(1, Math.max(0, v));
  }
}
export const DRAWN_STEPS = 16;

/** The wobble's shape at phase 0..1, as 0..1 before depth. */
export function wobbleShape(phase: number): number {
  const ph = (((phase + get('wobble.phase')) % 1) + 1) % 1;
  const shape = get('wobble.shape');
  const smooth = get('wobble.smooth');
  if (shape === 0) return 0.5 - 0.5 * Math.cos(2 * Math.PI * ph);
  if (shape === 1) {
    // A square with its corners rounded by SMOOTH: tanh of a sine, normalised.
    const k = 1 + (1 - smooth) * 9;
    return 0.5 - (0.5 * Math.tanh(k * Math.cos(2 * Math.PI * ph))) / Math.tanh(k);
  }
  // DRAW: linear between the drawn steps, then smoothed by SMOOTH.
  const x = ph * DRAWN_STEPS;
  const i = Math.floor(x) % DRAWN_STEPS;
  const f = x - Math.floor(x);
  const a = drawnShape[i] ?? 0;
  const b = drawnShape[(i + 1) % DRAWN_STEPS] ?? 0;
  const eased = smooth > 0 ? f + (0.5 - 0.5 * Math.cos(Math.PI * f) - f) * smooth : f;
  return a + (b - a) * eased;
}

/** The wobble's phase at time t (seconds), locked to the preview's tempo. */
export function wobblePhase(t: number): number {
  const cycles = CYCLES_PER_BEAT[get('wobble.rate')] ?? 2;
  return (t * (PREVIEW_BPM / 60) * cycles) % 1;
}

/** The wobble's current contribution to a destination, 0 when it is not routed. */
function wobbleTo(dest: string, t: number): number {
  if (!get('wobble.on') || !get(`wobble.to.${dest}`)) return 0;
  return (wobbleShape(wobblePhase(t)) - 0.5) * get('wobble.depth');
}

/* ----------------------------------------------------------------- helpers */

function clear(d: Display): void {
  const { ctx, width, height } = d;
  ctx.clearRect(0, 0, width, height);
  ctx.strokeStyle = 'rgba(160, 130, 255, 0.07)';
  ctx.lineWidth = 1;
  ctx.beginPath();
  for (let i = 1; i < 8; i += 1) {
    const x = Math.round((width * i) / 8) + 0.5;
    ctx.moveTo(x, 0);
    ctx.lineTo(x, height);
  }
  for (let i = 1; i < 4; i += 1) {
    const y = Math.round((height * i) / 4) + 0.5;
    ctx.moveTo(0, y);
    ctx.lineTo(width, y);
  }
  ctx.stroke();
}

function trace(
  d: Display,
  fn: (x: number) => number,
  color: string,
  opts: { glow?: boolean; width?: number; alpha?: number; dash?: number[]; fill?: boolean; pad?: number } = {},
): void {
  const { ctx, width, height } = d;
  const pad = opts.pad ?? 6;
  const steps = Math.max(64, Math.round(width));
  ctx.save();
  ctx.globalAlpha = opts.alpha ?? 1;
  ctx.beginPath();
  for (let i = 0; i <= steps; i += 1) {
    const x = i / steps;
    const y = pad + (1 - fn(x)) * (height - 2 * pad);
    if (i === 0) ctx.moveTo(x * width, y);
    else ctx.lineTo(x * width, y);
  }
  if (opts.fill) {
    ctx.save();
    ctx.lineTo(width, height);
    ctx.lineTo(0, height);
    ctx.closePath();
    const g = ctx.createLinearGradient(0, 0, 0, height);
    g.addColorStop(0, `${color}55`);
    g.addColorStop(1, `${color}00`);
    ctx.fillStyle = g;
    ctx.fill();
    ctx.restore();
    ctx.beginPath();
    for (let i = 0; i <= steps; i += 1) {
      const x = i / steps;
      const y = pad + (1 - fn(x)) * (height - 2 * pad);
      if (i === 0) ctx.moveTo(x * width, y);
      else ctx.lineTo(x * width, y);
    }
  }
  if (opts.dash) ctx.setLineDash(opts.dash);
  ctx.strokeStyle = color;
  ctx.lineWidth = opts.width ?? 1.6;
  ctx.lineJoin = 'round';
  if (opts.glow) {
    ctx.shadowColor = color;
    ctx.shadowBlur = 8;
  }
  ctx.stroke();
  ctx.restore();
}

/* -------------------------------------------------------------- oscillator */

const TAU = Math.PI * 2;

/** One cycle of oscillator n at phase 0..1, -1..1. */
function oscSample(n: 1 | 2, phase: number, wt: number): number {
  const mode = get(`osc${n}.mode`);
  const warp = get(`osc${n}.warp`);
  const fm = get(`osc${n}.fm`);

  // Warp the phase the way the mode's name says.
  let ph = phase;
  if (mode === 0) {
    // FORMANT: squeeze the cycle into the front of the period.
    const w = 1 + warp * 1.5;
    ph = Math.min(1, ph * w);
  } else if (mode === 1) {
    ph = (ph * (1 + warp * 2)) % 1; // SYNC
  } else if (mode === 2) {
    ph = Math.pow(ph, 1 + warp * 2.5); // BEND
  }
  ph += fm * 0.12 * Math.sin(TAU * phase * 2);

  // The table: osc 1 morphs sine -> saw -> growl, osc 2 square -> saw.
  const sine = Math.sin(TAU * ph);
  const saw = 1 - 2 * (((ph % 1) + 1) % 1);
  const square = Math.sin(TAU * ph) >= 0 ? 0.85 : -0.85;
  let v: number;
  if (n === 1) {
    const growl = Math.tanh(2 * Math.sin(TAU * ph) + 0.8 * Math.sin(TAU * 2 * ph));
    v = wt < 0.5 ? sine + (saw - sine) * (wt * 2) : saw + (growl - saw) * ((wt - 0.5) * 2);
  } else {
    v = square + (saw - square) * wt;
  }
  if (mode === 3) v = Math.sin(v * (1 + warp * 4) * 1.6); // FOLD
  return Math.max(-1, Math.min(1, v));
}

export function drawOsc(d: Display, n: 1 | 2, t: number): void {
  clear(d);
  const on = get(`osc${n}.on`);
  const wt = Math.min(1, Math.max(0, get(`osc${n}.wtpos`) + (n === 1 ? wobbleTo('wtpos', t) * 0.6 : 0)));
  const cycles = 2;

  // The table behind the live frame, faint and receding: a wavetable is a
  // stack of frames and the live one is a slice through it.
  const { ctx } = d;
  for (let k = 4; k >= 1; k -= 1) {
    const frame = Math.min(1, Math.max(0, wt + (k - 3) * 0.12));
    ctx.save();
    ctx.translate(k * 3, -k * 2.2);
    trace(d, (x) => 0.5 + 0.38 * oscSample(n, (x * cycles) % 1, frame), BLUE, { alpha: 0.06 + 0.02 * (4 - k), width: 1 });
    ctx.restore();
  }

  // Unison: detuned copies either side of the live frame.
  const voices = Math.min(2, Math.round(get(`osc${n}.unison`)) - 1);
  const detune = get(`osc${n}.detune`);
  for (let u = 1; u <= voices; u += 1) {
    const off = ((u % 2 ? 1 : -1) * Math.ceil(u / 2) * detune) / 14;
    trace(d, (x) => 0.5 + 0.4 * oscSample(n, (((x + off) * cycles) % 1 + 1) % 1, wt), BLUE, { alpha: 0.3, width: 1 });
  }
  trace(d, (x) => 0.5 + 0.4 * oscSample(n, (x * cycles) % 1, wt), on ? CYAN : '#6b7aa8', { glow: !!on, width: 1.7 });
}

/* --------------------------------------------------------------------- sub */

export function drawSub(d: Display): void {
  clear(d);
  const drive = get('sub.drive');
  const cycles = get('sub.oct') ? 1.5 : 3;
  const level = 0.25 + get('sub.level') * 0.75;
  const k = 1 + drive * 6;
  trace(
    d,
    (x) => 0.5 + 0.42 * level * (Math.tanh(k * Math.sin(TAU * x * cycles)) / Math.tanh(k)),
    get('sub.on') ? CYAN : '#6b7aa8',
    { glow: true, width: 1.7 },
  );
}

/* ------------------------------------------------------------------- vowel */

// Formant centres (Hz) for a bass voice - the first three formants of each
// vowel, the ones that make it readable as that vowel.
const FORMANTS: readonly (readonly [number, number, number])[] = [
  [600, 1040, 2250], // A
  [400, 1620, 2400], // E
  [250, 1750, 2600], // I
  [400, 750, 2400], // O
  [350, 600, 2400], // U
];

function formantsAt(pos: number): [number, number, number] {
  const p = Math.min(4, Math.max(0, pos));
  const i = Math.min(3, Math.floor(p));
  const f = p - i;
  const a = FORMANTS[i] ?? FORMANTS[0]!;
  const b = FORMANTS[i + 1] ?? a;
  // Interpolate in log frequency: a vowel glide is heard on a pitch scale.
  return [0, 1, 2].map((k) => Math.exp(Math.log(a[k]!) + (Math.log(b[k]!) - Math.log(a[k]!)) * f)) as [
    number,
    number,
    number,
  ];
}

function response(x: number, pos: number, t: number): number {
  const hz = 60 * Math.pow(8000 / 60, x);
  const shift = Math.pow(2, (get('vowel.cutoff') - 0.5 + wobbleTo('cutoff', t) * 0.8) * 2);
  const q = 2 + get('vowel.res') * 10;
  const gains = [1, 0.7, 0.45];
  let sum = 0;
  formantsAt(pos).forEach((f, k) => {
    const fc = f * shift;
    const r = Math.log2(hz / fc) * q;
    sum += (gains[k] ?? 0) / (1 + r * r);
  });
  const drive = get('vowel.drive');
  const v = Math.tanh(sum * (1 + drive * 2)) / Math.tanh(1 + drive * 2);
  return 0.06 + 0.84 * v;
}

export function drawVowel(d: Display, t: number): void {
  clear(d);
  const pos = get('vowel.vowel') + (get('vowel.morph') - 0.5) * 2 + wobbleTo('vowel', t) * 2;
  const on = get('vowel.on');
  trace(d, (x) => response(x, pos + 1, t), VIOLET, { alpha: 0.8, width: 1.2, dash: [3, 3] });
  trace(d, (x) => response(x, pos, t), on ? CYAN : '#6b7aa8', { glow: !!on, width: 1.7, fill: true });
}

/* ------------------------------------------------------------------ wobble */

export function drawWobble(d: Display, t: number): void {
  clear(d);
  const depth = 0.15 + get('wobble.depth') * 0.85;
  const cycles = 2;
  const on = get('wobble.on');
  const fn = (x: number): number => 0.5 + (wobbleShape((x * cycles) % 1) - 0.5) * depth;
  trace(d, fn, on ? VIOLET : '#6b7aa8', { glow: !!on, width: 1.8, fill: true });

  if (get('wobble.shape') === 2) {
    // The drawn steps, as handles.
    const { ctx, width, height } = d;
    ctx.fillStyle = 'rgba(176, 107, 255, 0.55)';
    for (let c = 0; c < cycles; c += 1) {
      for (let i = 0; i < DRAWN_STEPS; i += 1) {
        const x = ((c + i / DRAWN_STEPS) / cycles) * width;
        const y = 6 + (1 - fn((c + i / DRAWN_STEPS) / cycles)) * (height - 12);
        ctx.fillRect(x - 1.5, y - 1.5, 3, 3);
      }
    }
  }

  if (on) {
    // The playhead, where the wobble is on the grid right now.
    const { ctx, width, height } = d;
    const ph = wobblePhase(t);
    const x = (ph / cycles) * width;
    const y = 6 + (1 - fn(ph / cycles)) * (height - 12);
    ctx.save();
    ctx.strokeStyle = 'rgba(176, 107, 255, 0.35)';
    ctx.beginPath();
    ctx.moveTo(x + 0.5, 0);
    ctx.lineTo(x + 0.5, height);
    ctx.stroke();
    ctx.fillStyle = '#f1e6ff';
    ctx.shadowColor = VIOLET;
    ctx.shadowBlur = 10;
    ctx.beginPath();
    ctx.arc(x, y, 3.2, 0, TAU);
    ctx.fill();
    ctx.restore();
  }
}

/* ---------------------------------------------------------------- envelope */

export function drawEnvelope(d: Display): void {
  clear(d);
  const page = get('env.page') ? 'filter' : 'amp';
  const a = get(`env.${page}.att`);
  const dc = get(`env.${page}.dec`);
  const s = get(`env.${page}.sus`);
  const r = get(`env.${page}.rel`);
  // Segment widths follow the knobs' skew, with a floor so a zero-length
  // attack still reads as a (vertical) segment rather than vanishing.
  const wa = 0.02 + a * 0.3;
  const wd = 0.04 + dc * 0.3;
  const wr = 0.04 + r * 0.3;
  const hold = 0.22;
  const total = wa + wd + hold + wr;
  const xa = wa / total;
  const xd = xa + wd / total;
  const xs = xd + hold / total;
  const fn = (x: number): number => {
    if (x < xa) return x / xa;
    if (x < xd) {
      const f = (x - xa) / (xd - xa);
      return 1 - (1 - s) * (1 - Math.pow(1 - f, 3));
    }
    if (x < xs) return s;
    const f = (x - xs) / (1 - xs);
    return s * Math.pow(1 - f, 3);
  };
  trace(d, fn, CYAN, { glow: true, width: 1.7, fill: true });
}
