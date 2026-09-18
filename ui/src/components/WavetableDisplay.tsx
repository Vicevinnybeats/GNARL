import { useEffect, useRef } from 'react';

import {
  DISPLAY_HARMONIC_COUNT,
  getTableFrames,
  synthesiseFrame,
  synthesiseInterpolated,
} from '../bridge/wavetableData';
import { applyWarp, warpBandwidthExpansion } from '../bridge/warp';
import { isCanvasGlowEnabled } from '../settings';
import { useModulationRef } from '../bridge/useModulation';
import './WavetableDisplay.css';

export interface WavetableDisplayProps {
  tableIndex: number;
  /** 0..1 across the table. */
  position: number;
  warpMode: number;
  /** -1..1. */
  warpAmount: number;
  /** 2D single-frame view instead of the receding stack. */
  flat?: boolean;
  /** Which oscillator this display belongs to.
      When set, the drawn position follows the ENGINE's modulated table
      position rather than the knob, so the waveform moves on its own while an
      LFO is running. That is the whole point of a wavetable display in a
      modulation synth: what is drawn should be the frame being played, not
      the frame the user last dragged to. */
  oscIndex?: 0 | 1;
}

/** Frames drawn behind and in front of the active one in the stacked view. */
const STACK_DEPTH = 9;

/** Samples per drawn cycle. Enough that the line reads as a curve rather than
    a polygon, cheap enough to re-synthesise every animation frame. */
const CYCLE_SAMPLES = 220;

/**
 * The wavetable display: the visual centrepiece of the oscillator.
 *
 * Draws the frames receding in Z with the active frame bright and forward, so
 * the shape of the whole table is visible at once and moving the position
 * control reads as travelling THROUGH it rather than as a shape changing.
 * That is what makes a wavetable synth feel alive rather than like a menu of
 * waveforms.
 *
 * The drawn position is smoothed towards the parameter rather than following
 * it exactly, so a jumped value (a preset load, a big drag) animates instead
 * of teleporting - the movement is most of the point.
 *
 * Canvas 2D rather than WebGL: this is a few dozen polylines at 60 fps, which
 * canvas handles comfortably, and it avoids shipping shader code and a context
 * that some hosts' embedded webviews refuse to create.
 */
