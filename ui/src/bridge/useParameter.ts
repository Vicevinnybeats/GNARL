import { useCallback, useMemo, useEffect, useState } from 'react';

import { getSliderState } from '../juce/index.js';
import type { ParameterId } from './parameterIds';
import defaults from './parameterDefaults.json';
import {
  denormalise,
  format,
  pickFormatter,
  snapToInterval,
  type FormatterHints,
} from './formatters';

/*  The formatter hints come from the DUMP rather than from the relay, because
    the relay carries a parameter's range but not its unit - JUCE's `label` is
    empty for every parameter here, so the only evidence of which formatter
    C++ uses is the text C++ produced, which is what the dump holds. The same
    file already seeds the browser preview, so this costs nothing new. */
const FORMATTER_HINTS = defaults as Record<string, Partial<FormatterHints>>;

/** Step size, used to decide whether a value is an integer. */
export interface ParameterHandle {
  /** 0..1, what the control draws. */
  normalised: number;
  /** Real-world value with units, what the readout shows. */
  scaled: number;
  label: string;
  /** Ready-to-draw readout, including the unit. */
  text: string;
  /** Call on drag. Updates the UI immediately and the host in the same frame. */
  setNormalised: (v: number) => void;
  /** Wrap a drag gesture so host automation records one gesture, not N writes. */
  beginGesture: () => void;
  endGesture: () => void;
}

/**
 * Two-way binding to a single AudioProcessorValueTreeState parameter via
 * JUCE's WebSliderRelay.
 *
 * The UI never waits on C++: local state updates on the same frame as the
 * pointer event, and the relay write is fire-and-forget. Host-side changes
 * (automation, preset load, another editor) arrive through valueChangedEvent.
 */
export function useParameter(id: ParameterId): ParameterHandle {
  const [state] = useState(() => getSliderState(id));
  const [normalised, setNormalisedLocal] = useState(() => state.getNormalisedValue());
  const [scaled, setScaled] = useState(() => state.getScaledValue());
  const [label, setLabel] = useState(() => state.properties.label);
  const [interval, setInterval] = useState(() => state.properties.interval);
  const [rangeEnd, setRangeEnd] = useState(() => state.properties.end);
  const [rangeStart, setRangeStart] = useState(() => state.properties.start);
  const [skew, setSkew] = useState(() => state.properties.skew);

  useEffect(() => {
    const sync = () => {
      setNormalisedLocal(state.getNormalisedValue());
      setScaled(state.getScaledValue());
    };
    const syncProps = () => {
      setLabel(state.properties.label);
      setInterval(state.properties.interval);
      setRangeEnd(state.properties.end);
      setRangeStart(state.properties.start);
      setSkew(state.properties.skew);
    };

    const valueToken = state.valueChangedEvent.addListener(sync);
    const propsToken = state.propertiesChangedEvent.addListener(syncProps);
    sync();
    syncProps();

    return () => {
      state.valueChangedEvent.removeListener(valueToken);
      state.propertiesChangedEvent.removeListener(propsToken);
    };
  }, [state]);

  const setNormalised = useCallback(
    (v: number) => {
      const clamped = Math.min(1, Math.max(0, v));

      // Optimistic: no round trip before repaint. The READOUT is computed
      // locally too, not left to arrive with the backend's echo - otherwise
      // the number sits still while the control moves under the pointer,
      // which reads as a broken control. The echo overwrites it a frame later
      // with the authoritative value, which is what makes this safe rather
      // than a second source of truth.
      setNormalisedLocal(clamped);
      setScaled(denormalise(clamped, rangeStart, rangeEnd, skew));

      state.setNormalisedValue(clamped);
    },
    [state, rangeStart, rangeEnd, skew],
  );

  const beginGesture = useCallback(() => state.sliderDragStarted(), [state]);
  const endGesture = useCallback(() => state.sliderDragEnded(), [state]);

  /*  Formatted here rather than in every call site, so a knob only has to be
      handed a parameter.

      This used to be a best-effort guess from the unit label and step size,
      and its own comment admitted it had already disagreed with C++ once. It
      now goes through bridge/formatters.ts, which mirrors the formatters in
      ParameterRanges.h and is checked against all 430 parameters at 21 points
      each by `npm run check-reference`. The readout and the host's automation
      panel now say the same thing because a script proves they do, not
      because both were written carefully. */
  const text = useMemo(() => {
    const hints = FORMATTER_HINTS[id];

    const kind = pickFormatter({
      textAtMin: hints?.textAtMin ?? '',
      textAtMax: hints?.textAtMax ?? '',
      textAtDefault: hints?.textAtDefault ?? '',
      min: rangeStart,
      max: rangeEnd,
    });

    // A choice, a boolean or a counted integer: its text comes from the
    // choice list, which useDiscreteParameter handles. Falling back to the
    // number is better than an empty readout.
    if (kind === null) {
      return interval >= 1 ? String(Math.round(scaled)) : scaled.toFixed(2);
    }

    return format(kind, snapToInterval(scaled, interval));
  }, [id, scaled, interval, rangeStart, rangeEnd]);

  return { normalised, scaled, label, text, setNormalised, beginGesture, endGesture };
}
