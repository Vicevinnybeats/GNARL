import { getSliderState } from '../juce/index.js';
import defaults from './parameterDefaults.json';

const IDS = Object.keys(defaults as Record<string, unknown>);

/** A patch as normalised values, keyed by parameter ID. */
export type PatchSnapshot = Record<string, number>;

/**
 * Captures and restores the whole parameter set, in the browser.
 *
 * WHY THIS EXISTS. In the plugin a preset is the engine's ValueTree, written
 * and read by C++. In a browser there is no engine, and "save" previously
 * stored only the preset's NAME in a list that did not survive a reload — so
 * a patch somebody built by hand could not be kept, shared, or sent to
 * anybody. That makes the browser build useless for the one thing it is
 * best at: letting somebody design a sound and hand the exact numbers over.
 *
 * Normalised rather than real-world values, deliberately. Normalised is what
 * crosses the relay and what the engine stores; converting to real units
 * here would mean duplicating every range and skew a third time, after
 * ParameterRanges.h and formatters.ts, with the same drift problem §4
 * describes for the ID lists.
 *
 * IDs are read from parameterDefaults.json rather than being listed here,
 * so a parameter added to the engine is captured without this file changing
 * — provided that file is regenerated, which §6 already requires.
 */
export function capturePatch(): PatchSnapshot {
  const snapshot: PatchSnapshot = {};

  for (const id of IDS) {
    try {
      const value = getSliderState(id)?.getNormalisedValue?.();

      if (typeof value === 'number' && Number.isFinite(value)) snapshot[id] = value;
    } catch {
      //  A parameter the relay does not know about is skipped rather than
      //  failing the whole capture: a partial patch beats no patch.
    }
  }

  return snapshot;
}

/** Applies a snapshot, ignoring IDs this build does not have. */
export function applyPatch(snapshot: PatchSnapshot): void {
  for (const [id, value] of Object.entries(snapshot)) {
    if (typeof value !== 'number' || !Number.isFinite(value)) continue;

    try {
      getSliderState(id)?.setNormalisedValue?.(Math.min(1, Math.max(0, value)));
    } catch {
      //  Same reasoning as above: an unknown ID is old or new, not fatal.
    }
  }
}

/*  STORED IN localStorage, beside the view settings (§6) rather than in the
    ValueTree. In a browser there is nowhere else: no disk, no engine. The
    key carries a version so a future change of shape can be ignored rather
    than mis-read. */
const KEY = 'gnarl.patches.v1';

export interface StoredPatch {
  name: string;
  author: string;
  category: string;
  description: string;
  tags: string[];
  saved: string;
  values: PatchSnapshot;
}

export function readStored(): StoredPatch[] {
  try {
    const raw = window.localStorage.getItem(KEY);
    const parsed: unknown = raw === null ? [] : JSON.parse(raw);

    return Array.isArray(parsed) ? (parsed as StoredPatch[]) : [];
  } catch {
    //  Blocked site data, a private window, or a corrupt entry. An empty
    //  list is the right answer to all three; throwing here would take the
    //  preset browser down with it.
    return [];
  }
}

export function writeStored(patches: StoredPatch[]): void {
  try {
    window.localStorage.setItem(KEY, JSON.stringify(patches));
  } catch {
    //  Quota, or storage refused. Saving fails silently rather than losing
    //  the patch that is still live in the parameters.
  }
}

/** The text of a `.gnarl.json` file: what gets shared or sent. */
export function exportText(patch: StoredPatch): string {
  return JSON.stringify({ format: 'gnarl-patch', version: 1, ...patch }, null, 2);
}

export function importText(text: string): StoredPatch | null {
  try {
    const parsed = JSON.parse(text) as Partial<StoredPatch> & { format?: string };

    if (parsed.format !== 'gnarl-patch' || typeof parsed.values !== 'object') return null;

    return {
      name: String(parsed.name ?? 'Imported'),
      author: String(parsed.author ?? ''),
      category: String(parsed.category ?? 'Bass'),
      description: String(parsed.description ?? ''),
      tags: Array.isArray(parsed.tags) ? parsed.tags.map(String) : [],
      saved: String(parsed.saved ?? new Date().toISOString()),
      values: parsed.values as PatchSnapshot,
    };
  } catch {
    return null;
  }
}
