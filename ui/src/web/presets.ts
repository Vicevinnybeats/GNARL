/*
 * The mobile version's presets (docs/design/phase2-09-mobile.md): patches the
 * engine saves and loads itself, in the plugin's own .vital format, so a
 * patch made on the phone opens in the plugin and the other way round.
 *
 * Saved patches live in this browser's IndexedDB - a patch is about 180 kB,
 * mostly wavetables, which would fill localStorage after a couple of dozen.
 * Where IndexedDB is refused (a private window), they live for the visit.
 * OPEN reads a .vital file; EXPORT hands one back. The five starting sounds
 * are factory.json, applied over the init by the worklet.
 */

import factory from './factory.json';
import vinnyBass2 from '../../../presets/Vinny Bass 2.vital?raw';
import riddimSub from '../../../presets/Riddim Sub.vital?raw';

// The full patches the starting sounds name (presets/*.vital). A new one is
// added here and in factory.json.
const PATCHES: Readonly<Record<string, string>> = { 'Vinny Bass 2': vinnyBass2, 'Riddim Sub': riddimSub };

interface Stored {
  name: string;
  json: string;
  saved: number;
}

export interface LoadResult {
  /** 0 loaded, 1 not a patch, 2 from a newer GNARL, 3 from an older version. */
  result: number;
  name: string;
}

const DB_NAME = 'gnarl';
const STORE = 'presets';
const memory = new Map<string, Stored>();
let opening: Promise<IDBDatabase | null> | null = null;

function database(): Promise<IDBDatabase | null> {
  opening ??= new Promise((resolve) => {
    try {
      const request = indexedDB.open(DB_NAME, 1);
      request.onupgradeneeded = (): void => {
        request.result.createObjectStore(STORE, { keyPath: 'name' });
      };
      request.onsuccess = (): void => resolve(request.result);
      request.onerror = (): void => resolve(null);
      request.onblocked = (): void => resolve(null);
    } catch {
      resolve(null);
    }
  });
  return opening;
}

function transaction<T>(mode: IDBTransactionMode, run: (store: IDBObjectStore) => IDBRequest<T>): Promise<T | null> {
  return database().then(
    (db) =>
      new Promise<T | null>((resolve) => {
        if (!db) {
          resolve(null);
          return;
        }
        try {
          const request = run(db.transaction(STORE, mode).objectStore(STORE));
          request.onsuccess = (): void => resolve(request.result);
          request.onerror = (): void => resolve(null);
        } catch {
          resolve(null);
        }
      }),
  );
}

export async function listPresets(): Promise<Stored[]> {
  const stored = (await transaction('readonly', (store) => store.getAll() as IDBRequest<Stored[]>)) ?? [];
  const all = new Map<string, Stored>();
  for (const p of stored) all.set(p.name, p);
  for (const p of memory.values()) all.set(p.name, p);
  return [...all.values()].sort((a, b) => a.name.localeCompare(b.name));
}

async function storePreset(name: string, json: string): Promise<boolean> {
  const entry: Stored = { name, json, saved: Date.now() };
  const done = await transaction('readwrite', (store) => store.put(entry));
  if (done === null) memory.set(name, entry);
  return done !== null;
}

export async function deletePreset(name: string): Promise<void> {
  memory.delete(name);
  await transaction('readwrite', (store) => store.delete(name));
}

/* ------------------------------------------------- talking to the engine */

type Waiter = (payload: unknown) => void;
const waiting = new Map<string, Waiter[]>();
let listening = false;

function ask<T>(event: string, payload: unknown, answer: string): Promise<T> {
  const backend = window.__JUCE__?.backend;
  if (!backend) return Promise.reject(new Error('no engine'));
  if (!listening) {
    listening = true;
    for (const id of ['gnarlPresetSaved', 'gnarlPresetLoaded']) {
      backend.addEventListener(id, (reply) => waiting.get(id)?.shift()?.(reply));
    }
  }
  return new Promise<T>((resolve) => {
    waiting.set(answer, [...(waiting.get(answer) ?? []), resolve as Waiter]);
    backend.emitEvent(event, payload);
  });
}

/** Save the current patch under `name`: in the engine's .vital JSON, stored here. */
export async function savePatch(name: string): Promise<{ json: string; stored: boolean }> {
  const reply = await ask<{ name: string; json: string }>('gnarlPresetSave', { name }, 'gnarlPresetSaved');
  if (!reply.json) throw new Error('the engine could not save this patch');
  return { json: reply.json, stored: await storePreset(name, reply.json) };
}

export function loadPatch(json: string): Promise<LoadResult> {
  return ask<LoadResult>('gnarlPresetLoad', { json }, 'gnarlPresetLoaded');
}

/** The engine's init patch (the page then sends its own defaults over it). */
export function initPatch(): void {
  window.__JUCE__?.backend.emitEvent('gnarlPresetInit', { name: 'Init' });
}

/**
 * A starting sound (factory.json): a full patch (presets/*.vital), or the
 * init patch plus a few settings and a wobble shape.
 */
export interface FactorySound {
  name: string;
  patch?: string;
  shape?: string;
  settings?: Record<string, number>;
}

// JSON infers a union of the entries; each is a FactorySound.
export const FACTORY_SOUNDS = factory.sounds as unknown as readonly FactorySound[];

export function loadFactory(sound: FactorySound): void {
  const patch = sound.patch === undefined ? undefined : PATCHES[sound.patch];
  window.__JUCE__?.backend.emitEvent('gnarlPresetFactory', patch === undefined ? sound : { name: sound.name, patch });
}

/** Why a load was refused, in the page's words. */
export function loadProblem(result: number): string {
  switch (result) {
    case 2:
      return 'That patch was made by a newer GNARL. Update, then open it again.';
    case 3:
      return 'That patch is from an older version. Open it in the plugin and save it there once; then it opens here.';
    default:
      return 'That file is not a GNARL patch.';
  }
}

/*
 * Inside a claude.ai Artifact a page may not start a download itself; the
 * viewer's `downloads` capability offers the file instead, after the viewer
 * confirms. It accepts a fixed list of extensions, without .vital, so there
 * the file is NAME.vital.json: renamed to .vital it opens in the plugin.
 * Anywhere else (the page served on its own) it is a plain download.
 */
interface ViewerDownloads {
  save(request: { filename: string; data: string }): Promise<{ status: string }>;
}
interface Viewer {
  use(name: 'downloads'): Promise<ViewerDownloads | null>;
}

/** Hand the patch back as a .vital file. Resolves with a sentence for the page. */
export async function exportPatch(name: string, json: string): Promise<string> {
  const base = name.replace(/[\\/:*?"<>|]/g, '_') || 'GNARL';
  const viewer = (window as unknown as { claude?: Viewer }).claude;
  if (viewer?.use) {
    const downloads = await viewer.use('downloads').catch(() => null);
    if (downloads) {
      try {
        await downloads.save({ filename: `${base}.vital.json`, data: json });
        return `Saved ${base}.vital.json. Rename it to ${base}.vital to open it in the plugin.`;
      } catch (error) {
        const code = (error as { code?: string }).code ?? '';
        return code === 'declined' ? 'Export cancelled.' : 'This view cannot save files.';
      }
    }
  }
  const url = URL.createObjectURL(new Blob([json], { type: 'application/json' }));
  const a = document.createElement('a');
  a.href = url;
  a.download = `${base}.vital`;
  document.body.append(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 10_000);
  return `Exported ${base}.vital.`;
}
