import { getNativeFunction } from '../juce/index.js';
import { getPluginInfo } from './pluginInfo';

/**
 * The preset browser's side of the bridge.
 *
 * NATIVE FUNCTIONS, NOT RELAYS, and for the same reason as the FX chain
 * order: none of this is a host parameter. A preset's name is not
 * automatable, a browser row is not a value, and "save" is an action rather
 * than a setting. Everything in the patch itself still goes through relays,
 * so it keeps automation, undo and gesture handling - this module only moves
 * whole patches around.
 *
 * The browser preview gets a working in-memory implementation rather than a
 * disabled panel, because a preset browser nobody can click through is a
 * panel that cannot be judged for layout - which is the whole point of the
 * preview.
 */

export interface PresetRow {
  index: number;
  name: string;
  author: string;
  category: string;
  description: string;
  tags: string[];
  /** Ships in the binary; read-only. */
  factory: boolean;
}

export interface PresetListing {
  presets: PresetRow[];
  categories: string[];
}

export interface PresetStatus {
  name: string;
  author: string;
  category: string;
  description: string;
  tags: string[];
  /** Changed since it was loaded or saved - what puts the dot next to the
      name. */
  modified: boolean;
  /** Wavetables the patch asked for are still being generated, so the sound
      is not complete yet. The UI says so rather than pretending. */
  loadingTables: boolean;
}

export interface SaveFields {
  name: string;
  author: string;
  category: string;
  description: string;
  tags: string;
}

export interface RandomiseOptions {
  /** 0..1. 0.15 is a nudge, 1 is a fresh patch. */
  amount: number;
  oscillators: boolean;
  filters: boolean;
  envelopes: boolean;
  lfos: boolean;
  modMatrix: boolean;
  fx: boolean;
}

export const DEFAULT_RANDOMISE: RandomiseOptions = {
  // A nudge by default, not a reroll. "This is close, give me a variation" is
  // what people actually reach for, and a button that throws the patch away
  // on the first click is one they stop trusting.
  amount: 0.3,
  oscillators: true,
  filters: true,
  envelopes: true,
  lfos: true,
  modMatrix: true,
  fx: true,
};

function lazyNative(name: string): (...args: unknown[]) => Promise<unknown> {
  let bound: ((...args: unknown[]) => Promise<unknown>) | null = null;

  return (...args: unknown[]) => {
    if (!bound) bound = getNativeFunction(name);
    return bound(...args);
  };
}

const listNative = lazyNative('gnarlListPresets');
const loadNative = lazyNative('gnarlLoadPreset');
const saveNative = lazyNative('gnarlSavePreset');
const deleteNative = lazyNative('gnarlDeletePreset');
const randomiseNative = lazyNative('gnarlRandomise');
const morphNative = lazyNative('gnarlMorphPresets');
const statusNative = lazyNative('gnarlPresetStatus');

function splitTags(value: unknown): string[] {
  return String(value ?? '')
    .split(',')
    .map((tag) => tag.trim())
    .filter((tag) => tag.length > 0);
}

// --- The preview's own bank -------------------------------------------------

/** Mirrors the shipped factory bank closely enough to lay the browser out
    against something realistic. Not the real thing - the real one comes from
    the plugin - but a list of three rows tells you nothing about how a bank
    of fifty scrolls. */
const MOCK_CATEGORIES = [
  'Bass', 'Growl', 'Lead', 'Pluck', 'Pad', 'Keys',
  'Drums', 'FX', 'Sequence', 'Texture',
];

const MOCK_FACTORY: Omit<PresetRow, 'index'>[] = [
  { name: 'Triplet Growl', author: 'GNARL', category: 'Growl', factory: true,
    description: 'Formant filter under a drawn LFO at 1/8 triplet.',
    tags: ['growl', 'triplet', 'formant'] },
  { name: 'Sixteenth Wobble', author: 'GNARL', category: 'Bass', factory: true,
    description: 'A 24 dB low-pass wobbling at 1/16.', tags: ['wobble', 'bass'] },
  { name: 'Reese Foundation', author: 'GNARL', category: 'Bass', factory: true,
    description: 'Two oscillators detuned through one filter.', tags: ['reese'] },
  { name: 'Screech Lead', author: 'GNARL', category: 'Lead', factory: true,
    description: 'Band-pass screech, widened and delayed.', tags: ['screech'] },
  { name: 'Sub Drop', author: 'GNARL', category: 'Bass', factory: true,
    description: 'The sub alone with the top end cut.', tags: ['sub', 'drop'] },
  { name: 'Metal Pluck', author: 'GNARL', category: 'Pluck', factory: true,
    description: 'Comb filter tracking the note.', tags: ['pluck', 'comb'] },
  { name: 'Dream Pad', author: 'GNARL', category: 'Pad', factory: true,
    description: 'Graintable pad through the widener and a long reverb.',
    tags: ['pad', 'wide'] },
  { name: 'Bitcrushed Stab', author: 'GNARL', category: 'FX', factory: true,
    description: 'Bitcrush and fold in series.', tags: ['bitcrush', 'stab'] },
  { name: 'Stacked Drive', author: 'GNARL', category: 'Growl', factory: true,
    description: 'Two distortions with an EQ between them.', tags: ['drive'] },
  { name: 'Init', author: 'GNARL', category: 'Keys', factory: true,
    description: 'Everything at its default, with the limiter on.',
    tags: ['init'] },
];

