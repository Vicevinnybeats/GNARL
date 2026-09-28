/**
 * The home page being BUILT rather than revealed.
 *
 * Once the boot screen clears, the header, the hero card's lines and the
 * footer fly in from their own sides and settle, while `journey.ts` does the
 * same thing in 3D with the figure's strokes. Both are driven from one
 * progress number in `boot.ts`, so they are one event and not two animations
 * that happen to overlap.
 *
 * Driven per frame from that number rather than handed to a CSS transition or
 * a GSAP tween, for the reason this repository keeps relearning: the 3D half
 * advances on a clamped frame step, and anything easing on a different clock
 * drifts out of step with it on exactly the slow machine where the whole
 * transition is most visible.
 */

/** A piece of the page, and where it comes in from. */
interface Piece {
  readonly element: HTMLElement;
  /** Offset at the start, in pixels, in the page's own axes. */
  readonly x: number;
  readonly y: number;
  /** Fraction of the whole build before this one starts moving. */
  readonly delay: number;
}

export interface Assembly {
  apply(progress: number): void;
  /** Puts every piece back to plain CSS. */
  clear(): void;
}

/*  Each piece's slice of the build. Shorter than 1, so the later pieces are
    still arriving while the earlier ones have landed - the overlap is what
    makes it read as assembly rather than as a queue. */
const WINDOW = 0.55;

/*  Which elements move, and from where. Read as a composition: the mark
    comes from the left edge it sits on, the call to action from the right
    edge it sits on, the headline from the left under them, and everything
    that sits low rises. Nothing comes from a side it does not belong to -
    that reads as scattered rather than as built. */
const PLAN: ReadonlyArray<{
  selector: string;
  x: number;
  y: number;
  delay: number;
}> = [
  { selector: '.nav__mark', x: -90, y: 0, delay: 0.0 },
  { selector: '.nav__links', x: 0, y: -54, delay: 0.06 },
  { selector: '.nav__cta', x: 90, y: 0, delay: 0.1 },

  { selector: '.slab--hero .eyebrow', x: -70, y: 0, delay: 0.14 },
  { selector: '.slab--hero .display', x: -130, y: 0, delay: 0.2 },
  { selector: '.slab--hero .lede', x: -70, y: 26, delay: 0.28 },
  { selector: '.slab--hero .row', x: 0, y: 54, delay: 0.34 },

  { selector: '.scroll-hint', x: 0, y: 40, delay: 0.4 },
  { selector: '.foot', x: 0, y: 46, delay: 0.42 },
];

export function createAssembly(): Assembly {
  const pieces: Piece[] = [];

  for (const entry of PLAN) {
    const element = document.querySelector<HTMLElement>(entry.selector);

    // A missing selector is not an error: the copy is allowed to change
    // without this file having to be edited in the same commit.
    if (element) {
      pieces.push({ element, x: entry.x, y: entry.y, delay: entry.delay });
    }
  }

  /*  The hero CARD itself is not in the plan - it is the surface the lines
      are being laid onto, so it fades without moving. Moving it too would
      slide the whole composition and hide the fact that the parts arrived
      separately. */
  const card = document.querySelector<HTMLElement>('.slab--hero');

  const reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  return {
    apply(progress: number) {
      if (reduced) return;

      for (const piece of pieces) {
        const t = Math.max(0, Math.min(1, (progress - piece.delay) / WINDOW));

        // Cubic ease out: quick out of the dark, decelerating onto the mark.
        const e = 1 - Math.pow(1 - t, 3);
        const away = 1 - e;

        piece.element.style.transform = `translate3d(${piece.x * away}px, ${piece.y * away}px, 0)`;
        piece.element.style.opacity = String(e);
        piece.element.style.willChange = 'transform, opacity';
      }

      if (card) {
        card.style.opacity = String(Math.min(1, progress * 2.2));
      }
    },

    clear() {
      /*  Every inline style removed, not set back to its resting value: a
          left-behind `transform: none` still makes each of these a
          containing block and a compositing layer for the rest of the
          session, and `will-change` on nine elements is nine layers the
          compositor keeps alive for an animation that finished. */
      for (const piece of pieces) {
        piece.element.style.removeProperty('transform');
        piece.element.style.removeProperty('opacity');
        piece.element.style.removeProperty('will-change');
      }

      card?.style.removeProperty('opacity');
    },
  };
}
