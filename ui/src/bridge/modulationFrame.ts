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
  /** Post-master output level per channel, dBFS, floored at METER_FLOOR_DB.

      dB rather than a 0..1 bar position, so the UI owns the scale: the
      engine reports a measurement and the meter decides how to draw it. */
  outputDb: number[];
  /** The OTT's gain change per band, signed: negative is downward
      compression, positive the upward lift. */
  ottGainDb: number[];
  voices: number;
  /** True when something is sounding. With nothing playing there are no
      per-voice values to read, and the UI idles rather than freezing on the
      last note's numbers. */
  playing: boolean;
}

/** Bottom of the meter scale, matching `GnarlProcessor::kMeterFloorDb`. The
    engine already clamps to it; repeated here because the UI maps dB to a bar
    length and needs the same bottom to map against. */
export const METER_FLOOR_DB = -60;

/** dBFS to a 0..1 bar length.
 *
 * Deliberately NOT linear in dB. A linear-in-dB meter spends half its travel
 * below -30 dBFS, where a synth almost never sits, and the top 6 dB - the
 * only part anyone actually watches - gets a tenth of the bar. The exponent
 * pushes travel towards the top without the scale stopping being a dB scale.
 */
export function meterPosition(db: number): number {
  const clamped = Math.min(0, Math.max(METER_FLOOR_DB, db));
  return Math.pow(1 - clamped / METER_FLOOR_DB, 1.7);
}

/** Gain reduction to a -1..1 bipolar bar. The OTT can pull a band down by
    far more than it ever lifts one, but a bipolar meter has to be symmetric
    or the centre tick stops meaning unity. */
export const GAIN_REDUCTION_RANGE_DB = 24;

export function gainReductionPosition(db: number): number {
  return Math.min(1, Math.max(-1, db / GAIN_REDUCTION_RANGE_DB));
}

export const IDLE_FRAME: ModulationFrame = {
  lfoValues: [0, 0, 0, 0],
  lfoPhases: [0, 0, 0, 0],
  tablePositions: [0, 0],
  cutoffHz: [0, 0],
  outputDb: [METER_FLOOR_DB, METER_FLOOR_DB],
  ottGainDb: [0, 0, 0],
  voices: 0,
  playing: false,
};
