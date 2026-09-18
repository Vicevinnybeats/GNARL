import { getNativeFunction } from '../juce/index.js';
import { defaultRamp, type CurvePoint, type LfoCurveData } from '../dsp/lfoCurve';
import { getPluginInfo } from './pluginInfo';
import { MOD_DESTINATIONS } from './modDestinations';
import { COUNTS } from './parameterIds';

/**
 * The modulation state that is NOT a host parameter.
 *
 * Two things cannot be AudioProcessorValueTreeState parameters, and both live
 * in the plugin's ValueTree instead (see params/ModStateBridge.h):
 *
 *   - the drawable LFO curves, because 32 breakpoints x 4 fields x 4 LFOs is
 *     384 automatable floats nobody would ever automate individually;
 *   - the mod slots' destinations, because a host parameter is a number, and a
 *     number indexing a list of destinations cannot stay stable across
 *     releases. They are stored as parameter-ID strings.
 *
 * Everything else the UI touches is a parameter and goes through a relay, so it
 * keeps automation, undo and gesture handling. This module is the narrow
 * exception, not a second way to reach the engine.
 */

export interface DestinationOption {
  /** The parameter ID, which is what gets stored. */
  id: string;
  /** The label for the picker. */
  name: string;
}

export interface ModState {
  curves: CurvePoint[][];
  /** Parameter ID per slot; an empty string means "not routed". */
  destinations: string[];
  /** Every destination the engine knows, from its own table - so a
      destination cannot exist in the picker and not in the matrix. */
  available: DestinationOption[];
}

interface RawModState {
  curves?: unknown;
  destinations?: unknown;
  available?: unknown;
}

const getModStateNative = lazyNative('gnarlGetModState');
const setLfoCurveNative = lazyNative('gnarlSetLfoCurve');
const setModDestinationNative = lazyNative('gnarlSetModDestination');

/**
 * Binds a native function on first use rather than at module load.
 *
 * JUCE's frontend warns to the console for every binding it does not
 * recognise, and in the browser preview none of these exist. Binding lazily
 * means the preview starts clean instead of with three warnings.
 */
function lazyNative(name: string): (...args: unknown[]) => Promise<unknown> {
  let bound: ((...args: unknown[]) => Promise<unknown>) | null = null;

  return (...args: unknown[]) => {
    if (!bound) bound = getNativeFunction(name);
    return bound(...args);
  };
}

/** The preview's own copy, so the MOD tab is fully usable with no plugin
    behind the page - which is what makes the screenshots meaningful. The
    destination list comes from the mirror of the engine's own table, because
    an empty picker makes the whole matrix untestable in the preview. */
const mockState: ModState = {
  curves: Array.from({ length: COUNTS.lfos }, () => defaultRamp()),
  destinations: Array.from({ length: COUNTS.modSlots }, () => ''),
  available: MOD_DESTINATIONS.map((entry) => ({ ...entry })),
};

export async function fetchModState(): Promise<ModState> {
  if (getPluginInfo().isMock) {
    return {
      curves: mockState.curves.map((points) => points.map((point) => ({ ...point }))),
      destinations: [...mockState.destinations],
      available: mockState.available,
    };
  }

  const raw = (await getModStateNative()) as RawModState;

  return {
    curves: parseCurves(raw.curves),
    destinations: parseDestinations(raw.destinations),
    available: parseAvailable(raw.available),
  };
}

export async function pushLfoCurve(lfoIndex: number, points: LfoCurveData): Promise<void> {
  if (getPluginInfo().isMock) {
    mockState.curves[lfoIndex] = points.map((point) => ({ ...point }));
    return;
  }

  await setLfoCurveNative(lfoIndex, points.map((point) => ({ ...point })));
}

export async function pushModDestination(slotIndex: number, parameterId: string): Promise<void> {
  if (getPluginInfo().isMock) {
    mockState.destinations[slotIndex] = parameterId;
    return;
  }

  await setModDestinationNative(slotIndex, parameterId);
}

// --- Parsing ----------------------------------------------------------------
//
// Everything crossing the bridge arrives as `unknown`. A malformed field falls
// back to something drawable rather than throwing: a UI that blanks because one
// number arrived as a string is worse than one that shows a default ramp.

function parseCurves(raw: unknown): CurvePoint[][] {
  const curves: CurvePoint[][] = [];

  for (let i = 0; i < COUNTS.lfos; i += 1) {
    const entry = Array.isArray(raw) ? raw[i] : undefined;

    if (!Array.isArray(entry) || entry.length === 0) {
      curves.push(defaultRamp());
      continue;
    }

    const points = entry
      .map((point) => parsePoint(point))
      .filter((point): point is CurvePoint => point !== null);

    curves.push(points.length > 0 ? points : defaultRamp());
  }

  return curves;
}

function parsePoint(raw: unknown): CurvePoint | null {
  if (typeof raw !== 'object' || raw === null) return null;

  const record = raw as Record<string, unknown>;

  const time = Number(record.time);
  const value = Number(record.value);

  if (!Number.isFinite(time) || !Number.isFinite(value)) return null;

  const tension = Number(record.tension);

  return {
    time: Math.min(1, Math.max(0, time)),
    value: Math.min(1, Math.max(0, value)),
    tension: Number.isFinite(tension) ? Math.min(1, Math.max(-1, tension)) : 0,
    step: record.step === true,
  };
}

function parseDestinations(raw: unknown): string[] {
  return Array.from({ length: COUNTS.modSlots }, (_unused, i) => {
    const entry = Array.isArray(raw) ? raw[i] : undefined;
    return typeof entry === 'string' ? entry : '';
  });
}

function parseAvailable(raw: unknown): DestinationOption[] {
  if (!Array.isArray(raw)) return [];

  const options: DestinationOption[] = [];

  for (const entry of raw) {
    if (typeof entry !== 'object' || entry === null) continue;

    const record = entry as Record<string, unknown>;

    if (typeof record.id === 'string' && typeof record.name === 'string')
      options.push({ id: record.id, name: record.name });
  }

  return options;
}
