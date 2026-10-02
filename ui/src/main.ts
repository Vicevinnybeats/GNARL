/*
 * GNARL's panel. The layout is the producer's mock-up (one page, no tabs);
 * the surfaces, glow and knob readouts are the retired engine's "Dream" UI.
 *
 * The desktop panel is laid out at one design size and SCALED to the window,
 * never reflowed - a plugin window resizes by aspect ratio, and a synth whose
 * knobs move when you resize it is a synth you cannot learn.
 *
 * A phone gets its own layout (phone(), below) built from the same panels:
 * one section at a time, controls at finger size, keys always in reach.
 */

import './fonts.css';
import './styles.css';
import { connect, isPlugin, sendLicenceKey, sendRoute, sendWavetable, showClassic } from './bridge';
import { hasWebEngine, startWebEngine, webEngineProblem } from './web/host';
import { builtInPatch, currentPatch, deletePreset, exportPatch, FACTORY_SOUNDS, initPatch, listPresets, loadFactory, loadPatch, loadPatchText, loadProblem, savePatch } from './web/presets';
import { deleteCloud, listCloud, newCode, readCloud, storeCode, storedCode, syncAvailable, useCode, writeCloud } from './sync';
import { evolvePatch, GENERATOR_BASES, generatePatch } from './generate';
import type { Generated } from './generate';
import { decodeAudio, detectMidi, flNoteName, MATCH_BASE, matchSound, trimToWob } from './match/match';
import { drawEnvelope, drawOsc, drawScope, drawSub, drawVowel, drawWobble, setDrawnPoint, setPreviewBpm, DRAWN_STEPS } from './draw';
import { engine, engineViews } from './engine';
import { headerMark, mountLogo } from './logo';
import {
  MOD_DESTINATION_NAMES,
  MOD_DESTINATIONS,
  MOD_SOURCE_NAMES,
  MOD_SOURCES,
  PRESET_NAMES,
  WOBBLE_DESTINATIONS,
} from './params';
import { gesture, get, resetAll, set, subscribe } from './store';
import { chipButton, choiceRow, display, el, knob, panel, resizeDisplay, toggle, wheel } from './widgets';
import type { Display } from './widgets';
import { currentNote, noteName, noteOff, noteOn } from './voice';
import { TABLE_NAMES, TABLES } from './wavetables';
import { FX_PRESETS } from './fxpresets';

const DESIGN_W = 1280;
const DESIGN_H = 720;

const displays: { d: Display; paint: (t: number) => void }[] = [];
function addDisplay(d: Display, paint: (t: number) => void): HTMLElement {
  displays.push({ d, paint });
  return d.root;
}

const knobs = (...nodes: HTMLElement[]): HTMLElement => el('div', 'knobs', ...nodes);

/* ------------------------------------------------------------------ header */

// The preset index is shared: the desktop and phone layouts both show it.
let presetIndex = 0;
const presetViews = new Set<() => void>();

function presetPicker(): HTMLElement {
  const name = el('button', 'preset__name');
  name.type = 'button';
  // In the plugin the name is the engine's loaded preset, and Vital's own
  // browser (ADVANCED) has the presets, so the arrows are off rather than
  // lying. In the web build the arrows step through the patches saved in
  // this browser and the name opens the preset sheet (web/presets.ts).
  // In the plugin the sheet has the starting sounds and INIT (Vital's own
  // browser, behind ADVANCED, saves and opens files), and the arrows step
  // through the starting sounds.
  const web = hasWebEngine();
  const plugin = isPlugin();
  const sheet = web || plugin ? presetSheet(web) : null;
  const show = (): void => {
    name.textContent = engine.connected ? (engine.preset ?? '') : (PRESET_NAMES[presetIndex] ?? '');
    prev.disabled = next.disabled = false;
    name.disabled = !((web || plugin) && engine.connected);
  };
  const step = (by: number): void => {
    if (web && engine.connected) {
      void stepSaved(by);
      return;
    }
    if (plugin && engine.connected) {
      const at = FACTORY_SOUNDS.findIndex((s) => s.name === engine.preset);
      const n = FACTORY_SOUNDS.length;
      const target = FACTORY_SOUNDS[at < 0 ? (by > 0 ? 0 : n - 1) : (at + by + n) % n];
      if (target) loadFactory(target);
      return;
    }
    presetIndex = (presetIndex + by + PRESET_NAMES.length) % PRESET_NAMES.length;
    for (const v of presetViews) v();
  };
  const prev = el('button', 'preset__step', '\u2039');
  const next = el('button', 'preset__step', '\u203a');
  prev.type = next.type = 'button';
  prev.setAttribute('aria-label', 'Previous preset');
  next.setAttribute('aria-label', 'Next preset');
  prev.addEventListener('click', () => step(-1));
  next.addEventListener('click', () => step(1));
  name.addEventListener('click', () => sheet?.toggle());
  // The sheet closes on a tap anywhere outside the picker, as a menu does.
  if (sheet) {
    document.addEventListener('pointerdown', (e) => {
      if (!sheet.root.hidden && !root.contains(e.target as Node)) sheet.close();
    });
    document.addEventListener('keydown', (e) => {
      if (e.key === 'Escape' && !sheet.root.hidden) sheet.close();
    });
  }
  presetViews.add(show);
  engineViews.add(show);
  show();
  const root = el('div', 'preset', prev, name, next, ...(sheet ? [sheet.root] : []));
  return root;
}

/**
 * Load the patch `by` places from the current one: the starting sounds
 * first (factory.json's order), then the patches saved here (by name).
 */
async function stepSaved(by: number): Promise<void> {
  const saved = await listPresets();
  const all: { name: string; open: () => void }[] = [
    ...FACTORY_SOUNDS.map((sound) => ({ name: sound.name, open: () => loadFactory(sound) })),
    ...saved.map((p) => ({ name: p.name, open: () => void openPatch(p.json) })),
  ];
  const at = all.findIndex((p) => p.name === engine.preset);
  const n = all.length;
  const target = all[at < 0 ? (by > 0 ? 0 : n - 1) : (at + by + n) % n];
  target?.open();
}

async function openPatch(json: string): Promise<boolean> {
  const { result } = await loadPatch(json);
  if (result !== 0) toast(loadProblem(result));
  return result === 0;
}

/*
 * The web build's preset sheet: save under a name, the five starting sounds,
 * the saved list (tap to load, x to delete), OPEN a .vital file, EXPORT this
 * patch as one, INIT. Choosing a patch closes it; so do x, Escape and a tap
 * outside (presetPicker).
 */
