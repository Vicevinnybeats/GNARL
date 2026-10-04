/*
 * TIPS as a guided tour: a pointer on the control to touch, one step at a
 * time. The producer asked for "a pointer where you have to click instead of
 * a menu" - the recipes were a list of steps naming controls, which meant
 * finding each control by its name.
 *
 * Each step names its control the way the panel tags it (data-param, or a
 * button's text inside one), so the pointer finds it in either layout. A
 * control behind a phone tab or an FX page is brought forward first, by
 * clicking that tab as the producer would. Tapping a pointed-at BUTTON does
 * what it always does and moves the tour on; a knob is turned, then NEXT.
 * Nothing here sets a value itself: the tour only points.
 */

import { chipButton, el } from './widgets.ts';

/** Where a step points. `param`: a control's data-param; `text`: a button with
 * that label (inside `param`'s control, or `css`'s element when given);
 * `css`: any element, looked for in the whole page when `anywhere`. */
interface Target {
  param?: string;
  text?: string;
  css?: string;
  anywhere?: boolean;
  /** Not on every layout (the phone's keys): no pointer, and no complaint. */
  optional?: boolean;
}
interface Step {
  say: string;
  at?: Target;
}
interface Recipe {
  title: string;
  about: string;
  steps: Step[];
}

export const RECIPES: Recipe[] = [
  {
    title: 'BASIC WOB',
    about: 'The riddim wob: the sound chopped open and shut in time.',
    steps: [
      { say: 'Start here: tap the preset name and pick INIT for a clean start - or keep the sound you have.', at: { css: '.preset__name' } },
      { say: 'OSC 1: pick a table with the arrows - GROWL or WUB are good wobs.', at: { css: '.table-pick' } },
      { say: 'Switch the WOBBLE LFO on with this dot.', at: { param: 'wobble.on' } },
      { say: 'RATE: tap 1/8T for the triplet riddim wob (1/8 straight, 1/4 slow).', at: { param: 'wobble.rate', text: '1/8T' } },
      { say: 'SHAPE: SOFT SQR chops hard. SINE wubs smoothly, DRAW lets you draw it.', at: { param: 'wobble.shape', text: 'SOFT SQR' } },
      { say: 'Tap VOLUME: the wobble now opens and shuts the sound. Tap again to undo.', at: { css: '.wobble', text: 'VOLUME' } },
      { say: 'Now play a note - D#3 in FL, or hold a key here - to hear your wob.', at: { css: '.keys', optional: true } },
    ],
  },
  {
    title: 'MOVING TONE',
    about: 'The wob changes colour as it moves, not just volume.',
    steps: [
      { say: 'Make the BASIC WOB first. Then tap WT POS: the wobble moves through the wavetable.', at: { param: 'wobble.to.wtpos' } },
      { say: 'Tap CUTOFF too: the wobble opens the filter on each hit.', at: { param: 'wobble.to.cutoff' } },
      { say: 'DEPTH: turn it to 60-80%. Higher moves the tone further.', at: { param: 'wobble.depth' } },
      { say: 'OSC 1 WT POS: where in the table the movement starts.', at: { param: 'osc1.wtpos' } },
      { say: 'Switch the VOWEL FILTER on: the wobble\'s CUTOFF needs a filter to open.', at: { param: 'vowel.on' } },
      { say: 'CUTOFF near the middle. (With a vowel picked CUTOFF does nothing - see TALKING WOB.)', at: { param: 'vowel.cutoff' } },
      { say: 'SMOOTH softens the edges of each wob.', at: { param: 'wobble.smooth' } },
      { say: 'PHASE moves where in the beat each wob opens.', at: { param: 'wobble.phase' } },
    ],
  },
  {
    title: 'TALKING WOB',
    about: 'The "yoi" / "wow" wob: a vowel that changes on every hit.',
    steps: [
      { say: 'Switch the VOWEL FILTER on.', at: { param: 'vowel.on' } },
      { say: 'Pick a vowel: O or U for a dark "wow", A or E for a bright "yoi".', at: { param: 'vowel.vowel', text: 'O' } },
      { say: 'RES 50-70% makes it speak more.', at: { param: 'vowel.res' } },
      { say: 'Tap VOWEL here: the wobble now moves through the vowels.', at: { param: 'wobble.to.vowel' } },
      { say: 'Make sure the WOBBLE LFO is on.', at: { param: 'wobble.on' } },
      { say: 'Optional: OSC 1 on the VOWEL or YOI table talks even more.', at: { css: '.table-pick' } },
    ],
  },
  {
    title: 'GROWL',
    about: 'A dirtier, metallic growl from FM.',
    steps: [
      { say: 'OSC 1 FM: turn it up to 30-50%.', at: { param: 'osc1.fm' } },
      { say: 'Try the modes: FORMANT, SYNC, BEND, FOLD - each growls differently.', at: { param: 'osc1.mode' } },
      { say: 'WARP sets how hard that mode works.', at: { param: 'osc1.warp' } },
      { say: 'Tap FM here so the growl moves with the wob.', at: { param: 'wobble.to.fm' } },
      { say: 'UNISON 2-5 makes it wider...', at: { param: 'osc1.unison' } },
      { say: '...with a little DETUNE.', at: { param: 'osc1.detune' } },
    ],
  },
  {
    title: 'MAKE IT HEAVY',
    about: 'Distortion and OTT: where riddim gets its weight.',
    steps: [
      { say: 'Switch DIST on.', at: { param: 'dist.on' } },
      { say: 'Tap the mode until it says TUBE.', at: { param: 'dist.mode' } },
      { say: 'DRIVE 50-70%.', at: { param: 'dist.drive' } },
      { say: 'FOLD on, AMOUNT 20-40% adds grit; too much turns to noise.', at: { param: 'fold.amount' } },
      { say: 'CRUSH for a lo-fi edge: BITS around 8.', at: { param: 'crush.bits' } },
      { say: 'OTT on: the classic squashed, loud riddim sound.', at: { param: 'ott.on' } },
      { say: 'OTT DEPTH 40-60%.', at: { param: 'ott.depth' } },
      { say: 'Too loud or clipping? Turn MASTER down, not the drive.', at: { param: 'master' } },
    ],
  },
  {
    title: 'SUB + SPACE',
    about: 'A clean sub under the wob, and some room around it.',
    steps: [
      { say: 'Switch the SUB on. It stays clean while the wob above it moves.', at: { param: 'sub.on' } },
      { say: 'LEVEL 60-70%.', at: { param: 'sub.level' } },
      { say: 'MONO on keeps the low end in the middle.', at: { param: 'sub.mono' } },
      { say: '-1 OCT for deeper.', at: { param: 'sub.oct' } },
      { say: 'DELAY LINE on.', at: { param: 'delay.on' } },
      { say: 'Pick a step length that fits the rhythm: 1/8T for a triplet wob.', at: { param: 'delay.length' } },
      { say: 'Delay MIX 15-20%.', at: { param: 'delay.mix' } },
      { say: 'REVERB on, MIX 15-25%. Keep space light: too much fills the gaps between hits.', at: { param: 'reverb.mix' } },
    ],
  },
  {
    title: 'LET THE AI DO IT',
    about: 'Four wobs at a time, steered by your ears.',
    steps: [
      { say: 'Tap AI: four new wobs. Tap each to hear it.', at: { css: '.ai' } },
      { say: 'PICK the best: the next four grow from it.', at: { css: '.evolve:not([hidden]) .evolve__pick', anywhere: true } },
      { say: 'FROM MY SOUND: four new ones grown from the sound you have loaded.', at: { css: '.evolve:not([hidden]) .evolve__matchrow', text: 'FROM MY SOUND', anywhere: true } },
      { say: 'MATCH A SOUND: give it a 1-2 s file of one wob; it finds GNARL sounds close to it.', at: { css: '.evolve:not([hidden]) .evolve__match', anywhere: true } },
      { say: 'Like one? Close this, tap the preset name, type a name, SAVE.', at: { css: '.preset__name' } },
    ],
  },
];

