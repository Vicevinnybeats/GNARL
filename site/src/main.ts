import gsap from 'gsap';
import { ScrollTrigger } from 'gsap/ScrollTrigger';

import { decodeOnReveal } from './decode';
import { createMatrixRain } from './matrixRain';
import { createUniverse } from './universe';

import './styles.css';

gsap.registerPlugin(ScrollTrigger);

const canvas = document.querySelector<HTMLCanvasElement>('#stage');
const rainCanvas = document.querySelector<HTMLCanvasElement>('#rain');

if (canvas) {
  const universe = createUniverse(canvas);
  universe.resize();

  const rain = rainCanvas ? createMatrixRain(rainCanvas) : null;

  /*  ScrollTrigger only reports PROGRESS. All the easing lives in the render
      loop, because it has the frame clock and ScrollTrigger does not -
      animating a transform straight off the scroll event ties the motion to
      the event's rate, which is steppy on a trackpad and whatever the OS
      feels like on a phone. */
  ScrollTrigger.create({
    trigger: document.body,
    start: 'top top',
    end: 'bottom bottom',
    onUpdate: (self) => universe.setScroll(self.progress),
  });

  /*  Pointer in 0..1. `pointermove` rather than `mousemove` so a stylus and
      a finger-drag reach it too; on a touch screen there is no hover, so the
      view simply leans towards the last touch, which is the honest behaviour
      rather than a fake cursor. */
  window.addEventListener(
    'pointermove',
    (event) => {
      universe.setPointer(
        event.clientX / window.innerWidth,
        event.clientY / window.innerHeight,
      );
    },
    { passive: true },
  );

  window.addEventListener('resize', () => {
    universe.resize();
    rain?.resize();
    ScrollTrigger.refresh();
  });

  /*  THE LOOP MEASURES ITS OWN STEP, and hands it to the renderer.
      Everything that eases downstream is a function of that number rather
      than of "one frame", which is the whole reason the previous version
      appeared to teleport on a fast scroll: a per-frame lerp on a machine
      dropping frames advances in a handful of enormous jumps. Clamped,
      because a backgrounded tab returns with a delta measured in seconds
      and an unclamped step would fling the camera across the scene in one
      frame. */
  let previous = performance.now();

  const frame = (now: number) => {
    const delta = Math.min((now - previous) / 1000, 0.1);
    previous = now;

    universe.render(now / 1000, delta);
    rain?.render(now);

    requestAnimationFrame(frame);
  };

  requestAnimationFrame(frame);
}

/*  Cards rise as they arrive. Separate from the decoding, which runs on its
    own observer: one is the block moving, the other is the text resolving,
    and they are allowed to be out of step. */
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
