/**
 * Text that decodes out of static as it arrives.
 *
 * Every character starts as a random glyph and resolves to its real one, in
 * a left-to-right wave. Spaces are never scrambled — a decoding block with
 * its word boundaries intact still reads as language, and one without looks
 * like a wall of noise.
 *
 * Runs on `requestAnimationFrame` rather than a timer per character: one
 * loop for the whole string, and it stops itself when the last character
 * lands.
 *
 * THE CLOCK IS ELAPSED TIME, NOT A FRAME COUNT. The first version advanced
 * one step per animation frame, which ties how long the sentence takes to
 * read to how fast the machine happens to be drawing - and this page has a
 * WebGL scene running beside it. Measured in a screenshot on a software
 * renderer, every card on the page was still mid-static after two and a
 * half seconds; on a phone dropping frames it would be the same. A rate in
 * milliseconds says what the effect means, and looks identical at 30, 60
 * and 120 Hz.
 */

const GLYPHS = '▚▘▜▞▛▙▟▖▗▄▀■□◧◨/\\<>=+*·:.0123456789ABCDEFGHJKLMNPQRSTUVWXYZ';

/*  A LINE BREAK IS PART OF THE TEXT. Writing the result back with
    `textContent` deletes any <br> inside the element, and the headings here
    are broken by hand where the line should turn - so "FOURTEEN
    EFFECTS.<br>ANY ORDER." decoded to "FOURTEEN EFFECTS.ANY ORDER.", with
    the missing space making it look like a typo rather than a lost tag. The
    break is read out as a newline before the first pass and the headings
    are `white-space: pre-line`, so one character class covers it and the
    decoder already passes '\n' through unscrambled. */
function readTarget(element: HTMLElement): string {
  if (element.dataset.decoded !== undefined) return element.dataset.decoded;

  if (!element.querySelector('br')) return element.textContent ?? '';

  const html = element.innerHTML.replace(/<br\s*\/?>/gi, '\n');
  const scratch = document.createElement('div');
  // Through a detached element, so entities resolve the way the browser
  // rendered them rather than the way a regex guesses.
  scratch.innerHTML = html;
  return scratch.textContent ?? '';
}

export function decode(element: HTMLElement, options: { delay?: number } = {}): void {
  const target = readTarget(element);

  // Stored so a re-run (a second scroll past) decodes to the same string
  // rather than to whatever the last frame of static happened to be.
  element.dataset.decoded = target;

  const characters = [...target];

  /*  Milliseconds each character spends as static before it resolves, and
      how far apart their arrivals are - the stagger is what makes it a wave
      rather than a flicker. Capped total, so a long paragraph resolves in
      about the time it takes to notice it rather than crawling. */
  const settle = 150;
  const stagger = Math.min(24, 900 / Math.max(1, characters.length));
  const total = characters.length * stagger + settle;

  /*  The static is re-rolled on its own slower clock. Rolling it every frame
      strobes at 120 Hz on a fast display, which reads as a glitch rather
      than as a signal resolving - and costs a random per character per
      frame for the privilege. */
  const rollMs = 40;

  let origin = 0;
  let elapsed = 0;
  let roll = -1;
  let handle = 0;

  const step = (now: number) => {
    if (origin === 0) origin = now;
    elapsed = now - origin;

    const thisRoll = Math.floor(elapsed / rollMs);
    const rolled = thisRoll !== roll;
    roll = thisRoll;

    let output = '';

    for (let i = 0; i < characters.length; i += 1) {
      const character = characters[i] ?? '';
      const started = elapsed - i * stagger;

      if (character === ' ' || character === '\n' || started >= settle) {
        output += character;
      } else if (started < 0) {
        // Not arrived yet. A non-breaking space holds the line's width, so
        // the layout does not reflow as the text lands.
        output += ' ';
      } else {
        output += GLYPHS[Math.floor(Math.random() * GLYPHS.length)] ?? '·';
      }
    }

    // Only touch the DOM when something actually changed.
    if (rolled || elapsed >= total) element.textContent = output;

    if (elapsed <= total) {
      handle = requestAnimationFrame(step);
    } else {
      element.textContent = target;
      cancelAnimationFrame(handle);
    }
  };

  if (options.delay) {
    element.textContent = ' '.repeat(characters.length);
    window.setTimeout(() => { handle = requestAnimationFrame(step); }, options.delay);
  } else {
    handle = requestAnimationFrame(step);
  }
}

/** Decodes each element once, the first time it scrolls into view. */
export function decodeOnReveal(elements: Iterable<HTMLElement>): void {
  const reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  if (reduced) return;

  const observer = new IntersectionObserver(
    (entries) => {
      for (const entry of entries) {
        if (!entry.isIntersecting) continue;

        const element = entry.target as HTMLElement;
        observer.unobserve(element);

        decode(element, { delay: Number(element.dataset.decodeDelay ?? 0) });
      }
    },
    { rootMargin: '0px 0px -12% 0px' },
  );

  for (const element of elements) observer.observe(element);
}
