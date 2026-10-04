/*
 * The DRUMS sheet (docs/design/phase4-06-drums-riddimize.md): GENERATE a
 * riddim loop from the producer's measured references, edit it on a 16-step
 * grid bar by bar, shape the four sounds, hear it, EXPORT it as a 4-bar WAV
 * for the DAW's playlist.
 */

import { exportWav, revealExports } from '../audio/export.ts';
import { isPreviewing, playPreview, stopPreview } from '../audio/preview.ts';
import { chipButton, el } from '../widgets.ts';
import { BARS, generateLoop, renderLoop, ROW_NAMES, ROWS, STEPS } from './drums.ts';
import type { DrumLoop } from './drums.ts';

const SR = 44100;
const TEMPOS = [140, 145, 150] as const;

let singleton: { open(): void } | null = null;

export function drumsSheet(): { open(): void } {
  if (singleton) return singleton;
  const title = el('span', 'evolve__title', 'DRUMS');
  const closeButton = el('button', 'evolve__close', '×');
  closeButton.type = 'button';
  closeButton.setAttribute('aria-label', 'Close the drums');
  const hint = el('p', 'evolve__hint',
    'Riddim drum loops built from the tracks you sent: GENERATE, tap the grid to change a hit, PLAY, then EXPORT a 4-bar WAV for your playlist.');
  const from = el('div', 'drums__from');
  const bars = el('div', 'chips drums__bars');
  const grid = el('div', 'drums__grid');
  const knobs = el('div', 'drums__knobs');
  const status = el('div', 'evolve__status drums__status');
  const generate = chipButton('GENERATE', 'violet');
  generate.classList.add('drums__generate');
  const play = chipButton('PLAY');
  const tempo = chipButton('140 BPM');
  const exportButton = chipButton('EXPORT WAV');
  const folder = chipButton('OPEN FOLDER');
  const root = el('div', 'evolve drums',
    el('div', 'evolve__head', title, closeButton), hint,
    el('div', 'drums__top', generate, from),
    bars, grid, knobs,
    el('div', 'evolve__actions drums__actions', tempo, play, exportButton, folder),
    status);
  root.hidden = true;
  document.body.append(root);

  let seed = 1 + Math.floor(Math.random() * 999_999);
  let loop: DrumLoop = generateLoop(seed);
  let bar = 0;
  let bpm: number = TEMPOS[0];
  let count = 0;

  const say = (s: string): void => {
    status.textContent = s;
  };
  const render = (): [Float32Array, Float32Array] => renderLoop(loop, bpm, SR, seed);
  const restart = (): void => {
    if (!isPreviewing()) return;
    playPreview(render(), SR, true);
  };

  const drawBars = (): void => {
    bars.replaceChildren(...Array.from({ length: BARS }, (_, b) => {
      const c = chipButton(`BAR ${b + 1}`);
      c.dataset.on = b === bar ? 'true' : 'false';
      c.addEventListener('click', () => {
        bar = b;
        drawBars();
        drawGrid();
      });
      return c;
    }));
  };
  const drawGrid = (): void => {
    grid.replaceChildren(...ROWS.map((row) => {
      const cells = Array.from({ length: STEPS }, (_, s) => {
        const cell = el('button', 'drums__cell');
        cell.type = 'button';
        cell.dataset.on = loop.hits[row][bar]?.[s] ? 'true' : 'false';
        cell.dataset.beat = s % 4 === 0 ? 'true' : 'false';
        cell.setAttribute('aria-label', `${ROW_NAMES[row]} step ${s + 1}`);
        cell.addEventListener('click', () => {
          const line = loop.hits[row][bar];
          if (!line) return;
          line[s] = !line[s];
          cell.dataset.on = line[s] ? 'true' : 'false';
          restart();
        });
        return cell;
      });
      return el('div', 'drums__row', el('span', 'drums__label', ROW_NAMES[row]), el('div', 'drums__cells', ...cells));
    }));
  };

  // Four sliders over the generated sound: each scales what GENERATE chose.
  const sliders: { label: string; get(): number; set(v: number): void; min: number; max: number }[] = [
    { label: 'KICK TUNE', min: 70, max: 240, get: () => loop.sound.kick.pitchStart, set: (v) => (loop.sound.kick.pitchStart = v) },
    { label: 'KICK LENGTH', min: 80, max: 400, get: () => loop.sound.kick.decayMs, set: (v) => (loop.sound.kick.decayMs = v) },
    { label: 'SNARE LENGTH', min: 60, max: 300, get: () => loop.sound.snare.decayMs, set: (v) => (loop.sound.snare.decayMs = v) },
    { label: 'HAT LENGTH', min: 20, max: 160, get: () => loop.sound.hat.decayMs, set: (v) => (loop.sound.hat.decayMs = v) },
  ];
  const drawKnobs = (): void => {
    knobs.replaceChildren(...sliders.map((s) => {
      const input = el('input', 'drums__slider');
      input.type = 'range';
      input.min = String(s.min);
      input.max = String(s.max);
      input.step = '1';
      input.value = String(Math.round(s.get()));
      input.setAttribute('aria-label', s.label);
      input.addEventListener('input', () => s.set(Number(input.value)));
      input.addEventListener('change', restart);
      return el('label', 'drums__knob', el('span', 'drums__label', s.label), input);
    }));
  };
  const drawAll = (): void => {
    from.textContent = `from ${loop.name}`;
    drawBars();
    drawGrid();
    drawKnobs();
  };

  generate.addEventListener('click', () => {
    seed = 1 + Math.floor(Math.random() * 999_999);
    loop = generateLoop(seed);
    bar = 0;
    drawAll();
    restart();
    say(`New loop from ${loop.name}.`);
  });
  play.addEventListener('click', () => {
    if (isPreviewing()) {
      stopPreview();
      play.textContent = 'PLAY';
      return;
    }
    playPreview(render(), SR, true, () => (play.textContent = 'PLAY'));
    play.textContent = 'STOP';
  });
  tempo.addEventListener('click', () => {
    bpm = TEMPOS[(TEMPOS.indexOf(bpm as (typeof TEMPOS)[number]) + 1) % TEMPOS.length] ?? TEMPOS[0];
    tempo.textContent = `${bpm} BPM`;
    restart();
  });
  exportButton.addEventListener('click', () => {
    count += 1;
    say('Rendering...');
    void exportWav(`GNARL Drums ${bpm} ${count}`, render(), SR).then((r) => say(r.said));
  });
  // The folder is the plugin's; a browser downloads instead.
  folder.hidden = !(window.__JUCE__?.backend && window.__JUCE__.initialisationData.gnarlWeb !== true);
  folder.addEventListener('click', () => {
    if (!revealExports()) say('Exported loops are in your downloads.');
  });
  const close = (): void => {
    stopPreview();
    play.textContent = 'PLAY';
    root.hidden = true;
  };
  closeButton.addEventListener('click', close);
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && !root.hidden) close();
  });

  drawAll();
  singleton = {
    open(): void {
      root.hidden = false;
    },
  };
  return singleton;
}
