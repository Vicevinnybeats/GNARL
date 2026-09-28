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

      /*  CENTRED IN WHATEVER IS LEFT OVER.
       *
       *  The app scales from `top left`, so a window whose aspect does not
       *  match 1180x720 leaves the difference as dead space at the right and
       *  bottom. Inside a plugin the editor fixes the aspect and there is
       *  never any, which is why this went unnoticed - but a browser window
       *  is any shape at all, and a phone in landscape is 844x390 against a
       *  design that wants 1.64:1, so the interface sat against the left
       *  edge with a black column beside it.
       *
       *  Offsets rather than `transform-origin: center`, because the origin
       *  decides where the scale pivots and the layout's pixel budgets are
       *  measured from the top left corner. Moving the pivot would move
       *  every absolutely positioned popover with it.
       */
      const offsetX = Math.max(0, (window.innerWidth - DESIGN_WIDTH * scale) / 2);
      const offsetY = Math.max(0, (window.innerHeight - DESIGN_HEIGHT * scale) / 2);

      const root = document.documentElement.style;

      root.setProperty('--gn-scale', String(scale));
      root.setProperty('--gn-offset-x', `${Math.round(offsetX)}px`);
      root.setProperty('--gn-offset-y', `${Math.round(offsetY)}px`);
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
