import { evaluateBuiltInShape, evaluateCurve, type CurvePoint } from '../dsp/lfoCurve';

/**
 * The LFO, for the browser preview.
 *
 * THIS IS THE GROWL. Five Vital patches the client supplied all route their
 * first LFO to oscillator LEVEL at full depth — the gate, not a filter
 * sweep — and the drawn shape is uneven within one cycle. The preview had no
 * LFO of that kind at all: it applied a fixed sine to the cutoff, so the one
 * mechanism the whole genre rests on could not be heard or designed here.
 *
 * It also read `lfo1_rate`, which is not a parameter. The real ID is
 * `lfo1_rate_hz`, so every read fell back to a hardcoded 4 Hz and the rate
 * control did nothing either.
 *
 * ONE CYCLE IN A BUFFER, LOOPED. Web Audio has no custom-shape LFO, and the
 * alternative — scheduling `setValueCurveAtTime` repeatedly — drifts and
 * fights any other automation on the same parameter. A looping
 * AudioBufferSourceNode is sample-accurate, runs on the audio thread, and
 * costs nothing per frame.
 *
 * The curve is evaluated with the SAME functions the editor draws with
 * (`evaluateCurve`, `evaluateBuiltInShape`), so what you see and what you
 * hear cannot disagree — the duplication trap §6 describes for warp.ts.
 */

/** Cycle length in BEATS for each entry of LFO_RATE_DIVISION, in order. */
const DIVISION_BEATS = [
  32, 16, 8,          // 8 / 4 / 2 bars
  4,                  // 1/1
  3, 2, 4 / 3,        // 1/2 dotted, 1/2, 1/2 triplet
  1.5, 1, 2 / 3,      // 1/4 …
  0.75, 0.5, 1 / 3,   // 1/8 …
  0.375, 0.25, 1 / 6, // 1/16 …
  0.1875, 0.125, 1 / 12,
  0.0625,             // 1/64
];

/*  The preview has no host, so it has no tempo. 140 is not arbitrary: the
    client's five reference tracks all measure their modulation on exact
    subdivisions of 140 BPM, so a synced LFO here lands where theirs do. */
export const PREVIEW_BPM = 140;

export function divisionToHz(index: number): number {
  const beats = DIVISION_BEATS[Math.max(0, Math.min(DIVISION_BEATS.length - 1, index))] ?? 1;

  return PREVIEW_BPM / (60 * beats);
}

/**
 * One cycle of the LFO as a buffer, ready to loop.
 *
 * 2048 samples: the drawn curve has at most a few dozen points, and a step
 * in it needs enough resolution not to be a click. At a 4 Hz cycle that is
 * 8192 samples per second of shape, far above anything the curve contains.
 */
export function buildLfoBuffer(
  context: BaseAudioContext,
  shapeIndex: number,
  curve: CurvePoint[],
  bipolar: boolean,
): AudioBuffer {
  const length = 2048;
  const buffer = context.createBuffer(1, length, context.sampleRate);
  const data = buffer.getChannelData(0);

  for (let i = 0; i < length; i++) {
    const phase = i / length;

    //  Index 0 is the drawn curve — the drawable LFO is the feature, not an
    //  option behind the built-ins (choices.ts says so for the picker; this
    //  keeps the audio agreeing with it).
    const value =
      shapeIndex === 0 && curve.length > 0
        ? evaluateCurve(curve, phase)
        : evaluateBuiltInShape(shapeIndex, phase);

    //  Unipolar 0..1 is what a GATE needs: the bottom of the curve has to
    //  reach silence. Bipolar is right for a cutoff, which should swing
    //  either side of where the knob sits.
    data[i] = bipolar ? value * 2 - 1 : value;
  }

  return buffer;
}
