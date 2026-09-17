import { useCallback, useEffect, useState } from 'react';

import { getSliderState } from '../juce/index.js';
import type { ParameterId } from './parameterIds';

export interface ParameterHandle {
  /** 0..1, what the control draws. */
  normalised: number;
  /** Real-world value with units, what the readout shows. */
  scaled: number;
  label: string;
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

  useEffect(() => {
    const sync = () => {
      setNormalisedLocal(state.getNormalisedValue());
      setScaled(state.getScaledValue());
    };
    const syncProps = () => setLabel(state.properties.label);

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

  return { normalised, scaled, label, setNormalised, beginGesture, endGesture };
}