let mockUser: Omit<PresetRow, 'index'>[] = [];

let mockStatus: PresetStatus = {
  name: 'Init',
  author: '',
  category: 'Keys',
  description: '',
  tags: [],
  modified: false,
  loadingTables: false,
};

function mockRows(): PresetRow[] {
  return [...MOCK_FACTORY, ...mockUser].map((row, index) => ({ ...row, index }));
}

// --- The API ----------------------------------------------------------------

export async function listPresets(): Promise<PresetListing> {
  if (getPluginInfo().isMock) {
    return { presets: mockRows(), categories: MOCK_CATEGORIES };
  }

  const raw = (await listNative()) as { presets?: unknown; categories?: unknown };

  const presets = Array.isArray(raw?.presets)
    ? raw.presets.map((entry, fallbackIndex) => {
        const row = entry as Record<string, unknown>;

        return {
          index: typeof row.index === 'number' ? row.index : fallbackIndex,
          name: String(row.name ?? 'Untitled'),
          author: String(row.author ?? ''),
          category: String(row.category ?? ''),
          description: String(row.description ?? ''),
          tags: splitTags(row.tags),
          factory: row.factory === true,
        } satisfies PresetRow;
      })
    : [];

  const categories = Array.isArray(raw?.categories)
    ? raw.categories.map((value) => String(value))
    : [];

  return { presets, categories };
}

export async function loadPreset(index: number): Promise<boolean> {
  if (getPluginInfo().isMock) {
    const row = mockRows()[index];
    if (!row) return false;

    mockStatus = {
      name: row.name,
      author: row.author,
      category: row.category,
      description: row.description,
      tags: row.tags,
      modified: false,
      loadingTables: false,
    };

    return true;
  }

  return (await loadNative(index)) === true;
}

export async function savePreset(fields: SaveFields): Promise<boolean> {
  // Checked here as well as in C++ so the UI can say WHY rather than just
  // having the call come back false.
  if (fields.name.trim().length === 0) return false;

  if (getPluginInfo().isMock) {
    const row = {
      name: fields.name,
      author: fields.author,
      category: fields.category,
      description: fields.description,
      tags: splitTags(fields.tags),
      factory: false,
    };

    // Saving over a name that already exists replaces it, as it does on disk.
    mockUser = [...mockUser.filter((existing) => existing.name !== row.name), row];
    mockStatus = { ...row, modified: false, loadingTables: false };

    return true;
  }

  return (await saveNative({ ...fields })) === true;
}

export async function deletePreset(index: number): Promise<boolean> {
  if (getPluginInfo().isMock) {
    const row = mockRows()[index];
    if (!row || row.factory) return false;

    mockUser = mockUser.filter((existing) => existing.name !== row.name);
    return true;
  }

  return (await deleteNative(index)) === true;
}

export async function randomise(options: RandomiseOptions): Promise<boolean> {
  if (getPluginInfo().isMock) {
    mockStatus = { ...mockStatus, modified: true };
    return true;
  }

  return (await randomiseNative({ ...options })) === true;
}

export async function morphPresets(
  a: number,
  b: number,
  position: number,
): Promise<boolean> {
  if (getPluginInfo().isMock) {
    mockStatus = { ...mockStatus, modified: true };
    return true;
  }

  return (await morphNative(a, b, position)) === true;
}

export async function getPresetStatus(): Promise<PresetStatus> {
  if (getPluginInfo().isMock) return { ...mockStatus };

  const raw = (await statusNative()) as Record<string, unknown>;

  return {
    name: String(raw?.name ?? 'Init'),
    author: String(raw?.author ?? ''),
    category: String(raw?.category ?? ''),
    description: String(raw?.description ?? ''),
    tags: splitTags(raw?.tags),
    modified: raw?.modified === true,
    loadingTables: raw?.loadingTables === true,
  };
}

/** Resets the preview's bank. Only used by the tests. */
export function resetMockPresets(): void {
  mockUser = [];
  mockStatus = {
    name: 'Init', author: '', category: 'Keys', description: '',
    tags: [], modified: false, loadingTables: false,
  };
}
