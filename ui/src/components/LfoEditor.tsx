import { useCallback, useEffect, useRef, useState } from 'react';

import {
  MAX_CURVE_POINTS,
  evaluateBuiltInShape,
  evaluateCurve,
  normaliseCurve,
  type CurvePoint,
} from '../dsp/lfoCurve';
import { getGridLineCount, isTripletGrid } from '../dsp/syncRates';
import { isCanvasGlowEnabled } from '../settings';
import { useModulationRef } from '../bridge/useModulation';
import './LfoEditor.css';

/**
 * The drawable breakpoint LFO editor.
 *
 * This is the control the plugin is bought for, so it is a real editor rather
 * than a preset picker:
 *
 *   - click empty space to add a point, drag one to move it;
 *   - drag the handle in the middle of a segment to bend it (tension);
 *   - double-click a point to toggle it between curved and STEP, which is how
 *     the staircase shapes this genre runs on get drawn;
 *   - right-click or alt-click a point to delete it. The first and last points
 *     cannot be deleted: a curve has to span the cycle.
 *
 * TRIPLET GRIDS ARE DRAWN DIFFERENTLY from straight ones - a warmer colour and
 * a heavier line. Mistaking a 1/12 grid for a 1/8 one produces a wobble that
 * is subtly out of time in a way that is very hard to diagnose by ear, and the
 * editor is the only place that confusion can be prevented.
 *
 * The playhead is driven by the ENGINE's phase, read from a ref inside the
 * animation loop rather than through React state: at 60 Hz, setState per frame
 * would re-render the whole tab sixty times a second.
 */

export interface LfoEditorProps {
  lfoIndex: number;
  points: CurvePoint[];
  /** choices::LfoShape. Non-zero means a built-in shape, which is drawn but
      not editable - the drawn curve is not what the LFO is reading. */
  shapeIndex: number;
  /** choices::GridDivision. */
  gridIndex: number;
  bipolar: boolean;
  onChange: (points: CurvePoint[]) => void;
}

/** How close the pointer has to be to grab something, in pixels. */
const GRAB_RADIUS = 9;

type DragTarget =
  | { kind: 'point'; index: number }
  | { kind: 'tension'; index: number; startY: number; startTension: number };

