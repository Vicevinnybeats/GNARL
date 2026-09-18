import { useCallback, useEffect, useState } from 'react';

import { getSliderState } from '../juce/index.js';
import type { ParameterId } from './parameterIds';

/**
 * Choice and toggle parameters.
 *
 * Both are backed by JUCE's slider relay rather than its toggle and combo-box
 * relays. That is deliberate: the C++ side declares them as
 * AudioParameterChoice and AudioParameterBool, which JUCE exposes to the web
 * view through WebSliderRelay when they are attached that way, and using ONE
 * relay type for everything keeps WebUIEditor's attachment loop uniform. The
 * conversion between a normalised value and an index lives here.
 */
export interface ChoiceHandle {
  index: number;
  setIndex: (index: number) => void;
}

export function useChoiceParameter(id: ParameterId, count: number): ChoiceHandle {
  const [state] = useState(() => getSliderState(id));
  const [index, setIndexLocal] = useState(() => toIndex(state.getNormalisedValue(), count));

  useEffect(() => {
    const sync = () => setIndexLocal(toIndex(state.getNormalisedValue(), count));
    const token = state.valueChangedEvent.addListener(sync);
    sync();
    return () => state.valueChangedEvent.removeListener(token);
  }, [state, count]);

  const setIndex = useCallback(
    (next: number) => {
      const clamped = Math.min(count - 1, Math.max(0, Math.round(next)));
      setIndexLocal(clamped);
      // A choice parameter's normalised value is the index divided by the
      // number of STEPS, which is one fewer than the number of choices.
      state.setNormalisedValue(count > 1 ? clamped / (count - 1) : 0);
    },
    [state, count],
  );

  return { index, setIndex };
}

export interface ToggleHandle {
  value: boolean;
  setValue: (value: boolean) => void;
}

export function useToggleParameter(id: ParameterId): ToggleHandle {
  const [state] = useState(() => getSliderState(id));
  const [value, setValueLocal] = useState(() => state.getNormalisedValue() >= 0.5);

  useEffect(() => {
    const sync = () => setValueLocal(state.getNormalisedValue() >= 0.5);
    const token = state.valueChangedEvent.addListener(sync);
    sync();
    return () => state.valueChangedEvent.removeListener(token);
  }, [state]);

  const setValue = useCallback(
    (next: boolean) => {
      setValueLocal(next);
      state.setNormalisedValue(next ? 1 : 0);
    },
    [state],
  );

  return { value, setValue };
}

function toIndex(normalised: number, count: number): number {
  if (count <= 1) return 0;
  return Math.min(count - 1, Math.max(0, Math.round(normalised * (count - 1))));
}