function presetSheet(web: boolean): { root: HTMLElement; toggle(): void; close(): void } {
  const nameInput = el('input', 'presets__input');
  nameInput.type = 'text';
  nameInput.id = 'preset-name';
  nameInput.placeholder = 'Patch name';
  nameInput.maxLength = 60;
  nameInput.autocomplete = 'off';
  nameInput.spellcheck = false;
  const save = chipButton('SAVE');
  save.type = 'submit';
  const form = el('form', 'presets__form', nameInput, save);
  const list = el('ul', 'presets__list');
  const factoryList = el('ul', 'presets__list presets__list--factory');
  const closeButton = el('button', 'presets__close', '\u00d7');
  closeButton.type = 'button';
  closeButton.setAttribute('aria-label', 'Close presets');
  const file = el('input', 'presets__file');
  file.type = 'file';
  file.accept = '.vital,application/json';
  file.hidden = true;
  const open = chipButton('OPEN FILE');
  const exportButton = chipButton('EXPORT');
  const init = chipButton('INIT');
  const actions = el('div', 'presets__actions', open, exportButton, init, file);
  const root = el(
    'div',
    'presets',
    el('div', 'presets__head', el('span', 'presets__title', 'PRESETS'), closeButton),
    form,
    el('div', 'presets__label', 'FACTORY'),
    factoryList,
    el('div', 'presets__label', 'SAVED'),
    list,
    actions,
  );
  root.hidden = true;
  const cloud = cloudSection(web, () => close());
  if (cloud) root.insertBefore(cloud.root, actions);
  // Saving, the saved list and files are the web build's (IndexedDB); the
  // plugin has Vital's browser for those.
  if (!web) {
    form.hidden = true;
    list.hidden = true;
    for (const node of root.querySelectorAll<HTMLElement>('.presets__label')) node.hidden = node.textContent === 'SAVED';
    open.hidden = exportButton.hidden = true;
  }
  const close = (): void => {
    root.hidden = true;
  };
  closeButton.addEventListener('click', close);

  // Typing a name must not play notes (the computer keyboard plays C2..C3).
  for (const type of ['keydown', 'keyup'] as const) nameInput.addEventListener(type, (e) => e.stopPropagation());

  const render = async (): Promise<void> => {
    if (cloud) void cloud.render();
    factoryList.replaceChildren(
      ...FACTORY_SOUNDS.map((sound) => {
        const load = el('button', 'presets__load', sound.name);
        load.type = 'button';
        load.dataset.on = sound.name === engine.preset ? 'true' : 'false';
        load.addEventListener('click', () => {
          loadFactory(sound);
          close();
        });
        return el('li', 'presets__item', load);
      }),
    );
    if (!web) return;
    const saved = await listPresets();
    list.replaceChildren(
      ...(saved.length === 0
        ? [el('li', 'presets__empty', 'Saved patches appear here.')]
        : saved.map((p) => {
            const load = el('button', 'presets__load', p.name);
            load.type = 'button';
            load.dataset.on = p.name === engine.preset ? 'true' : 'false';
            load.addEventListener('click', () => void openPatch(p.json).then((ok) => ok && close()));
            const remove = el('button', 'presets__delete', '\u00d7');
            remove.type = 'button';
            remove.setAttribute('aria-label', `Delete ${p.name}`);
            remove.addEventListener('click', () => void deletePreset(p.name).then(render));
            return el('li', 'presets__item', load, remove);
          })),
    );
  };

  form.addEventListener('submit', (e) => {
    e.preventDefault();
    const patchName = nameInput.value.trim() || engine.preset || 'Untitled';
    savePatch(patchName)
      .then(({ stored }) => {
        toast(stored ? `Saved ${patchName}.` : `Saved ${patchName} for this visit (this browser keeps no storage).`);
        nameInput.blur();
        close();
      })
      .catch((error: unknown) => toast(String(error)));
  });
  open.addEventListener('click', () => file.click());
  file.addEventListener('change', () => {
    const chosen = file.files?.[0];
    file.value = '';
    if (chosen) void chosen.text().then(openPatch).then((ok) => ok && close());
  });
  exportButton.addEventListener('click', () => {
    const patchName = engine.preset || 'GNARL';
    savePatch(patchName)
      .then(({ json }) => exportPatch(patchName, json))
      .then((said) => {
        toast(said);
        return render();
      })
      .catch((error: unknown) => toast(String(error)));
  });
  // Vital's init patch: one saw on osc 1, as the engine opens.
  init.addEventListener('click', () => {
    initPatch();
    close();
  });

  engineViews.add(() => {
    if (!root.hidden) void render();
    if (document.activeElement !== nameInput) nameInput.value = engine.preset ?? '';
  });

  return {
    root,
    close,
    toggle(): void {
      root.hidden = !root.hidden;
      if (!root.hidden) {
        nameInput.value = engine.preset ?? '';
        void render();
      }
    },
  };
}

/*
 * CLOUD (sync.ts, docs/design/phase8-01-sync.md): the same list of patches
 * in the plugin and on the phone. Without a code: NEW CODE or USE CODE.
 * With one: the code (tap to copy), UPLOAD the current patch, the list (tap
 * to load, x to delete), FORGET. Null when this build has no server.
 */
function cloudSection(web: boolean, done: () => void): { root: HTMLElement; render(): Promise<void> } | null {
  if (!syncAvailable()) return null;
  const body = el('div', 'cloud');
  const list = el('ul', 'presets__list cloud__list');
  const status = el('p', 'cloud__status');
  const say = (text: string): void => {
    status.textContent = text;
  };
  const fail = (error: unknown): void => say(error instanceof Error ? error.message : String(error));

  const render = async (): Promise<void> => {
    const code = storedCode();
    if (!code) {
      const make = chipButton('NEW CODE');
      const use = chipButton('USE CODE');
      const input = el('input', 'presets__input cloud__input');
      input.type = 'text';
      input.placeholder = 'SYNC-XXXX-XXXX-XXXX-XXXX';
      input.autocomplete = 'off';
      input.spellcheck = false;
      input.hidden = true;
      for (const type of ['keydown', 'keyup'] as const) input.addEventListener(type, (e) => e.stopPropagation());
      make.addEventListener('click', () => {
        say('Making a code...');
        newCode().then((made) => {
          say(`Your code is ${made}. Enter it on your other device to share this list.`);
          return render();
        }).catch(fail);
      });
      use.addEventListener('click', () => {
        if (input.hidden) {
          input.hidden = false;
          input.focus();
          return;
        }
        say('Checking...');
        useCode(input.value).then(() => {
          say('');
          return render();
        }).catch(fail);
      });
      body.replaceChildren(
        el('p', 'cloud__hint', 'One list of patches for the plugin and your phone. No account: a code opens it.'),
        el('div', 'presets__actions', make, use),
        input,
        status,
      );
      return;
    }
    const codeButton = chipButton(code);
    codeButton.classList.add('cloud__code');
    codeButton.title = 'Copy the code';
    codeButton.addEventListener('click', () => {
      void navigator.clipboard?.writeText(code).then(() => say('Code copied.'), () => say(code));
    });
    const upload = chipButton('UPLOAD');
    const forget = chipButton('FORGET');
    upload.addEventListener('click', () => {
      const name = (engine.preset || 'Untitled').slice(0, 64);
      say(`Uploading ${name}...`);
      currentPatch(name)
        .then((json) => writeCloud(name, json))
        .then(() => {
          say(`${name} is in the cloud.`);
          return refresh();
        })
        .catch(fail);
    });
    forget.addEventListener('click', () => {
      storeCode(null);
      say(`Forgotten on this device. The list stays in the cloud under ${code}.`);
      void render();
    });
    body.replaceChildren(el('div', 'presets__actions', codeButton, upload, forget), list, status);
    await refresh();
  };

  const refresh = async (): Promise<void> => {
    try {
      const patches = await listCloud();
      list.replaceChildren(
        ...(patches.length === 0
          ? [el('li', 'presets__empty', 'UPLOAD puts the current patch here.')]
          : patches.map((p) => {
              const load = el('button', 'presets__load', p.name);
              load.type = 'button';
              load.dataset.on = p.name === engine.preset ? 'true' : 'false';
              load.addEventListener('click', () => {
                say(`Opening ${p.name}...`);
                readCloud(p.name)
                  .then(async (json) => {
                    if (web) {
                      if (!(await openPatch(json))) return;
                    } else {
                      loadPatchText(p.name, json);
                    }
                    say('');
                    done();
                  })
                  .catch(fail);
              });
              const remove = el('button', 'presets__delete', '\u00d7');
              remove.type = 'button';
              remove.setAttribute('aria-label', `Delete ${p.name} from the cloud`);
              remove.addEventListener('click', () => void deleteCloud(p.name).then(refresh).catch(fail));
              return el('li', 'presets__item', load, remove);
            })),
      );
    } catch (error) {
      list.replaceChildren();
      fail(error);
    }
  };

  const root = el('div', 'cloud__section', el('div', 'presets__label', 'CLOUD'), body);
  return { root, render };
}

/*
 * The AI button opens the pick-the-best mode (docs/design/phase4-03-pick.md):
 * four sounds, tap to hear each, PICK the closest, and the next four grow
 * from it - three near it and one wilder. The producer's ears steer; the
 * steps shrink as the picks go on. Each sound loads as a starting sound does
 * (gnarlPresetFactory), so the picked one is the patch left playing and
 * SAVE keeps it. No network.
 */
const PICK_NOTE = 39; // the producer's D#3 in FL Studio (MIDI 39, about 78 Hz)
const PICK_SECONDS = 1.8; // a two-beat wob at 140 BPM, and its tail
let evolveSheetSingleton: { open(): void } | null = null;

