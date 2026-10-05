/*
 * The DRUMS sheet (docs/design/phase4-06-drums-riddimize.md), worked like
 * DrumSmith (the producer's model): each row - kick, snare, hat, open hat -
 * has a pattern from its library (< 03 >), a dice (RND: a new pattern and
 * sound for that row alone) and a LOCK that GENERATE leaves alone. The grid
 * is one bar, coloured as FL Studio's step sequencer is (beats 1 and 3 one
 * colour, 2 and 4 another, the beat numbers above), repeated four times in
 * the WAV; while it plays, the sounding step and bar light up. EXPORT writes
 * the loop, EXPORT STEMS each row on its own.
 */

import { exportWav, revealExports } from '../audio/export.ts';
import { isPreviewing, playPreview, previewPosition, stopPreview } from '../audio/preview.ts';
import { chipButton, el } from '../widgets.ts';
import { applyPattern, BARS, generateLoop, PATTERNS, randomizeRow, renderLoop, ROW_NAMES, ROWS, setStep, STEPS } from './drums.ts';
import type { DrumLoop, Row } from './drums.ts';

const SR = 44100;
const TEMPOS = [140, 145, 150] as const;

let singleton: { open(): void } | null = null;

export function drumsSheet(): { open(): void } {
  if (singleton) return singleton;
  const closeButton = el('button', 'evolve__close', '×');
  closeButton.type = 'button';
  closeButton.setAttribute('aria-label', 'Close the drums');
  const hint = el('p', 'evolve__hint',
    'Kick on 1, snare on 3, hats your way. GENERATE rolls every row; RND rolls one row, LOCK keeps it. ' +
    '< > steps through a row\'s patterns. Tap a step to change it (tap a HAT step again for a roll: 2, then 3). ' +
    'PLAY, then EXPORT a 4-bar WAV for your playlist.');
  const status = el('div', 'evolve__status drums__status');
  const generate = chipButton('GENERATE', 'violet');
  generate.classList.add('drums__generate');
  const from = el('div', 'drums__from');
  const bars = el('div', 'drums__bars');
  const grid = el('div', 'drums__grid');
  const knobs = el('div', 'drums__knobs');
  const play = chipButton('PLAY');
  const tempo = chipButton('140 BPM');
  const exportButton = chipButton('EXPORT WAV');
  const stems = chipButton('EXPORT STEMS');
  const folder = chipButton('OPEN FOLDER');
  const root = el('div', 'evolve drums',
    el('div', 'evolve__head', el('span', 'evolve__title', 'DRUMS'), closeButton), hint,
    el('div', 'drums__top', generate, tempo, play, from),
    bars, grid, knobs,
    el('div', 'evolve__actions drums__actions', exportButton, stems, folder),
    status);
  root.hidden = true;
  document.body.append(root);

  let seed = 1 + Math.floor(Math.random() * 999_999);
  let loop: DrumLoop = generateLoop(seed);
  const locked = new Set<Row>();
  let bpm: number = TEMPOS[0];
  let count = 0;

  const say = (s: string): void => {
    status.textContent = s;
  };
  const render = (only?: Row): [Float32Array, Float32Array] => renderLoop(loop, bpm, SR, seed, only);
  // An edit swaps the sound in where it is playing, not from the top.
  const restart = (): void => {
    if (isPreviewing()) playPreview(render(), SR, true, undefined, true);
  };

  // The four bars the pattern plays, the sounding one lit: where a bar ends.
  const barCells = Array.from({ length: BARS }, (_, b) => el('span', 'drums__bar', `BAR ${b + 1}`));
  bars.replaceChildren(...barCells);

  const cellsByStep: HTMLElement[][] = Array.from({ length: STEPS }, () => []);
  const drawGrid = (): void => {
    for (const list of cellsByStep) list.length = 0;
    const beats = el('div', 'drums__cells drums__beats', ...Array.from({ length: STEPS }, (_, s) => {
      const n = el('span', 'drums__beatnum', s % 4 === 0 ? String(s / 4 + 1) : '');
      n.dataset.group = String(Math.floor(s / 4) % 2);
      return n;
    }));
    grid.replaceChildren(beats, ...ROWS.map((row) => drawRow(row)));
    from.textContent = `kick from ${loop.sources.kick} / snare from ${loop.sources.snare}`;
  };

  const drawRow = (row: Row): HTMLElement => {
    const pattern = PATTERNS[row];
    const name = el('span', 'drums__pattern');
    const showName = (): void => {
      const k = loop.picks[row];
      name.textContent = k < 0 ? 'EDITED' : `${String(k + 1).padStart(2, '0')} ${pattern[k]?.name ?? ''}`;
    };
    const step = (by: number): HTMLButtonElement => {
      const b = chipButton(by < 0 ? '<' : '>');
      b.classList.add('drums__arrow');
      b.setAttribute('aria-label', `${ROW_NAMES[row]} ${by < 0 ? 'previous' : 'next'} pattern`);
      b.addEventListener('click', () => {
        applyPattern(loop, row, (loop.picks[row] < 0 ? 0 : loop.picks[row]) + by);
        redraw();
      });
      return b;
    };
    const dice = chipButton('RND');
    dice.title = `A new ${ROW_NAMES[row].toLowerCase()} pattern and sound`;
    dice.addEventListener('click', () => {
      randomizeRow(loop, row);
      redraw();
      say(`${ROW_NAMES[row]}: ${PATTERNS[row][loop.picks[row]]?.name ?? ''}, sound from ${loop.sources[row]}.`);
    });
    const lock = chipButton('LOCK');
    lock.classList.add('drums__lock');
    lock.dataset.on = locked.has(row) ? 'true' : 'false';
    lock.title = 'GENERATE leaves a locked row alone';
    lock.addEventListener('click', () => {
      if (locked.has(row)) locked.delete(row);
      else locked.add(row);
      lock.dataset.on = locked.has(row) ? 'true' : 'false';
    });
    const cells = Array.from({ length: STEPS }, (_, s) => {
      const cell = el('button', 'drums__cell');
      cell.type = 'button';
      // Beats 1 and 3 one colour, 2 and 4 another, as FL Studio's steps.
      cell.dataset.group = String(Math.floor(s / 4) % 2);
      const show = (): void => {
        const on = loop.hits[row][0]?.[s] === true;
        const roll = row === 'hat' && on ? (loop.rolls[s] ?? 1) : 1;
        cell.dataset.on = on ? 'true' : 'false';
        cell.textContent = roll > 1 ? String(roll) : '';
      };
      show();
      cell.setAttribute('aria-label', `${ROW_NAMES[row]} step ${s + 1}`);
      cell.addEventListener('click', () => {
        const on = loop.hits[row][0]?.[s] === true;
        // HAT: off, on, a roll of 2, a roll of 3, off. The others: on/off.
        if (row === 'hat' && on && (loop.rolls[s] ?? 1) < 3) {
          loop.rolls[s] = (loop.rolls[s] ?? 1) + 1;
        } else {
          setStep(loop, row, s, !on);
          if (row === 'hat') loop.rolls[s] = 1;
        }
        loop.picks[row] = -1;
        show();
        showName();
        restart();
      });
      cellsByStep[s]?.push(cell);
      return cell;
    });
    showName();
    const line = el('div', 'drums__row',
      el('div', 'drums__rowhead', el('span', 'drums__label', ROW_NAMES[row]), step(-1), name, step(1), dice, lock),
      el('div', 'drums__cells', ...cells));
    line.dataset.row = row;
    const redraw = (): void => {
      line.replaceWith(drawRow(row));
      // The row's old cells leave the playhead's list with it.
      for (const list of cellsByStep) for (let k = list.length - 1; k >= 0; k -= 1) if (!list[k]?.isConnected) list.splice(k, 1);
      from.textContent = `kick from ${loop.sources.kick} / snare from ${loop.sources.snare}`;
      drawKnobs();
      lastStep = -1;
      restart();
    };
    return line;
  };

  // The playhead: which bar and step is sounding, every frame while open.
  let lastStep = -1;
  let lastBar = -1;
  const follow = (): void => {
    if (root.hidden) return;
    requestAnimationFrame(follow);
    const t = previewPosition();
    const barLength = (4 * 60) / bpm;
    const step = t === null ? -1 : Math.floor((t / barLength) * STEPS) % STEPS;
    const b = t === null ? -1 : Math.floor(t / barLength) % BARS;
    if (step !== lastStep) {
      for (const list of cellsByStep) for (const c of list) delete c.dataset.now;
      for (const c of cellsByStep[step] ?? []) c.dataset.now = 'true';
      lastStep = step;
    }
    if (b !== lastBar) {
      barCells.forEach((c, k) => (c.dataset.on = k === b ? 'true' : 'false'));
      lastBar = b;
    }
  };

  // Four sliders over the sound: each scales what GENERATE or RND chose.
  const sliders: { label: string; get(): number; set(v: number): void; min: number; max: number }[] = [
    { label: 'KICK TUNE', min: 60, max: 400, get: () => loop.sound.kick.pitchStart, set: (v) => (loop.sound.kick.pitchStart = v) },
    { label: 'KICK LENGTH', min: 60, max: 600, get: () => loop.sound.kick.decayMs, set: (v) => (loop.sound.kick.decayMs = v) },
    { label: 'SNARE LENGTH', min: 40, max: 400, get: () => loop.sound.snare.decayMs, set: (v) => (loop.sound.snare.decayMs = v) },
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

  generate.addEventListener('click', () => {
    seed = 1 + Math.floor(Math.random() * 999_999);
    loop = generateLoop(seed, undefined, { keep: [...locked], from: loop });
    drawGrid();
    drawKnobs();
    restart();
    say(`New loop${locked.size ? ` (kept: ${[...locked].map((r) => ROW_NAMES[r]).join(', ')})` : ''}.`);
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
  // Stems, as DrumSmith drags them: each row alone, the same four bars, so
  // they line up in the playlist. Each is levelled on its own (-1 dBFS).
  stems.addEventListener('click', () => {
    count += 1;
    const n = count;
    say('Rendering stems...');
    const rows = ROWS.filter((row) => loop.hits[row].some((bar) => bar.some(Boolean)));
    void rows.reduce<Promise<string>>((done, row) => done.then(() =>
      exportWav(`GNARL Drums ${bpm} ${n} ${ROW_NAMES[row]}`, render(row), SR).then((r) => r.said)), Promise.resolve(''))
      .then((last) => say(`${rows.length} stems: ${rows.map((r) => ROW_NAMES[r]).join(', ')}. ${last}`));
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

  drawGrid();
  drawKnobs();
  singleton = {
    open(): void {
      root.hidden = false;
      requestAnimationFrame(follow);
    },
  };
  return singleton;
}
