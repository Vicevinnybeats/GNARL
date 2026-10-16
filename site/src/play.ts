/*  PLAY IT NOW: the real GNARL over the page. /app is the phone version -
 *  the plugin's panel with the engine compiled to WebAssembly
 *  (docs/design/phase2-09-mobile.md), put there by tools/assemble_deploy.mjs
 *  from the latest release - and on a desktop it opens here in a frame, so
 *  the visitor plays it without leaving the page.
 *
 *  On a phone the link is followed instead: the instrument wants the whole
 *  screen, and the phone's own back button is the way out of it. The frame
 *  is made when the dialog opens and removed when it closes: the page inside
 *  owns its audio, and removing the frame is what stops a held note at once.
 *  Nothing of the engine loads until PLAY is pressed. */

const APP = '/app/';

export function mountPlay(): void {
  const triggers = document.querySelectorAll<HTMLAnchorElement>('[data-play]');
  if (triggers.length === 0) return;

  let dialog: HTMLDialogElement | null = null;

  const close = (): void => {
    if (!dialog) return;
    // The frame goes with the dialog: its engine and its sound stop with it.
    dialog.querySelector('iframe')?.remove();
    dialog.close();
    document.documentElement.classList.remove('gn-playing');
  };

  const open = (): void => {
    if (!dialog) {
      dialog = document.createElement('dialog');
      dialog.className = 'play';
      dialog.setAttribute('aria-label', 'GNARL');
      dialog.innerHTML = `
        <div class="play__bar">
          <span class="play__title">GNARL &middot; THE REAL ENGINE, IN YOUR BROWSER</span>
          <a class="play__full" href="${APP}" target="_blank" rel="noopener">FULL SCREEN</a>
          <button class="play__close" type="button" aria-label="Close">&times;</button>
        </div>`;
      dialog.querySelector('.play__close')?.addEventListener('click', close);
      dialog.addEventListener('cancel', (e) => {
        e.preventDefault();
        close();
      });
      // A click on the backdrop (the dialog itself, outside its box) closes it.
      dialog.addEventListener('click', (e) => {
        if (e.target === dialog) close();
      });
      document.body.append(dialog);
    }
    if (!dialog.querySelector('iframe')) {
      const frame = document.createElement('iframe');
      frame.className = 'play__frame';
      frame.title = 'GNARL';
      frame.src = APP;
      frame.allow = 'autoplay; fullscreen';
      dialog.append(frame);
    }
    dialog.showModal();
    document.documentElement.classList.add('gn-playing');
  };

  const wantsFrame = (): boolean =>
    window.innerWidth >= 860 && window.matchMedia('(pointer: fine)').matches && typeof HTMLDialogElement === 'function';

  for (const trigger of triggers) {
    trigger.addEventListener('click', (e) => {
      if (!wantsFrame() || e.metaKey || e.ctrlKey || e.shiftKey) return;
      e.preventDefault();
      open();
    });
  }
}
