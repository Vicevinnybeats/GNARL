/*  MOTION ON SCROLL, DRIVEN BY THE SCROLL ITSELF.
 *
 *  Every card has a reveal from 0 (below the fold) to 1 (well inside it),
 *  computed from where it sits in the viewport each frame, and written as
 *  CSS variables that styles.css turns into transforms: the card rises, its
 *  number's rule draws out, the heading wipes in, the list rows and sound
 *  rows slide in one after another, and a line at the top of the screen
 *  measures how far down the page you are.
 *
 *  A function of POSITION, not of time: there is no second clock here
 *  (CLAUDE.md section 10's five mistakes were all a second clock), it plays
 *  backwards when you scroll back up, and it cannot be caught half-way by a
 *  fast flick. The boot loop calls `updateMotion` once a frame; every read
 *  comes before every write, so a frame costs one layout at most. */

interface Card {
  el: HTMLElement;
  rows: HTMLElement[];
  last: number;
}

let cards: Card[] = [];
let bar: HTMLElement | null = null;
let lastProgress = -1;

/** Collects the cards; call again when content arrives (the sound list). */
export function mountMotion(): void {
  if (!bar) {
    bar = document.createElement('div');
    bar.className = 'progress';
    bar.setAttribute('aria-hidden', 'true');
    document.body.append(bar);
  }
  cards = [...document.querySelectorAll<HTMLElement>('main .slab')].map((el) => ({
    el,
    rows: [...el.querySelectorAll<HTMLElement>('.specs li, .sound, .price__list li, .faq details')],
    last: -1,
  }));
  for (const card of cards) {
    card.rows.forEach((row, i) => row.style.setProperty('--i', String(i)));
  }
  document.documentElement.classList.add('gn-motion');
}

const clamp = (v: number): number => (v < 0 ? 0 : v > 1 ? 1 : v);

export function updateMotion(): void {
  if (cards.length === 0) return;
  const h = window.innerHeight;

  // Reads.
  const reveals = cards.map(({ el }) => {
    const r = el.getBoundingClientRect();
    // 0 when the card's top is at the bottom edge, 1 once it is a third up.
    return clamp((h - r.top) / (h * 0.42));
  });
  const max = document.documentElement.scrollHeight - h;
  const progress = max > 0 ? clamp(window.scrollY / max) : 0;

  // Writes, only where something changed (to three places).
  cards.forEach((card, i) => {
    const v = Math.round((reveals[i] ?? 1) * 1000) / 1000;
    if (v === card.last) return;
    card.last = v;
    card.el.style.setProperty('--reveal', String(v));
  });
  const p = Math.round(progress * 1000) / 1000;
  if (bar && p !== lastProgress) {
    lastProgress = p;
    bar.style.setProperty('--progress', String(p));
  }
}
