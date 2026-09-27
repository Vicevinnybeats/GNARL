import { useEffect } from 'react';

/** The design size, matching `--gn-design-*` and the editor's own constants
 *  in `WebUIEditor.cpp`. */
const DESIGN_WIDTH = 1180;
const DESIGN_HEIGHT = 720;

/**
 * Scales the whole interface to the window instead of reflowing it.
 *
 * WHY SCALE AND NOT REFLOW. The tab layout is built on exact pixel budgets —
 * 720 minus the header, status bar and padding leaves 624px, and every row
 * inside is an explicit height (CLAUDE.md §6). That arithmetic is what stops
 * panels overflowing onto each other, and it holds at exactly one size.
 * Making the rows proportional is the thing that has broken this layout four
 * times.
 *
 * The editor fixes the aspect ratio, so one uniform scale is enough and
 * nothing ever stretches. `min` of the two ratios rather than just the width,
 * because a host that ignores the constrainer by a pixel would otherwise
 * clip the bottom row.
 */
export function useAppScale(): void {
  useEffect(() => {
    const apply = () => {
      const scale = Math.min(
        window.innerWidth / DESIGN_WIDTH,
        window.innerHeight / DESIGN_HEIGHT,
      );

      document.documentElement.style.setProperty('--gn-scale', String(scale));
    };

    apply();

    /*  ResizeObserver on the element rather than a window `resize` listener:
        a plugin window inside a DAW is resized by the host, and some hosts
        resize the webview without firing `resize` on the window. */
    const observer = new ResizeObserver(apply);
    observer.observe(document.documentElement);

    window.addEventListener('resize', apply);

    return () => {
      observer.disconnect();
      window.removeEventListener('resize', apply);
    };
  }, []);
}
