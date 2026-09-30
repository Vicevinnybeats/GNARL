/*
 * The preview voice: which note is held and how loud its envelope is.
 *
 * In the browser there is no engine, so the scope draws a MODEL of the patch
 * (draw.ts: scopeSample) shaped by this envelope. In the plugin the note is
 * also played by the engine (bridge.ts sets `noteSink`) and the scope shows
 * the engine's real output.
 */

import { get } from './store';

let note: number | null = null;
let onAt = 0;
let offAt = 0;
let levelAtOff = 0;

/** Time controls use the same skew the readouts show: v^3 over 0..8 s. */
function seconds(id: string): number {
  return Math.pow(get(id), 3) * 8;
}

function sustainLevel(): number {
  return get('env.amp.sus');
}

/** The amp envelope while held, `dt` seconds after note-on. */
function held(dt: number): number {
  const a = Math.max(0.0005, seconds('env.amp.att'));
  const d = Math.max(0.001, seconds('env.amp.dec'));
  if (dt < a) return dt / a;
  // Decay is exponential towards the sustain level, reaching ~95% in `d`.
  const s = sustainLevel();
  return s + (1 - s) * Math.exp(((a - dt) * 3) / d);
}

/** Where notes go besides the preview: the engine, inside the plugin. */
export let noteSink: ((midi: number, on: boolean) => void) | null = null;
export function setNoteSink(sink: (midi: number, on: boolean) => void): void {
  noteSink = sink;
}

export function noteOn(midi: number, now: number): void {
  if (note !== null && note !== midi) noteSink?.(note, false);
  noteSink?.(midi, true);
  note = midi;
  onAt = now;
}

export function noteOff(midi: number, now: number): void {
  if (note !== midi) return;
  noteSink?.(midi, false);
  levelAtOff = held(now - onAt);
  offAt = now;
  note = null;
}

export function currentNote(): number | null {
  return note;
}

/** The last note played, for the pitch while the release rings out. */
let lastNote = 36;
export function sounding(now: number): { midi: number; amp: number } {
  if (note !== null) {
    lastNote = note;
    return { midi: note, amp: held(now - onAt) };
  }
  const r = Math.max(0.002, seconds('env.amp.rel'));
  const amp = offAt > 0 ? levelAtOff * Math.exp((-(now - offAt) * 3) / r) : 0;
  return { midi: lastNote, amp: amp < 0.002 ? 0 : amp };
}

const NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'] as const;
export function noteName(midi: number): string {
  return `${NAMES[midi % 12]}${Math.floor(midi / 12) - 1}`;
}
