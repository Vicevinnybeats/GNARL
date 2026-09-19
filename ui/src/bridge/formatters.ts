/**
 * Mirror of the formatters in plugin/source/params/ParameterRanges.h.
 *
 * WHY THIS IS DUPLICATED. A knob's readout has to say what the host's
 * automation panel says, and the host's string is produced in C++ on demand.
 * Asking the plugin to format every value would be a bridge round trip per
 * frame per knob while dragging, which is exactly the traffic the modulation
 * frames are careful to avoid.
 *
 * Duplicated logic is a liability, and the liability is not that it is written
 * twice - it is that it can drift and nothing notices. The previous version of
 * this was a "best-effort" guess from the unit label and step size, and its
 * own comment admitted it had already disagreed with C++ once.
 *
 * So: `ui/scripts/check-reference.mjs` compares this against
 * `ui/tests/referenceVectors.json`, which is dumped from the real parameters
 * by `tools/dump_reference_vectors.cpp` - every parameter, at 21 points across
 * its range. A change on one side that is not mirrored on the other fails
 * rather than quietly showing the wrong number.
 *
 * THE FORMATTER IS INFERRED, NOT NAMED, because there is no way to ask a
 * juce::AudioProcessorParameter which function it was given. That would be
 * unsafe on its own; it is safe because the check verifies all 430 parameters
 * at every sampled point, so a wrong inference fails loudly.
 */

export type FormatterKind =
  | 'hertz'
  | 'seconds'
  | 'milliseconds'
  | 'percent'
  | 'signedPercent'
  | 'decibels'
  | 'semitones'
  | 'cents'
  | 'pan'
  | 'raw';

/** juce::String (float, int) - fixed decimals, rounded half away from zero,
    which is what both printf and toFixed do for the values here. */
function fixed(value: number, decimals: number): string {
  // -0 formats as "-0.00" through toFixed, and a readout of "-0.0 dB" for a
  // value that is zero is a small lie the C++ side does not tell.
  const safe = Object.is(value, -0) ? 0 : value;
  return safe.toFixed(decimals);
}

/** Below this the master fader is treated as silence. Matches
    ranges::kMasterGainMinDb. */
export const MASTER_GAIN_MIN_DB = -60;

export function formatHertz(value: number): string {
  if (value >= 1000) return `${fixed(value / 1000, 2)} kHz`;
  return `${fixed(value, value < 10 ? 2 : 1)} Hz`;
}

export function formatSeconds(value: number): string {
  if (value < 1) return `${fixed(value * 1000, value < 0.01 ? 2 : 1)} ms`;
  return `${fixed(value, 2)} s`;
}

export function formatMilliseconds(value: number): string {
  return `${fixed(value, value < 10 ? 2 : 1)} ms`;
}

export function formatPercent(value: number): string {
  return `${Math.round(value * 100)} %`;
}

export function formatSignedPercent(value: number): string {
  const percent = Math.round(value * 100);
  return `${percent > 0 ? '+' : ''}${percent} %`;
}

export function formatDecibels(value: number): string {
  if (value <= MASTER_GAIN_MIN_DB) return '-inf dB';
  return `${fixed(value, 1)} dB`;
}

export function formatSemitones(value: number): string {
  const semis = Math.round(value);
  return `${semis > 0 ? '+' : ''}${semis} st`;
}

export function formatCents(value: number): string {
  return `${value > 0 ? '+' : ''}${fixed(value, 1)} ct`;
}

/** "L50", "C", "R30" reads faster than "-0.50". */
export function formatPan(value: number): string {
  const amount = Math.round(Math.abs(value) * 100);
  if (amount === 0) return 'C';
  return `${value < 0 ? 'L' : 'R'}${amount}`;
}

