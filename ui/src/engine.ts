/*
 * What the engine tells the page when it runs inside the plugin: the real
 * output, where the wobble is, its real shape, the loaded preset. bridge.ts
 * fills it; the displays read it. In the browser `connected` stays false and
 * the displays draw their model of the patch instead.
 */

export const engine = {
  connected: false,
  /** The output, one note period as Vital's oscilloscope captures it, -1..1. */
  scope: null as Float32Array | null,
  /** 0..1 through the wobble's cycle, or -1 when no voice is playing. */
  wobblePhase: -1,
  /** The wobble's shape over one cycle, 0..1 (1 is the top). */
  curve: null as readonly number[] | null,
  preset: null as string | null,
  /** The engine's own readout per control id: only it knows its units. */
  text: new Map<string, string>(),
};

/** Views that show engine state outside the per-frame displays (the preset). */
export const engineViews = new Set<() => void>();
