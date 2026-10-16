/*  SOUNDS: the clips in public/sounds/, as sounds.json lists them
 *  (tools/render_demos.mjs writes both). One <audio> for the whole list, so
 *  starting a clip stops the last: two wobs at once is noise, not a demo.
 *
 *  Nothing loads until a clip is pressed (preload="none"), so the section
 *  costs the page a few hundred bytes of JSON and nothing else. */

interface Sound {
  title: string;
  file: string;
  note?: string;
}

const BASE = './sounds/';

export async function mountSounds(): Promise<void> {
  const list = document.querySelector<HTMLOListElement>('.sounds');
  if (!list) return;

  let sounds: Sound[] = [];
  try {
    const response = await fetch(`${BASE}sounds.json`);
    if (!response.ok) throw new Error(String(response.status));
    sounds = ((await response.json()) as { sounds?: Sound[] }).sounds ?? [];
  } catch {
    list.hidden = true;
    return;
  }
  if (sounds.length === 0) {
    list.hidden = true;
    return;
  }

  const audio = new Audio();
  audio.preload = 'none';
  let current: HTMLLIElement | null = null;

  const setState = (item: HTMLLIElement | null, playing: boolean): void => {
    for (const li of list.querySelectorAll<HTMLLIElement>('li')) {
      const on = li === item && playing;
      li.dataset.playing = String(on);
      li.querySelector('button')?.setAttribute('aria-pressed', String(on));
    }
  };

  for (const sound of sounds) {
    const li = document.createElement('li');
    li.className = 'sound';
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'sound__play';
    button.setAttribute('aria-pressed', 'false');
    button.setAttribute('aria-label', `Play ${sound.title}`);
    const title = document.createElement('span');
    title.className = 'sound__title';
    title.textContent = sound.title;
    const note = document.createElement('span');
    note.className = 'sound__note';
    note.textContent = sound.note ?? '';
    const bar = document.createElement('span');
    bar.className = 'sound__bar';
    bar.append(document.createElement('i'));
    button.append(title, note, bar);
    li.append(button);
    list.append(li);

    button.addEventListener('click', () => {
      if (current === li && !audio.paused) {
        audio.pause();
        return;
      }
      if (current !== li) {
        current = li;
        audio.src = BASE + sound.file;
      }
      void audio.play().catch(() => setState(li, false));
    });
  }

  audio.addEventListener('play', () => setState(current, true));
  audio.addEventListener('pause', () => setState(current, false));
  audio.addEventListener('ended', () => {
    setState(current, false);
    audio.currentTime = 0;
  });
  // Progress as a CSS variable on the playing row: the bar is drawn by CSS,
  // so a timeupdate (a few a second) costs one style write.
  audio.addEventListener('timeupdate', () => {
    if (!current || !audio.duration) return;
    current.style.setProperty('--progress', String(audio.currentTime / audio.duration));
  });
}
