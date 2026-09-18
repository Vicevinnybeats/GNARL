import { getSliderState } from '../juce/index.js';
import { evaluateBuiltInShape, evaluateCurve, type CurvePoint } from '../dsp/lfoCurve';
import { getFrequencyHz, getGridFraction } from '../dsp/syncRates';
import { FILTER, LFO, MOD, OSC } from './parameterIds';
import { fetchModState } from './modState';
import type { ModulationFrame } from './modulationFrame';

/**
 * A simulation of the engine's modulation, for the BROWSER PREVIEW ONLY.
 *
 * In the plugin the live values are pushed from C++ (see WebUIEditor's timer),
 * because the UI must show what the engine is actually doing. With no plugin
 * behind the page there is nothing to push, and a modulation UI that sits
 * perfectly still is impossible to design against or screenshot honestly - the
 * whole point of the MOD tab is that things move.
 *
 * So this evaluates the LFOs and the matrix in TypeScript, from the same
 * parameter values and the same curve data, and produces the same frame shape
 * the plugin pushes. It is NOT a second engine: it is never loaded when a
 * plugin is present, it only computes what the display draws, and any
 * disagreement with C++ shows up as the preview animating differently from the
 * plugin rather than as a wrong sound.
 */

/** The preview's own transport. A fixed tempo, running whenever the page is
    open, because a preview with a stopped transport shows nothing. */
const PREVIEW_BPM = 140;

/** Which display field each modulation destination feeds. Only the
    destinations the display actually draws are listed; the rest are evaluated
    by the engine and simply not shown yet. */
const DESTINATION_TARGETS: Record<string, { field: 'table' | 'cutoff'; index: number }> = {
  [OSC[0].tablePos]: { field: 'table', index: 0 },
  [OSC[1].tablePos]: { field: 'table', index: 1 },
  [FILTER[0].cutoff]: { field: 'cutoff', index: 0 },
  [FILTER[1].cutoff]: { field: 'cutoff', index: 1 },
};

/** Matches VoiceSettings.cpp: a full-depth modulation sweeps the cutoff six
    octaves, so a wobble travels evenly across the keyboard rather than
    spending its whole range above 10 kHz. */
const OCTAVES_AT_FULL_DEPTH = 6;

type Listener = (frame: ModulationFrame) => void;

let listeners: Listener[] = [];
let rafHandle: number | null = null;
let startedAt = 0;
let curves: CurvePoint[][] = [];
let destinations: string[] = [];

/** Read once per frame. Reading a slider state is a property access on an
    object the mock already holds, so this is cheap enough to do per frame and
    means the preview responds to a knob immediately. */
function read(id: string): number {
  try {
    return getSliderState(id).getScaledValue();
  } catch {
    return 0;
  }
}

function readIndex(id: string): number {
  return Math.round(read(id));
}

function readBool(id: string): boolean {
  return read(id) >= 0.5;
}

/** One LFO's value at a moment in the preview's timeline. */
function evaluateLfo(index: number, seconds: number): { value: number; phase: number } {
  const ids = LFO[index];
  if (!ids) return { value: 0, phase: 0 };

  const shape = readIndex(ids.shape);
  const mode = readIndex(ids.mode);
  const synced = readBool(ids.syncEnabled);
  const division = readIndex(ids.rateDivision);
  const bipolar = readBool(ids.bipolar);
  const offset = read(ids.phase);

  const frequency = synced ? getFrequencyHz(division, PREVIEW_BPM) : read(ids.rateHz);

  let phase = frequency > 0 ? (seconds * frequency) % 1 : 0;

  // Envelope mode is a one-shot, and the preview has no note to trigger it, so
  // it is drawn parked at its end value - which is what it would be doing for
  // all but the first moment of a note anyway.
  if (mode === 1) phase = 1;

  phase = (phase + offset) % 1;

  let value =
    shape === 0
      ? evaluateCurve(curves[index] ?? [], phase)
      : evaluateBuiltInShape(shape, phase);

  // S&H quantises the shape onto the grid, so a smooth curve becomes the
  // staircase this genre is built on.
  if (mode === 3) {
    const fraction = getGridFraction(readIndex(ids.gridDivision));

    if (fraction > 0) {
      const stepped = Math.floor(phase / fraction) * fraction;

      value =
        shape === 0
          ? evaluateCurve(curves[index] ?? [], stepped)
          : evaluateBuiltInShape(shape, stepped);
    }
  }

  return { value: bipolar ? value * 2 - 1 : value, phase };
}

