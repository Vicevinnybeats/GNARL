import type { Journey } from './journey';

/**
 * Leaving the landing page for the checkout, without a cut.
 *
 * The site is ONE WORLD and the scroll journey never cuts between objects -
 * the figure morphs, the driver cross-fades in over the lines that already
 * have its shape. A page navigation is the one place that promise breaks: the
 * WebGL context dies, the new document paints, and the illusion of a
 * continuous space ends on a white flash.
 *
 * So the camera TRUCKS sideways first and the copy leaves with it, and the
 * navigation happens at the moment the world is off-frame. The checkout page
 * then opens with its own field already drifting and its cards arriving from
 * the same side, so the two documents read as one move.
 *
 * ONE CLOCK, A FRAME STEP, NOT A TIMER. This repository has made the
 * wall-clock mistake five times (CLAUDE.md lists them: the meter that read
 * the block size, the text decode that counted frames, the scroll damping,
 * the screenshot tool's fixed wait, the loader torn down on a timer while its
 * fade advanced per frame). A setTimeout for the navigation would be a sixth:
 * on a slow phone the timer fires while the camera is still mid-move and the
 * page cuts anyway, which is the exact fault this exists to remove.
 *
 * AND IT NAVIGATES EVEN IF THE ANIMATION CANNOT RUN. Reduced motion, a lost
 * context, a browser that throttles rAF in a background tab - none of those
 * may leave somebody stuck on a page unable to reach the checkout. The link
 * works as a link first; the animation is an enhancement on top.
 */

const DEPART_SECONDS = 0.85;

export function wireDeparture(
  getJourney: () => Journey | null,
  prefersReducedMotion: boolean,
): void {
  const links = document.querySelectorAll<HTMLAnchorElement>('a[href$="checkout.html"]');

  if (links.length === 0) return;

  let leaving = false;

  for (const link of links) {
    link.addEventListener('click', (event) => {
      const journey = getJourney();

      //  Modified clicks are the user asking for a new tab; hijacking one is
      //  rude and breaks middle-click entirely.
      if (event.metaKey || event.ctrlKey || event.shiftKey || event.button !== 0) return;
      if (!journey || prefersReducedMotion) return;
      if (leaving) {
        event.preventDefault();
        return;
      }

      event.preventDefault();
      leaving = true;

      const target = link.href;

      document.body.classList.add('is-departing');

      let elapsed = 0;
      let previous = performance.now();

      const step = (now: number) => {
        /*  Clamped, for the reason the boot clock is: a backgrounded tab
            returns with a huge delta and would fling the camera across the
            world in one frame. A long gap is not no time passed, so it is
            clamped rather than zeroed - zeroing it froze the page once
            already. */
        const delta = Math.min((now - previous) / 1000, 0.1);
        previous = now;
        elapsed += delta;

        const t = Math.min(1, elapsed / DEPART_SECONDS);

        /*  Ease IN, not in-out. The camera should gather speed and still be
            moving when the page changes - a move that decelerates to a stop
            announces that something is about to happen, and then the
            navigation lands like a cut after all. */
        journey.setLateral(t * t);

        if (t < 1) {
          requestAnimationFrame(step);
          return;
        }

        window.location.href = target;
      };

      requestAnimationFrame(step);

      /*  A FLOOR UNDER THE ANIMATION. If rAF stops - a hidden tab, a lost
          context, a browser that decides this page is idle - the navigation
          still happens. Generous enough never to race a healthy run. */
      window.setTimeout(() => {
        if (window.location.href !== target) window.location.href = target;
      }, DEPART_SECONDS * 1000 + 1200);
    });
  }
}
