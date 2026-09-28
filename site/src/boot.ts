import gsap from 'gsap';
import { ScrollTrigger } from 'gsap/ScrollTrigger';
import * as THREE from 'three';

import { SECTIONS, mountChrome, trackSections } from './chrome';
import { type Assembly, createAssembly } from './assemble';
import { decodeOnReveal } from './decode';
/*  TYPE ONLY. `import type` is erased at build, so naming the Journey
    interface here does not drag journey.ts into the first chunk and undo the
    dynamic import below - which is the whole reason the boot screen can be
    drawn before the scene is parsed. */
import type { Journey } from './journey';
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

let journey: Journey | null = null;
let rain: { render(now: number): void; resize(): void } | null = null;
let loaderAlive = loader !== null;
let entering = false;
let assembly: Assembly | null = null;

function finishBuild() {
  entering = false;

  /*  Snapped to the end rather than left wherever the last frame reached,
      then every inline style taken off. See assemble.ts: a left-behind
      transform is a compositing layer kept alive for the rest of the
      session for an animation that has finished. */
  applyBuild(1);
  assembly?.clear();

  root.classList.remove('gn-entering');
  root.classList.remove('gn-booting');

  // The page just changed size on the way in, and ScrollTrigger measured it
  // mid-build.
  ScrollTrigger.refresh();
}

/*  ONE LOOP for both, measuring its own step. Everything downstream eases as
    a function of that number rather than of "one frame". Clamped, because a
    backgrounded tab comes back with a delta measured in seconds and an
    unclamped step would fling the camera across the world in a single
    frame. */
let previous = performance.now();
let framesRendered = 0;

/*  Returning to a backgrounded tab is the one case the clamp above cannot
    tell apart from a slow frame, so it is handled at the source: reset the
    clock, and the first frame back measures from now rather than from
    whenever the tab was last drawn. */
document.addEventListener('visibilitychange', () => {
  previous = performance.now();
});

const root = document.documentElement;

/*  ONE CLOCK DRIVES THE WHOLE ARRIVAL. The loader's dial rushing past the
    camera, the journey's dolly, the figure's strokes flying in from every
    side and the page's own pieces sliding in from theirs are all views of
    one event. Anything easing on its own timer would drift out of step with
    the rest on exactly the slow machine where the transition is most
    visible - the loader's exit advances on the same clamped frame step this
    does, which is what keeps them together.

    It runs for BUILD_SECONDS, which outlasts the loader: the dial is gone
    about a third of the way through and the page spends the rest of it
    assembling. That ordering is the point - the boot screen leaves and the
    home page is visibly put together behind it, rather than being revealed
    already finished. */
const BUILD_SECONDS = 2.1;

let build = 0;

/*  Where the journey finishes, as a fraction of the document's scroll. Set
    once the layout is known and re-measured on every ScrollTrigger refresh;
    1 until then, so a frame drawn before the measurement simply reads the
    raw scroll rather than dividing by something meaningless. */
let end = 1;

/*  Published for `tools/screenshot_site.mjs`. The journey no longer finishes
    at the foot of the document - on a phone it ends above the download card,
    so the last section's composition is at THIS fraction of the scroll and
    not at 1. A tool that only photographs 1 photographs the page after the
    card has slid up over the instrument. */
function publishEnd() {
  const w = window as unknown as Record<string, unknown>;
  const max = maxScroll;

  w.__gnarlJourneyEnd = end;

  /*  And the ABSOLUTE pixel offset, which is the one the tool should use.
      A fraction has to be multiplied back out by the document's scrollable
      height, and Playwright's phone emulation transiently reports a viewport
      several times too tall - so `scrollHeight - innerHeight` computed on
      that side is wrong, the fraction resolves to the wrong pixel, and the
      landing check passes because it verifies the browser reached the target
      it was given rather than that the target was right. Computed here, it
      is self-consistent with the numbers the journey itself measured. */
  w.__gnarlJourneyEndPx = Math.max(0, Math.round(end * max));
}

