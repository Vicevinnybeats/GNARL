import { useEffect, useState } from 'react';

import { playGrowl } from '../bridge/growl';
import { useSettings } from '../settings';

import './Splash.css';

/** How long the whole thing is up. Short on purpose: a splash that outlasts
 *  the window opening is one people learn to dread, and this one is seen
 *  every time a project loads. */
const DURATION_MS = 900;

/**
 * The opening animation.
 *
 * Covers the interface while it mounts, then lifts. Which is also the honest
 * reason to have one: the first frame of a webview UI inside a DAW is not
 * instant, and a brief deliberate cover reads better than a half-drawn
 * window.
 *
 * GATED ON THE MOTION SETTING, like everything else that moves (CLAUDE.md
 * §6) — and this is the most motion the interface ever makes, so somebody
 * with prefers-reduced-motion set gets no splash at all rather than a
 * shorter one. The growl is a separate setting and defaults OFF: a plugin
 * that makes a noise every time its window opens is one somebody eventually
 * opens forty times in a row.
 */
export function Splash() {
  const settings = useSettings();

  // Decided once, on mount. Toggling the setting mid-session must not make a
  // splash appear over a window that has been open for an hour.
  const [visible, setVisible] = useState(() => settings.splash);
  const [leaving, setLeaving] = useState(false);

  useEffect(() => {
    if (!visible) return;

    if (settings.splashSound) void playGrowl();

    const lift = window.setTimeout(() => setLeaving(true), DURATION_MS - 260);
    const done = window.setTimeout(() => setVisible(false), DURATION_MS);

    return () => {
      window.clearTimeout(lift);
      window.clearTimeout(done);
    };
    // Mount only: see the note on `visible` above.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  if (!visible) return null;

  return (
    <div className="gn-splash" data-leaving={leaving} aria-hidden="true">
      <div className="gn-splash__mark">
        <span className="gn-splash__word">GNARL</span>

        {/*  The bars sweep like a formant opening and closing - the same
             gesture the instrument is built on, rather than a generic
             loading animation. */}
        <div className="gn-splash__bars">
          {Array.from({ length: 11 }, (_unused, i) => (
            <span
              className="gn-splash__bar"
              key={i}
              style={{ animationDelay: `${i * 34}ms` }}
            />
          ))}
        </div>
      </div>
    </div>
  );
}
