import { useEffect, useRef } from 'react';

import { applyTension } from '../dsp/lfoCurve';
import './EnvelopeDisplay.css';

/**
 * Draws a DAHDSR envelope's shape.
 *
 * PER-SEGMENT CURVES are the reason this is drawn rather than described by its
 * numbers. A linear attack-decay cannot produce a convincing pluck, and the
 * difference between a linear decay and an exponential one is obvious in a
 * picture and invisible in a row of knob readouts.
 *
 * Segment widths are proportional to their times, with the sustain given a
 * fixed slice: sustain has no duration, and letting the times alone set the
 * widths would make a 5 ms attack a shape you cannot see next to a 4 s
 * release.
 */

export interface EnvelopeDisplayProps {
  delay: number;
  attack: number;
  hold: number;
  decay: number;
  sustain: number;
  release: number;
  attackCurve: number;
  decayCurve: number;
  releaseCurve: number;
  /** DAHDSR draws the delay and hold segments; ADSR skips them. */
  dahdsr: boolean;
  /** Draws the envelope lit, for the one that is the amp envelope. */
  highlight?: boolean;
}

/** The sustain segment's share of the width. Enough to read as a plateau,
    little enough that the timed segments keep most of the space. */
const SUSTAIN_SHARE = 0.18;

export function EnvelopeDisplay(props: EnvelopeDisplayProps) {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const propsRef = useRef(props);
  propsRef.current = props;

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

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

      drawEnvelope(context, rect, propsRef.current, accent);
    };

    draw();

    // Redrawn on resize as well as on every prop change, because the shape is
    // laid out in canvas pixels rather than in CSS.
    const observer = new ResizeObserver(draw);
    observer.observe(canvas);

    return () => observer.disconnect();
  }, [props]);

  return (
    <div className="gn-envelope-display" data-highlight={props.highlight ? 'true' : 'false'}>
      <canvas ref={canvasRef} className="gn-envelope-display__canvas" />
    </div>
  );
}

function drawEnvelope(
  context: CanvasRenderingContext2D,
  rect: DOMRect,
  props: EnvelopeDisplayProps,
  accent: string,
): void {
  const delay = props.dahdsr ? props.delay : 0;
  const hold = props.dahdsr ? props.hold : 0;

  const timed = delay + props.attack + hold + props.decay + props.release;
  const timedWidth = rect.width * (1 - SUSTAIN_SHARE);

  // A patch with every time at zero would divide by zero; it is also a real
  // patch - a gate - and has to draw as one.
  const scale = timed > 0 ? timedWidth / timed : 0;

  const bottom = rect.height - 1;
  const top = 1;
  const toY = (level: number) => bottom - level * (bottom - top);

  const path: Array<{ x: number; y: number }> = [];

  let x = 0;
  path.push({ x, y: toY(0) });

  // Delay: silent, so a flat run along the bottom.
  x += delay * scale;
  path.push({ x, y: toY(0) });

  // Attack. The curve is inverted relative to the others because the segment
  // rises: the same tension has to bend an attack and a decay the same way to
  // the eye.
  pushSegment(path, x, props.attack * scale, 0, 1, -props.attackCurve, toY);
  x += props.attack * scale;

  x += hold * scale;
  path.push({ x, y: toY(1) });

  pushSegment(path, x, props.decay * scale, 1, props.sustain, props.decayCurve, toY);
  x += props.decay * scale;

  x += rect.width * SUSTAIN_SHARE;
  path.push({ x, y: toY(props.sustain) });

  pushSegment(path, x, props.release * scale, props.sustain, 0, props.releaseCurve, toY);
  x += props.release * scale;

  path.push({ x, y: toY(0) });

  context.beginPath();

  for (let i = 0; i < path.length; i += 1) {
    const point = path[i];
    if (!point) continue;

    if (i === 0) context.moveTo(point.x, point.y);
    else context.lineTo(point.x, point.y);
  }

  const fill = context.createLinearGradient(0, 0, 0, rect.height);
  fill.addColorStop(0, hexWithAlpha(accent, 0.24));
  fill.addColorStop(1, hexWithAlpha(accent, 0.02));

  context.save();
  context.lineTo(x, bottom);
  context.lineTo(0, bottom);
  context.closePath();
  context.fillStyle = fill;
  context.fill();
  context.restore();

  context.strokeStyle = accent;
  context.lineWidth = 1.5;
  context.stroke();

  // The sustain level, marked so it can be read off without dragging.
  context.strokeStyle = hexWithAlpha(accent, 0.22);
  context.setLineDash([2, 3]);
  context.lineWidth = 1;
  context.beginPath();
  context.moveTo(0, toY(props.sustain));
  context.lineTo(rect.width, toY(props.sustain));
  context.stroke();
  context.setLineDash([]);
}

function pushSegment(
  path: Array<{ x: number; y: number }>,
  startX: number,
  width: number,
  from: number,
  to: number,
  curve: number,
  toY: (level: number) => number,
): void {
  if (width <= 0.5) return;

  const steps = Math.max(2, Math.min(64, Math.round(width)));

  for (let i = 1; i <= steps; i += 1) {
    const t = i / steps;
    const shaped = applyTension(t, curve);

    path.push({ x: startX + t * width, y: toY(from + (to - from) * shaped) });
  }
}

function hexWithAlpha(colour: string, alpha: number): string {
  const hex = colour.trim();

  if (!hex.startsWith('#') || hex.length !== 7) return hex;

  const r = parseInt(hex.slice(1, 3), 16);
  const g = parseInt(hex.slice(3, 5), 16);
  const b = parseInt(hex.slice(5, 7), 16);

  return `rgba(${r}, ${g}, ${b}, ${alpha})`;
}
