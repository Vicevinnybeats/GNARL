/**
 * Phase warps, for the display only.
 *
 * A PORT of plugin/source/dsp/WarpProcessor.h. The display has to show what
 * the warp actually does to the waveform, and the engine's version runs on the
 * audio thread in C++ where the UI cannot reach it.
 *
 * This is duplicated logic, which is a liability. `tests/warp.test.ts` checks
 * it against reference values dumped from the C++ implementation, so a change
 * on one side that is not mirrored on the other fails rather than quietly
 * drawing the wrong shape.
 *
 * FM and ring modulation are absent on purpose: they need the other
 * oscillator's output, so they are not phase warps and the display leaves the
 * waveform alone for them - which is also what the engine does to the phase.
 */
export const WARP_OFF = 0;
export const WARP_SYNC = 1;
export const WARP_BEND_PLUS = 2;
export const WARP_BEND_MINUS = 3;
export const WARP_PWM = 4;
export const WARP_ASYMMETRY = 5;
export const WARP_MIRROR = 6;
export const WARP_QUANTIZE = 7;
export const WARP_FM = 8;
export const WARP_RING_MOD = 9;
export const WARP_PHASE_DISTORTION = 10;
export const WARP_REMAP = 11;

function wrapPhase(phase: number): number {
  let p = phase;
  while (p >= 1) p -= 1;
  while (p < 0) p += 1;
  return p;
}

function clamp(value: number, low: number, high: number): number {
  return Math.min(high, Math.max(low, value));
}

function lerp(a: number, b: number, t: number): number {
  return a + (b - a) * t;
}

/** Mirrors warp::applyPhase. Returns the input unchanged at amount 0. */
export function applyWarp(phase: number, mode: number, amount: number): number {
  if (amount === 0) return phase;

  const a = clamp(amount, -1, 1);
  const magnitude = Math.abs(a);

  switch (mode) {
    case WARP_SYNC:
      return wrapPhase(phase * (1 + magnitude * 7));

    case WARP_BEND_PLUS:
      return Math.pow(phase, 1 - magnitude * 0.9);

    case WARP_BEND_MINUS:
      return Math.pow(phase, 1 + magnitude * 9);

    case WARP_PWM: {
      const width = Math.max(0.02, 1 - magnitude * 0.96);
      return Math.min(1, phase / width);
    }

    case WARP_ASYMMETRY: {
      const pivot = clamp(0.5 + a * 0.45, 0.05, 0.95);
      return phase < pivot
        ? (0.5 * phase) / pivot
        : 0.5 + (0.5 * (phase - pivot)) / (1 - pivot);
    }

    case WARP_MIRROR: {
      const folded = phase < 0.5 ? phase * 2 : (1 - phase) * 2;
      return wrapPhase(lerp(phase, folded, magnitude));
    }

    case WARP_QUANTIZE: {
      const steps = Math.max(2, (1 - magnitude) * 126 + 2);
      return Math.floor(phase * steps) / steps;
    }

    case WARP_PHASE_DISTORTION:
      return wrapPhase(phase + a * 0.5 * Math.sin(2 * Math.PI * phase));

    case WARP_REMAP: {
      const centred = phase * 2 - 1;
      const shaped = centred * (1 - magnitude) + centred * centred * centred * magnitude;
      return wrapPhase((shaped + 1) * 0.5);
    }

    default:
      // Off, FM, ring mod: the phase is untouched.
      return phase;
  }
}

/** Matches warp::getBandwidthExpansion, so the display band-limits like the
    engine does rather than drawing detail the oscillator cannot produce. */
export function warpBandwidthExpansion(mode: number, amount: number): number {
  const a = Math.abs(amount);

  switch (mode) {
    case WARP_OFF:
      return 1;
    case WARP_SYNC:
      return 1 + a * 7;
    case WARP_BEND_PLUS:
    case WARP_BEND_MINUS:
      return 1 + a * 3;
    case WARP_PWM:
      return 1 + a * 4;
    case WARP_ASYMMETRY:
      return 1 + a * 2.5;
    case WARP_MIRROR:
      return 1 + a;
    case WARP_QUANTIZE:
      return 1 + a * 12;
    case WARP_PHASE_DISTORTION:
    case WARP_REMAP:
      return 1 + a * 4;
    case WARP_FM:
      return 1 + a * 8;
    case WARP_RING_MOD:
      return 1 + a;
    default:
      return 1;
  }
}