/** The matrix, as ModMatrix::apply does it: sources are 0..1, a bipolar slot
    re-centres them, and the depth scales the result. Curves are omitted here
    because the display would not show the difference. */
function accumulateOffsets(lfoValues: number[]): Map<string, number> {
  const offsets = new Map<string, number>();

  for (let slot = 0; slot < MOD.length; slot += 1) {
    const ids = MOD[slot];
    const destination = destinations[slot];

    if (!ids || !destination) continue;
    if (!readBool(ids.enabled)) continue;

    const source = readIndex(ids.source);

    // Sources 5..8 are LFO 1..4; the preview only animates those, since the
    // others are per-note and there are no notes here.
    if (source < 5 || source > 8) continue;

    const raw = lfoValues[source - 5] ?? 0;
    const bipolar = readBool(ids.bipolar);
    const value = bipolar ? raw * 2 - 1 : raw;

    offsets.set(destination, (offsets.get(destination) ?? 0) + value * read(ids.depth));
  }

  return offsets;
}

function buildFrame(seconds: number): ModulationFrame {
  const lfoValues: number[] = [];
  const lfoPhases: number[] = [];
  const rawLfoValues: number[] = [];

  for (let i = 0; i < LFO.length; i += 1) {
    const { value, phase } = evaluateLfo(i, seconds);
    lfoValues.push(value);
    lfoPhases.push(phase);

    // The matrix works on the unipolar source value; the frame reports what
    // the LFO outputs, which may be bipolar.
    const ids = LFO[i];
    rawLfoValues.push(ids && readBool(ids.bipolar) ? (value + 1) / 2 : value);
  }

  const offsets = accumulateOffsets(rawLfoValues);

  const tablePositions = OSC.map((ids) => read(ids.tablePos));
  const cutoffHz = FILTER.map((ids) => read(ids.cutoff));

  for (const [destination, offset] of offsets) {
    const target = DESTINATION_TARGETS[destination];
    if (!target) continue;

    if (target.field === 'table') {
      const base = tablePositions[target.index] ?? 0;
      tablePositions[target.index] = Math.min(1, Math.max(0, base + offset));
    } else {
      const base = cutoffHz[target.index] ?? 0;
      cutoffHz[target.index] = Math.min(
        20000,
        Math.max(20, base * Math.pow(2, offset * OCTAVES_AT_FULL_DEPTH)),
      );
    }
  }

  return {
    lfoValues,
    lfoPhases,
    tablePositions,
    cutoffHz,
    voices: 0,
    playing: true,
  };
}

function tick(now: number): void {
  if (startedAt === 0) startedAt = now;

  const frame = buildFrame((now - startedAt) / 1000);

  for (const listener of listeners) listener(frame);

  rafHandle = requestAnimationFrame(tick);
}

/** Subscribes to the preview's simulated frames. Returns an unsubscribe. */
export function subscribeToPreviewFrames(listener: Listener): () => void {
  listeners.push(listener);

  if (rafHandle === null) {
    // The curves come from the same place the plugin's would, so the preview
    // animates the shape the user has actually drawn.
    void fetchModState().then((state) => {
      curves = state.curves;
      destinations = state.destinations;
    });

    rafHandle = requestAnimationFrame(tick);
  }

  return () => {
    listeners = listeners.filter((entry) => entry !== listener);

    if (listeners.length === 0 && rafHandle !== null) {
      cancelAnimationFrame(rafHandle);
      rafHandle = null;
      startedAt = 0;
    }
  };
}

/** Called by the MOD tab after an edit, so the preview animates the new shape
    without waiting for a refetch. */
export function setPreviewCurves(next: CurvePoint[][]): void {
  curves = next;
}

export function setPreviewDestinations(next: string[]): void {
  destinations = next;
}
