/**
 * The header and footer, injected rather than written into the page.
 *
 * ONE PAGE AGAIN. It was briefly five, and the forward-link chain that
 * needed is gone with them: a one-pager's "next" is the scroll, and a button
 * saying so is a button that does what the wheel already did. What the nav
 * marks now is which SECTION you are in, which the scroll can tell it.
 */

export interface Section {
  readonly id: string;
  readonly label: string;
}

/** The sections, in the order the page passes through them. */
export const SECTIONS: readonly Section[] = [
  { id: 'engine', label: 'ENGINE' },
  { id: 'presets', label: 'PRESETS' },
  { id: 'fx', label: 'FX' },
  { id: 'download', label: 'DOWNLOAD' },
];

export function mountChrome(): void {
  const header = document.querySelector('.nav');

  if (header) {
    const links = SECTIONS.filter((section) => section.id !== 'download')
      .map((section) => `<a href="#${section.id}">${section.label}</a>`)
      .join('');

    header.innerHTML = `
      <a class="nav__mark" href="#top">GNARL</a>
      <nav class="nav__links">${links}</nav>
      <a class="nav__cta" href="#download">DOWNLOAD</a>
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
 * Marks the section the reader is actually in.
 *
 * An IntersectionObserver rather than a scroll handler doing the arithmetic:
 * the browser already knows what is on screen, and asking it costs nothing
 * where recomputing four bounding boxes on every scroll event costs a layout
 * flush per frame.
 */
export function trackSections(): void {
  const links = new Map<string, Element>();

  for (const section of SECTIONS) {
    const link = document.querySelector(`.nav__links a[href="#${section.id}"]`);
    if (link) links.set(section.id, link);
  }

  if (links.size === 0) return;

  const observer = new IntersectionObserver(
    (entries) => {
      for (const entry of entries) {
        const link = links.get(entry.target.id);
        if (!link) continue;

        // aria-current is what tells a screen reader where it is; the
        // underline is only the sighted half of the same fact.
        if (entry.isIntersecting) link.setAttribute('aria-current', 'true');
        else link.removeAttribute('aria-current');
      }
    },
    // The middle band of the viewport: a section counts as "here" when it is
    // where the eye is, not when one pixel of it has appeared at the bottom.
    { rootMargin: '-40% 0px -40% 0px' },
  );

  for (const section of SECTIONS) {
    const element = document.getElementById(section.id);
    if (element) observer.observe(element);
  }
}
