import gsap from 'gsap';
import { ScrollTrigger } from 'gsap/ScrollTrigger';

import { decodeOnReveal } from './decode';
import { createScene } from './scene';

import './styles.css';

gsap.registerPlugin(ScrollTrigger);

const canvas = document.querySelector<HTMLCanvasElement>('#stage');

if (canvas) {
  const scene = createScene(canvas);
  scene.resize();

  /*  THE SCROLL DRIVES THE ZOOM, and ScrollTrigger only reports progress -
      the easing lives in the scene's render loop. Animating the panel's
      transform directly from ScrollTrigger would tie the motion to the
      scroll event's rate, which on a trackpad is steppy and on a phone is
      whatever the OS feels like. */
  ScrollTrigger.create({
    trigger: document.body,
    start: 'top top',
    end: 'bottom bottom',
    onUpdate: (self) => scene.setScroll(self.progress),
  });

  /*  Which face of the instrument shows. Each section owns one, so
      scrolling through the page walks the three tabs - the site showing the
      product rather than a picture of it. */
  const faces: Array<[string, number]> = [
    ['#top', 0],
    ['#engine', 0],
    ['#presets', 1],
    ['#fx', 2],
    ['#download', 2],
  ];

  for (const [selector, index] of faces) {
    const element = document.querySelector(selector);
    if (!element) continue;

    ScrollTrigger.create({
      trigger: element,
      start: 'top 60%',
      end: 'bottom 40%',
      onEnter: () => scene.setTexture(index),
      onEnterBack: () => scene.setTexture(index),
    });
  }

  /*  Pointer in 0..1. `pointermove` rather than `mousemove` so a stylus and
      a finger-drag reach it too; on a touch screen there is no hover, so the
      lattice simply lights where the last touch was, which is the honest
      behaviour rather than a fake cursor. */
  window.addEventListener(
    'pointermove',
    (event) => {
      scene.setPointer(event.clientX / window.innerWidth, event.clientY / window.innerHeight);
    },
    { passive: true },
  );

  window.addEventListener('resize', () => {
    scene.resize();
    ScrollTrigger.refresh();
  });

  let last = performance.now();

  const frame = (now: number) => {
    last = now;
    scene.render(last / 1000);
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
