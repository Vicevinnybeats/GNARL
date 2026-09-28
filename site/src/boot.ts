import gsap from 'gsap';
import { ScrollTrigger } from 'gsap/ScrollTrigger';

import { mountChrome, trackSections } from './chrome';
import { decodeOnReveal } from './decode';
import { createJourney } from './journey';
import { createMatrixRain } from './matrixRain';

import './styles.css';

gsap.registerPlugin(ScrollTrigger);

mountChrome();
trackSections();

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

if (canvas) {
  void createJourney(canvas).then((journey) => {
    journey.resize();

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
      onUpdate: (self) => journey.setScroll(self.progress),
    });

    window.addEventListener(
      'pointermove',
      (event) => {
        journey.setPointer(
          event.clientX / window.innerWidth,
          event.clientY / window.innerHeight,
        );
      },
      { passive: true },
    );

    window.addEventListener('resize', () => {
      journey.resize();
      rain?.resize();
      ScrollTrigger.refresh();
    });

    /*  THE LOOP MEASURES ITS OWN STEP and hands it to the scene. Everything
        downstream eases as a function of that number rather than of "one
        frame". Clamped, because a backgrounded tab comes back with a delta
        measured in seconds and an unclamped step would fling the camera
        across the world in a single frame. */
    let previous = performance.now();

    const frame = (now: number) => {
      const delta = Math.min((now - previous) / 1000, 0.1);
      previous = now;

      journey.render(now / 1000, delta);
      rain?.render(now);

      requestAnimationFrame(frame);
    };

    requestAnimationFrame(frame);
  });
}
