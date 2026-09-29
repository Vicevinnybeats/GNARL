import { useEffect, useState } from 'react';

/**
 * The numbers that decide the layout, on screen.
 *
 * WHY THIS EXISTS. Whether the interface uses the compact layout or the
 * scaled one is decided by the viewport, the device pixel ratio and the
 * pointer type — and on a phone there is no way to read any of them. Three
 * separate diagnoses in a row were made by measuring a screenshot and
 * solving backwards for what the viewport must have been, and they
 * disagreed with each other, because a screenshot is in DEVICE pixels, the
 * layout is in CSS pixels, and the ratio between them is one of the very
 * things being guessed at.
 *
 * So the phone reports instead. Off unless `?diag=1` is in the URL: this is
 * a diagnostic, not a feature, and a permanent readout of the layout engine
 * across the bottom of a synthesizer is somebody else's bug report.
 */
export interface ViewportReadout {
  width: number;
  height: number;
  dpr: number;
  pointer: string;
  scale: number;
  compact: string;
}

const read = (): ViewportReadout => ({
  width: window.innerWidth,
  height: window.innerHeight,
  dpr: Math.round(window.devicePixelRatio * 100) / 100,
  pointer: window.matchMedia('(pointer: coarse)').matches ? 'coarse' : 'fine',
  //  Recomputed here rather than read from useAppScale, so the readout says
  //  what the inputs imply even if the two ever disagree — which is exactly
  //  the failure it would be used to find.
  scale: Math.round(Math.min(window.innerWidth / 1180, window.innerHeight / 720) * 1000) / 1000,
  compact: document.documentElement.dataset.compact ?? '?',
});

export function useViewportReadout(): ViewportReadout | null {
  const enabled =
    typeof window !== 'undefined' &&
    new URLSearchParams(window.location.search).get('diag') === '1';

  const [state, setState] = useState<ViewportReadout | null>(enabled ? read : null);

  useEffect(() => {
    if (!enabled) return undefined;

    /*  Rotating and the address bar sliding both change these, and the
        second one is the case worth catching: it changes the height without
        any layout meaning to change. */
    const update = () => setState(read());
    const timer = window.setInterval(update, 500);

    window.addEventListener('resize', update);
    window.addEventListener('orientationchange', update);

    return () => {
      window.clearInterval(timer);
      window.removeEventListener('resize', update);
      window.removeEventListener('orientationchange', update);
    };
  }, [enabled]);

  return state;
}
