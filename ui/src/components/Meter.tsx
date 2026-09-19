import { useEffect, useRef } from 'react';

import type { ModulationFrame } from '../bridge/modulationFrame';
import { useModulationRef } from '../bridge/useModulation';

import './Meter.css';

export interface MeterProps {
  /** 0..1, or -1..1 when bipolar. Ignored when `read` is given. */
  value?: number;
  label?: string;
  /** Bipolar meters fill from the centre, for gain reduction. */
  bipolar?: boolean;
  /**
   * Drives the meter from the live engine frame instead of from `value`.
   *
   * WHY NOT JUST A PROP. The frame arrives at 60 Hz, and a `setState` per
   * frame re-renders the whole tab sixty times a second - the same trap the
   * wavetable display and the LFO playhead avoid by reading a ref inside
   * their own animation loop (CLAUDE.md section 10). A meter is one CSS
   * length, so it does not even need a canvas: the loop writes the style
   * directly and React never hears about it.
   */
  read?: (frame: ModulationFrame) => number;
}

/** Bar geometry from a signed value, so the loop and the first paint agree. */
function geometry(value: number, bipolar: boolean) {
  const clamped = Math.min(1, Math.max(bipolar ? -1 : 0, value));
  const magnitude = Math.abs(clamped) * (bipolar ? 50 : 100);

  return {
    width: `${magnitude}%`,
    left: bipolar ? (clamped < 0 ? `${50 - magnitude}%` : '50%') : '0',
  };
}

export function Meter({ value = 0, label, bipolar = false, read }: MeterProps) {
  const fillRef = useRef<HTMLSpanElement>(null);
  const frameRef = useModulationRef();

  // Held so the loop can skip the write when nothing moved. Touching a style
  // every frame on a needle that is parked is layout work for no picture.
  const lastDrawn = useRef(Number.NaN);

  useEffect(() => {
    if (!read) return;

    let handle = 0;

    const draw = () => {
      handle = requestAnimationFrame(draw);

      const fill = fillRef.current;
      if (!fill) return;

      const next = read(frameRef.current);

      // Quantised to the third decimal before comparing, matching what the
      // engine already rounds to: below that the difference is far under one
      // pixel and only costs a style recalculation.
      const quantised = Math.round(next * 1000) / 1000;
      if (quantised === lastDrawn.current) return;

      lastDrawn.current = quantised;

      const { width, left } = geometry(quantised, bipolar);
      fill.style.width = width;
      fill.style.left = left;
    };

    handle = requestAnimationFrame(draw);

    return () => cancelAnimationFrame(handle);
  }, [read, bipolar, frameRef]);

  return (
    <div className="gn-meter">
      {label && <span className="gn-meter__label">{label}</span>}
      <div className="gn-meter__track" data-bipolar={bipolar}>
        <span className="gn-meter__fill" ref={fillRef} style={geometry(value, bipolar)} />
      </div>
    </div>
  );
}
