/**
 * The drawable LFO curve, ported from plugin/source/dsp/LfoCurve.h.
 *
 * TWO IMPLEMENTATIONS OF ONE SHAPE is a risk, so this file is a deliberate,
 * narrow port: `evaluate` and `applyTension` only, matching the C++ line for
 * line. The editor has to draw exactly what the engine will play, and the
 * browser preview has to move the same way the plugin does, so the alternative
 * is round-tripping every pixel of the curve over the bridge at 60 Hz.
 *
 * The C++ side owns the data; this side only reads and draws it. Any change to
 * the evaluation rules has to land in both, which is what the
 * cross-implementation test in ui/src/dsp/lfoCurve.test.ts checks.
 */

/** Matches LfoCurve::kMaxPoints. */
export const MAX_CURVE_POINTS = 32;

export interface CurvePoint {
  /** 0..1 across one cycle of the LFO, not seconds - so one shape works at
      every rate and tempo. */
  time: number;
  /** 0..1. */
  value: number;
  /** -1..1, shaping the segment that FOLLOWS this point. */
  tension: number;
  /** Holds this point's value until the next one, instead of interpolating. */
  step: boolean;
}

export type LfoCurveData = readonly CurvePoint[];

/** The shape a fresh LFO opens with: a falling ramp. */
export function defaultRamp(): CurvePoint[] {
  return [
    { time: 0, value: 1, tension: 0, step: false },
    { time: 1, value: 0, tension: 0, step: false },
  ];
}

/**
 * Bends a 0..1 ramp by a tension in -1..1.
 *
 * Positive holds low then rises late; negative rises fast then flattens; zero
 * is exactly linear. Monotonic for every tension, because a non-monotonic
 * segment in an LFO reads as a glitch rather than as a curve.
 */
export function applyTension(t: number, tension: number): number {
  const clampedTension = Math.min(1, Math.max(-1, tension));

  if (clampedTension === 0) return t;

  // Tension +-1 maps to an exponent of 8 or 1/8, matching the C++ side.
  const exponent = Math.pow(2, clampedTension * 3);

  return Math.pow(Math.min(1, Math.max(0, t)), exponent);
}

/** Evaluates the curve at a normalised phase. */
export function evaluateCurve(points: LfoCurveData, phase: number): number {
  if (points.length === 0) return 0;

  const t = Math.min(1, Math.max(0, phase));
  const first = points[0];
  if (!first) return 0;

  if (points.length === 1) return first.value;

  // Before the first point and after the last, the end values are held rather
  // than wrapped: a curve that does not start at 0 or end at 1 is legal, and
  // wrapping would put a discontinuity in it.
  if (t <= first.time) return first.value;

  const last = points[points.length - 1];
  if (!last) return 0;

  if (t >= last.time) return last.value;

  for (let i = 0; i < points.length - 1; i += 1) {
    const a = points[i];
    const b = points[i + 1];

    if (!a || !b) break;
    if (t > b.time) continue;

    if (a.step) return a.value;

    const span = b.time - a.time;

    if (span <= 0) return b.value;

    const local = (t - a.time) / span;

    return a.value + (b.value - a.value) * applyTension(local, a.tension);
  }

  return last.value;
}

/** Keeps a curve in the shape `evaluate` relies on: ascending in time, with
    every field in range. The editor calls this after every edit, because a
    drag can move a point past its neighbour. */
export function normaliseCurve(points: LfoCurveData): CurvePoint[] {
  return points
    .map((point) => ({
      time: Math.min(1, Math.max(0, point.time)),
      value: Math.min(1, Math.max(0, point.value)),
      tension: Math.min(1, Math.max(-1, point.tension)),
      step: point.step,
    }))
    .sort((a, b) => a.time - b.time)
    .slice(0, MAX_CURVE_POINTS);
}

/** Built-in shapes, matching Lfo::evaluateShape. Index is choices::LfoShape. */
export function evaluateBuiltInShape(shapeIndex: number, phase: number): number {
  const t = Math.min(1, Math.max(0, phase));

  switch (shapeIndex) {
    case 1: // sine
      return 0.5 - 0.5 * Math.cos(2 * Math.PI * t);
    case 2: // triangle
      return t < 0.5 ? t * 2 : 2 - t * 2;
    case 3: // saw up
      return t;
    case 4: // saw down
      return 1 - t;
    case 5: // square
      return t < 0.5 ? 1 : 0;
    case 6: // random step
    case 7: // random smooth
      return hashedRandom(t, shapeIndex === 7);
    default:
      return 0;
  }
}

/**
 * Deterministic per-step random, matching Lfo::evaluateRandom.
 *
 * Hashed from the step index rather than drawn from a running generator,
 * because a wobble that never repeats cannot be played to.
 */
function hashedRandom(phase: number, smooth: boolean, steps = 16): number {
  const position = phase * steps;
  const step = Math.floor(position);
  const value = hashToUnit(step);

  if (!smooth) return value;

  const next = hashToUnit(step + 1);
  return value + (next - value) * (position - step);
}

function hashToUnit(step: number): number {
  // The C++ side's hash, in 32-bit unsigned arithmetic. Math.imul keeps the
  // multiplications 32-bit; >>> 0 keeps the results unsigned.
  let h = Math.imul(step | 0, 0x9e3779b1) >>> 0;
  h = (h ^ (h >>> 15)) >>> 0;
  h = Math.imul(h, 0x2545f491) >>> 0;
  h = (h ^ (h >>> 13)) >>> 0;

  return (h & 0xffffff) / 0xffffff;
}