export function LfoEditor({
  lfoIndex,
  points,
  shapeIndex,
  gridIndex,
  bipolar,
  onChange,
}: LfoEditorProps) {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const pointsRef = useRef(points);
  const [drag, setDrag] = useState<DragTarget | null>(null);
  const [hover, setHover] = useState<{ x: number; y: number } | null>(null);

  const modulation = useModulationRef();

  pointsRef.current = points;

  const isCustom = shapeIndex === 0;

  // --- Geometry -------------------------------------------------------------

  const toCanvas = useCallback((canvas: HTMLCanvasElement, time: number, value: number) => {
    const rect = canvas.getBoundingClientRect();
    return {
      x: time * rect.width,
      y: (1 - value) * rect.height,
    };
  }, []);

  const fromPointer = useCallback((event: { clientX: number; clientY: number }) => {
    const canvas = canvasRef.current;
    if (!canvas) return { time: 0, value: 0 };

    const rect = canvas.getBoundingClientRect();

    return {
      time: Math.min(1, Math.max(0, (event.clientX - rect.left) / rect.width)),
      value: Math.min(1, Math.max(0, 1 - (event.clientY - rect.top) / rect.height)),
    };
  }, []);

  /** Snaps a time onto the grid, so a drawn shape lands on the beat.
      Shift bypasses it, because a shape sometimes needs a point the grid has
      no line for and switching the grid off to place one is a detour. */
  const snapTime = useCallback(
    (time: number, bypass = false) => {
      if (bypass) return time;

      const lines = getGridLineCount(gridIndex);
      if (lines <= 0) return time;

      return Math.round(time * lines) / lines;
    },
    [gridIndex],
  );

  // --- Drawing --------------------------------------------------------------

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    let frame = 0;

    const draw = () => {
      const context = canvas.getContext('2d');
      if (!context) return;

      const rect = canvas.getBoundingClientRect();
      const ratio = window.devicePixelRatio || 1;
      const width = Math.max(1, Math.round(rect.width * ratio));
      const height = Math.max(1, Math.round(rect.height * ratio));

      if (canvas.width !== width || canvas.height !== height) {
        canvas.width = width;
        canvas.height = height;
      }

      context.setTransform(ratio, 0, 0, ratio, 0, 0);
      context.clearRect(0, 0, rect.width, rect.height);

      const styles = getComputedStyle(canvas);
      const accent = styles.getPropertyValue('--gn-accent').trim() || '#b4ff2e';
      const border = styles.getPropertyValue('--gn-border').trim() || '#23232f';
      const warn = styles.getPropertyValue('--gn-warn').trim() || '#ffb02e';

      drawGrid(context, rect, { border, warn, gridIndex, bipolar });

      const current = pointsRef.current;

      const evaluate = (phase: number) =>
        isCustom ? evaluateCurve(current, phase) : evaluateBuiltInShape(shapeIndex, phase);

      drawCurve(context, rect, evaluate, accent);

      if (isCustom) drawHandles(context, rect, current, accent);

      // The playhead, from the engine's own phase. Drawn last so it sits over
      // the curve rather than under it.
      const phase = modulation.current.lfoPhases[lfoIndex] ?? 0;
      const playing = modulation.current.playing;

      drawPlayhead(context, rect, phase, evaluate(phase), accent, playing);

      frame = requestAnimationFrame(draw);
    };

    frame = requestAnimationFrame(draw);

    return () => cancelAnimationFrame(frame);
  }, [bipolar, gridIndex, isCustom, lfoIndex, modulation, shapeIndex]);

  // --- Editing --------------------------------------------------------------

  const findGrab = useCallback(
    (event: { clientX: number; clientY: number }): DragTarget | null => {
      const canvas = canvasRef.current;
      if (!canvas) return null;

      const rect = canvas.getBoundingClientRect();
      const pointerX = event.clientX - rect.left;
      const pointerY = event.clientY - rect.top;
      const current = pointsRef.current;

      for (let i = 0; i < current.length; i += 1) {
        const point = current[i];
        if (!point) continue;

        const position = toCanvas(canvas, point.time, point.value);

        if (Math.hypot(position.x - pointerX, position.y - pointerY) <= GRAB_RADIUS)
          return { kind: 'point', index: i };
      }

      // Tension handles sit at the midpoint of each curved segment. Checked
      // after the points, so a handle never steals a point's own grab.
      for (let i = 0; i < current.length - 1; i += 1) {
        const a = current[i];
        const b = current[i + 1];
        if (!a || !b || a.step) continue;

        const handle = tensionHandlePositionForRect(rect, a, b);

        if (Math.hypot(handle.x - pointerX, handle.y - pointerY) <= GRAB_RADIUS)
          return { kind: 'tension', index: i, startY: pointerY, startTension: a.tension };
      }

      return null;
    },
    [toCanvas],
  );

  const handlePointerDown = useCallback(
    (event: React.PointerEvent<HTMLCanvasElement>) => {
      if (!isCustom) return;

      const grabbed = findGrab(event);
      const current = pointsRef.current;

      // Right-click or alt-click deletes. The end points stay: a curve that
      // does not span the cycle has nothing to interpolate between.
      if (event.button === 2 || event.altKey) {
        event.preventDefault();

        if (grabbed?.kind === 'point' && grabbed.index > 0 && grabbed.index < current.length - 1)
          onChange(current.filter((_unused, i) => i !== grabbed.index));

        return;
      }

      if (event.button !== 0) return;

      event.currentTarget.setPointerCapture(event.pointerId);

      if (grabbed) {
        setDrag(grabbed);
        return;
      }

      // Empty space: add a point there and start dragging it, so one gesture
      // both creates and places.
      if (current.length >= MAX_CURVE_POINTS) return;

      const { time, value } = fromPointer(event);
      const snapped = snapTime(time, event.shiftKey);

      const next = normaliseCurve([
        ...current,
        { time: snapped, value, tension: 0, step: false },
      ]);

      const index = next.findIndex((point) => point.time === snapped && point.value === value);

      onChange(next);
      if (index >= 0) setDrag({ kind: 'point', index });
    },
    [findGrab, fromPointer, isCustom, onChange, snapTime],
  );

  const handlePointerMove = useCallback(
    (event: React.PointerEvent<HTMLCanvasElement>) => {
      const canvas = canvasRef.current;

      if (canvas) {
        const rect = canvas.getBoundingClientRect();
        setHover({ x: event.clientX - rect.left, y: event.clientY - rect.top });
      }

      if (!drag || !isCustom) return;

      const current = [...pointsRef.current];

      if (drag.kind === 'point') {
        const point = current[drag.index];
        if (!point) return;

        const { time, value } = fromPointer(event);

        // The first and last points keep their time so the curve always spans
        // the cycle; only their value moves.
        const isEnd = drag.index === 0 || drag.index === current.length - 1;

        current[drag.index] = {
          ...point,
          time: isEnd ? point.time : snapTime(time, event.shiftKey),
          value,
        };

        onChange(normaliseCurve(current));
        return;
      }

      const point = current[drag.index];
      if (!point) return;

      // Tension is dragged vertically, a full swing over about 90 px - far
      // enough to be controllable, short enough to reach either extreme
      // without running off the panel.
      const rect = canvasRef.current?.getBoundingClientRect();
      const pointerY = rect ? event.clientY - rect.top : drag.startY;
      const delta = (drag.startY - pointerY) / 90;

      current[drag.index] = {
        ...point,
        tension: Math.min(1, Math.max(-1, drag.startTension + delta)),
      };

      onChange(current);
    },
    [drag, fromPointer, isCustom, onChange, snapTime],
  );

  const handlePointerUp = useCallback(() => setDrag(null), []);

  const handleDoubleClick = useCallback(
    (event: React.MouseEvent<HTMLCanvasElement>) => {
      if (!isCustom) return;

      const grabbed = findGrab(event);
      if (grabbed?.kind !== 'point') return;

      const current = [...pointsRef.current];
      const point = current[grabbed.index];
      if (!point) return;

      current[grabbed.index] = { ...point, step: !point.step };
      onChange(current);
    },
    [findGrab, isCustom, onChange],
  );

  return (
    <div className="gn-lfo-editor" data-readonly={!isCustom}>
      <canvas
        ref={canvasRef}
        className="gn-lfo-editor__canvas"
        onPointerDown={handlePointerDown}
        onPointerMove={handlePointerMove}
        onPointerUp={handlePointerUp}
        onPointerLeave={() => {
          setHover(null);
          setDrag(null);
        }}
        onDoubleClick={handleDoubleClick}
        onContextMenu={(event) => event.preventDefault()}
      />

      {!isCustom && (
        <div className="gn-lfo-editor__overlay">
          Built-in shape — switch Shape to Custom to draw
        </div>
      )}

      {isCustom && hover && !drag && (
        <div className="gn-lfo-editor__hint">
          click to add · drag to move · double-click for step · alt-click to delete · shift to ignore the grid
        </div>
      )}
    </div>
  );
}