function evolveSheet(): { open(): void } {
  if (evolveSheetSingleton) return evolveSheetSingleton;
  const title = el('span', 'evolve__title', 'AI');
  const closeButton = el('button', 'evolve__close', '\u00d7');
  closeButton.type = 'button';
  closeButton.setAttribute('aria-label', 'Close the AI');
  const hint = el('p', 'evolve__hint', 'Tap a sound to hear it. PICK the closest: the next four grow from it.');
  const grid = el('div', 'evolve__grid');
  const back = chipButton('BACK');
  const fresh = chipButton('NEW');
  const keep = chipButton('KEEP');
  // MATCH A SOUND (match/match.ts): a file of one wob in, the four closest
  // patches out, as this round's cards.
  const matchButton = chipButton('MATCH A SOUND');
  matchButton.classList.add('evolve__match');
  matchButton.title = 'Drop in an audio file of one wob (1-2 s, bass only); GNARL finds the patch';
  const file = el('input', 'evolve__file');
  file.type = 'file';
  file.accept = 'audio/*,.wav,.mp3,.flac,.ogg,.aif,.aiff';
  file.hidden = true;
  const status = el('div', 'evolve__status');
  const bar = el('div', 'evolve__bar', el('div', 'evolve__fill'));
  const cancel = chipButton('STOP');
  const progress = el('div', 'evolve__progress', status, bar, cancel);
  progress.hidden = true;
  matchButton.hidden = typeof window.__GNARL_WASM__ !== 'string';
  const root = el('div', 'evolve', el('div', 'evolve__head', title, closeButton), hint,
    el('div', 'evolve__matchrow', matchButton, file), progress, grid,
    el('div', 'evolve__actions', back, fresh, keep),
    // The producer asked to be reminded, after the sounds, how to steer
    // what comes next (phase4-05).
    el('p', 'evolve__help', 'None right? Tell Claude what is wrong, in sound words: wob too slow or too fast, ' +
      'needs more sub, too thin, too bright or screechy, too much distortion, too much delay or reverb. ' +
      'Or send a .vital patch of yours you like: each one is a new recipe for the AI.'));
  root.hidden = true;
  document.body.append(root);

  let round = 1;
  let candidates: Generated[] = [];
  const history: Generated[][] = [];
  let playing: number | null = null;
  let stopTimer = 0;
  const seed = (): number => 1 + Math.floor(Math.random() * 999_999);

  const bases = (): Record<string, string> => {
    const found: Record<string, string> = {};
    for (const name of GENERATOR_BASES) {
      const text = builtInPatch(name);
      if (text) found[name] = text;
    }
    return found;
  };
  const load = (g: Generated): void => {
    window.__JUCE__?.backend.emitEvent('gnarlPresetFactory', { name: g.name, patch: g.patch });
  };
  const audition = (i: number): void => {
    const g = candidates[i];
    if (!g) return;
    window.clearTimeout(stopTimer);
    noteOff(PICK_NOTE, clock());
    load(g);
    playing = i;
    noteOn(PICK_NOTE, clock());
    stopTimer = window.setTimeout(() => {
      noteOff(PICK_NOTE, clock());
      playing = null;
      render();
    }, PICK_SECONDS * 1000);
    render();
  };
  const render = (): void => {
    title.textContent = `AI \u00b7 ROUND ${round}`;
    back.disabled = history.length === 0;
    grid.replaceChildren(
      ...candidates.map((g, i) => {
        const play = el('button', 'evolve__play', el('span', 'evolve__icon', playing === i ? '\u25a0' : '\u25b6'),
          el('span', 'evolve__name', g.name));
        play.type = 'button';
        play.addEventListener('click', () => audition(i));
        const pick = chipButton('PICK');
        pick.classList.add('evolve__pick');
        pick.addEventListener('click', () => choose(i));
        const card = el('div', 'evolve__card', play, pick);
        card.dataset.on = playing === i ? 'true' : 'false';
        if (round > 1 && i === candidates.length - 1) card.append(el('span', 'evolve__badge', 'WILD'));
        return card;
      }),
    );
  };
  const start = (): void => {
    const found = bases();
    if (Object.keys(found).length === 0) {
      toast('The AI needs its base patches, which this build does not have.');
      return;
    }
    round = 1;
    history.length = 0;
    // One of the four is always a 1/4 wob (the producer's ask), at a random
    // place; the others take whatever their seeds pick.
    const slow = Math.floor(Math.random() * 4);
    candidates = Array.from({ length: 4 }, (_, i) => generatePatch(found, seed(), i === slow ? '1/4' : undefined));
    render();
  };
  const choose = (i: number): void => {
    const parent = candidates[i];
    if (!parent) return;
    load(parent);
    history.push(candidates);
    round += 1;
    // Steps shrink as the picks go on (1, 0.74, 0.59 ... never below 0.25),
    // so the search settles where the producer's picks lead it.
    const strength = Math.max(0.25, 1 / (1 + 0.35 * (round - 1)));
    const label = `picked from ${parent.name}`;
    candidates = [
      ...Array.from({ length: 3 }, () => evolvePatch(parent.patch, seed(), strength, label)),
      evolvePatch(parent.patch, seed(), 1, `a wild step from ${parent.name}`),
    ];
    render();
  };
  let matching = false;
  let stop = false;
  matchButton.addEventListener('click', () => file.click());
  cancel.addEventListener('click', () => {
    stop = true;
  });
  file.addEventListener('change', () => {
    const chosen = file.files?.[0];
    file.value = '';
    if (!chosen || matching) return;
    void (async (): Promise<void> => {
      matching = true;
      stop = false;
      matchButton.disabled = true;
      progress.hidden = false;
      const fill = bar.firstElementChild as HTMLElement;
      fill.style.width = '0%';
      try {
        status.textContent = `Listening to ${chosen.name}...`;
        const audio = trimToWob(await decodeAudio(await chosen.arrayBuffer()));
        if (audio.length < 0.15 * 44100) throw new Error('that file is too short: one wob of 1-2 s works best');
        const midi = detectMidi(audio) ?? 29;
        const base = builtInPatch(MATCH_BASE);
        if (!base) throw new Error('this build lacks the matcher\'s base patch');
        const found = await matchSound({
          // A fresh search each time; a page may pin it (tests/web.test.mjs):
          // the closest found varies with the seed (phase4-04).
          audio, midi, baseText: base,
          seed: (window as { __GNARL_MATCH_SEED__?: number }).__GNARL_MATCH_SEED__ ?? 1 + Math.floor(Math.random() * 999_999),
          onProgress: (done, total, best) => {
            fill.style.width = `${Math.round((100 * done) / total)}%`;
            status.textContent = `Note ${flNoteName(midi)} - trying sounds: ${done} of ${total}, closest ${best.toFixed(2)} dB`;
          },
          cancelled: () => stop,
        });
        if (found.length === 0) throw new Error('no sound came close; try a cleaner wob');
        history.push(candidates);
        candidates = found;
        round += 1;
        render();
        title.textContent = 'AI \u00b7 MATCHED';
        status.textContent = `Closest four to your sound (note ${flNoteName(midi)}, closest ${(found[0]?.distance ?? 0).toFixed(2)} dB). Tap to hear, PICK to refine by ear.`;
        fill.style.width = '100%';
      } catch (error) {
        status.textContent = `Could not match: ${(error as Error).message}`;
      } finally {
        matching = false;
        matchButton.disabled = false;
      }
    })();
  });
  back.addEventListener('click', () => {
    const previous = history.pop();
    if (!previous) return;
    candidates = previous;
    round -= 1;
    render();
  });
  fresh.addEventListener('click', start);
  const close = (): void => {
    window.clearTimeout(stopTimer);
    noteOff(PICK_NOTE, clock());
    playing = null;
    root.hidden = true;
  };
  keep.addEventListener('click', () => {
    const kept = playing ?? null;
    close();
    toast(kept === null ? 'Kept the last sound you heard. SAVE it from the presets.' : 'Kept. SAVE it from the presets.');
  });
  closeButton.addEventListener('click', close);
  evolveSheetSingleton = {
    open(): void {
      if (candidates.length === 0) start();
      root.hidden = false;
    },
  };
  return evolveSheetSingleton;
}

function aiButton(label: string): HTMLButtonElement {
  const ai = el('button', 'ai', el('span', 'ai__spark', '\u2726'), label);
  ai.type = 'button';
  ai.title = 'Pick-the-best: make new wob sounds and steer them by ear';
  ai.addEventListener('click', () => evolveSheet().open());
  return ai;
}

