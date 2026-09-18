/**
 * One frame of live modulation state.
 *
 * Pushed from C++ at 60 Hz (WebUIEditor's timer) when a plugin is behind the
 * page, and simulated in previewEngine.ts when one is not. Consumers never
 * need to know which.
 */
export interface ModulationFrame {
  /** Each LFO's current output, -1..1 when bipolar and 0..1 otherwise. */
  lfoValues: number[];
  /** Each LFO's normalised phase, for the editor's playhead. */
  lfoPhases: number[];
  /** Post-modulation table position per oscillator, 0..1. */
  tablePositions: number[];
  /** Post-modulation cutoff per filter, in Hz. */
  cutoffHz: number[];
  voices: number;
  /** True when something is sounding. With nothing playing there are no
      per-voice values to read, and the UI idles rather than freezing on the
      last note's numbers. */
  playing: boolean;
}

export const IDLE_FRAME: ModulationFrame = {
  lfoValues: [0, 0, 0, 0],
  lfoPhases: [0, 0, 0, 0],
  tablePositions: [0, 0],
  cutoffHz: [0, 0],
  voices: 0,
  playing: false,
};
