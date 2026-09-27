import { useEffect, useRef } from 'react';

import { SPECTRUM_BINS, SPECTRUM_FLOOR_DB } from '../bridge/modulationFrame';
import { useModulationRef } from '../bridge/useModulation';

import './Spectrum.css';

/**
 * The output spectrum.
 *
 * REDRAWS EVERY FRAME, so it re-reads its colours from `getComputedStyle`
 * each time rather than taking `useTheme()` as a dependency — the second of
 * the two kinds of canvas control in CLAUDE.md §6, like the wavetable
 * display and the LFO editor.
 *
 * Reads the frame from a ref inside its own animation loop. A `setState` per
 * frame would re-render the whole tab thirty times a second for a picture
 * that owns one canvas.
 *
 * The bins arrive already grouped and in dB — the engine does the FFT on the
 * message thread and hands over 128 numbers. Nothing here knows what an FFT
 * is, which is the point: the analysis belongs where the audio is.
 */
export function Spectrum() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const frameRef = useModulationRef();

  /** Smoothed towards the incoming bins, so the display falls rather than
      flickers. Peaks rise instantly — a spectrum that lags its attack reads
      as a slow synth. */
  const heldRef = useRef<number[]>(new Array<number>(SPECTRUM_BINS).fill(SPECTRUM_FLOOR_DB));

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    let handle = 0;

    const draw = () => {
      handle = requestAnimationFrame(draw);

      const context = canvas.getContext('2d');
      if (!context) return;

      const style = getComputedStyle(canvas);
      const accent = style.getPropertyValue('--gn-accent').trim() || '#b4ff2e';
      const dim = style.getPropertyValue('--gn-border').trim() || '#2a2a36';

      const ratio = window.devicePixelRatio || 1;
      const width = canvas.clientWidth;
      const height = canvas.clientHeight;

      if (canvas.width !== width * ratio || canvas.height !== height * ratio) {
        canvas.width = width * ratio;
        canvas.height = height * ratio;
      }

      context.setTransform(ratio, 0, 0, ratio, 0, 0);
      context.clearRect(0, 0, width, height);

      const bins = frameRef.current.spectrum;
      const held = heldRef.current;

      // Octave gridlines, so the eye has something to measure against.
      context.strokeStyle = dim;
      context.lineWidth = 1;

      for (let i = 1; i < 5; i += 1) {
        const x = Math.round((width * i) / 5) + 0.5;
        context.beginPath();
        context.moveTo(x, 0);
        context.lineTo(x, height);
        context.stroke();
      }

      context.beginPath();
      context.moveTo(0, height);

      for (let i = 0; i < SPECTRUM_BINS; i += 1) {
        const incoming = bins[i] ?? SPECTRUM_FLOOR_DB;
        const previous = held[i] ?? SPECTRUM_FLOOR_DB;

        // Instant attack, eased release.
        held[i] = incoming > previous ? incoming : previous + (incoming - previous) * 0.25;

        const normalised = 1 - (held[i] ?? SPECTRUM_FLOOR_DB) / SPECTRUM_FLOOR_DB;
        const x = (i / (SPECTRUM_BINS - 1)) * width;
        const y = height - Math.max(0, Math.min(1, normalised)) * height;

        context.lineTo(x, y);
      }

      context.lineTo(width, height);
      context.closePath();

      const fill = context.createLinearGradient(0, 0, 0, height);
      fill.addColorStop(0, accent);
      fill.addColorStop(1, 'transparent');

      context.globalAlpha = 0.28;
      context.fillStyle = fill;
      context.fill();

      context.globalAlpha = 1;
      context.strokeStyle = accent;
      context.lineWidth = 1.5;
      context.stroke();
    };

    handle = requestAnimationFrame(draw);

    return () => cancelAnimationFrame(handle);
  }, [frameRef]);

  return <canvas className="gn-spectrum" ref={canvasRef} aria-label="Output spectrum" />;
}
