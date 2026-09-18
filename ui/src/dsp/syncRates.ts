/**
 * Tempo-synced rate divisions, mirroring plugin/source/dsp/SyncRates.h.
 *
 * Used for the editor's grid and for the browser preview's simulated LFO. A
 * "beat" is a quarter note, matching the host's PPQ position.
 */

import { GRID_DIVISION, LFO_RATE_DIVISION } from '../bridge/choices';

/** Beats per cycle, indexed by choices::LfoRateDivision. */
const BEATS_PER_CYCLE: readonly number[] = [
  32, 16, 8, // 8, 4, 2 bars
  4,         // 1/1
  3, 2, 4 / 3,           // 1/2 dotted, straight, triplet
  1.5, 1, 2 / 3,         // 1/4
  0.75, 0.5, 1 / 3,      // 1/8
  0.375, 0.25, 1 / 6,    // 1/16
  0.1875, 0.125, 1 / 12, // 1/32
  0.0625,                // 1/64
];

/**
 * Steps the grid divides ONE LFO CYCLE into, indexed by choices::GridDivision.
 *
 * THE GRID DIVIDES THE CYCLE, NOT THE BAR - see SyncRates.h for why. The
 * tempo-relative reading gave the default patch a two-step grid, so a drag
 * snapped to the start, the middle and the end and no shape could be drawn.
 */
const GRID_STEPS_PER_CYCLE: readonly number[] = [
  0,   // off
  4,   // 1/4
  8,   // 1/8
  12,  // 1/12 - triplet
  16,  // 1/16
  24,  // 1/24 - triplet
  32,  // 1/32
];

/** Divisions whose grid is a triplet one. Drawn differently in the editor,
    because mistaking a triplet grid for a straight one produces a wobble that
    is subtly out of time in a way that is very hard to diagnose by ear. */
const TRIPLET_DIVISIONS = new Set([6, 9, 12, 15, 18]);
const TRIPLET_GRIDS = new Set([3, 5]);

export function getBeatsPerCycle(divisionIndex: number): number {
  return BEATS_PER_CYCLE[divisionIndex] ?? 0.5;
}

export function getGridStepsPerCycle(gridIndex: number): number {
  return GRID_STEPS_PER_CYCLE[gridIndex] ?? 0;
}

/** The grid step as a fraction of one cycle, or 0 when the grid is off. */
export function getGridFraction(gridIndex: number): number {
  const steps = getGridStepsPerCycle(gridIndex);
  return steps > 0 ? 1 / steps : 0;
}

export function isTripletDivision(divisionIndex: number): boolean {
  return TRIPLET_DIVISIONS.has(divisionIndex);
}

export function isTripletGrid(gridIndex: number): boolean {
  return TRIPLET_GRIDS.has(gridIndex);
}

export function getFrequencyHz(divisionIndex: number, bpm: number): number {
  const beats = getBeatsPerCycle(divisionIndex);

  if (beats <= 0 || bpm <= 0) return 0;

  return bpm / 60 / beats;
}

/**
 * How many grid lines the editor draws across one cycle.
 *
 * Independent of the rate, because the grid divides the cycle. The editor
 * draws exactly one cycle, so this is the only reading that gives the user the
 * same grid whatever rate they pick.
 */
export function getGridLineCount(gridIndex: number): number {
  return getGridStepsPerCycle(gridIndex);
}

export const RATE_DIVISION_NAMES = LFO_RATE_DIVISION;
export const GRID_DIVISION_NAMES = GRID_DIVISION;