/**
 * The live waveform beside MASTER. Pressing and holding it plays a note, so
 * the patch can be seen moving without a keyboard.
 */
function scope(width: number, height: number): HTMLElement {
  const d = display(width, height, 'scope');
  const root = addDisplay(d, (t) => drawScope(d, t, t));
  root.title = 'Hold to play a note (or use the keys A to K)';
  const PREVIEW_NOTE = 36; // C2: low enough for riddim, high enough to see.
  root.addEventListener('pointerdown', (e) => {
    root.setPointerCapture(e.pointerId);
    noteOn(PREVIEW_NOTE, clock());
  });
  const release = (): void => noteOff(PREVIEW_NOTE, clock());
  root.addEventListener('pointerup', release);
  root.addEventListener('pointercancel', release);
  return root;
}

function masterKnob(): HTMLElement {
  const master = knob('master', { size: 28 });
  master.classList.add('knob--inline');
  return master;
}

/** Inside the plugin: Vital's full editor, for everything this panel lacks. */
function advancedButton(): HTMLButtonElement {
  const b = chipButton('ADVANCED');
  b.classList.add('advanced');
  b.title = 'The full editor: every parameter, the modulation matrix, wavetables';
  b.hidden = !isPlugin();
  b.addEventListener('click', showClassic);
  return b;
}

/** Pitch (springs back) and mod wheel, as on a hardware keyboard's left end. */
function wheels(): HTMLElement {
  return el('div', 'wheels', wheel('pitch', { spring: true }), wheel('modwheel', { accent: 'violet' }));
}

/*
 * The licence chip (Phase 7, docs/design/phase7-01-licence.md). Shown only
 * when the plugin has something to say - offline, expired, not yet entered,
 * a development build - and never because of anything about audio, which
 * no licence state can stop. Tapping it shows the sentence and, where a key
 * would help, a field for one. The key goes to the machine's settings, never
 * into a preset.
 */
/*
 * The page's tempo, in the web build only: the plugin follows the DAW's.
 * Riddim is written at 140 and 145 and mixed at 150 (the producer), so those
 * three, remembered in this browser. Every rate in GNARL is tempo-synced, so
 * this sets how long a wob is.
 */
const TEMPOS = [140, 145, 150] as const;
function tempoChip(): HTMLElement | null {
  if (!hasWebEngine()) return null;
  let bpm: number = TEMPOS[0];
  try {
    const kept = Number(localStorage.getItem('gnarl.bpm'));
    if ((TEMPOS as readonly number[]).includes(kept)) bpm = kept;
  } catch {
    // No storage here: 140.
  }
  const b = chipButton('');
  b.classList.add('tempo');
  b.title = 'Tempo: 140, 145 or 150 BPM';
  let sent = false;
  const send = (): void => {
    const backend = window.__JUCE__?.backend;
    if (!backend) return;
    backend.emitEvent('gnarlTempo', { bpm });
    sent = true;
  };
  const show = (): void => {
    b.textContent = `${bpm} BPM`;
    setPreviewBpm(bpm);
  };
  b.addEventListener('click', () => {
    bpm = TEMPOS[(TEMPOS.indexOf(bpm as (typeof TEMPOS)[number]) + 1) % TEMPOS.length] ?? TEMPOS[0];
    try {
      localStorage.setItem('gnarl.bpm', String(bpm));
    } catch {
      // Kept for this visit only.
    }
    show();
    send();
  });
  // The engine starts at 140; a kept 145 or 150 goes to it once it is up.
  engineViews.add(() => {
    if (!sent && bpm !== TEMPOS[0]) send();
  });
  show();
  return b;
}

function licenceChip(): HTMLElement {
  const button = el('button', 'licence__chip', 'LICENCE');
  button.type = 'button';
  const message = el('p', 'licence__message');
  const input = el('input', 'licence__input');
  input.type = 'text';
  input.id = 'licence-key';
  input.placeholder = 'GNARL-XXXX-XXXX-XXXX';
  input.autocomplete = 'off';
  input.spellcheck = false;
  const save = chipButton('CHECK KEY');
  save.type = 'submit';
  const form = el('form', 'licence__form', input, save);
  const pop = el('div', 'licence__pop', message, form);
  pop.hidden = true;
  const root = el('div', 'licence', button, pop);
  root.hidden = true;

  button.addEventListener('click', () => (pop.hidden = !pop.hidden));
  form.addEventListener('submit', (e) => {
    e.preventDefault();
    sendLicenceKey(input.value);
    input.value = '';
    message.textContent = 'Checking...';
  });
  // Typing a key must not play notes (the computer keyboard plays C2..C3).
  input.addEventListener('keydown', (e) => e.stopPropagation());
  input.addEventListener('keyup', (e) => e.stopPropagation());

  engineViews.add(() => {
    const licence = engine.licence;
    root.hidden = !licence || licence.message === '';
    if (!licence) return;
    root.dataset.status = licence.status;
    button.textContent = licence.saving ? 'LICENCE' : 'NOT SAVING';
    message.textContent = licence.message;
    // No key to enter in a development build: nothing would check it.
    form.hidden = licence.status === 'unenforced';
    if (licence.status === 'licensed') pop.hidden = true;
  });
  return root;
}

function header(): HTMLElement {
  return el(
    'header',
    'top',
    el('div', 'brand', headerMark(), el('span', 'brand__word', 'GNARL')),
    presetPicker(),
    aiButton('AI PRESET'),
    advancedButton(),
    licenceChip(),
    el('div', 'top__spacer'),
    ...[tempoChip()].filter((x): x is HTMLElement => x !== null),
    scope(250, 34),
    masterKnob(),
  );
}

/* ------------------------------------------------------------ oscillators */

/*
 * OSC n's table: GNARL's own (wavetables.ts), stepped with the arrows or
 * picked from the list. The name is the engine's, so a preset's own table
 * (Vital's init, a producer's import) shows as itself, outside the list.
 */
function tablePicker(n: 1 | 2): HTMLElement {
  const select = el('select', 'table-pick__list');
  select.setAttribute('aria-label', `OSC ${n} wavetable`);
  const other = el('option', '', '');
  other.disabled = true;
  select.append(other);
  for (const t of TABLES) {
    const o = el('option', '', t.name.toUpperCase());
    o.value = t.name;
    o.title = t.about;
    select.append(o);
  }
  const arrow = (step: number, glyph: string): HTMLButtonElement => {
    const b = el('button', 'table-pick__arrow', glyph);
    b.type = 'button';
    b.setAttribute('aria-label', step < 0 ? `OSC ${n}: previous wavetable` : `OSC ${n}: next wavetable`);
    b.addEventListener('click', () => {
      const at = TABLE_NAMES.indexOf(select.value);
      const next = at < 0 ? (step > 0 ? 0 : TABLE_NAMES.length - 1) : (at + step + TABLE_NAMES.length) % TABLE_NAMES.length;
      pick(TABLE_NAMES[next] ?? 'Basic');
    });
    return b;
  };
  const pick = (name: string): void => {
    select.value = name;
    sendWavetable(n, name);
  };
  select.addEventListener('change', () => pick(select.value));
  const show = (): void => {
    const name = engine.tables[n - 1];
    if (name == null) return;
    if (TABLE_NAMES.includes(name)) {
      select.value = name;
    } else {
      other.textContent = (name || 'INIT').toUpperCase();
      select.value = '';
      other.selected = true;
    }
  };
  engineViews.add(show);
  other.textContent = 'INIT';
  other.selected = true;
  show();
  return el('span', 'table-pick', arrow(-1, '\u2039'), select, arrow(1, '\u203a'));
}

function oscPanel(n: 1 | 2): HTMLElement {
  const d = display(280, 110);
  return panel(
    { title: `OSC ${n}`, aside: tablePicker(n), power: `osc${n}.on` },
    addDisplay(d, (t) => drawOsc(d, n, t)),
    knobs(knob(`osc${n}.wtpos`), knob(`osc${n}.warp`), knob(`osc${n}.fm`), knob(`osc${n}.unison`), knob(`osc${n}.detune`)),
    choiceRow(`osc${n}.mode`),
  );
}