export function WavetableDisplay({
  tableIndex,
  position,
  warpMode,
  warpAmount,
  flat,
  oscIndex,
}: WavetableDisplayProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  // A ref, not React state: the frames arrive at 60 Hz, and re-rendering the
  // whole oscillator panel sixty times a second is exactly what makes a
  // webview UI feel heavy inside a DAW. The draw loop reads it directly.
  const modulation = useModulationRef();

  // Animation state lives in refs so the render loop is not restarted by a
  // React re-render, which would make the motion stutter.
  const target = useRef({ tableIndex, position, warpMode, warpAmount, flat: !!flat });
  const drawn = useRef({ position });
  const scratch = useRef(new Float32Array(CYCLE_SAMPLES));

  target.current = { tableIndex, position, warpMode, warpAmount, flat: !!flat };

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const context = canvas.getContext('2d');
    if (!context) return;

    let running = true;
    let frameHandle = 0;

    const resize = () => {
      const dpr = window.devicePixelRatio || 1;
      const rect = canvas.getBoundingClientRect();

      if (rect.width <= 0 || rect.height <= 0) return false;

      const width = Math.round(rect.width * dpr);
      const height = Math.round(rect.height * dpr);

      if (canvas.width !== width || canvas.height !== height) {
        canvas.width = width;
        canvas.height = height;
      }

      context.setTransform(dpr, 0, 0, dpr, 0, 0);
      return true;
    };

    const render = () => {
      if (!running) return;

      if (resize()) {
        const styles = getComputedStyle(canvas);
        const accent = styles.getPropertyValue('--gn-accent').trim() || '#b4ff2e';
        const dim = styles.getPropertyValue('--gn-border-strong').trim() || '#33333f';

        const rect = canvas.getBoundingClientRect();
        draw(context, rect.width, rect.height, accent, dim);
      }

      frameHandle = requestAnimationFrame(render);
    };

    const draw = (
      ctx: CanvasRenderingContext2D,
      width: number,
      height: number,
      accent: string,
      dim: string,
    ) => {
      const state = target.current;

      // Ease the drawn position towards the parameter. A first-order filter
      // rather than a fixed duration, so a small nudge settles instantly and
      // a big jump still travels visibly.
      // The engine's own modulated position when there is one, so the display
      // shows the frame being PLAYED rather than the frame the knob is at.
      const live =
        oscIndex !== undefined && modulation.current.playing
          ? modulation.current.tablePositions[oscIndex]
          : undefined;

      const targetPosition = live !== undefined ? live : state.position;

      // A live value already moves at 60 Hz, so it only needs enough smoothing
      // to hide the frame quantisation; a knob value can jump a long way at
      // once and needs the slower ease so it animates rather than teleporting.
      const ease = live !== undefined ? 0.5 : 0.18;

      drawn.current.position += (targetPosition - drawn.current.position) * ease;

      const frames = getTableFrames(state.tableIndex);

      const expansion = warpBandwidthExpansion(state.warpMode, state.warpAmount);
      const harmonicLimit = Math.max(
        4,
        Math.round(DISPLAY_HARMONIC_COUNT / Math.max(1, expansion)),
      );

      const warpFn =
        state.warpAmount === 0
          ? undefined
          : (phase: number) => applyWarp(phase, state.warpMode, state.warpAmount);

      ctx.clearRect(0, 0, width, height);

      const out = scratch.current;

      const plot = (
        spectrumPosition: number,
        depth: number,
        alpha: number,
        colour: string,
        lineWidth: number,
        glow: number,
      ) => {
        synthesiseInterpolated(frames, spectrumPosition, out, harmonicLimit, warpFn);

        // A shallow oblique projection. Frames further back are narrower and
        // higher, which reads as depth without needing a real 3D pipeline.
        const perspective = 1 - depth * 0.34;
        const drawWidth = width * perspective;
        const left = (width - drawWidth) / 2;
        const baseline = height * (0.62 - depth * 0.3);
        const amplitude = height * 0.3 * perspective;

        ctx.beginPath();

        for (let i = 0; i < out.length; i += 1) {
          const x = left + (i / (out.length - 1)) * drawWidth;
          const y = baseline - (out[i] ?? 0) * amplitude;

          if (i === 0) ctx.moveTo(x, y);
          else ctx.lineTo(x, y);
        }

        ctx.globalAlpha = alpha;
        ctx.strokeStyle = colour;
        ctx.lineWidth = lineWidth;
        ctx.lineJoin = 'round';

        // The glow is what makes the active frame read as lit rather than
        // merely brighter. Only applied to the front frames: a shadow on every
        // line in the stack is a lot of compositing for something the eye
        // cannot pick out anyway.
        // Read per FRAME, like the colours around it - a dataset lookup costs
        // nothing, and this display redraws continuously anyway, so it needs
        // no render dependency (CLAUDE.md section 6).
        const shadow = isCanvasGlowEnabled() ? glow : 0;

        ctx.shadowBlur = shadow;
        ctx.shadowColor = shadow > 0 ? colour : 'transparent';

        ctx.stroke();

        ctx.shadowBlur = 0;
        ctx.globalAlpha = 1;
      };

      const activePosition = drawn.current.position;

      if (state.flat) {
        plot(activePosition, 0, 1, accent, 2, 10);
      } else {
        // Back to front, so nearer frames paint over further ones.
        for (let step = STACK_DEPTH; step >= 1; step -= 1) {
          const offset = step / (frames.length - 1);

          for (const direction of [-1, 1]) {
            const spectrumPosition = activePosition + offset * direction * 2.2;

            if (spectrumPosition < 0 || spectrumPosition > 1) continue;

            const fade = 1 - step / (STACK_DEPTH + 1);
            plot(spectrumPosition, step / STACK_DEPTH, fade * 0.42, dim, 1, 0);
          }
        }

        plot(activePosition, 0, 1, accent, 2, 12);
      }

      // Position readout, so the display doubles as the control's value.
      ctx.globalAlpha = 0.65;
      ctx.fillStyle = dim;
      ctx.font = '9px ui-monospace, monospace';
      // The DRAWN position, so the readout agrees with the line that is
      // actually lit while modulation is moving it.
      ctx.fillText(`${Math.round(drawn.current.position * 100)}%`, 6, height - 6);
      ctx.globalAlpha = 1;
    };

    frameHandle = requestAnimationFrame(render);

    return () => {
      running = false;
      cancelAnimationFrame(frameHandle);
    };
  }, [modulation, oscIndex]);

  return <canvas ref={canvasRef} className="gn-wavetable" />;
}

/** Exported for the gallery and tests. */
export { synthesiseFrame };
