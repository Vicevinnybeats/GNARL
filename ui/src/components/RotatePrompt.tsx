import './RotatePrompt.css';

/**
 * Asks for landscape on a small screen held upright.
 *
 * NOT A REFLOW, DELIBERATELY. This interface is built on exact pixel budgets —
 * 720 less the header, status bar and padding leaves 624, and every row inside
 * is an explicit height (CLAUDE.md §6). It scales to the window as one piece
 * and never reflows, because turning those rows into proportions is the thing
 * that has broken this layout four separate times.
 *
 * At 390px wide that uniform scale is 0.33, and a 10px label becomes 3px. The
 * honest options are a second layout built from scratch for a phone, or asking
 * for the orientation the existing one fits. Every hardware synth and most
 * phone DAWs ask; a tiny unreadable copy of a desktop interface helps nobody.
 *
 * SHOWN BY CSS, NOT BY JAVASCRIPT. A media query on orientation is evaluated
 * before first paint and re-evaluated by the browser as the device turns, so
 * there is no flash of the wrong state and no resize listener racing the
 * rotation animation. The component always renders; the stylesheet decides.
 *
 * The threshold is on HEIGHT, not on a device class. A phone in landscape is
 * about 390px tall and a tablet is 768 — so `max-height` catches the case that
 * actually matters (too little vertical room) without trying to guess what
 * kind of machine this is from a user-agent string.
 */
export function RotatePrompt() {
  return (
    <div className="gn-rotate" role="status" aria-live="polite">
      <div className="gn-rotate__inner">
        <svg className="gn-rotate__icon" viewBox="0 0 64 64" aria-hidden="true">
          <rect x="20" y="6" width="24" height="42" rx="3" />
          <path d="M14 52 a22 22 0 0 0 36 0" className="gn-rotate__arc" />
          <path d="M50 44 l2 9 l-9 -2" className="gn-rotate__head" />
        </svg>
        <p className="gn-rotate__title">TURN YOUR DEVICE</p>
        <p className="gn-rotate__body">
          GNARL is a wide instrument. Landscape gives it the room its panels need.
        </p>
      </div>
    </div>
  );
}
