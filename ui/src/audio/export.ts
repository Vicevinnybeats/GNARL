/*
 * Getting a rendered sound out of GNARL as a WAV (DRUMS, RIDDIMIZE).
 *
 * In a browser (the phone page): an ordinary download. In the plugin, where a
 * web view's download goes nowhere useful, the page hands the bytes to
 * web_panel.cpp (gnarlExportWav), which writes them to GNARL's exports folder
 * (Documents/GNARL/Exports on Windows) and answers with the file's path; the
 * producer drags it from there into FL Studio's playlist. gnarlRevealExports
 * opens that folder.
 */

import { encodeWav } from './wav.ts';

export interface Exported {
  /** What to tell the producer. */
  said: string;
  path?: string;
}

function base64(bytes: Uint8Array): string {
  let s = '';
  const chunk = 0x8000;
  for (let i = 0; i < bytes.length; i += chunk) s += String.fromCharCode(...bytes.subarray(i, i + chunk));
  return btoa(s);
}

// The plugin answers each gnarlExportWav once, in order.
const waiting: ((reply: { path?: string; error?: string }) => void)[] = [];
let listening = false;

function inPlugin(): boolean {
  const juce = window.__JUCE__;
  return juce?.backend !== undefined && juce.initialisationData.gnarlWeb !== true;
}

/** A name a file system takes: letters, digits, space, - and _. */
export function fileName(name: string): string {
  const clean = name.replace(/[^A-Za-z0-9 _-]+/g, '').trim().slice(0, 60);
  return `${clean || 'GNARL'}.wav`;
}

export function exportWav(name: string, channels: readonly Float32Array[], sampleRate: number): Promise<Exported> {
  const bytes = encodeWav(channels, sampleRate);
  const file = fileName(name);
  if (inPlugin()) {
    const backend = window.__JUCE__?.backend;
    if (!listening && backend) {
      listening = true;
      backend.addEventListener('gnarlExported', (reply: unknown) => waiting.shift()?.(reply as { path?: string; error?: string }));
    }
    return new Promise((resolve) => {
      waiting.push((r) =>
        resolve(r.error ? { said: r.error } : { said: `Saved ${file} in ${r.path ?? 'GNARL/Exports'}. Drag it into your playlist.`, path: r.path }));
      backend?.emitEvent('gnarlExportWav', { name: file, data: base64(bytes) });
    });
  }
  const url = URL.createObjectURL(new Blob([bytes as Uint8Array<ArrayBuffer>], { type: 'audio/wav' }));
  const a = document.createElement('a');
  a.href = url;
  a.download = file;
  document.body.append(a);
  a.click();
  a.remove();
  window.setTimeout(() => URL.revokeObjectURL(url), 10_000);
  return Promise.resolve({ said: `Downloaded ${file}.` });
}

/** Open the plugin's exports folder (nothing in a browser). */
export function revealExports(): boolean {
  if (!inPlugin()) return false;
  window.__JUCE__?.backend.emitEvent('gnarlRevealExports', {});
  return true;
}
