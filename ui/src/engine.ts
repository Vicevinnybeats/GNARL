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
  /** The modulation matrix: every connection, amount -1..1. */
  routes: [] as EngineRoute[],
  /** The plugin's licence banner (Phase 7); null in a build without one. */
  licence: null as EngineLicence | null,
};

/** src/plugin/web_panel.cpp WebPanel::licence. */
export interface EngineLicence {
  /** licensed, offline, expired, invalid, unlicensed, unenforced, personal */
  status: string;
  /** The sentence for the banner; empty when there is nothing to say. */
  message: string;
  /** Whether preset files may be saved. Audio is never gated. */
  saving: boolean;
  hasKey: boolean;
}

export interface EngineRoute {
  source: string;
  destination: string;
  amount: number;
}

/** Views that show engine state outside the per-frame displays (the preset). */
export const engineViews = new Set<() => void>();