/*  The document's scrollable height, MEASURED WHEN THE LAYOUT CHANGES and not
    on every frame. Two separate faults came from reading it in the render
    loop, and both are worst on a phone, which is where they were reported:

    reading `scrollHeight` forces a synchronous LAYOUT, so the loop paid for a
    full reflow on every frame it drew, competing with the WebGL scene for the
    same budget;

    and on a phone the address bar collapses as you scroll, which changes
    `innerHeight` and therefore `max` MID-GESTURE - so the normalised position
    jumped discontinuously while the raw scroll was perfectly smooth. That is
    the "it jumps between objects" that survived fixing the scroll sampling:
    the sampling was fine by then, and the thing being divided by was moving. */
let maxScroll = 0;

/*  Queried from `document.scrollingElement`, which is the element the window
    actually scrolls, rather than from `body`. In this document the two happen
    to report the same height, so this is correctness rather than a fix for
    anything observed - `body` is simply not guaranteed to be the scrolling
    box and there is no reason to ask the wrong element.

    MEASURED HERE AND NOT IN THE RENDER LOOP, which is the part that mattered.
    Reading `scrollHeight` forces a synchronous LAYOUT, so sampling it every
    frame made the loop pay for a full reflow per frame, competing with the
    WebGL scene for the same budget - worst on a phone, which is where the
    jank was reported. And on a phone the address bar collapses as you scroll,
    changing `innerHeight` and therefore this number MID-GESTURE: the
    normalised position then jumped while the raw scroll was perfectly smooth,
    which is the "it jumps between objects" that survived fixing the scroll
    sampling. The sampling was right by then; the thing being divided by was
    moving. */
function measureScroll() {
  const scroller = document.scrollingElement ?? document.documentElement;

  maxScroll = Math.max(0, scroller.scrollHeight - window.innerHeight);
}

measureScroll();

/** The live scroll position, normalised so the journey ends at 1. */
const readScroll = () => {
  if (maxScroll <= 0) return 0;

  return Math.min(1, window.scrollY / maxScroll / end);
};

const applyBuild = (progress: number) => {
  /*  The camera arrives FASTER than the parts assemble, and finishes first.
      A dolly still running while the last strokes land makes two things move
      at once and reads as drift; landing the camera first gives the build
      something still to be built against. */
  const camera = 1 - Math.pow(1 - Math.min(1, progress / 0.55), 3);

  journey?.setEntry(1 - camera);
  journey?.setAssemble(Math.min(1, progress / 0.92));

  assembly?.apply(progress);
};