const visible = (e: Element): boolean => {
  const r = e.getBoundingClientRect();
  return r.width > 0 && r.height > 0;
};

/** The layout on screen: both are built, one is shown (main.ts). */
function layout(): Element {
  const phone = document.body.dataset.mode === 'phone';
  return document.querySelector(phone ? '.m' : '.app') ?? document.body;
}

/** The element a step points at, or null when it is not on the page. */
export function resolve(at: Target): HTMLElement | null {
  const scope: ParentNode = at.anywhere ? document : layout();
  const css = at.css ?? (at.param ? `[data-param="${at.param}"]` : null);
  if (!css) return null;
  for (const host of scope.querySelectorAll<HTMLElement>(css)) {
    if (!at.text) return host;
    const hit = [...host.querySelectorAll<HTMLElement>('button')].find((b) => b.textContent?.trim() === at.text);
    if (hit) return hit;
  }
  return null;
}

/**
 * Bring a control forward: open the phone tab or the FX page it is behind,
 * by clicking that tab's button as a person would.
 */
function reveal(target: HTMLElement): void {
  const pages: [string, string, string][] = [
    ['.m__page', '.m__pages', '.m__tabs'],
    ['.fxrack__page', '.fxrack', '.fxrack__nav'],
  ];
  for (const [pageCss, holderCss, navCss] of pages) {
    const page = target.closest<HTMLElement>(pageCss);
    if (!page || !page.hidden) continue;
    const holder = page.closest(holderCss);
    const index = holder ? [...holder.children].indexOf(page) : -1;
    const nav = holder?.parentElement?.querySelector(`:scope > ${navCss}`) ??
      holder?.parentElement?.parentElement?.querySelector(navCss);
    const button = nav?.querySelectorAll<HTMLElement>('button')[index];
    button?.click();
  }
  target.scrollIntoView({ block: 'center', inline: 'nearest' });
}

