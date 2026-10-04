/*
 * The mobile version's engine (docs/design/phase2-09-mobile.md): GNARL's real
 * engine, compiled to WebAssembly (wasm/), running in an AudioWorklet.
 *
 * The page already knows how to talk to an engine - the plugin's, through
 * window.__JUCE__ (bridge.ts). So this installs a window.__JUCE__ whose other
 * end is the worklet, speaking the same protocol as src/plugin/web_panel.cpp,
 * and the rest of the page cannot tell the difference. Only the web page
 * carries the engine as data (data.ts); in the plugin, where window.__JUCE__
 * exists, it serves MATCH only.
 */

import coreSource from './engine-core.js?raw';
import { engineWasm, hasEngineData } from '../data';
import workletSource from './worklet.js?raw';

declare global {
  interface Navigator {
    /** Safari 17+: 'playback' plays through the ring/silent switch. */
    audioSession?: { type: string };
  }
}

type Listener = (payload: unknown) => void;

export function hasWebEngine(): boolean {
  return hasEngineData() && window.__JUCE__ === undefined;
}

function decodeBase64(text: string): ArrayBuffer {
  const binary = atob(text);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i += 1) bytes[i] = binary.charCodeAt(i);
  return bytes.buffer;
}

/** What stops the engine starting here, or null when nothing does. */
export function webEngineProblem(): string | null {
  if (typeof AudioWorkletNode === 'undefined') return 'This browser has no AudioWorklet (Safari 14.5+, Chrome 66+).';
  // The engine is built for WebAssembly SIMD (poly_float): Safari 16.4+.
  const simdProbe = new Uint8Array([
    0, 97, 115, 109, 1, 0, 0, 0, 1, 5, 1, 96, 0, 1, 123, 3, 2, 1, 0, 10, 10, 1, 8, 0, 65, 0, 253, 15, 253, 98, 11,
  ]);
  if (!WebAssembly.validate(simdProbe)) return 'This browser has no WebAssembly SIMD (Safari 16.4+, Chrome 91+).';
  return null;
}

let context: AudioContext | null = null;

/**
 * Start the engine. Call it from a tap: a phone only lets a page make sound
 * from inside a user gesture, and the AudioContext is created right here.
 * Resolves once window.__JUCE__ is in place for bridge.connect().
 */
export async function startWebEngine(): Promise<void> {
  const wasmText = engineWasm();
  if (!wasmText) throw new Error('no engine in this page');
  if (navigator.audioSession) navigator.audioSession.type = 'playback';
  context = new AudioContext({ latencyHint: 'interactive' });
  const resumed = context.resume();

  const source = `${coreSource}\n${workletSource}`;
  const url = URL.createObjectURL(new Blob([source], { type: 'text/javascript' }));
  try {
    await context.audioWorklet.addModule(url);
  } finally {
    URL.revokeObjectURL(url);
  }
  const node = new AudioWorkletNode(context, 'gnarl', {
    numberOfInputs: 0,
    outputChannelCount: [2],
    processorOptions: { wasm: decodeBase64(wasmText) },
  });
  node.connect(context.destination);

  const port = node.port;
  const listeners = new Map<string, Listener[]>();
  const emitToPage = (id: string, payload: unknown): void => {
    for (const fn of listeners.get(id) ?? []) fn(payload);
  };

  await new Promise<void>((resolve, reject) => {
    port.onmessage = (event: MessageEvent): void => {
      const message = event.data as { type: string; message?: string };
      if (message.type === 'ready') resolve();
      else if (message.type === 'error') reject(new Error(message.message));
    };
  });
  await resumed;

  port.onmessage = (event: MessageEvent): void => {
    const message = event.data as { type: string; id?: string | number; payload?: unknown; result?: unknown };
    if (message.type === 'event' && typeof message.id === 'string') emitToPage(message.id, message.payload);
    else if (message.type === 'connected') emitToPage('__juce__complete', { promiseId: message.id, result: message.result });
  };

  // A phone suspends audio when the page is hidden or a call comes in; take
  // it back on the next touch.
  const wake = (): void => {
    if (context && context.state !== 'running') void context.resume();
  };
  window.addEventListener('pointerdown', wake);
  document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'hidden') port.postMessage({ type: 'allNotesOff' });
  });

  window.__JUCE__ = {
    initialisationData: { gnarlWeb: true },
    backend: {
      addEventListener(id: string, fn: Listener): void {
        listeners.set(id, [...(listeners.get(id) ?? []), fn]);
      },
      emitEvent(id: string, payload: unknown): void {
        const p = payload as Record<string, unknown>;
        switch (id) {
          case '__juce__invoke':
            if (p.name === 'gnarlConnect') {
              const params = p.params as unknown[];
              port.postMessage({ type: 'connect', id: p.resultId, names: params[0], version: 'web' });
            }
            break;
          case 'gnarlSet':
            port.postMessage({ type: 'set', name: p.name, value: p.value });
            break;
          case 'gnarlNote':
            wake();
            port.postMessage({ type: 'note', note: p.note, on: p.on });
            break;
          case 'gnarlWobbleShape':
            port.postMessage({ type: 'shape', kind: p.kind, points: p.points });
            break;
          case 'gnarlRoute':
            port.postMessage({ type: 'route', source: p.source, destination: p.destination, amount: p.amount, remove: p.remove });
            break;
          // Presets, the web build's own (web/presets.ts).
          case 'gnarlPresetSave':
            port.postMessage({ type: 'save', name: p.name });
            break;
          case 'gnarlPresetLoad':
            port.postMessage({ type: 'load', json: p.json });
            break;
          case 'gnarlPresetInit':
            port.postMessage({ type: 'init', name: p.name });
            break;
          case 'gnarlPresetFactory':
            port.postMessage({ type: 'factory', name: p.name, shape: p.shape, settings: p.settings, patch: p.patch });
            break;
          case 'gnarlTempo':
            port.postMessage({ type: 'bpm', bpm: p.bpm });
            break;
          case 'gnarlWavetable':
            port.postMessage({ type: 'wavetable', osc: p.osc, table: p.table });
            break;
          default:
            // gnarlGesture: no host to record automation. gnarlClassic: no
            // classic editor in the browser.
            break;
        }
      },
    },
  };
}
