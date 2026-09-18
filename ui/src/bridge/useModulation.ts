import { useEffect, useRef, useState } from 'react';

import { IDLE_FRAME, type ModulationFrame } from './modulationFrame';
import { getPluginInfo } from './pluginInfo';
import { subscribeToPreviewFrames } from './previewEngine';

/**
 * The live modulation state, pushed from the engine at 60 Hz.
 *
 * WHY PUSHED AND NOT COMPUTED HERE. The display has to show what the engine is
 * actually doing. A UI that runs its own copy of the LFO drifts from the
 * sound - different clock, different phase, and no way to tell which one is
 * lying - so in the plugin the values come from the voice that is playing.
 * The browser preview has no voice to read, so previewEngine.ts simulates one;
 * consumers do not need to know which they are getting.
 *
 * Kept OUT of React state by default. At 60 Hz, setState per frame re-renders
 * the whole subtree sixty times a second, which is exactly what makes a
 * webview UI feel heavy inside a DAW. Canvas consumers use
 * `useModulationRef`, which mutates a ref the animation loop reads;
 * `useModulationFrame` exists for the rare consumer that really does need a
 * re-render.
 */

const JUCE_EVENT_ID = 'gnarlModulation';

type Listener = (frame: ModulationFrame) => void;

let listeners: Listener[] = [];
let unsubscribeBackend: (() => void) | null = null;
let latest: ModulationFrame = IDLE_FRAME;

function parseFrame(payload: unknown): ModulationFrame | null {
  if (typeof payload !== 'object' || payload === null) return null;

  const record = payload as Record<string, unknown>;

  const numbers = (value: unknown, length: number): number[] =>
    Array.from({ length }, (_unused, i) => {
      const entry = Array.isArray(value) ? Number(value[i]) : NaN;
      return Number.isFinite(entry) ? entry : 0;
    });

  return {
    lfoValues: numbers(record.lfoValues, 4),
    lfoPhases: numbers(record.lfoPhases, 4),
    tablePositions: numbers(record.tablePositions, 2),
    cutoffHz: numbers(record.cutoffHz, 2),
    voices: Number(record.voices) || 0,
    playing: record.playing === true,
  };
}

function ensureSubscribed(): void {
  if (unsubscribeBackend !== null) return;

  if (getPluginInfo().isMock) {
    unsubscribeBackend = subscribeToPreviewFrames(publish);
    return;
  }

  const token = window.__JUCE__.backend.addEventListener(JUCE_EVENT_ID, (payload) => {
    const frame = parseFrame(payload);
    if (frame) publish(frame);
  });

  unsubscribeBackend = () => window.__JUCE__.backend.removeEventListener(token);
}

function publish(frame: ModulationFrame): void {
  latest = frame;
  for (const listener of listeners) listener(frame);
}

function subscribe(listener: Listener): () => void {
  listeners.push(listener);
  ensureSubscribed();

  return () => {
    listeners = listeners.filter((entry) => entry !== listener);

    if (listeners.length === 0 && unsubscribeBackend) {
      unsubscribeBackend();
      unsubscribeBackend = null;
      latest = IDLE_FRAME;
    }
  };
}

/**
 * A ref holding the newest frame, updated without re-rendering.
 *
 * The right hook for anything drawing on a canvas: the draw loop already runs
 * on requestAnimationFrame and can simply read the ref.
 */
export function useModulationRef(): React.MutableRefObject<ModulationFrame> {
  const ref = useRef<ModulationFrame>(latest);

  useEffect(() => {
    ref.current = latest;
    return subscribe((frame) => {
      ref.current = frame;
    });
  }, []);

  return ref;
}

/**
 * The newest frame as React state, re-rendering on every change.
 *
 * Use sparingly - see the note above. `throttleMs` exists so a text readout
 * can update a few times a second without costing sixty renders.
 */
export function useModulationFrame(throttleMs = 100): ModulationFrame {
  const [frame, setFrame] = useState<ModulationFrame>(latest);
  const lastPublished = useRef(0);

  useEffect(() => {
    return subscribe((next) => {
      const now = performance.now();

      if (now - lastPublished.current < throttleMs) return;

      lastPublished.current = now;
      setFrame(next);
    });
  }, [throttleMs]);

  return frame;
}