export function format(kind: FormatterKind, value: number): string {
  switch (kind) {
    case 'hertz':         return formatHertz(value);
    case 'seconds':       return formatSeconds(value);
    case 'milliseconds':  return formatMilliseconds(value);
    case 'percent':       return formatPercent(value);
    case 'signedPercent': return formatSignedPercent(value);
    case 'decibels':      return formatDecibels(value);
    case 'semitones':     return formatSemitones(value);
    case 'cents':         return formatCents(value);
    case 'pan':           return formatPan(value);
    case 'raw':
    default:              return String(value);
  }
}

/** What the dump knows about a parameter, which is all the inference has. */
export interface FormatterHints {
  textAtMin: string;
  textAtMax: string;
  textAtDefault: string;
  min: number;
  max: number;
}

/**
 * Which formatter a parameter uses, inferred from what C++ produced.
 *
 * The parameters carry no unit label - JUCE's `label` is empty for all of
 * them - so the only evidence is the shape of the strings themselves. Every
 * branch here is verified against all 430 parameters by the reference check.
 */
export function pickFormatter(hints: FormatterHints): FormatterKind | null {
  const { textAtMin, textAtMax } = hints;

  const endsWith = (suffix: string) =>
    textAtMin.endsWith(suffix) || textAtMax.endsWith(suffix);

  // Pan first: "C" / "L50" / "R30" has no unit to confuse anything else with.
  if (/^(C|[LR]\d+)$/.test(textAtMin) && /^(C|[LR]\d+)$/.test(textAtMax)) {
    return 'pan';
  }

  if (endsWith(' Hz') || endsWith(' kHz')) return 'hertz';
  if (endsWith(' ct')) return 'cents';
  if (endsWith(' st')) return 'semitones';
  if (endsWith(' dB') || textAtMin === '-inf dB') return 'decibels';

  /*  Seconds and milliseconds produce the SAME text below one second, so the
      evidence has to be the text rather than the range: `grain_size` runs to
      500 and is formatted in MILLISECONDS, so a rule of "max >= 1 means
      seconds" reads 500 ms as 500 seconds. Only the seconds formatter ever
      emits " s", and it only does so above one second - which every
      seconds-formatted parameter here reaches. */
  if (textAtMax.endsWith(' s')) return 'seconds';
  if (endsWith(' ms')) return 'milliseconds';

  if (endsWith(' %')) {
    // Signed and unsigned differ only by a leading "+", which only appears
    // above zero - so the evidence is whether the range goes negative.
    return hints.min < 0 ? 'signedPercent' : 'percent';
  }

  // A choice, a boolean or an integer count. Those are not formatted here:
  // their text comes from the choice list, which choices.ts already mirrors
  // and ParameterMirrorTests already guards.
  return null;
}

/**
 * Normalised value to real value, matching juce::NormalisableRange.
 *
 * JUCE applies the skew as `proportion^(1/skew)` before mapping onto the
 * range, so a skewed knob's readout has to do the same or it disagrees with
 * the value the engine actually received.
 */
export function denormalise(
  normalised: number,
  min: number,
  max: number,
  skew: number,
): number {
  const proportion =
    skew === 1 || skew <= 0 ? normalised : Math.pow(normalised, 1 / skew);

  /*  ROUNDED TO FLOAT AT EVERY STEP, because JUCE does this arithmetic in
      float and several formatters branch on an exact threshold. formatSeconds
      picks two decimals below 10 ms and one at or above, and the envelope
      attack's midpoint is exactly 10 ms - where float and double land on
      opposite sides and the readout differs in its last digit. The same thing
      put the master fader at "-60.0 dB" instead of "-inf dB" at the bottom of
      its travel. Matching the precision is the only way to match the
      branch. */
  return Math.fround(Math.fround(min) + Math.fround(Math.fround(max - min) * proportion));
}

/** Snaps to the parameter's step, as JUCE does before formatting. */
export function snapToInterval(value: number, interval: number): number {
  if (!(interval > 0)) return value;
  return Math.fround(Math.round(value / interval) * interval);
}