function subPanel(): HTMLElement {
  const d = display(170, 78);
  return panel(
    { title: 'SUB', power: 'sub.on' },
    addDisplay(d, () => drawSub(d)),
    knobs(knob('sub.level'), knob('sub.drive')),
    el('div', 'chips', toggle('sub.mono', 'MONO'), toggle('sub.oct', '-1 OCT')),
  );
}

/** Filter 1's model, which the bridge writes in once it knows (bridge.ts). */
function filterModelAside(): HTMLElement {
  const aside = el('span', 'panel__aside', 'FORMANT');
  aside.dataset.filterModel = 'true';
  return aside;
}

function vowelPanel(): HTMLElement {
  const d = display(360, 110);
  return panel(
    { title: 'VOWEL FILTER', aside: filterModelAside(), power: 'vowel.on' },
    addDisplay(d, (t) => drawVowel(d, t)),
    choiceRow('vowel.vowel', { cls: 'chips--vowels' }),
    knobs(knob('vowel.cutoff'), knob('vowel.res'), knob('vowel.morph'), knob('vowel.drive')),
  );
}

/* ------------------------------------------------------------------ wobble */

function wobblePanel(): HTMLElement {
  const d = display(210, 96);
  // DRAW mode: drag across the display to set the steps.
  let drawing = false;
  const paintAt = (e: PointerEvent): void => {
    const r = d.canvas.getBoundingClientRect();
    const x = (e.clientX - r.left) / r.width;
    const y = (e.clientY - r.top) / r.height;
    const step = Math.floor(((x * 2) % 1) * DRAWN_STEPS);
    setDrawnPoint(step, 1 - y);
  };
  d.canvas.addEventListener('pointerdown', (e) => {
    if (get('wobble.shape') !== 2) {
      toast('Pick DRAW under SHAPE to draw the wobble.');
      return;
    }
    drawing = true;
    d.canvas.setPointerCapture(e.pointerId);
    paintAt(e);
  });
  d.canvas.addEventListener('pointermove', (e) => drawing && paintAt(e));
  d.canvas.addEventListener('pointerup', () => (drawing = false));

  const destinations = el(
    'div',
    'chips',
    ...WOBBLE_DESTINATIONS.map((dest) => toggle(dest.id, dest.label, 'violet')),
  );

  return panel(
    { title: 'WOBBLE LFO', aside: wobbleAside(), accent: 'violet', power: 'wobble.on', cls: 'wobble' },
    el(
      'div',
      'wobble__grid',
      el(
        'div',
        'wobble__left',
        addDisplay(d, (t) => drawWobble(d, t)),
        knobs(
          knob('wobble.depth', { accent: 'violet' }),
          knob('wobble.smooth', { accent: 'violet' }),
          knob('wobble.phase', { accent: 'violet' }),
        ),
      ),
      el(
        'div',
        'wobble__right',
        el('span', 'field', 'RATE'),
        choiceRow('wobble.rate', { accent: 'violet' }),
        el('span', 'field', 'DESTINATION'),
        destinations,
        el('span', 'field', 'SHAPE'),
        choiceRow('wobble.shape'),
      ),
    ),
  );
}

/* ---------------------------------------------------------------- envelope */

function envelopePanel(): HTMLElement {
  const d = display(220, 96);
  const row = el('div', 'knobs');
  const build = (): void => {
    const page = get('env.page') ? 'filter' : 'amp';
    row.replaceChildren(
      knob(`env.${page}.att`),
      knob(`env.${page}.dec`),
      knob(`env.${page}.sus`),
      knob(`env.${page}.rel`),
    );
  };
  subscribe((id) => id === 'env.page' && build());
  build();
  return panel(
    { title: 'ENVELOPE', aside: choiceRow('env.page', { cls: 'chips--tabs' }) },
    addDisplay(d, () => drawEnvelope(d)),
    row,
  );
}

/* -------------------------------------------------------------- mod matrix */

interface Route {
  source: number;
  dest: number;
  amount: number;
}

// Shared by both layouts, so a route added on the phone shows on the desktop.
// In the plugin it is rebuilt from the engine's matrix (engine.routes); every
// edit is sent to the engine, which answers with the new matrix.
const routes: Route[] = [
  { source: 0, dest: 0, amount: 0.72 },
  { source: 0, dest: 2, amount: 0.45 },
  { source: 1, dest: 3, amount: 0.55 },
];
const MAX_ROUTES = 4;
// Engine connections this panel has no names for (made in the full editor).
let hiddenRoutes = 0;
const routeViews = new Set<() => void>();
const rerenderRoutes = (): void => {
  for (const v of routeViews) v();
};

function routesFromEngine(): void {
  const shown: Route[] = [];
  hiddenRoutes = 0;
  for (const r of engine.routes) {
    const source = (MOD_SOURCE_NAMES as readonly string[]).indexOf(r.source);
    const dest = (MOD_DESTINATION_NAMES as readonly string[]).indexOf(r.destination);
    if (source < 0 || dest < 0 || shown.length >= MAX_ROUTES) hiddenRoutes += 1;
    else shown.push({ source, dest, amount: Math.min(1, Math.max(-1, r.amount)) });
  }
  routes.splice(0, routes.length, ...shown);
}
engineViews.add(() => {
  if (!engine.connected) return;
  routesFromEngine();
  rerenderRoutes();
});

const names = (r: Route): [string, string] => [MOD_SOURCE_NAMES[r.source] ?? '', MOD_DESTINATION_NAMES[r.dest] ?? ''];
const taken = (source: number, dest: number, except: Route): boolean =>
  routes.some((r) => r !== except && r.source === source && r.dest === dest);

/** Move a route to a new source or destination: in the engine, disconnect and connect. */
function retarget(route: Route, source: number, dest: number): void {
  if (taken(source, dest, route)) return;
  if (engine.connected) {
    const [s0, d0] = names(route);
    sendRoute(s0, d0, 0, true);
    sendRoute(MOD_SOURCE_NAMES[source] ?? '', MOD_DESTINATION_NAMES[dest] ?? '', route.amount);
  }
  route.source = source;
  route.dest = dest;
  rerenderRoutes();
}

function modPanel(): HTMLElement {
  const list = el('div', 'routes');
  const count = el('span', 'panel__aside');

  const render = (): void => {
    list.replaceChildren(
      ...routes.map((route, i) => {
        const src = chipButton(MOD_SOURCES[route.source] ?? '', 'violet');
        src.dataset.on = 'true';
        src.title = 'Click to change the source';
        src.addEventListener('click', () => {
          for (let k = 1; k <= MOD_SOURCES.length; k += 1) {
            const next = (route.source + k) % MOD_SOURCES.length;
            if (!taken(next, route.dest, route)) return retarget(route, next, route.dest);
          }
        });

        const dest = el('button', 'route__dest', MOD_DESTINATIONS[route.dest] ?? '');
        dest.type = 'button';
        dest.title = 'Click to change the destination; right-click to remove';
        dest.addEventListener('click', () => {
          for (let k = 1; k <= MOD_DESTINATIONS.length; k += 1) {
            const next = (route.dest + k) % MOD_DESTINATIONS.length;
            if (!taken(route.source, next, route)) return retarget(route, route.source, next);
          }
        });
        dest.addEventListener('contextmenu', (e) => {
          e.preventDefault();
          if (engine.connected) sendRoute(...names(route), 0, true);
          routes.splice(i, 1);
          rerenderRoutes();
        });

        // Bipolar, as Vital's matrix: the centre is zero, left of it a
        // negative amount (a wobble on a LEVEL that cuts rather than boosts).
        const fill = el('span', 'amount__fill');
        const bar = el('div', 'amount', el('span', 'amount__zero'), fill);
        const draw = (): void => {
          fill.style.left = `${50 + Math.min(0, route.amount) * 50}%`;
          fill.style.width = `${Math.abs(route.amount) * 50}%`;
          bar.title = `${Math.round(route.amount * 100)} %`;
        };
        draw();
        const setFrom = (e: PointerEvent): void => {
          const r = bar.getBoundingClientRect();
          const amount = Math.min(1, Math.max(-1, (2 * (e.clientX - r.left)) / r.width - 1));
          // A little detent at zero, so a route can be set back to none.
          route.amount = Math.abs(amount) < 0.04 ? 0 : amount;
          draw();
          if (engine.connected) sendRoute(...names(route), route.amount);
        };
        bar.addEventListener('pointerup', rerenderRoutes);
        bar.addEventListener('pointerdown', (e) => {
          bar.setPointerCapture(e.pointerId);
          setFrom(e);
        });
        bar.addEventListener('pointermove', (e) => bar.hasPointerCapture(e.pointerId) && setFrom(e));
        return el('div', 'route', src, dest, bar);
      }),
    );
    const add = el('button', 'route__add', '+ ADD ROUTE');
    add.type = 'button';
    add.disabled = routes.length >= MAX_ROUTES;
    add.addEventListener('click', () => {
      // The first source/destination pair not already in the matrix.
      for (let n = 0; n < MOD_SOURCES.length * MOD_DESTINATIONS.length; n += 1) {
        const route = { source: (2 + Math.floor(n / MOD_DESTINATIONS.length)) % MOD_SOURCES.length,
                        dest: (4 + n) % MOD_DESTINATIONS.length, amount: 0.5 };
        if (taken(route.source, route.dest, route)) continue;
        if (engine.connected) sendRoute(...names(route), route.amount);
        routes.push(route);
        rerenderRoutes();
        return;
      }
    });
    list.append(add);
    count.textContent = `${routes.length}/${MAX_ROUTES}` + (hiddenRoutes ? ` +${hiddenRoutes} in ADVANCED` : '');
  };
  routeViews.add(render);
  render();

  return panel({ title: 'MOD MATRIX', aside: count, accent: 'violet' }, list);
}

