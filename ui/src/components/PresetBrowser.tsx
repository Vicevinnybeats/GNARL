import { useCallback, useEffect, useMemo, useRef, useState } from 'react';

import {
  DEFAULT_RANDOMISE,
  deletePreset,
  listPresets,
  loadPreset,
  morphPresets,
  randomise,
  savePreset,
  type PresetRow,
  type RandomiseOptions,
} from '../bridge/presets';
import './PresetBrowser.css';

/**
 * The preset browser.
 *
 * A PANEL, NOT A MODAL, and it closes on Escape or a click outside like the
 * settings popover does. A browser you have to dismiss with a button is a
 * browser that interrupts; auditioning a bank means clicking down it, and
 * every extra gesture between rows is one the user pays per preset.
 *
 * THE LIST IS RE-READ ON EVERY OPEN, because a producer who drops a .gnarl
 * file into the folder expects it to be there - and the alternative, watching
 * the directory, is a file watcher running for the life of the plugin to save
 * a directory scan that takes a few milliseconds when the panel opens.
 *
 * MORPH NEEDS TWO ROWS, so it is a mode rather than a button: pick A, pick B,
 * then the slider is live. Making it a button would mean guessing which two
 * presets were meant, and guessing wrong throws away the patch.
 */

interface Props {
  open: boolean;
  onClose: () => void;
  /** Called after anything that changes the patch, so the header can re-read
      the current name. */
  onPatchChanged: () => void;
  /*  EMBEDDED IN A TAB rather than hanging off the header. One component,
      two homes: the popover for a quick jump between two patches without
      leaving the panel you are working in, and the LIBRARY tab for actually
      auditioning a bank. A second implementation would be two things to keep
      in step and two places for a bug to hide.

      It changes three things and no behaviour: no absolute positioning, no
      close button, and Escape does not dismiss - navigating away is the
      dismiss, and swallowing Escape in a tab would break the browser's own
      search field, where Escape means "clear what I typed". */
  embedded?: boolean;
}

const ALL_CATEGORIES = 'All';

/*  SOURCE IS A SEPARATE AXIS FROM CATEGORY, not another entry in the same
    list. "Growl" and "Factory" answer different questions - what the patch
    sounds like, and where it came from - and folding them into one row of
    buttons means you cannot ask both at once, which is exactly what somebody
    hunting their own bass in a bank of 150 wants to do. */
type Source = 'all' | 'factory' | 'user';

const SOURCES: ReadonlyArray<{ id: Source; label: string; title: string }> = [
  { id: 'all', label: 'ALL', title: 'Every preset' },
  { id: 'factory', label: 'FACTORY', title: 'The 150 presets that ship with GNARL' },
  { id: 'user', label: 'USER', title: 'Presets you saved' },
];