const frame = (now: number) => {
  const elapsed = (now - previous) / 1000;
  previous = now;

  /*  A QUARTER SECOND, not a tenth. During a stutter a tenth advances the
      scene by less time than actually passed, so it falls behind the scroll
      and then catches up in a rush when the frames return - which reads as
      the page JUMPING to the next object rather than travelling to it.

      Still clamped, because a backgrounded tab hands over a delta measured
      in seconds and easing through that in one step flings the camera across
      the world. That case is handled by resetting the clock on
      `visibilitychange` below, so the clamp here only ever has to cover a
      genuinely slow frame.

      It must never be ZERO. An earlier attempt made a long gap snap by
      zeroing the step, and on a renderer slow enough to exceed the threshold
      every frame that froze the whole page: the build never advanced, the
      boot screen never dismissed, and the site sat behind it. A long gap is
      not "no time passed". */
  const delta = Math.min(elapsed, 0.25);

  if (journey) {
    /*  SAMPLED EVERY FRAME, not driven from a scroll event. Mobile browsers
        throttle or withhold scroll events during momentum, so a handler-fed
        position goes stale mid-flick and then arrives all at once - the
        scene sits still and then jumps. `scrollY` read here is whatever the
        compositor is showing right now, on every frame it draws. */
    journey.setScroll(readScroll());
    journey.render(now / 1000, delta);
    rain?.render(now);
    framesRendered += 1;
  }

  if (loaderAlive && loader) loader.render(now / 1000, delta);

  if (entering) {
    build = Math.min(1, build + delta / BUILD_SECONDS);
    applyBuild(build);

    if (build >= 1) finishBuild();
  }

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

  const journeyRef = journey;

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
    // `maxScroll`, not body.scrollHeight - see measureScroll for why those
    // are different numbers and which one the browser actually honours.
    const max = maxScroll;
    const last = document.getElementById(SECTIONS[SECTIONS.length - 1]?.id ?? '');

    if (!last || max <= 0) return 1;

    const rect = last.getBoundingClientRect();
    const top = rect.top + window.scrollY;

    /*  ON A PHONE THE JOURNEY ENDS ABOVE THE CARD, NOT AT THE SECTION'S
        CENTRE. The download card carries the FAQ and is most of a screen
        tall, so centring that section puts the card over the middle of the
        viewport and the instrument - the thing the whole page has been
        travelling towards - ends up behind it. Ending when the card's TOP
        edge sits low instead leaves the upper two thirds of the screen
        clear, which is where the instrument resolves; the card is then read
        by scrolling the last stretch, with the journey already complete. */
    if (window.innerWidth < 860) {
      const slab = last.querySelector('.slab');
      const slabTop = slab
        ? slab.getBoundingClientRect().top + window.scrollY
        : top;

      return Math.max(
        0.1,
        Math.min(1, (slabTop - window.innerHeight * 0.66) / max),
      );
    }

    const centre = top + rect.height / 2 - window.innerHeight / 2;

    // Never 0, or the first sample divides by it.
    return Math.max(0.1, Math.min(1, centre / max));
  };

  end = journeyEnd();

  /*  ScrollTrigger no longer drives the journey - the render loop samples
      `scrollY` itself every frame (see `readScroll`). It is still what
      re-measures the end point when the layout changes, because that is a
      layout event rather than a per-frame one. */
  ScrollTrigger.create({
    trigger: document.body,
    start: 'top top',
    end: 'bottom bottom',
    onRefresh: () => {
      // Re-measured together: `end` is a fraction OF `maxScroll`, so a stale
      // one paired with a fresh one is worse than either being old.
      measureScroll();
      end = journeyEnd();
      publishEnd();
    },
  });

  measureScroll();
  end = journeyEnd();
  publishEnd();

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
    measureScroll();
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
    /*  Collected HERE, not at module scope: `.nav__mark` and the rest are
        injected by mountChrome, and the hero's lines only exist once the
        markup has been parsed. Querying for them earlier finds nothing and
        the build silently animates an empty list. */
    assembly = createAssembly();
    assembly.apply(0);

    build = 0;
    entering = true;
    root.classList.add('gn-entering');

    /*  Reduced motion gets the destination, not the journey. The CSS half
        already opts out, but the figure's strokes flying in from every side
        is motion too, and leaving that running would honour the preference
        in the DOM and ignore it in the part that actually moves. */
    if (reduced) {
      build = 1;
      finishBuild();
      return;
    }

    /*  The scroll stays LOCKED through the zoom. The page is mid-transform
        and the journey is mid-dolly; letting the wheel in here would drive
        the scroll position of a composition that is not in place yet, and
        the first thing the viewer did would fight the arrival. It is under
        a second. */
  });

  /*  Torn down when the dismissal has finished DRAWING, never on a timer.
      The fade advances per frame; a wall-clock wait for it is a guess at the
      frame rate, and at a few frames a second the 1400ms this used to wait
      disposed the renderer a third of the way through - leaving the canvas
      frozen on a half-faded loader over the page, permanently. */
  /*  The loader's own exit finishes long before the build does, so this only
      tears the loader down. The page is still assembling behind it and the
      frame loop keeps running - which is exactly the ordering asked for:
      the boot screen leaves, and the home page is visibly put together
      rather than found already made. */
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
