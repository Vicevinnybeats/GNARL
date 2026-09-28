import gsap from 'gsap';
import { ScrollTrigger } from 'gsap/ScrollTrigger';

import { mountChrome, mountNextLink } from './chrome';
import { decodeOnReveal } from './decode';
import { createMatrixRain } from './matrixRain';

import './styles.css';

gsap.registerPlugin(ScrollTrigger);

/** What every page needs from its scene, whichever scene it is. */
export interface Scene {
  setScroll(progress: number): void;
  setPointer(x: number, y: number): void;
  resize(): void;
  render(elapsed: number, delta: number): void;
  dispose(): void;
}

/**
 * Everything the five pages do identically.
 *
 * Extracted because they DO do it identically, and five copies of a render
 * loop is five places for the delta-time damping to be reintroduced wrongly
 * in four of them. The pages differ by which scene they build and what their
 * copy says; nothing else about them should be a decision.
 */
export function boot(id: string, build: () => Scene | Promise<Scene>): void {
  mountChrome(id);
  mountNextLink(id);

  const reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  if (!reduced) {
    for (const slab of document.querySelectorAll('.slab')) {
      gsap.from(slab, {
        y: 34,
        opacity: 0,
        duration: 0.7,
        ease: 'power2.out',
        scrollTrigger: { trigger: slab, start: 'top 85%' },
      });
    }
  }

  decodeOnReveal(document.querySelectorAll<HTMLElement>('[data-decode]'));

  const canvas = document.querySelector<HTMLCanvasElement>('#stage');
  const rainCanvas = document.querySelector<HTMLCanvasElement>('#rain');

  if (!canvas) return;

  void Promise.resolve(build()).then((scene) => {
    scene.resize();

    const rain = rainCanvas ? createMatrixRain(rainCanvas) : null;

    /*  ScrollTrigger only reports PROGRESS. All the easing lives in the
        render loop, because it has the frame clock and ScrollTrigger does
        not — animating a transform straight off the scroll event ties the
        motion to the event's rate, which is steppy on a trackpad and
        whatever the OS feels like on a phone. */
    ScrollTrigger.create({
      trigger: document.body,
      start: 'top top',
      end: 'bottom bottom',
      onUpdate: (self) => scene.setScroll(self.progress),
    });

    window.addEventListener(
      'pointermove',
      (event) => {
        scene.setPointer(
          event.clientX / window.innerWidth,
          event.clientY / window.innerHeight,
        );
      },
      { passive: true },
    );

    window.addEventListener('resize', () => {
      scene.resize();
      rain?.resize();
      ScrollTrigger.refresh();
    });

    /*  THE LOOP MEASURES ITS OWN STEP and hands it to the scene. Everything
        that eases downstream is a function of that number rather than of
        "one frame", which is the whole reason an earlier version appeared to
        teleport on a fast scroll. Clamped, because a backgrounded tab comes
        back with a delta measured in seconds and an unclamped step would
        fling the camera across the scene in one frame. */
    let previous = performance.now();

    const frame = (now: number) => {
      const delta = Math.min((now - previous) / 1000, 0.1);
      previous = now;

      scene.render(now / 1000, delta);
      rain?.render(now);

      requestAnimationFrame(frame);
    };

    requestAnimationFrame(frame);
  });
}
