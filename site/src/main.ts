import gsap from 'gsap';
import { ScrollTrigger } from 'gsap/ScrollTrigger';

import { decodeOnReveal } from './decode';
import { createWorld } from './world';

import './styles.css';

gsap.registerPlugin(ScrollTrigger);

const canvas = document.querySelector<HTMLCanvasElement>('#stage');

if (canvas) {
  const world = createWorld(canvas);
  world.resize();

  /*  THE SCROLL DRIVES THE ZOOM, and ScrollTrigger only reports progress -
      the easing lives in the scene's render loop. Animating the panel's
      transform directly from ScrollTrigger would tie the motion to the
      scroll event's rate, which on a trackpad is steppy and on a phone is
      whatever the OS feels like. */
  ScrollTrigger.create({
    trigger: document.body,
    start: 'top top',
    end: 'bottom bottom',
    onUpdate: (self) => world.setScroll(self.progress),
  });

  /*  NO PER-SECTION TEXTURE SWAP ANY MORE. The flat version had one panel
      and changed the picture on it as each section arrived, which is a
      slideshow wearing a 3D costume. The three tabs are three PLACES in the
      hall now, standing at fixed points down its length, and you see the one
      you have flown to because you have flown to it. Scroll progress is the
      only thing the world needs.

      That also deletes five ScrollTriggers and the ordering bug waiting in
      them - `onEnterBack` firing for a section you are leaving is a class of
      mistake that simply has no analogue in a world with positions. */

  /*  Pointer in 0..1. `pointermove` rather than `mousemove` so a stylus and
      a finger-drag reach it too; on a touch screen there is no hover, so the
      lattice simply lights where the last touch was, which is the honest
      behaviour rather than a fake cursor. */
  window.addEventListener(
    'pointermove',
    (event) => {
      world.setPointer(event.clientX / window.innerWidth, event.clientY / window.innerHeight);
    },
    { passive: true },
  );

  window.addEventListener('resize', () => {
    world.resize();
    ScrollTrigger.refresh();
  });

  let last = performance.now();

  const frame = (now: number) => {
    last = now;
    world.render(last / 1000);
    requestAnimationFrame(frame);
  };

  requestAnimationFrame(frame);
}

/*  Panels rise as they arrive. Separate from the decoding, which runs on
    its own observer: one is the block moving, the other is the text
    resolving, and they are allowed to be out of step. */
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