/** The wobble panel's corner: what its RATE moves (bridge.ts fills it in). */
function wobbleAside(): HTMLElement {
  const aside = el('span', 'panel__aside', 'SYNC TO HOST');
  aside.dataset.wobbleSource = 'true';
  return aside;
}

/* ---------------------------------------------------------------------- fx */

interface FxSlot {
  slot: string;
  title: string;
  knobs: readonly string[];
  choices: readonly string[];
  /** Built before the knobs: the delay line's LED. */
  lead?: () => HTMLElement;
  /** The choices go in the header even beside two knobs. */
  header?: boolean;
}

// Every effect the panel shows, in pages: the drive chain GNARL built
// (docs/design/phase2-07-drive-chain.md), then Vital's own effects
// (docs/design/phase2-10-fx.md). The numbers name a slot, not the signal
// order: the engine runs chorus, OTT, delay, the drive chain, EQ, flanger,
// phaser, reverb.
const FX_PAGES: readonly { label: string; slots: readonly FxSlot[] }[] = [
  {
    label: 'DRIVE',
    slots: [
      { slot: 'dist', title: 'DIST', knobs: ['dist.drive', 'dist.mix'], choices: ['dist.mode'] },
      { slot: 'fold', title: 'FOLD', knobs: ['fold.amount', 'fold.mix'], choices: ['fold.mode'] },
      { slot: 'crush', title: 'CRUSH', knobs: ['crush.bits', 'crush.rate'], choices: ['crush.mode'] },
      { slot: 'ott', title: 'OTT', knobs: ['ott.depth', 'ott.time'], choices: ['ott.mode'] },
    ],
  },
  {
    label: 'MOD',
    slots: [
      { slot: 'chorus', title: 'CHORUS', knobs: ['chorus.depth', 'chorus.feedback', 'chorus.mix'], choices: [] },
      { slot: 'flanger', title: 'FLANGER', knobs: ['flanger.depth', 'flanger.feedback', 'flanger.mix'], choices: ['flanger.rate'] },
      { slot: 'phaser', title: 'PHASER', knobs: ['phaser.feedback', 'phaser.center', 'phaser.mix'], choices: ['phaser.rate'] },
      { slot: 'eq', title: 'EQ', knobs: ['eq.low', 'eq.mid', 'eq.freq', 'eq.high'], choices: [] },
    ],
  },
  {
    label: 'SPACE',
    slots: [
      {
        slot: 'delay',
        title: 'DELAY LINE',
        knobs: ['delay.feedback', 'delay.mix'],
        choices: ['delay.style'],
        lead: delayLine,
        header: true,
      },
      { slot: 'reverb', title: 'REVERB', knobs: ['reverb.size', 'reverb.decay', 'reverb.mix'], choices: [] },
    ],
  },
];

/*
 * The slot's PRESET button (fxpresets.ts): each tap applies the next preset,
 * as a knob turn would - a gesture per control, so a host records it - and
 * switches the effect on. It names the preset it applied until a control
 * of the slot is moved by hand.
 */
function fxPresetButton(slot: string): HTMLElement | null {
  const presets = FX_PRESETS[slot];
  if (!presets || presets.length === 0) return null;
  const b = chipButton('PRESET');
  b.classList.add('fx__preset');
  b.title = `${presets.map((p) => p.name).join(', ')}: tap for the next`;
  let at = -1;
  let applying = false;
  b.addEventListener('click', () => {
    at = (at + 1) % presets.length;
    const preset = presets[at];
    if (!preset) return;
    applying = true;
    for (const [id, value] of [[`${slot}.on`, 1] as const, ...Object.entries(preset.values)]) {
      gesture(id, true);
      set(id, value);
      gesture(id, false);
    }
    applying = false;
    b.textContent = preset.name;
    b.dataset.on = 'true';
  });
  const own = new Set(presets.flatMap((p) => Object.keys(p.values)));
  subscribe((id, _value, fromEngine) => {
    if (applying || fromEngine || !own.has(id)) return;
    b.textContent = 'PRESET';
    b.dataset.on = 'false';
  });
  return b;
}

function fxPanel(index: number, fx: FxSlot): HTMLElement {
  const number = index < 10 ? `0${index}` : `${index}`;
  const choices = fx.choices.map((id) => choiceRow(id, { cls: 'chips--cycle' }));
  // Beside three knobs a button does not fit the row: it goes in the header.
  const inHeader = (fx.header === true || fx.knobs.length > 2) && choices.length > 0;
  const preset = fxPresetButton(fx.slot);
  return panel(
    {
      title: `${number} ${fx.title}`,
      power: `${fx.slot}.on`,
      cls: `fx fx--${fx.slot}`,
      aside: el('div', 'fx__aside', ...(preset ? [preset] : []), ...(inHeader ? choices : [])),
    },
    el(
      'div',
      'fx__row',
      ...(fx.lead ? [fx.lead()] : []),
      ...fx.knobs.map((id) => knob(id)),
      ...(inHeader ? [] : choices),
    ),
  );
}

/*
 * The delay line's counter (docs/design/phase2-11-ddl.md), after a hardware
 * step delay's: a four-digit LED with the delay's length in STEPS of the step
 * length, or in MS; up and down buttons that repeat while held; drag the LED
 * up or down. The unlit segments stay faintly visible, as an LED's do.
 */
const SEGMENTS: Readonly<Record<string, readonly number[]>> = {
  // a b c d e f g, as 0..6
  '0': [0, 1, 2, 3, 4, 5],
  '1': [1, 2],
  '2': [0, 1, 6, 4, 3],
  '3': [0, 1, 6, 2, 3],
  '4': [5, 6, 1, 2],
  '5': [0, 5, 6, 2, 3],
  '6': [0, 5, 6, 4, 3, 2],
  '7': [0, 1, 2],
  '8': [0, 1, 2, 3, 4, 5, 6],
  '9': [0, 1, 2, 3, 5, 6],
};
// x, y, width, height of segments a..g in a 10 x 18 cell.
const SEGMENT_RECTS = [
  [2, 0.4, 6, 1.6], [7.9, 2, 1.6, 6.3], [7.9, 9.7, 1.6, 6.3], [2, 16, 6, 1.6],
  [0.5, 9.7, 1.6, 6.3], [0.5, 2, 1.6, 6.3], [2, 8.2, 6, 1.6],
] as const;
const LED_DIGITS = 4;

/** The next time a tap gives: `by` ms on, or from an odd time (a preset's
 * 215 ms) first onto the 10 ms grid in that direction. */