// --- Canvas helpers ---------------------------------------------------------

interface GridOptions {
  border: string;
  warn: string;
  gridIndex: number;
  bipolar: boolean;
}

function drawGrid(
  context: CanvasRenderingContext2D,
  rect: DOMRect,
  { border, warn, gridIndex, bipolar }: GridOptions,
): void {
  context.lineWidth = 1;

  // The centre line matters for a bipolar LFO, where it is zero rather than
  // just the middle of the range.
  context.strokeStyle = border;
  context.beginPath();
  context.moveTo(0, rect.height / 2);
  context.lineTo(rect.width, rect.height / 2);
  context.stroke();

  if (bipolar) {
    context.fillStyle = border;
    context.font = '9px ui-monospace, monospace';
    context.fillText('0', 3, rect.height / 2 - 3);
  }

  const lines = getGridLineCount(gridIndex);
  if (lines <= 0) return;

  const triplet = isTripletGrid(gridIndex);

  // A triplet grid is warm and heavier; a straight one is the neutral border
  // colour. The difference has to be obvious at a glance.
  context.strokeStyle = triplet ? warn : border;
  context.globalAlpha = triplet ? 0.45 : 0.6;
  context.lineWidth = triplet ? 1.25 : 1;

  for (let i = 1; i < lines; i += 1) {
    const x = Math.round((i / lines) * rect.width) + 0.5;

    context.beginPath();
    context.moveTo(x, 0);
    context.lineTo(x, rect.height);
    context.stroke();
  }

  context.globalAlpha = 1;
}

