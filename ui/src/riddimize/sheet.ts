/*
 * The RIDDIMIZE sheet (docs/design/phase4-06-drums-riddimize.md): a sound in
 * (a file, or GNARL's own loaded sound), a riddim one-shot out, as a WAV.
 */

import { exportWav, revealExports } from '../audio/export.ts';
import { isPreviewing, playPreview, stopPreview } from '../audio/preview.ts';
import { decodeAudio, renderPatch } from '../match/match.ts';
import { engine } from '../engine.ts';
import { hasEngineData } from '../data.ts';
import { currentPatch } from '../web/presets.ts';
import { chipButton, el } from '../widgets.ts';
import { DEFAULTS, FILTERS, LENGTHS, randomSettings, RHYTHMS, riddimize } from './riddimize.ts';
import type { Settings } from './riddimize.ts';

const SR = 44100;
const TEMPOS = [140, 145, 150] as const;
// The producer's note (D#3 in FL Studio), held long enough for two bars.
const MY_SOUND_MIDI = 39;
const MY_SOUND_SECONDS = 3.5;

let singleton: { open(): void } | null = null;

export function riddimizeSheet(): { open(): void } {
  if (singleton) return singleton;
  const closeButton = el('button', 'evolve__close', '×');
  closeButton.type = 'button';
  closeButton.setAttribute('aria-label', 'Close RIDDIMIZE');
  const hint = el('p', 'evolve__hint',
    'Put any sound in - a Serum growl, a bounce, your GNARL sound - and get a riddim one-shot out. GENERATE tries settings; EXPORT saves a WAV for your playlist.');
  const file = el('input', 'evolve__file');
  file.type = 'file';
  file.accept = 'audio/*,.wav,.mp3,.flac,.ogg,.aif,.aiff';
  file.hidden = true;
  const choose = chipButton('CHOOSE SAMPLE', 'violet');
  const mine = chipButton('USE MY SOUND');
  mine.hidden = !hasEngineData();
  const source = el('div', 'drums__from', 'No sound yet: choose a sample or drop one here.');
  const controls = el('div', 'riddim__controls');
  const status = el('div', 'evolve__status drums__status');
  const generate = chipButton('GENERATE', 'violet');
  const play = chipButton('PLAY');
  const tempo = chipButton('140 BPM');
  const exportButton = chipButton('EXPORT WAV');
  const folder = chipButton('OPEN FOLDER');
  folder.hidden = !(window.__JUCE__?.backend && window.__JUCE__.initialisationData.gnarlWeb !== true);
  const root = el('div', 'evolve drums riddim',
    el('div', 'evolve__head', el('span', 'evolve__title', 'RIDDIMIZE'), closeButton), hint,
    el('div', 'drums__top', choose, mine, file), source,
    controls,
    el('div', 'evolve__actions drums__actions', generate, tempo, play, exportButton, folder),
    status);
  root.hidden = true;
  document.body.append(root);

  let input: Float32Array | null = null;
  let inputName = 'Sound';
  let settings: Settings = { ...DEFAULTS };
  let out: [Float32Array, Float32Array] | null = null;
  let count = 0;
  const say = (s: string): void => {
    status.textContent = s;
  };

  const rerender = (): void => {
    if (!input) return;
    const r = riddimize(input, SR, settings);
    out = r.channels;
    if (isPreviewing()) playPreview(out, SR, true);
  };

  const chips = <T extends string>(label: string, options: readonly T[], get: () => T, set: (v: T) => void): HTMLElement => {
    const row = el('div', 'chips riddim__chips');
    const draw = (): void => {
      row.replaceChildren(el('span', 'drums__label', label), ...options.map((o) => {
        const c = chipButton(o);
        c.dataset.on = o === get() ? 'true' : 'false';
        c.addEventListener('click', () => {
          set(o);
          draw();
          rerender();
        });
        return c;
      }));
    };
    draw();
    return row;
  };
  const slider = (label: string, min: number, max: number, step: number, get: () => number, set: (v: number) => void): HTMLElement => {
    const input = el('input', 'drums__slider');
    input.type = 'range';
    input.min = String(min);
    input.max = String(max);
    input.step = String(step);
    input.value = String(get());
    input.setAttribute('aria-label', label);
    input.addEventListener('input', () => set(Number(input.value)));
    input.addEventListener('change', rerender);
    return el('label', 'drums__knob', el('span', 'drums__label', label), input);
  };
  const drawControls = (): void => {
    const s = settings;
    controls.replaceChildren(
      chips('RHYTHM', RHYTHMS, () => s.rhythm, (v) => (s.rhythm = v)),
      chips('LENGTH', LENGTHS, () => s.length, (v) => (s.length = v)),
      chips('FILTER', FILTERS, () => s.filter, (v) => (s.filter = v)),
      el('div', 'drums__knobs',
        slider('CHOP', 0, 1, 0.01, () => s.chop, (v) => (s.chop = v)),
        slider('GATE', 0.3, 0.9, 0.01, () => s.gate, (v) => (s.gate = v)),
        slider('SWEEP', 0, 1, 0.01, () => s.sweep, (v) => (s.sweep = v)),
        slider('PITCH', -12, 12, 1, () => s.pitch, (v) => (s.pitch = v)),
        slider('DRIVE', 0, 1, 0.01, () => s.drive, (v) => (s.drive = v)),
        slider('FOLD', 0, 1, 0.01, () => s.fold, (v) => (s.fold = v)),
        slider('CRUSH', 0, 1, 0.01, () => s.crush, (v) => (s.crush = v)),
        slider('OTT', 0, 1, 0.01, () => s.ott, (v) => (s.ott = v)),
        slider('SUB', 0, 1, 0.01, () => s.sub, (v) => (s.sub = v))),
    );
  };

  const take = (audio: Float32Array, name: string): void => {
    input = audio;
    inputName = name.replace(/\.[a-z0-9]+$/i, '');
    source.textContent = `${inputName}: ${(audio.length / SR).toFixed(1)} s`;
    rerender();
    say('Ready: PLAY to hear it, GENERATE for other settings.');
  };
  const load = async (f: File): Promise<void> => {
    try {
      say(`Reading ${f.name}...`);
      take(await decodeAudio(await f.arrayBuffer()), f.name);
    } catch (error) {
      say(`Could not read that file: ${(error as Error).message}`);
    }
  };
  choose.addEventListener('click', () => file.click());
  file.addEventListener('change', () => {
    const f = file.files?.[0];
    file.value = '';
    if (f) void load(f);
  });
  root.addEventListener('dragover', (e) => e.preventDefault());
  root.addEventListener('drop', (e) => {
    e.preventDefault();
    const f = e.dataTransfer?.files?.[0];
    if (f) void load(f);
  });
  mine.addEventListener('click', () => {
    say('Rendering your sound...');
    // Under its own name: reading a patch also names it (presets.ts).
    const name = engine.preset || 'My sound';
    void currentPatch(name)
      .then((patch) => renderPatch(patch, MY_SOUND_MIDI, MY_SOUND_SECONDS))
      .then((audio) => {
        if (!audio) throw new Error('the engine could not play it');
        take(audio, name);
      })
      .catch((error: Error) => say(`Could not use your sound: ${error.message}`));
  });
  generate.addEventListener('click', () => {
    settings = randomSettings(1 + Math.floor(Math.random() * 999_999), settings.bpm);
    drawControls();
    rerender();
    say(input ? `${settings.rhythm}, ${settings.length}, ${settings.filter}.` : 'Choose a sample first.');
  });
  play.addEventListener('click', () => {
    if (isPreviewing()) {
      stopPreview();
      play.textContent = 'PLAY';
      return;
    }
    if (!out) {
      say('Choose a sample first.');
      return;
    }
    playPreview(out, SR, true, () => (play.textContent = 'PLAY'));
    play.textContent = 'STOP';
  });
  tempo.addEventListener('click', () => {
    settings.bpm = TEMPOS[(TEMPOS.indexOf(settings.bpm as (typeof TEMPOS)[number]) + 1) % TEMPOS.length] ?? TEMPOS[0];
    tempo.textContent = `${settings.bpm} BPM`;
    rerender();
  });
  exportButton.addEventListener('click', () => {
    if (!out) {
      say('Choose a sample first.');
      return;
    }
    count += 1;
    void exportWav(`${inputName} riddim ${settings.rhythm.replace('/', '-')} ${count}`, out, SR).then((r) => say(r.said));
  });
  folder.addEventListener('click', () => {
    revealExports();
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

  drawControls();
  singleton = {
    open(): void {
      root.hidden = false;
    },
  };
  return singleton;
}