function nextMs(ms: number, sign: number, by: number): number {
  const v = Math.round(ms);
  if (v % 10 !== 0) return sign > 0 ? Math.ceil(v / 10) * 10 : Math.floor(v / 10) * 10;
  return v + sign * by;
}
const SVG_NS = 'http://www.w3.org/2000/svg';

function delayLine(): HTMLElement {
  const svg = document.createElementNS(SVG_NS, 'svg');
  svg.setAttribute('viewBox', `0 0 ${LED_DIGITS * 12 - 2} 18`);
  svg.setAttribute('class', 'ddl__digits');
  const segments: SVGRectElement[][] = [];
  for (let d = 0; d < LED_DIGITS; d += 1) {
    const g = document.createElementNS(SVG_NS, 'g');
    g.setAttribute('transform', `translate(${d * 12} 0) skewX(-6)`);
    segments.push(
      SEGMENT_RECTS.map(([x, y, w, h]) => {
        const r = document.createElementNS(SVG_NS, 'rect');
        r.setAttribute('x', `${x}`);
        r.setAttribute('y', `${y}`);
        r.setAttribute('width', `${w}`);
        r.setAttribute('height', `${h}`);
        r.setAttribute('rx', '0.6');
        g.append(r);
        return r;
      }),
    );
    svg.append(g);
  }
  const led = el('div', 'ddl__led');
  led.append(svg);
  led.title = 'Drag up or down';
  const up = el('button', 'ddl__step', '\u25b2');
  const down = el('button', 'ddl__step', '\u25bc');
  up.type = down.type = 'button';
  up.setAttribute('aria-label', 'Longer');
  down.setAttribute('aria-label', 'Shorter');

  const ms = (): boolean => get('delay.unit') === 1;
  const id = (): string => (ms() ? 'delay.ms' : 'delay.steps');
  const limits = (): [number, number] => (ms() ? [2, 4000] : [1, 16]);
  const put = (value: number): void => {
    const [lo, hi] = limits();
    set(id(), Math.min(hi, Math.max(lo, Math.round(value))));
  };

  const draw = (): void => {
    const text = `${Math.round(get(id()))}`.padStart(LED_DIGITS, ' ').slice(-LED_DIGITS);
    [...text].forEach((ch, d) => {
      const lit = new Set(SEGMENTS[ch] ?? []);
      segments[d]?.forEach((r, s) => r.setAttribute('class', lit.has(s) ? 'on' : ''));
    });
    led.setAttribute('aria-label', ms() ? `${text.trim()} milliseconds` : `${text.trim()} steps`);
  };
  subscribe((changed) => (changed === 'delay.unit' || changed === 'delay.steps' || changed === 'delay.ms') && draw());
  draw();

  // In MS a tap moves 10 ms - 1 ms is not a change anyone hears (the
  // producer: "not working") - and a held button, after a moment, 50.
  // STEPS move one at a time.
  const hold = (button: HTMLButtonElement, sign: number): void => {
    let timer = 0;
    let count = 0;
    const stop = (): void => {
      window.clearTimeout(timer);
      if (count > 0) gesture(id(), false);
      count = 0;
    };
    const tick = (): void => {
      put(ms() ? nextMs(get(id()), sign, count > 12 ? 50 : 10) : get(id()) + sign);
      count += 1;
      timer = window.setTimeout(tick, count === 1 ? 380 : 70);
    };
    button.addEventListener('pointerdown', (e) => {
      e.preventDefault();
      button.setPointerCapture(e.pointerId);
      gesture(id(), true);
      tick();
    });
    button.addEventListener('pointerup', stop);
    button.addEventListener('pointercancel', stop);
  };
  hold(up, 1);
  hold(down, -1);

  // A drag: 8 px a step, or in MS an octave of time every 120 px.
  let from: { y: number; value: number } | null = null;
  led.addEventListener('pointerdown', (e) => {
    led.setPointerCapture(e.pointerId);
    from = { y: e.clientY, value: get(id()) };
    gesture(id(), true);
  });
  led.addEventListener('pointermove', (e) => {
    if (!from || !led.hasPointerCapture(e.pointerId)) return;
    const dy = from.y - e.clientY;
    put(ms() ? from.value * Math.pow(2, dy / 120) : from.value + dy / 8);
  });
  const end = (): void => {
    if (from) gesture(id(), false);
    from = null;
  };
  led.addEventListener('pointerup', end);
  led.addEventListener('pointercancel', end);

  return el(
    'div',
    'ddl',
    led,
    el('div', 'ddl__steps', up, down),
    el(
      'div',
      'ddl__modes',
      choiceRow('delay.unit', { cls: 'chips--cycle' }),
      choiceRow('delay.length', { cls: 'chips--cycle' }),
    ),
  );
}

/** The effects behind page buttons: DRIVE, MOD, SPACE. */
function fxRack(): { nav: HTMLElement; pages: HTMLElement } {
  let index = 0;
  const pages = FX_PAGES.map((page) =>
    el('div', 'fxrack__page', ...page.slots.map((fx) => fxPanel(++index, fx))),
  );
  const nav = el('nav', 'fxrack__nav chips');
  const buttons = FX_PAGES.map((page, i) => {
    const b = chipButton(page.label);
    b.addEventListener('click', () => select(i));
    nav.append(b);
    return b;
  });
  const select = (i: number): void => {
    pages.forEach((p, k) => (p.hidden = k !== i));
    buttons.forEach((b, k) => (b.dataset.on = k === i ? 'true' : 'false'));
  };
  select(0);
  return { nav, pages: el('div', 'fxrack', ...pages) };
}

const logos: { canvas: HTMLCanvasElement; paint: (t: number) => void }[] = [];
function logoCanvas(cls: string): HTMLCanvasElement {
  const canvas = el('canvas', cls);
  logos.push({ canvas, paint: mountLogo(canvas) });
  return canvas;
}

/* ------------------------------------------------------------------ layout */

function desktop(): HTMLElement {
  return el(
    'div',
    'app',
    header(),
    el('div', 'row row--1', oscPanel(1), oscPanel(2), subPanel(), vowelPanel()),
    el('div', 'row row--2', wobblePanel(), envelopePanel(), modPanel()),
    (() => {
      const rack = fxRack();
      // The wheels where the corner mark was: 16 x 30 px in the header was
      // too small to play (the producer: "somewhere else and bigger").
      return el('div', 'row row--3', rack.nav, rack.pages, el('section', 'panel wheels-panel', wheels()));
    })(),
  );
}

/* ---------------------------------------------------------------- keyboard */

// C1 to C3: where riddim basses live, and two octaves fit a phone's width.
const KEY_LOW = 24;
const KEY_HIGH = 48;
const BLACK = new Set([1, 3, 6, 8, 10]);

const keyViews = new Set<() => void>();
function keyboard(): HTMLElement {
  const root = el('div', 'keys');
  const whites = el('div', 'keys__whites');
  const blacks = el('div', 'keys__blacks');
  const byNote = new Map<number, HTMLElement>();
  let whiteCount = 0;
  for (let n = KEY_LOW; n <= KEY_HIGH; n += 1) if (!BLACK.has(n % 12)) whiteCount += 1;

  let w = 0;
  for (let n = KEY_LOW; n <= KEY_HIGH; n += 1) {
    const black = BLACK.has(n % 12);
    const key = el('span', black ? 'key key--black' : 'key key--white');
    if (black) {
      key.style.left = `${(w / whiteCount) * 100}%`;
      key.style.width = `${(0.62 / whiteCount) * 100}%`;
      blacks.append(key);
    } else {
      if (n % 12 === 0) key.append(el('span', 'key__label', noteName(n)));
      whites.append(key);
      w += 1;
    }
    key.dataset.note = `${n}`;
    byNote.set(n, key);
  }
  root.append(whites, blacks);

  // One finger, one note: a glide across the keys retriggers as it crosses.
  let held: number | null = null;
  const noteAt = (e: PointerEvent): number | null => {
    const target = document.elementFromPoint(e.clientX, e.clientY);
    const key = target instanceof HTMLElement ? target.closest<HTMLElement>('.key') : null;
    return key && root.contains(key) ? Number(key.dataset.note) : null;
  };
  const play = (n: number | null): void => {
    if (n === held) return;
    if (held !== null) noteOff(held, clock());
    held = n;
    if (n !== null) noteOn(n, clock());
    for (const v of keyViews) v();
  };
  root.addEventListener('pointerdown', (e) => {
    root.setPointerCapture(e.pointerId);
    play(noteAt(e));
  });
  root.addEventListener('pointermove', (e) => root.hasPointerCapture(e.pointerId) && play(noteAt(e)));
  const up = (): void => play(null);
  root.addEventListener('pointerup', up);
  root.addEventListener('pointercancel', up);

  keyViews.add(() => {
    const on = currentNote();
    for (const [n, key] of byNote) key.dataset.on = n === on ? 'true' : 'false';
  });
  return root;
}

