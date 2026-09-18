import { useCallback, useRef } from 'react';

import './DepthSlider.css';

/**
 * A compact horizontal depth control, for the mod matrix's rows.
 *
 * A knob cannot be used here. The matrix shows sixteen slots at once, which
 * gives each row about 22 px, and a knob stacks its dial, label and readout
 * vertically - so it either does not fit or has to hide its value. Hiding the
 * value is not an option: knob values are always visible in this UI, because a
 * dense synth you have to hover one control at a time to read is unusable.
 *
 * So the value IS the control: a bar filled from the centre for a bipolar
 * parameter (or from the left for a unipolar one) with the number drawn over
 * it. Filling from the centre matters - a depth of -50% and +50% are opposite
 * modulations, and a left-filled bar would draw them identically far from
 * empty.
 */

export interface DepthSliderProps {
  /** 0..1, as the parameter stores it. */
  value: number;
  /** Ready-to-draw readout, e.g. "-45%". */
  readout: string;
  /** Fills from the centre rather than from the left. */
  bipolar?: boolean;
  title?: string;
  onChange: (normalised: number) => void;
  onGestureStart?: () => void;
  onGestureEnd?: () => void;
}

/** Pixels of horizontal drag for a full sweep. Shorter than a knob's 200 px
    because the control itself is short, and a depth is usually nudged rather
    than swept. */
const DRAG_RANGE_PX = 120;
const FINE_MULTIPLIER = 0.2;

export function DepthSlider({
  value,
  readout,
  bipolar = true,
  title,
  onChange,
  onGestureStart,
  onGestureEnd,
}: DepthSliderProps) {
  const dragRef = useRef<{ startX: number; startValue: number } | null>(null);

  const handlePointerDown = useCallback(
    (event: React.PointerEvent<HTMLDivElement>) => {
      if (event.button !== 0) return;

      event.currentTarget.setPointerCapture(event.pointerId);
      dragRef.current = { startX: event.clientX, startValue: value };
      onGestureStart?.();
    },
    [onGestureStart, value],
  );

  const handlePointerMove = useCallback(
    (event: React.PointerEvent<HTMLDivElement>) => {
      const drag = dragRef.current;
      if (!drag) return;

      const multiplier = event.shiftKey ? FINE_MULTIPLIER : 1;
      const delta = ((event.clientX - drag.startX) / DRAG_RANGE_PX) * multiplier;

      onChange(Math.min(1, Math.max(0, drag.startValue + delta)));
    },
    [onChange],
  );

  const endDrag = useCallback(() => {
    if (!dragRef.current) return;

    dragRef.current = null;
    onGestureEnd?.();
  }, [onGestureEnd]);

  // Double-click resets to the neutral position: centre for a bipolar depth,
  // zero for a unipolar amount.
  const handleDoubleClick = useCallback(() => {
    onChange(bipolar ? 0.5 : 0);
  }, [bipolar, onChange]);

  const fill = bipolar
    ? { left: `${Math.min(value, 0.5) * 100}%`, width: `${Math.abs(value - 0.5) * 100}%` }
    : { left: '0%', width: `${value * 100}%` };

  return (
    <div
      className="gn-depth"
      data-bipolar={bipolar}
      data-active={bipolar ? Math.abs(value - 0.5) > 0.005 : value > 0.005}
      title={title}
      role="slider"
      aria-valuenow={Math.round(value * 100)}
      aria-valuemin={0}
      aria-valuemax={100}
      aria-label={title ?? 'Depth'}
      tabIndex={0}
      onPointerDown={handlePointerDown}
      onPointerMove={handlePointerMove}
      onPointerUp={endDrag}
      onPointerCancel={endDrag}
      onDoubleClick={handleDoubleClick}
    >
      <span className="gn-depth__fill" style={fill} />
      {bipolar && <span className="gn-depth__centre" />}
      <span className="gn-depth__text">{readout}</span>
    </div>
  );
}
