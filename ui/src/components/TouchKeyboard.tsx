import { useCallback, useEffect, useRef, useState } from 'react';

import { getAudioPreview } from '../bridge/audioPreview';

import './TouchKeyboard.css';

/**
 * A keyboard for the browser and the installed app.
 *
 * NOT SHOWN IN THE PLUGIN. A DAW already sends MIDI and the host's own
 * keyboard is better than this one; the 58px it costs belongs to the panels
 * there. `getAudioPreview()` returns null when a real engine is behind the
 * page, which is the same signal the rest of the bridge uses.
 *
 * TWO OCTAVES FROM D#1, because that is where this instrument is played. The
 * client answered "D#3 or D#2" when asked where the bank sits in a piano
 * roll, and FL Studio names middle C two octaves above the convention the
 * rest of the world uses — so those are MIDI 27 and 39, and a keyboard
 * starting at middle C would put every factory patch two octaves above
 * anything it contains.
 *
 * POINTER EVENTS, NOT TOUCH OR MOUSE. One code path covers a finger, a
 * stylus and a mouse, and `setPointerCapture` means a drag that starts on one
 * key and slides onto another is a glissando rather than a stuck note — which
 * is what happens if you listen for pointerleave instead.
 */

const START_NOTE = 27;          // D#1 in C4=60 naming; D#2 as FL labels it.
const WHITE_PER_OCTAVE = 7;
const OCTAVES = 2;

//  Semitone offsets of the white keys within an octave, and of the black keys
//  with the white index they sit after.
const WHITE = [0, 2, 4, 5, 7, 9, 11];
const BLACK: Array<[number, number]> = [
  [1, 0], [3, 1], [6, 3], [8, 4], [10, 5],
];

