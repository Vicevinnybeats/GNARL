import {
  DISPLAY_FRAME_COUNT,
  DISPLAY_HARMONIC_COUNT,
  DISPLAY_SPECTRA_BASE64,
  DISPLAY_TABLE_COUNT,
} from './wavetableSpectra';

/**
 * Wavetable data for the display.
 *
 * Stored as harmonics rather than samples, so the waveform can be synthesised
 * at exactly the width the canvas is drawing and re-synthesised when the warp
 * changes. The data is analysed back out of the tables the engine generates,
 * so what is drawn is what the oscillator plays.
 */
export interface FrameSpectrum {
  /** Linear magnitudes, index 0 is the fundamental. */
  magnitude: Float32Array;
  /** Radians. */
  phase: Float32Array;
}

let decoded: FrameSpectrum[][] | null = null;

function decodeAll(): FrameSpectrum[][] {
  if (decoded) return decoded;

  const binary = atob(DISPLAY_SPECTRA_BASE64);
  const tables: FrameSpectrum[][] = [];

  let offset = 0;

  for (let table = 0; table < DISPLAY_TABLE_COUNT; table += 1) {
    const frames: FrameSpectrum[] = [];

    for (let frame = 0; frame < DISPLAY_FRAME_COUNT; frame += 1) {
      const magnitude = new Float32Array(DISPLAY_HARMONIC_COUNT);
      const phase = new Float32Array(DISPLAY_HARMONIC_COUNT);

      for (let h = 0; h < DISPLAY_HARMONIC_COUNT; h += 1) {
        // Magnitude is an unsigned byte 0..127; phase is signed -127..127
        // mapping onto -pi..pi.
        const rawMagnitude = binary.charCodeAt(offset) & 0xff;
        offset += 1;

        let rawPhase = binary.charCodeAt(offset) & 0xff;
        offset += 1;
        if (rawPhase > 127) rawPhase -= 256;

        magnitude[h] = rawMagnitude / 127;
        phase[h] = (rawPhase / 127) * Math.PI;
      }

      frames.push({ magnitude, phase });
    }

    tables.push(frames);
  }

  decoded = tables;
  return tables;
}

export function getTableFrames(tableIndex: number): FrameSpectrum[] {
  const tables = decodeAll();
  const index = Math.min(tables.length - 1, Math.max(0, Math.round(tableIndex)));
  return tables[index] ?? tables[0] ?? [];
}

export { DISPLAY_FRAME_COUNT, DISPLAY_HARMONIC_COUNT };

/**
 * Synthesises one cycle of a frame into `out`.
 *
 * A direct additive sum rather than an inverse FFT: at display widths there
 * are only a few hundred output samples and a few dozen harmonics, so the
 * naive loop is fast enough and avoids shipping an FFT to draw a line.
 *
 * `harmonicLimit` band-limits the drawn waveform the way the engine's mip map
 * band-limits the played one, so the display does not show detail the
 * oscillator would never produce.
 */
export function synthesiseFrame(
  spectrum: FrameSpectrum,
  out: Float32Array,
  harmonicLimit = DISPLAY_HARMONIC_COUNT,
  warp?: (phase: number) => number,
): void {
  const width = out.length;
  const limit = Math.min(harmonicLimit, spectrum.magnitude.length);

  let peak = 0;

  for (let i = 0; i < width; i += 1) {
    const basePhase = i / width;
    const phase = warp ? warp(basePhase) : basePhase;

    let sum = 0;

    for (let h = 0; h < limit; h += 1) {
      const magnitude = spectrum.magnitude[h] ?? 0;
      if (magnitude === 0) continue;

      sum += magnitude * Math.sin(2 * Math.PI * (h + 1) * phase + (spectrum.phase[h] ?? 0));
    }

    out[i] = sum;
    const absolute = Math.abs(sum);
    if (absolute > peak) peak = absolute;
  }

  // Normalised per frame so every frame fills the display's height. The engine
  // normalises the whole table instead, but a display that shows quiet frames
  // as flat lines is not telling the user anything.
  if (peak > 0) {
    const scale = 1 / peak;
    for (let i = 0; i < width; i += 1) out[i] = (out[i] ?? 0) * scale;
  }
}

/** Blends two frames, for a table position between them. */
export function synthesiseInterpolated(
  frames: FrameSpectrum[],
  position: number,
  out: Float32Array,
  harmonicLimit?: number,
  warp?: (phase: number) => number,
): void {
  const clamped = Math.min(1, Math.max(0, position));
  const scaled = clamped * (frames.length - 1);
  const lower = Math.floor(scaled);
  const upper = Math.min(frames.length - 1, lower + 1);
  const blend = scaled - lower;

  const a = frames[lower];
  const b = frames[upper];

  if (!a || !b) return;

  if (blend <= 0.001) {
    synthesiseFrame(a, out, harmonicLimit, warp);
    return;
  }

  // Blended in the harmonic domain, matching the engine's frame interpolation
  // more closely than crossfading two rendered waveforms would.
  const blended: FrameSpectrum = {
    magnitude: new Float32Array(a.magnitude.length),
    phase: new Float32Array(a.phase.length),
  };

  for (let h = 0; h < a.magnitude.length; h += 1) {
    const magnitudeA = a.magnitude[h] ?? 0;
    const magnitudeB = b.magnitude[h] ?? 0;
    const phaseA = a.phase[h] ?? 0;
    const phaseB = b.phase[h] ?? 0;

    blended.magnitude[h] = magnitudeA + (magnitudeB - magnitudeA) * blend;
    blended.phase[h] = phaseA + (phaseB - phaseA) * blend;
  }

  synthesiseFrame(blended, out, harmonicLimit, warp);
}
