/**
 * The header and footer, injected rather than copied into five files.
 *
 * WHY THIS IS IN JAVASCRIPT. Five hand-written copies of a nav is five
 * chances for one of them to keep a link the others dropped, and that kind
 * of drift is invisible until somebody clicks the odd one out. This project
 * already treats duplicated-by-hand structure as a defect to be checked
 * (the parameter mirror, the reference vectors); a nav is small enough that
 * the fix is simply not to duplicate it.
 *
 * The cost is honest and worth naming: with JavaScript off there is no nav.
 * That is acceptable HERE and nowhere else on the site — every page's
 * subject is a WebGL scene, so a browser that cannot run the nav could not
 * have drawn the page either. The reading matter itself stays in the HTML.
 */

export interface Page {
  readonly id: string;
  readonly href: string;
  readonly label: string;
}

/** The five pages, in the order the nav shows them. One source of truth. */
export const PAGES: readonly Page[] = [
  { id: 'home', href: './index.html', label: 'HOME' },
  { id: 'engine', href: './engine.html', label: 'ENGINE' },
  { id: 'presets', href: './presets.html', label: 'PRESETS' },
  { id: 'fx', href: './fx.html', label: 'FX' },
  { id: 'download', href: './download.html', label: 'DOWNLOAD' },
];

export function mountChrome(current: string): void {
  const header = document.querySelector('.nav');

  if (header) {
    const links = PAGES.filter((page) => page.id !== 'download')
      .map((page) => {
        // aria-current is what tells a screen reader which page this is;
        // the underline is only the sighted half of the same fact.
        const active = page.id === current ? ' aria-current="page"' : '';
        return `<a href="${page.href}"${active}>${page.label}</a>`;
      })
      .join('');

    header.innerHTML = `
      <a class="nav__mark" href="./index.html">GNARL</a>
      <nav class="nav__links">${links}</nav>
      <a class="nav__cta" href="./download.html">DOWNLOAD</a>
    `;
  }

  const foot = document.querySelector('.foot');

  if (foot) {
    foot.innerHTML = `
      <span>GNARL</span>
      <span>BUILT WITH JUCE · THE INTERFACE IS A WEB VIEW</span>
    `;
  }
}

/**
 * The next page, for the arrow at the foot of each one.
 *
 * A five-page site with no forward path is five dead ends: somebody who
 * reads to the bottom of ENGINE has to go back up to the nav to find out
 * there is anything else. The order here is the order in PAGES, so adding a
 * page puts it in the chain without touching anything.
 */
export function nextPage(current: string): Page | null {
  const index = PAGES.findIndex((page) => page.id === current);

  if (index < 0 || index >= PAGES.length - 1) return null;

  return PAGES[index + 1] ?? null;
}

export function mountNextLink(current: string): void {
  const slot = document.querySelector('.next');
  const next = nextPage(current);

  if (!slot || !next) return;

  slot.innerHTML = `
    <a class="next__link" href="${next.href}">
      <span class="next__label">NEXT</span>
      <span class="next__name">${next.label}</span>
      <span class="next__arrow" aria-hidden="true">&rarr;</span>
    </a>
  `;
}
