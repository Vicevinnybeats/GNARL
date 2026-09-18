import { useCallback, useEffect, useState } from 'react';

import { getSliderState } from '../juce/index.js';
import type { ParameterId } from './parameterIds';

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

  useEffect(() => {
    const sync = () => {
      setNormalisedLocal(state.getNormalisedValue());
      setScaled(state.getScaledValue());
    };
    const syncProps = () => {
      setLabel(state.properties.label);
      setInterval(state.properties.interval);
      setRangeEnd(state.properties.end);
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
      setNormalisedLocal(clamped); // optimistic: no round trip before repaint
      state.setNormalisedValue(clamped);
    },
    [state],
  );

  const beginGesture = useCallback(() => state.sliderDragStarted(), [state]);
  const endGesture = useCallback(() => state.sliderDragEnded(), [state]);

  // Formatted here rather than in every call site, so a knob only has to be
  // handed a parameter. The C++ side has its own formatters for the host's
  // automation panel; these two agree on units but not on precision, and the
  // real fix in Phase 6 is to relay the C++ string over the bridge.
  const text = formatValue(scaled, label, interval, rangeEnd);

  return { normalised, scaled, label, text, setNormalised, beginGesture, endGesture };
}

/**
 * Best-effort formatting from the parameter's unit and step size.
 *
 * A stopgap: the C++ side already formats every parameter for the host's
 * automation panel, and Phase 6 should relay that string rather than keeping a
 * second formatter here that can disagree with it.
 */
function formatValue(
  value: number,
  label: string,
  interval: number,
  rangeEnd: number,
): string {
  const unit = label.trim();

  // -0.0 dB is not a thing anyone wants to read.
  const clean = Math.abs(value) < 5e-5 ? 0 : value;

  // A step of 1 or more means the parameter counts things - voices, semitones,
  // octaves - and "16.00 voices" is not how anyone reads that.
  const isInteger = interval >= 1;

  if (unit === 'dB') return `${clean.toFixed(1)} dB`;

  if (unit === 'Hz') {
    return Math.abs(clean) >= 1000
      ? `${(clean / 1000).toFixed(clean >= 10000 ? 1 : 2)} kHz`
      : `${clean.toFixed(Math.abs(clean) < 100 ? 1 : 0)} Hz`;
  }

  if (unit === 'ms') return `${clean.toFixed(Math.abs(clean) < 10 ? 2 : 1)} ms`;

  if (unit === 's') {
    return Math.abs(clean) < 1
      ? `${(clean * 1000).toFixed(0)} ms`
      : `${clean.toFixed(2)} s`;
  }

  if (unit === 'st' || unit === 'ct') {
    return `${clean > 0 ? '+' : ''}${isInteger ? clean.toFixed(0) : clean.toFixed(1)} ${unit}`;
  }

  if (unit === '%') {
    // The C++ formatter multiplies a 0..1 parameter by 100 to display it, so
    // its unit says "%" while the underlying value is still a fraction.
    // Reading that as an already-scaled percentage turns 0.5 into "1%", which
    // is exactly the regression this branch caused when it was first added.
    const scale = rangeEnd <= 1.0001 ? 100 : 1;
    return `${Math.round(clean * scale)}%`;
  }

  if (isInteger) return clean.toFixed(0);

  // No unit at all: a 0..1 control is a percentage, anything else a number.
  if (Math.abs(clean) <= 1.0001) return `${Math.round(clean * 100)}%`;
  if (Math.abs(clean) < 100) return clean.toFixed(2);

  return Math.round(clean).toString();
}
