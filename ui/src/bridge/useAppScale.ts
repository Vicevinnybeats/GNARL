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
      /*  COMPACT MODE: fill the screen instead of scaling.
       *
       *  The design is 1180x720 and scales as one piece, which is right for a
       *  plugin window and for a desktop browser. On a phone in landscape
       *  that scale is 0.54 and in portrait 0.33 - a 10px label becomes 3px,
       *  and the interface sits in the middle of the screen with black bars
       *  either side. "Zoomed out" is exactly what it is.
       *
       *  So below this size the transform comes OFF and a stylesheet lays the
       *  same components out as one scrolling column at their real size. The
       *  layout budgets that must not be reflowed (CLAUDE.md section 6) are
       *  the DESKTOP ones; they are not in play here, because in compact mode
       *  nothing is competing for a fixed 624px of height - the body scrolls.
       *
       *  Keyed on the viewport, not on a user-agent string: a small window on
       *  a desktop deserves the same treatment, and a phone in landscape at
       *  844px wide does not. The pointer test keeps a narrow desktop window
       *  on the scaled layout, where a mouse can still hit a 16px control.
       */
      const coarse = window.matchMedia('(pointer: coarse)').matches;
      const compact = coarse && (window.innerWidth < 900 || window.innerHeight < 500);

      document.documentElement.dataset.compact = compact ? 'true' : 'false';

      if (compact) {
        //  No transform at all: the stylesheet owns the layout here.
        document.documentElement.style.setProperty('--gn-scale', '1');
        document.documentElement.style.setProperty('--gn-offset-x', '0px');
        document.documentElement.style.setProperty('--gn-offset-y', '0px');
        return;
      }

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
