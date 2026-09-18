import { getNativeFunction } from '../juce/index.js';
import { getPluginInfo } from './pluginInfo';
import { FX_SLOT_NAME } from './choices';

/**
 * The FX chain's ORDER.
 *
 * Not a parameter, and deliberately so. A host parameter is a number, and a
 * number indexing an ordered list of effects changes meaning the moment the
 * list does - so every saved preset's chain would silently rearrange. It is
 * also not something anyone automates: a lane that reordered the chain
 * mid-note would be a glitch generator rather than a feature. So the order
 * lives in the plugin's ValueTree and reaches the UI through native functions,
 * exactly like the drawable LFO curves and the mod destinations.
 *
 * Everything else in the rack - every enable, mix, drive and cutoff - IS a
 * parameter and goes through a relay, so it keeps automation, undo and gesture
 * handling. This module is the narrow exception.
 *
 * See docs/fx-architecture.md and plugin/source/params/FxOrderBridge.h.
 */

export const FX_SLOT_COUNT = FX_SLOT_NAME.length;

/** Slot indices, in the order they run. */
export type FxOrder = readonly number[];

interface RawFxOrder {
  slots?: unknown;
}

function lazyNative(name: string): (...args: unknown[]) => Promise<unknown> {
  let bound: ((...args: unknown[]) => Promise<unknown>) | null = null;

  return (...args: unknown[]) => {
    if (!bound) bound = getNativeFunction(name);
    return bound(...args);
  };
}

const getFxOrderNative = lazyNative('gnarlGetFxOrder');
const setFxOrderNative = lazyNative('gnarlSetFxOrder');
const moveFxSlotNative = lazyNative('gnarlMoveFxSlot');

export function defaultOrder(): number[] {
  return Array.from({ length: FX_SLOT_COUNT }, (_, i) => i);
}

/**
 * True only for a list that is a PERMUTATION of the slots.
 *
 * The same check the C++ side makes, and for the same reason: a chain that
 * runs one effect twice and another never is not a chain that can exist, and a
 * reorder that produced one would be invisible in the list while changing what
 * the user hears. Checking here as well means the UI never sends one.
 */
export function isPermutation(order: readonly number[]): boolean {
  if (order.length !== FX_SLOT_COUNT) return false;

  const seen = new Array<boolean>(FX_SLOT_COUNT).fill(false);

  for (const slot of order) {
    if (!Number.isInteger(slot) || slot < 0 || slot >= FX_SLOT_COUNT) return false;
    if (seen[slot]) return false;
    seen[slot] = true;
  }

  return true;
}

/** Moves one slot to a new position, shifting the rest - what a drag does. */
export function withMoved(order: readonly number[], from: number, to: number): number[] {
  const moved = [...order];

  if (from < 0 || to < 0 || from >= moved.length || to >= moved.length) return moved;

  const [slot] = moved.splice(from, 1);
  if (slot === undefined) return [...order];

  moved.splice(to, 0, slot);
  return moved;
}

/** The preview's own copy, so the rack is fully usable with no plugin behind
    the page - which is what makes a screenshot of the FX tab meaningful. */
let mockOrder: number[] = defaultOrder();

export async function fetchFxOrder(): Promise<number[]> {
  if (getPluginInfo().isMock) return [...mockOrder];

  const raw = (await getFxOrderNative()) as RawFxOrder;

  if (!Array.isArray(raw?.slots)) return defaultOrder();

  const parsed = raw.slots.map((value) => Number(value));

  // A malformed reply falls back to the default rather than being patched up:
  // a half-valid order is worse than a known one, because the list would then
  // disagree with what the engine runs.
  return isPermutation(parsed) ? parsed : defaultOrder();
}

export async function pushFxOrder(order: readonly number[]): Promise<boolean> {
  if (!isPermutation(order)) return false;

  if (getPluginInfo().isMock) {
    mockOrder = [...order];
    return true;
  }

  return (await setFxOrderNative([...order])) === true;
}

export async function moveFxSlot(from: number, to: number): Promise<boolean> {
  if (getPluginInfo().isMock) {
    mockOrder = withMoved(mockOrder, from, to);
    return true;
  }

  return (await moveFxSlotNative(from, to)) === true;
}

/** Resets the preview's copy. Only used by the tests. */
export function resetMockFxOrder(): void {
  mockOrder = defaultOrder();
}