export function TouchKeyboard() {
  const preview = getAudioPreview();
  const [held, setHeld] = useState<ReadonlySet<number>>(new Set());
  const heldRef = useRef(held);
  heldRef.current = held;

  const press = useCallback(
    (note: number) => {
      if (heldRef.current.has(note)) return;
      preview?.noteOn(note, 1);
      setHeld((prev) => new Set(prev).add(note));
    },
    [preview],
  );

  const release = useCallback(
    (note: number) => {
      preview?.noteOff(note);
      setHeld((prev) => {
        const next = new Set(prev);
        next.delete(note);
        return next;
      });
    },
    [preview],
  );

  //  A computer keyboard too, for the browser on a desktop.
  useEffect(() => {
    if (!preview) return undefined;

    const MAP: Record<string, number> = {
      KeyA: 0, KeyW: 1, KeyS: 2, KeyE: 3, KeyD: 4, KeyF: 5, KeyT: 6,
      KeyG: 7, KeyY: 8, KeyH: 9, KeyU: 10, KeyJ: 11, KeyK: 12,
    };

    const down = (e: KeyboardEvent) => {
      //  Ignore auto-repeat and anything typed into a field.
      if (e.repeat) return;
      if (e.target instanceof HTMLInputElement || e.target instanceof HTMLTextAreaElement) return;

      const offset = MAP[e.code];
      if (offset !== undefined) press(START_NOTE + 12 + offset);
    };

    const up = (e: KeyboardEvent) => {
      const offset = MAP[e.code];
      if (offset !== undefined) release(START_NOTE + 12 + offset);
    };

    window.addEventListener('keydown', down);
    window.addEventListener('keyup', up);

    //  A note held when the window loses focus never gets its keyup.
    const blur = () => preview.allNotesOff();
    window.addEventListener('blur', blur);

    return () => {
      window.removeEventListener('keydown', down);
      window.removeEventListener('keyup', up);
      window.removeEventListener('blur', blur);
    };
  }, [preview, press, release]);

  /*  ABOVE THE EARLY RETURN. A hook after `if (!preview) return null` is
      called on some renders and not others, which changes the hook ORDER
      and is undefined behaviour in React - the lint rule that caught it is
      not a style preference. It was only ever going to misbehave on the
      render where the preview appears or disappears, which is exactly the
      kind of bug that survives testing. */
  const sounding = useRef(new Map<number, number>());

  if (!preview) return null;

  const whites: number[] = [];
  const blacks: Array<{ note: number; after: number }> = [];

  for (let octave = 0; octave < OCTAVES; octave++) {
    for (const semi of WHITE) whites.push(START_NOTE + octave * 12 + semi);
    for (const [semi, after] of BLACK) {
      blacks.push({ note: START_NOTE + octave * 12 + semi, after: octave * WHITE_PER_OCTAVE + after });
    }
  }

  /*  SLIDING ACROSS THE KEYS, which is how anybody plays a keyboard on glass
   *  and which `setPointerCapture` made impossible. Capturing binds every
   *  later event for that finger to the key it STARTED on, so moving to the
   *  next key delivered its events to the old one and the note never
   *  changed. The capture was there to stop a drag that leaves the keyboard
   *  getting stuck on - which is a real problem, solved below by tracking
   *  the pointer instead of pinning it.
   *
   *  Which key is under the finger is asked of the DOM rather than computed
   *  from geometry: the black keys overlap the white ones and sit above them
   *  in the stacking order, so hit-testing reproduces that for free, where
   *  arithmetic over key widths would have to special-case every black key's
   *  offset and would drift the moment the layout changed. */

  const noteUnder = (x: number, y: number): number | null => {
    const el = document.elementFromPoint(x, y);
    const attr = el instanceof HTMLElement ? el.dataset.note : undefined;

    return attr === undefined ? null : Number(attr);
  };

  const moveTo = (pointerId: number, x: number, y: number) => {
    const next = noteUnder(x, y);
    const current = sounding.current.get(pointerId);

    if (next === current || (next === null && current === undefined)) return;

    //  Release first, so a slide is legato rather than briefly two notes.
    if (current !== undefined) release(current);

    if (next === null) sounding.current.delete(pointerId);
    else {
      sounding.current.set(pointerId, next);
      press(next);
    }
  };

  const lift = (pointerId: number) => {
    const note = sounding.current.get(pointerId);

    if (note !== undefined) {
      release(note);
      sounding.current.delete(pointerId);
    }
  };

  /*  Bound on the CONTAINER, not per key: one listener that owns the whole
      gesture, which is also what makes a finger leaving the bottom edge
      release properly rather than sticking. */
  const surface = {
    onPointerDown: (e: React.PointerEvent<HTMLDivElement>) => {
      e.preventDefault();
      moveTo(e.pointerId, e.clientX, e.clientY);
    },
    onPointerMove: (e: React.PointerEvent<HTMLDivElement>) => {
      //  Only while a finger is down. `buttons` is 0 for a hover.
      if (e.buttons === 0 && !sounding.current.has(e.pointerId)) return;

      moveTo(e.pointerId, e.clientX, e.clientY);
    },
    onPointerUp: (e: React.PointerEvent<HTMLDivElement>) => lift(e.pointerId),
    onPointerCancel: (e: React.PointerEvent<HTMLDivElement>) => lift(e.pointerId),
    onPointerLeave: (e: React.PointerEvent<HTMLDivElement>) => lift(e.pointerId),
  };

  const bind = (note: number) => ({ 'data-note': note });

  return (
    <div className="gn-keys" role="group" aria-label="Preview keyboard" {...surface}>
      <div className="gn-keys__row">
        {whites.map((note) => (
          <button
            key={note}
            type="button"
            className={'gn-keys__white' + (held.has(note) ? ' gn-keys__key--on' : '')}
            aria-label={`Note ${note}`}
            {...bind(note)}
          />
        ))}

        {blacks.map(({ note, after }) => (
          <button
            key={note}
            type="button"
            className={'gn-keys__black' + (held.has(note) ? ' gn-keys__key--on' : '')}
            style={{ left: `calc(${((after + 1) * 100) / whites.length}% - var(--gn-key-black-half))` }}
            aria-label={`Note ${note}`}
            {...bind(note)}
          />
        ))}
      </div>
    </div>
  );
}
