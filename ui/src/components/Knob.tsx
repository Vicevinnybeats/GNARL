import { useCallback, useEffect, useRef, useState } from 'react';

import { useTheme } from '../theme';
import { useSettings } from '../settings';
import './Knob.css';

export interface KnobProps {
  label: string;
  /** 0..1 */
  value: number;
  /** Formatted real-world value for the readout. */
  readout: string;
  size?: number;
  onChange: (normalised: number) => void;
  onGestureStart?: () => void;
  onGestureEnd?: () => void;
  /** 0..1 default, restored on double click. */
  defaultValue?: number;
}

const ARC_START = 0.75 * Math.PI;
const ARC_SWEEP = 1.5 * Math.PI;

/*  Pixels of vertical drag for a full 0->1 sweep. The DEFAULT lives in
    settings.ts, because it is adjustable: a knob that feels right on a
    trackpad is twitchy on a mouse, and vice versa. The fine multipliers below
    scale whatever that setting is, so the ratio between normal, fine and
    ultra-fine holds at every sensitivity. */
const FINE_MULTIPLIER = 0.2;
const ULTRA_FINE_MULTIPLIER = 0.04;

/**
 * Phase 0 knob: canvas-rendered, vertical drag, modifier precision,
 * double-click reset. Enough to prove the parameter round trip.
 *
 * Phase 6 adds the parts that make it feel like Serum: the modulation depth
 * arc, the live modulated-position dot, drag-to-assign mod sources, and
 * right-click assignment.
 */
export function Knob({
  label,
  value,
  readout,
  size = 36,
  onChange,
  onGestureStart,
  onGestureEnd,
  defaultValue = 0,
}: KnobProps) {
  const theme = useTheme();

  /*  A render dependency for the same reason the theme is: this control draws
      itself on a canvas, and a canvas does not repaint when a CSS custom
      property changes (CLAUDE.md section 6). It also reads the drag
      sensitivity, which is a plain value rather than a colour. */
  const { knobDragPx: dragRangePx } = useSettings();
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const dragRef = useRef<{ startY: number; startValue: number } | null>(null);
  const [isActive, setIsActive] = useState(false);

  // --- Paint ---------------------------------------------------------------
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const dpr = window.devicePixelRatio || 1;
    canvas.width = size * dpr;
    canvas.height = size * dpr;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, size, size);

    const cx = size / 2;
    const cy = size / 2;
    const radius = size / 2 - 4;
    const styles = getComputedStyle(canvas);
    const track = styles.getPropertyValue('--gn-border-strong').trim() || '#33333f';
    const accent = styles.getPropertyValue('--gn-accent').trim() || '#b4ff2e';

    ctx.lineCap = 'butt';
    ctx.lineWidth = 3;

    // Track
    ctx.beginPath();
    ctx.arc(cx, cy, radius, ARC_START, ARC_START + ARC_SWEEP);
    ctx.strokeStyle = track;
    ctx.stroke();

    // Value
    if (value > 0) {
      ctx.beginPath();
      ctx.arc(cx, cy, radius, ARC_START, ARC_START + ARC_SWEEP * value);
      ctx.strokeStyle = accent;
      ctx.stroke();
    }

    // Pointer
    const angle = ARC_START + ARC_SWEEP * value;
    ctx.beginPath();
    ctx.moveTo(cx + Math.cos(angle) * (radius - 9), cy + Math.sin(angle) * (radius - 9));
    ctx.lineTo(cx + Math.cos(angle) * (radius - 2), cy + Math.sin(angle) * (radius - 2));
    ctx.strokeStyle = accent;
    ctx.lineWidth = 2;
    ctx.stroke();
    // `theme` is unused in the body on purpose: it is a REDRAW TRIGGER. The
    // colours come from getComputedStyle above, and a canvas does not repaint
    // when a CSS custom property changes.
  }, [value, size, theme]);

  // --- Drag ----------------------------------------------------------------
  const handlePointerDown = useCallback(
    (e: React.PointerEvent<HTMLCanvasElement>) => {
      e.currentTarget.setPointerCapture(e.pointerId);
      dragRef.current = { startY: e.clientY, startValue: value };
      setIsActive(true);
      onGestureStart?.();
    },
    [value, onGestureStart],
  );

  const handlePointerMove = useCallback(
    (e: React.PointerEvent<HTMLCanvasElement>) => {
      const drag = dragRef.current;
      if (!drag) return;

      const multiplier = e.metaKey || e.ctrlKey
        ? ULTRA_FINE_MULTIPLIER
        : e.shiftKey
          ? FINE_MULTIPLIER
          : 1;

      const delta = ((drag.startY - e.clientY) / dragRangePx) * multiplier;
      onChange(Math.min(1, Math.max(0, drag.startValue + delta)));
    },
    // dragRangePx belongs here: without it the callback closes over whatever
    // the sensitivity was when the knob mounted, so changing it in the
    // settings would do nothing until the panel was re-rendered for some
    // other reason - which is the hardest kind of "sometimes it works".
    [onChange, dragRangePx],
  );

  const endDrag = useCallback(() => {
    if (!dragRef.current) return;
    dragRef.current = null;
    setIsActive(false);
    onGestureEnd?.();
  }, [onGestureEnd]);

  const handleDoubleClick = useCallback(() => {
    onGestureStart?.();
    onChange(defaultValue);
    onGestureEnd?.();
  }, [defaultValue, onChange, onGestureStart, onGestureEnd]);

  const handleKeyDown = useCallback(
    (e: React.KeyboardEvent) => {
      const step = e.shiftKey ? 0.001 : 0.01;
      if (e.key === 'ArrowUp' || e.key === 'ArrowRight') {
        onChange(Math.min(1, value + step));
        e.preventDefault();
      } else if (e.key === 'ArrowDown' || e.key === 'ArrowLeft') {
        onChange(Math.max(0, value - step));
        e.preventDefault();
      }
    },
    [onChange, value],
  );

  return (
    <div className="gn-knob">
      <canvas
        ref={canvasRef}
        className="gn-knob__dial"
        style={{ width: size, height: size }}
        role="slider"
        tabIndex={0}
        aria-label={label}
        aria-valuemin={0}
        aria-valuemax={1}
        aria-valuenow={Number(value.toFixed(3))}
        aria-valuetext={readout}
        onPointerDown={handlePointerDown}
        onPointerMove={handlePointerMove}
        onPointerUp={endDrag}
        onPointerCancel={endDrag}
        onDoubleClick={handleDoubleClick}
        onKeyDown={handleKeyDown}
      />
      <div className="gn-knob__label">{label}</div>
      {/* Readout only while interacting, so the panel stays quiet at rest. */}
      <div className="gn-knob__readout" data-visible={isActive}>
        {readout}
      </div>
    </div>
  );
}
