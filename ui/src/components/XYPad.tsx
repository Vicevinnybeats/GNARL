import { useCallback, useRef } from 'react';

import './XYPad.css';

export interface XYPadAnchor {
  label: string;
  x: number;
  y: number;
}

export interface XYPadProps {
  x: number;
  y: number;
  onChange: (x: number, y: number) => void;
  /** Labelled positions drawn on the pad, e.g. the five vowels. */
  anchors?: readonly XYPadAnchor[];
  label?: string;
  size?: number;
}

/**
 * The formant filter's vowel pad, and the macro pad.
 *
 * The anchors are drawn as labelled targets because a bare pad tells the user
 * nothing: the whole value of the vowel filter is knowing that dragging
 * towards "A" gets you an "aah".
 */
export function XYPad({ x, y, onChange, anchors, label, size = 132 }: XYPadProps) {
  const ref = useRef<HTMLDivElement>(null);
  const dragging = useRef(false);

  const updateFromEvent = useCallback(
    (clientX: number, clientY: number) => {
      const element = ref.current;
      if (!element) return;

      const rect = element.getBoundingClientRect();
      const nextX = Math.min(1, Math.max(0, (clientX - rect.left) / rect.width));
      // Y is inverted: screen coordinates grow downwards, the parameter does not.
      const nextY = Math.min(1, Math.max(0, 1 - (clientY - rect.top) / rect.height));

      onChange(nextX, nextY);
    },
    [onChange],
  );

  return (
    <div className="gn-xypad-wrap">
      {label && <span className="gn-xypad__label">{label}</span>}
      <div
        ref={ref}
        className="gn-xypad"
        style={{ width: size, height: size }}
        role="application"
        aria-label={label ?? 'XY pad'}
        tabIndex={0}
        onPointerDown={(e) => {
          e.currentTarget.setPointerCapture(e.pointerId);
          dragging.current = true;
          updateFromEvent(e.clientX, e.clientY);
        }}
        onPointerMove={(e) => {
          if (dragging.current) updateFromEvent(e.clientX, e.clientY);
        }}
        onPointerUp={() => {
          dragging.current = false;
        }}
        onKeyDown={(e) => {
          const step = e.shiftKey ? 0.01 : 0.05;
          if (e.key === 'ArrowLeft') onChange(Math.max(0, x - step), y);
          else if (e.key === 'ArrowRight') onChange(Math.min(1, x + step), y);
          else if (e.key === 'ArrowUp') onChange(x, Math.min(1, y + step));
          else if (e.key === 'ArrowDown') onChange(x, Math.max(0, y - step));
          else return;
          e.preventDefault();
        }}
      >
        {anchors?.map((anchor) => (
          <span
            key={anchor.label}
            className="gn-xypad__anchor"
            style={{ left: `${anchor.x * 100}%`, bottom: `${anchor.y * 100}%` }}
          >
            {anchor.label}
          </span>
        ))}

        <span
          className="gn-xypad__puck"
          style={{ left: `${x * 100}%`, bottom: `${y * 100}%` }}
        />
      </div>
    </div>
  );
}