/* ------------------------------------------------------------------- phone */

const TABS = [
  { label: 'OSC', build: (): HTMLElement[] => [oscPanel(1), oscPanel(2), subPanel()] },
  { label: 'FILTER', build: (): HTMLElement[] => [vowelPanel(), envelopePanel()] },
  { label: 'WOBBLE', build: (): HTMLElement[] => [wobblePanel()] },
  { label: 'MOD', build: (): HTMLElement[] => [modPanel()] },
  {
    label: 'FX',
    build: (): HTMLElement[] => {
      const rack = fxRack();
      return [rack.nav, rack.pages];
    },
  },
] as const;

/**
 * The phone layout: one section at a time behind tabs, controls at finger
 * size, the scope and a keyboard always in reach. Upright, everything stacks;
 * sideways, the scope and keys take the left and the section the right.
 */
function phone(): HTMLElement {
  const pages = TABS.map((tab) => el('div', 'm__page', ...tab.build()));
  const tabs = el('nav', 'm__tabs chips');
  const buttons = TABS.map((tab, i) => {
    const b = chipButton(tab.label);
    b.addEventListener('click', () => select(i));
    tabs.append(b);
    return b;
  });
  const content = el('div', 'm__pages', ...pages);
  const select = (i: number): void => {
    pages.forEach((p, k) => (p.hidden = k !== i));
    buttons.forEach((b, k) => (b.dataset.on = k === i ? 'true' : 'false'));
    content.scrollTop = 0;
    // A page that was hidden has zero width; size its displays now.
    requestAnimationFrame(() => fit());
  };
  select(0);

  return el(
    'div',
    'm',
    el(
      'div',
      'm__side',
      el('header', 'm__top', logoCanvas('m__logo'), el('span', 'brand__word', 'GNARL'), el('div', 'top__spacer'), licenceChip(),
        ...[tempoChip()].filter((x): x is HTMLElement => x !== null), masterKnob()),
      el('div', 'm__preset', presetPicker(), aiButton('AI')),
      scope(320, 64),
      el('div', 'play', wheels(), keyboard()),
    ),
    el('div', 'm__main', tabs, content),
  );
}

/* ------------------------------------------------------------- web engine */

/*
 * The web build carries GNARL's engine (web/host.ts). A phone only lets a
 * page make sound from a tap, so the engine starts from this one. Until then
 * - or if this browser cannot run it - the page is the preview it always was.
 */
function webStart(): HTMLElement {
  const problem = webEngineProblem();
  const button = chipButton(problem ? 'PREVIEW ONLY' : 'TAP TO PLAY');
  button.classList.add('webstart__button');
  const note = el('p', 'webstart__note', problem ?? 'The real GNARL engine, running in this browser.');
  // GPLv3: whoever is handed this page is owed the source, so it says where.
  const source = el('a', 'webstart__source', 'Free software (GPLv3) - source code');
  source.href = 'https://github.com/Vicevinnybeats/GNARL';
  source.target = '_blank';
  source.rel = 'noreferrer';
  const root = el('div', 'webstart', logoCanvas('webstart__logo'), button, note, source);
  button.addEventListener('click', () => {
    if (problem) {
      root.remove();
      return;
    }
    button.disabled = true;
    button.textContent = 'STARTING';
    startWebEngine()
      .then(() => connect())
      .then(() => {
        // The page opens on a wobble: the first starting sound, Riddim
        // Wobble, designed against measured reference drops (docs/design/
        // phase3-02-isolate.md). INIT in the preset sheet is still Vital's
        // plain saw, which the plugin opens on.
        const opening = FACTORY_SOUNDS[0];
        if (opening) loadFactory(opening);
        for (const v of presetViews) v();
        root.remove();
      })
      .catch((error: unknown) => {
        root.remove();
        toast(`The engine could not start: ${String(error)}. This is the preview.`);
      });
  });
  return root;
}

/* ------------------------------------------------------------------- toast */

let toastTimer = 0;
function toast(message: string): void {
  const node = document.querySelector<HTMLElement>('.toast');
  if (!node) return;
  node.textContent = message;
  node.dataset.show = 'true';
  window.clearTimeout(toastTimer);
  toastTimer = window.setTimeout(() => (node.dataset.show = 'false'), 2600);
}

/* -------------------------------------------------------------------- fit */

let desktopApp: HTMLElement | null = null;
let stage: HTMLElement | null = null;

/** Phone layout when the window is phone-sized in either orientation. */
function isPhone(): boolean {
  const w = window.innerWidth;
  const h = window.innerHeight;
  return w < 760 || (h < 500 && w < 1000);
}

function fit(): void {
  const phoneMode = isPhone();
  document.body.dataset.mode = phoneMode ? 'phone' : 'desktop';
  document.body.dataset.orient = window.innerHeight > window.innerWidth ? 'portrait' : 'landscape';

  let scale = 1;
  if (!phoneMode && desktopApp && stage) {
    scale = Math.min(window.innerWidth / DESIGN_W, window.innerHeight / DESIGN_H);
    desktopApp.style.transform = `scale(${scale})`;
    desktopApp.dataset.scale = `${scale}`;
    stage.style.width = `${DESIGN_W * scale}px`;
    stage.style.height = `${DESIGN_H * scale}px`;
  }
  for (const { d } of displays) if (isShown(d.root)) resizeDisplay(d, phoneMode ? 1 : scale);
  for (const { canvas } of logos) {
    if (!isShown(canvas)) continue;
    const density = Math.min(3, (window.devicePixelRatio || 1) * (phoneMode ? 1 : scale));
    canvas.width = Math.round(canvas.clientWidth * density);
    canvas.height = Math.round(canvas.clientHeight * density);
  }
}

function isShown(node: HTMLElement): boolean {
  return node.offsetParent !== null;
}

/* ------------------------------------------------------------------ start */

const t0 = performance.now();
function clock(): number {
  return (performance.now() - t0) / 1000;
}

// Computer keys for the desktop preview: A W S E D F T G Y H U J K = C2..C3.
const COMPUTER_KEYS = 'awsedftgyhujk';

function start(): void {
  stage = el('div', 'stage');
  desktopApp = desktop();
  stage.append(desktopApp);
  document.body.append(stage, phone(), el('div', 'toast'));

  fit();
  window.addEventListener('resize', fit);
  void connect().then(() => {
    for (const v of presetViews) v();
  });
  if (hasWebEngine()) document.body.append(webStart());

  // Double-click the wordmark to reset the whole panel.
  // Not in the plugin: the page's defaults are not a patch, and the engine
  // would take every one of them.
  desktopApp.querySelector('.brand')?.addEventListener('dblclick', () => !engine.connected && resetAll());

  window.addEventListener('keydown', (e) => {
    const i = COMPUTER_KEYS.indexOf(e.key.toLowerCase());
    if (i < 0 || e.repeat || e.metaKey || e.ctrlKey) return;
    noteOn(36 + i, clock());
    for (const v of keyViews) v();
  });
  window.addEventListener('keyup', (e) => {
    const i = COMPUTER_KEYS.indexOf(e.key.toLowerCase());
    if (i < 0) return;
    noteOff(36 + i, clock());
    for (const v of keyViews) v();
  });

  const frame = (): void => {
    const t = clock();
    // Only what is on screen: the hidden layout costs nothing per frame.
    for (const { d, paint } of displays) if (isShown(d.root)) paint(t);
    for (const { canvas, paint } of logos) if (isShown(canvas)) paint(t);
    requestAnimationFrame(frame);
  };
  requestAnimationFrame(frame);
}

start();