function drawCurve(
  context: CanvasRenderingContext2D,
  rect: DOMRect,
  evaluate: (phase: number) => number,
  accent: string,
): void {
  // One sample per pixel: the curve can contain steps, and interpolating
  // between sparse samples would round them off into ramps.
  const steps = Math.max(2, Math.round(rect.width));

  context.beginPath();

  for (let i = 0; i <= steps; i += 1) {
    const phase = i / steps;
    const x = phase * rect.width;
    const y = (1 - evaluate(phase)) * rect.height;

    if (i === 0) context.moveTo(x, y);
    else context.lineTo(x, y);
  }

  // A filled area under the curve reads as a level rather than as a line, and
  // makes a step shape legible at a glance.
  context.save();
  context.lineTo(rect.width, rect.height);
  context.lineTo(0, rect.height);
  context.closePath();
  context.fillStyle = withAlpha(accent, 0.1);
  context.fill();
  context.restore();

  context.strokeStyle = accent;
  context.lineWidth = 1.75;
  context.shadowColor = withAlpha(accent, 0.5);
  context.shadowBlur = isCanvasGlowEnabled() ? 7 : 0;
  context.stroke();
  context.shadowBlur = 0;
}

function drawHandles(
  context: CanvasRenderingContext2D,
  rect: DOMRect,
  points: readonly CurvePoint[],
  accent: string,
): void {
  // Tension handles first, so a breakpoint is never hidden behind one.
  context.fillStyle = withAlpha(accent, 0.35);

  for (let i = 0; i < points.length - 1; i += 1) {
    const a = points[i];
    const b = points[i + 1];
    if (!a || !b || a.step) continue;

    const handle = tensionHandlePositionForRect(rect, a, b);

    context.beginPath();
    context.arc(handle.x, handle.y, 3, 0, Math.PI * 2);
    context.fill();
  }

  for (const point of points) {
    const x = point.time * rect.width;
    const y = (1 - point.value) * rect.height;

    context.beginPath();

    // A step point is a square and a curved one a circle, so the shape of a
    // curve is readable without clicking anything.
    if (point.step) context.rect(x - 3.5, y - 3.5, 7, 7);
    else context.arc(x, y, 3.5, 0, Math.PI * 2);

    context.fillStyle = accent;
    context.fill();

    context.strokeStyle = '#07070a';
    context.lineWidth = 1.5;
    context.stroke();
  }
}

function drawPlayhead(
  context: CanvasRenderingContext2D,
  rect: DOMRect,
  phase: number,
  value: number,
  accent: string,
  playing: boolean,
): void {
  const x = phase * rect.width;
  const y = (1 - value) * rect.height;

  context.strokeStyle = withAlpha('#ffffff', playing ? 0.35 : 0.14);
  context.lineWidth = 1;
  context.beginPath();
  context.moveTo(x, 0);
  context.lineTo(x, rect.height);
  context.stroke();

  context.beginPath();
  context.arc(x, y, 4, 0, Math.PI * 2);
  context.fillStyle = playing ? '#ffffff' : withAlpha(accent, 0.5);
  context.shadowColor = withAlpha(accent, 0.9);
  context.shadowBlur = playing && isCanvasGlowEnabled() ? 10 : 0;
  context.fill();
  context.shadowBlur = 0;
}

/** The handle sits where the curve actually passes at the segment's midpoint,
    so dragging it feels like bending the line rather than moving an abstract
    control point. */
function tensionHandlePositionForRect(
  rect: { width: number; height: number },
  a: CurvePoint,
  b: CurvePoint,
): { x: number; y: number } {
  const midTime = (a.time + b.time) / 2;
  const shaped = evaluateCurve([a, b], midTime);

  return { x: midTime * rect.width, y: (1 - shaped) * rect.height };
}

/** Hex or rgb colour with an alpha applied. Kept local: this is the only place
    in the UI that needs it, and a colour utility module for one function is
    more indirection than it saves. */
function withAlpha(colour: string, alpha: number): string {
  const hex = colour.trim();

  if (hex.startsWith('#') && (hex.length === 7 || hex.length === 4)) {
    const full =
      hex.length === 4
        ? `#${hex[1]}${hex[1]}${hex[2]}${hex[2]}${hex[3]}${hex[3]}`
        : hex;

    const r = parseInt(full.slice(1, 3), 16);
    const g = parseInt(full.slice(3, 5), 16);
    const b = parseInt(full.slice(5, 7), 16);

    return `rgba(${r}, ${g}, ${b}, ${alpha})`;
  }

  return hex;
}
