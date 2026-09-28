import gsap from 'gsap';
import { ScrollTrigger } from 'gsap/ScrollTrigger';
import * as THREE from 'three';

import { SECTIONS, mountChrome, trackSections } from './chrome';
import { decodeOnReveal } from './decode';
import { createLoader } from './loader';

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
const bootCanvas = document.querySelector<HTMLCanvasElement>('#boot');

/*  THE BOOT SCREEN GOES UP FIRST, before anything expensive is asked for, and
    the page is held still underneath it - a loader you can scroll behind is
    just a decoration over a page that already started.

    `journey.ts` is reached through a DYNAMIC import for the same reason. A
    static one puts the whole scene in the first chunk, so the browser parses
    every byte of it before a single frame of the loader can be drawn, and the
    screen that exists to cover the wait would arrive at the end of it. */
const loader = bootCanvas ? createLoader(bootCanvas) : null;

if (loader) document.documentElement.classList.add('gn-booting');

loader?.setProgress(0.08);

type Renderable = { render(elapsed: number, delta: number): void };

let journey: (Renderable & { resize(): void }) | null = null;
let rain: { render(now: number): void; resize(): void } | null = null;
let loaderAlive = loader !== null;

/*  ONE LOOP for both, measuring its own step. Everything downstream eases as
    a function of that number rather than of "one frame". Clamped, because a
    backgrounded tab comes back with a delta measured in seconds and an
    unclamped step would fling the camera across the world in a single
    frame. */
let previous = performance.now();
let framesRendered = 0;

const frame = (now: number) => {
  const delta = Math.min((now - previous) / 1000, 0.1);
  previous = now;

  if (journey) {
    journey.render(now / 1000, delta);
    rain?.render(now);
    framesRendered += 1;
  }

  if (loaderAlive) loader?.render(now / 1000, delta);

  requestAnimationFrame(frame);
};

requestAnimationFrame(frame);

/*  Real progress, not a timer. Three's loading manager reports the textures,
    and the stages either side of it are the module fetch and the scene build.
    A bar driven by a clock is a lie, and on the one connection where it
    matters it is confidently wrong. */
THREE.DefaultLoadingManager.onProgress = (_url, loaded, total) => {
  if (total > 0) loader?.setProgress(0.45 + (loaded / total) * 0.4);
};

async function start() {
  if (!canvas) return;

  const { createJourney } = await import('./journey');
  loader?.setProgress(0.45);

  journey = await createJourney(canvas);
  journey.resize();
  loader?.setProgress(0.9);

  rain = rainCanvas ? createMatrixRainDeferred(rainCanvas) : null;

  const journeyRef = journey as unknown as {
    setScroll(v: number): void;
    setPointer(x: number, y: number): void;
    resize(): void;
  };

  /*  WHERE THE JOURNEY ENDS IS MEASURED, NOT ASSUMED. The five formations
      belong to the five sections, so the last formation has to arrive when
      the LAST SECTION is centred - which is not the bottom of the document,
      because the download section carries a FAQ and is taller than a screen.

      It was a constant 0.82, and that constant was simply wrong: the sections
      centre at 0, 0.244, 0.489, 0.733 and 0.977 of the scroll, so dividing by
      0.82 put the interface on the FX section and the dissolve on PRESETS -
      every object one section early. Measuring it also means it cannot drift
      the next time a paragraph gets longer. */
  const journeyEnd = () => {
    const max = document.body.scrollHeight - window.innerHeight;
    const last = document.getElementById(SECTIONS[SECTIONS.length - 1]?.id ?? '');

    if (!last || max <= 0) return 1;

    const rect = last.getBoundingClientRect();
    const centre = rect.top + window.scrollY + rect.height / 2 - window.innerHeight / 2;

    // Never 0, or the first scroll event divides by it.
    return Math.max(0.1, Math.min(1, centre / max));
  };

  let end = journeyEnd();

  ScrollTrigger.create({
    trigger: document.body,
    start: 'top top',
    end: 'bottom bottom',
    onUpdate: (self) => journeyRef.setScroll(Math.min(1, self.progress / end)),
    onRefresh: () => {
      end = journeyEnd();
    },
  });

  window.addEventListener(
    'pointermove',
    (event) => {
      journeyRef.setPointer(
        event.clientX / window.innerWidth,
        event.clientY / window.innerHeight,
      );
    },
    { passive: true },
  );

  window.addEventListener('resize', () => {
    journeyRef.resize();
    rain?.resize();
    loader?.resize();
    ScrollTrigger.refresh();
  });

  /*  Exposed for `tools/screenshot_site.mjs`, which waits for the scene to
      settle rather than for a fixed number of milliseconds. Under software GL
      the page runs at a few frames a second and a fixed wait photographs the
      camera mid-move. */
  (window as unknown as Record<string, unknown>).__gnarlJourney = journey;

  if (!loader) return;

  /*  Not ready when the objects EXIST - ready when one frame of them has
      actually been drawn. The first frame is where the shaders compile, and
      on a slow machine that is the longest single pause of the whole load.
      Dismissing before it means handing the viewer a frozen page. */
  await new Promise<void>((resolve) => {
    const check = () => (framesRendered > 1 ? resolve() : requestAnimationFrame(check));
    check();
  });

  loader.ready();

  loader.onEnter(() => {
    document.documentElement.classList.remove('gn-booting');
  });

  /*  Torn down when the dismissal has finished DRAWING, never on a timer.
      The fade advances per frame; a wall-clock wait for it is a guess at the
      frame rate, and at a few frames a second the 1400ms this used to wait
      disposed the renderer a third of the way through - leaving the canvas
      frozen on a half-faded loader over the page, permanently. */
  loader.onDismissed(() => {
    loaderAlive = false;
    loader.dispose();
  });
}

/*  matrixRain is small, but it is only ever needed alongside the journey, so
    it rides the same dynamic boundary rather than the first chunk. */
function createMatrixRainDeferred(element: HTMLCanvasElement) {
  const proxy = {
    render: (_now: number) => {},
    resize: () => {},
  };

  void import('./matrixRain').then(({ createMatrixRain }) => {
    const real = createMatrixRain(element);
    proxy.render = (now: number) => real.render(now);
    proxy.resize = () => real.resize();
  });

  return proxy;
}

void start();