export function PresetBrowser({ open, onClose, onPatchChanged, embedded = false }: Props) {
  const [rows, setRows] = useState<PresetRow[]>([]);
  const [categories, setCategories] = useState<string[]>([]);
  const [category, setCategory] = useState(ALL_CATEGORIES);
  const [source, setSource] = useState<Source>('all');
  const [search, setSearch] = useState('');
  const [selected, setSelected] = useState<number | null>(null);
  const [status, setStatus] = useState('');

  const [morphA, setMorphA] = useState<number | null>(null);
  const [morphB, setMorphB] = useState<number | null>(null);
  const [morphPosition, setMorphPosition] = useState(0.5);

  const [randomOptions, setRandomOptions] =
    useState<RandomiseOptions>(DEFAULT_RANDOMISE);

  const [saveName, setSaveName] = useState('');
  const [saveCategory, setSaveCategory] = useState('Bass');
  const [saveTags, setSaveTags] = useState('');

  const panelRef = useRef<HTMLDivElement>(null);

  const refresh = useCallback(async () => {
    const listing = await listPresets();
    setRows(listing.presets);
    setCategories(listing.categories);
  }, []);

  useEffect(() => {
    if (!open) return;
    void refresh();
  }, [open, refresh]);

  /*  Escape and click-outside, as the settings popover has. Listeners only
      exist while the panel is open - and never when embedded in a tab, which
      has nothing to dismiss to. Registering them there would also make
      Escape in the search field close nothing and clear nothing, which is
      worse than either. */
  useEffect(() => {
    if (!open || embedded) return undefined;

    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === 'Escape') onClose();
    };

    const onPointerDown = (event: PointerEvent) => {
      const panel = panelRef.current;
      const target = event.target as Element | null;

      if (
        panel &&
        target &&
        !panel.contains(target) &&
        !target.closest?.('.gn-preset')
      ) {
        onClose();
      }
    };

    document.addEventListener('keydown', onKeyDown);
    document.addEventListener('pointerdown', onPointerDown);

    return () => {
      document.removeEventListener('keydown', onKeyDown);
      document.removeEventListener('pointerdown', onPointerDown);
    };
  }, [open, onClose, embedded]);

  const visible = useMemo(() => {
    const needle = search.trim().toLowerCase();

    return rows.filter((row) => {
      if (source === 'factory' && !row.factory) return false;
      if (source === 'user' && row.factory) return false;
      if (category !== ALL_CATEGORIES && row.category !== category) return false;
      if (needle.length === 0) return true;

      // Name, tags and description all searched: "growl" should find the
      // patch called Triplet Growl AND the one merely tagged growl.
      return (
        row.name.toLowerCase().includes(needle) ||
        row.category.toLowerCase().includes(needle) ||
        row.description.toLowerCase().includes(needle) ||
        row.tags.some((tag) => tag.toLowerCase().includes(needle))
      );
    });
  }, [rows, category, search, source]);

  const load = useCallback(
    async (index: number) => {
      setSelected(index);

      const ok = await loadPreset(index);
      setStatus(ok ? '' : 'That preset would not load.');

      if (ok) onPatchChanged();
    },
    [onPatchChanged],
  );

  const doSave = useCallback(async () => {
    if (saveName.trim().length === 0) {
      setStatus('Give it a name first.');
      return;
    }

    const ok = await savePreset({
      name: saveName,
      author: '',
      category: saveCategory,
      description: '',
      tags: saveTags,
    });

    setStatus(ok ? `Saved "${saveName}".` : 'Could not save that.');

    if (ok) {
      setSaveName('');
      setSaveTags('');
      await refresh();
      onPatchChanged();
    }
  }, [saveName, saveCategory, saveTags, refresh, onPatchChanged]);

  const doDelete = useCallback(async () => {
    if (selected === null) return;

    const row = rows.find((entry) => entry.index === selected);

    if (!row || row.factory) {
      setStatus('Factory presets cannot be deleted.');
      return;
    }

    const ok = await deletePreset(selected);
    setStatus(ok ? `Deleted "${row.name}".` : 'Could not delete that.');

    if (ok) {
      setSelected(null);
      await refresh();
    }
  }, [selected, rows, refresh]);

  const doRandomise = useCallback(async () => {
    await randomise(randomOptions);
    setStatus('Randomised.');
    onPatchChanged();
  }, [randomOptions, onPatchChanged]);

  const applyMorph = useCallback(
    async (position: number) => {
      if (morphA === null || morphB === null) return;

      setMorphPosition(position);
      await morphPresets(morphA, morphB, position);
      onPatchChanged();
    },
    [morphA, morphB, onPatchChanged],
  );

  if (!open) return null;

  const nameOf = (index: number | null) =>
    index === null ? '—' : (rows.find((row) => row.index === index)?.name ?? '—');

  return (
    <div
      className={'gn-browser' + (embedded ? ' gn-browser--embedded' : '')}
      ref={panelRef}
      /*  A tab is not a dialog. role="dialog" tells a screen reader that
          everything behind it is inert and that Escape returns you - both
          untrue here, and the second is actively wrong now that Escape is
          not handled. */
      role={embedded ? 'region' : 'dialog'}
      aria-label="Presets"
    >
      <header className="gn-browser__head">
        <input
          className="gn-browser__search"
          type="search"
          placeholder="Search name, tag or category"
          value={search}
          onChange={(event) => setSearch(event.target.value)}
          aria-label="Search presets"
        />

        <select
          className="gn-browser__category"
          value={category}
          onChange={(event) => setCategory(event.target.value)}
          aria-label="Category"
        >
          <option value={ALL_CATEGORIES}>{ALL_CATEGORIES}</option>
          {categories.map((name) => (
            <option key={name} value={name}>
              {name}
            </option>
          ))}
        </select>

        {/*  Segmented, not a dropdown. Three options that the user switches
             between constantly while hunting deserve one click each; a select
             costs two and hides the other two behind a menu. It also makes
             the count visible - "FACTORY 150" answers "did my presets load?"
             without anybody having to open anything. */}
        <div className="gn-browser__source" role="group" aria-label="Preset source">
          {SOURCES.map(({ id, label, title }) => {
            const count =
              id === 'all'
                ? rows.length
                : rows.filter((row) => (id === 'factory' ? row.factory : !row.factory)).length;

            return (
              <button
                key={id}
                type="button"
                className={
                  'gn-browser__source-btn' + (source === id ? ' gn-browser__source-btn--on' : '')
                }
                onClick={() => setSource(id)}
                aria-pressed={source === id}
                title={title}
              >
                {label} <span className="gn-browser__source-count">{count}</span>
              </button>
            );
          })}
        </div>
      </header>

      <div className="gn-browser__body">
        <div className="gn-browser__list" role="listbox" aria-label="Presets">
          {visible.length === 0 && (
            <p className="gn-browser__empty">
              Nothing matches. {rows.length} preset{rows.length === 1 ? '' : 's'} in
              the bank.
            </p>
          )}

          {visible.map((row) => (
            <button
              key={row.index}
              className="gn-browser__row"
              type="button"
              role="option"
              aria-selected={row.index === selected}
              data-selected={row.index === selected}
              data-factory={row.factory}
              onClick={() => void load(row.index)}
              title={row.description}
            >
              <span className="gn-browser__name">{row.name}</span>
              <span className="gn-browser__meta">{row.category}</span>

              {/* Marking A and B on the row itself, so a morph is visible in
                  the list rather than only in the panel below it. */}
              {row.index === morphA && <span className="gn-browser__mark">A</span>}
              {row.index === morphB && <span className="gn-browser__mark">B</span>}
            </button>
          ))}
        </div>

        <aside className="gn-browser__side">
          <section className="gn-browser__section">
            <h3>Save</h3>
            <input
              type="text"
              placeholder="Name"
              value={saveName}
              onChange={(event) => setSaveName(event.target.value)}
              aria-label="Preset name"
            />
            <select
              value={saveCategory}
              onChange={(event) => setSaveCategory(event.target.value)}
              aria-label="Preset category"
            >
              {categories.map((name) => (
                <option key={name} value={name}>
                  {name}
                </option>
              ))}
            </select>
            <input
              type="text"
              placeholder="Tags, comma separated"
              value={saveTags}
              onChange={(event) => setSaveTags(event.target.value)}
              aria-label="Preset tags"
            />
            <div className="gn-browser__buttons">
              <button type="button" onClick={() => void doSave()}>
                Save
              </button>
              <button type="button" onClick={() => void doDelete()}>
                Delete
              </button>
            </div>
          </section>

          <section className="gn-browser__section">
            <h3>Randomise</h3>

            {/* An amount, not a button. See bridge/presets.ts. */}
            <label className="gn-browser__slider">
              <span>Amount</span>
              <input
                type="range"
                min={0}
                max={100}
                value={Math.round(randomOptions.amount * 100)}
                onChange={(event) =>
                  setRandomOptions({
                    ...randomOptions,
                    amount: Number(event.target.value) / 100,
                  })
                }
                aria-label="Randomise amount"
              />
              <span className="gn-browser__value">
                {Math.round(randomOptions.amount * 100)}%
              </span>
            </label>

            <div className="gn-browser__sections">
              {(
                [
                  ['oscillators', 'Osc'],
                  ['filters', 'Filter'],
                  ['envelopes', 'Env'],
                  ['lfos', 'LFO'],
                  ['modMatrix', 'Mod'],
                  ['fx', 'FX'],
                ] as const
              ).map(([key, label]) => (
                <button
                  key={key}
                  type="button"
                  className="gn-browser__chip"
                  data-on={randomOptions[key]}
                  onClick={() =>
                    setRandomOptions({ ...randomOptions, [key]: !randomOptions[key] })
                  }
                >
                  {label}
                </button>
              ))}
            </div>

            <button type="button" onClick={() => void doRandomise()}>
              Randomise
            </button>
          </section>

          <section className="gn-browser__section">
            <h3>Morph</h3>
            <p className="gn-browser__note">
              Pick two, then sweep. Curves and switches step at the half-way
              point.
            </p>

            <div className="gn-browser__buttons">
              <button
                type="button"
                onClick={() => setMorphA(selected)}
                disabled={selected === null}
              >
                A: {nameOf(morphA)}
              </button>
              <button
                type="button"
                onClick={() => setMorphB(selected)}
                disabled={selected === null}
              >
                B: {nameOf(morphB)}
              </button>
            </div>

            <label className="gn-browser__slider">
              <span>Morph</span>
              <input
                type="range"
                min={0}
                max={100}
                value={Math.round(morphPosition * 100)}
                disabled={morphA === null || morphB === null}
                onChange={(event) =>
                  void applyMorph(Number(event.target.value) / 100)
                }
                aria-label="Morph position"
              />
              <span className="gn-browser__value">
                {Math.round(morphPosition * 100)}%
              </span>
            </label>
          </section>

          {status && <p className="gn-browser__status">{status}</p>}
        </aside>
      </div>
    </div>
  );
}