let tourSingleton: { open(): void } | null = null;

export function tour(): { open(): void } {
  if (tourSingleton) return tourSingleton;
  const ring = el('div', 'tour__ring');
  const hand = el('div', 'tour__hand');
  const text = el('p', 'tour__say');
  const count = el('span', 'tour__count');
  const title = el('span', 'tour__title');
  const back = chipButton('BACK');
  const next = chipButton('NEXT', 'violet');
  const end = el('button', 'evolve__close', '×');
  end.type = 'button';
  end.setAttribute('aria-label', 'End the tips');
  const choices = el('div', 'chips tour__choices');
  const actions = el('div', 'tour__actions', back, next);
  const bubble = el('div', 'tour__bubble', el('div', 'tour__head', title, count, end), text, choices, actions);
  const root = el('div', 'tour', ring, hand, bubble);
  root.hidden = true;
  document.body.append(root);

  let recipe: Recipe | null = null;
  let at = 0;
  let target: HTMLElement | null = null;
  let frame = 0;

  // Tapping the pointed-at button does its job, then the tour moves on.
  const onTargetClick = (): void => {
    window.setTimeout(() => go(at + 1), 350);
  };
  const aim = (t: HTMLElement | null): void => {
    target?.removeEventListener('click', onTargetClick);
    target = t;
    if (t && t.tagName === 'BUTTON') t.addEventListener('click', onTargetClick);
  };

  // Follow the target as the page scrolls, resizes or re-lays out.
  const place = (): void => {
    frame = requestAnimationFrame(place);
    const t = target && target.isConnected && visible(target) ? target : null;
    ring.hidden = hand.hidden = !t;
    const vw = window.innerWidth;
    const vh = window.innerHeight;
    const bw = bubble.offsetWidth;
    const bh = bubble.offsetHeight;
    if (!t) {
      bubble.style.left = `${Math.max(8, (vw - bw) / 2)}px`;
      bubble.style.top = `${Math.max(8, (vh - bh) / 2)}px`;
      return;
    }
    const r = t.getBoundingClientRect();
    const pad = 6;
    Object.assign(ring.style, {
      left: `${r.left - pad}px`, top: `${r.top - pad}px`,
      width: `${r.width + pad * 2}px`, height: `${r.height + pad * 2}px`,
    });
    // The bubble below the control if it fits, else above; the hand between.
    const below = r.bottom + 44 + bh < vh;
    const top = below ? r.bottom + 40 : r.top - 40 - bh;
    const left = Math.min(Math.max(8, r.left + r.width / 2 - bw / 2), vw - bw - 8);
    bubble.style.left = `${left}px`;
    bubble.style.top = `${Math.max(8, Math.min(top, vh - bh - 8))}px`;
    hand.dataset.dir = below ? 'up' : 'down';
    hand.style.left = `${r.left + r.width / 2 - 18}px`;
    hand.style.top = below ? `${r.bottom + 6}px` : `${r.top - 38}px`;
  };

  const pick = (): void => {
    recipe = null;
    aim(null);
    title.textContent = 'TIPS';
    count.textContent = '';
    text.textContent = 'What do you want to make? The pointer shows you where to tap, one step at a time.';
    choices.hidden = false;
    actions.hidden = true;
  };
  const go = (i: number): void => {
    if (!recipe) return;
    if (i >= recipe.steps.length) {
      const finished = recipe.title;
      pick();
      text.textContent = `${finished}: done! Hold a key to hear it, pick another, or close.`;
      return;
    }
    at = Math.max(0, i);
    const step = recipe.steps[at];
    if (!step) return;
    title.textContent = recipe.title;
    count.textContent = `${at + 1}/${recipe.steps.length}`;
    choices.hidden = true;
    actions.hidden = false;
    back.disabled = at === 0;
    next.textContent = at === recipe.steps.length - 1 ? 'DONE' : 'NEXT';
    const t = step.at ? resolve(step.at) : null;
    if (t) reveal(t);
    text.textContent = t || !step.at || step.at.optional ? step.say : `${step.say} (Not on screen right now.)`;
    bubble.dataset.optional = step.at?.optional && !t ? 'true' : 'false';
    aim(t);
  };
  RECIPES.forEach((r) => {
    const chip = chipButton(r.title, 'violet');
    chip.title = r.about;
    chip.addEventListener('click', () => {
      recipe = r;
      go(0);
    });
    choices.append(chip);
  });
  back.addEventListener('click', () => go(at - 1));
  next.addEventListener('click', () => go(at + 1));
  const close = (): void => {
    aim(null);
    cancelAnimationFrame(frame);
    root.hidden = true;
  };
  end.addEventListener('click', close);
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && !root.hidden) close();
  });

  tourSingleton = {
    open(): void {
      pick();
      root.hidden = false;
      cancelAnimationFrame(frame);
      place();
    },
  };
  return tourSingleton;
}
