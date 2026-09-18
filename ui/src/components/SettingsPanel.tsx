import { useCallback, useEffect, useRef } from 'react';

import { Toggle } from './Toggle';
import { getPluginInfo } from '../bridge/pluginInfo';
import {
  MAX_KNOB_DRAG_PX,
  MIN_KNOB_DRAG_PX,
  THEMES,
  resetSettings,
  updateSettings,
  useSettings,
  type Theme,
} from '../settings';
import './SettingsPanel.css';

/**
 * The settings popover.
 *
 * WHAT IS IN HERE AND WHAT IS NOT. Everything that changes how the instrument
 * SOUNDS is a host parameter and lives on a panel with the rest of the sound -
 * voice count, glide, oversampling and the master fader are all in the header
 * already, because they are automatable and belong in a preset. What is left
 * for a settings menu is the things that belong to the person rather than to
 * the patch, and there are not many of them. A settings menu that fills up is
 * usually a sign that controls are hiding from the panel they belong on.
 *
 * NO UI SCALE CONTROL, deliberately. The editor is already resizable with a
 * fixed aspect ratio (setResizable in WebUIEditor.cpp), so dragging the window
 * corner is the scale control and a second one would fight it. Faking scale
 * with a CSS transform would also break the exact vertical budget every tab's
 * row heights are built on.
 */

const THEME_LABELS: Record<Theme, string> = {
  acid: 'Acid',
  ember: 'Ember',
  dream: 'Dream',
};

function GearIcon() {
  return (
    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" aria-hidden>
      <circle cx="12" cy="12" r="3.2" />
      <path d="M12 2.4v3M12 18.6v3M2.4 12h3M18.6 12h3M5.2 5.2l2.1 2.1M16.7 16.7l2.1 2.1M18.8 5.2l-2.1 2.1M7.3 16.7l-2.1 2.1" />
    </svg>
  );
}

/** A labelled row, so every setting lines up on the same grid. */
function SettingRow({
  label, hint, children,
}: { label: string; hint: string; children: React.ReactNode }) {
  return (
    <div className="gn-settings__row">
      <div className="gn-settings__text">
        <span className="gn-settings__label">{label}</span>
        <span className="gn-settings__hint">{hint}</span>
      </div>
      <div className="gn-settings__control">{children}</div>
    </div>
  );
}

export function SettingsPanel({ open, onClose }: { open: boolean; onClose: () => void }) {
  const settings = useSettings();
  const info = getPluginInfo();
  const panelRef = useRef<HTMLDivElement>(null);

  /*  Escape closes, and a click anywhere else closes. Both matter more here
      than in a web page: this floats over an instrument someone is playing,
      and a panel you have to aim at a small X to dismiss is a panel in the
      way. The listeners only exist while it is open. */
  useEffect(() => {
    if (!open) return undefined;

    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === 'Escape') onClose();
    };

    const onPointerDown = (event: PointerEvent) => {
      const panel = panelRef.current;
      if (!panel) return;

      // The button that opened it is outside the panel, so it has to be
      // excluded explicitly or the click that closes would immediately reopen.
      const target = event.target as Node | null;
      if (target && !panel.contains(target) && !(target as Element).closest?.('.gn-settings__button')) {
        onClose();
      }
    };

    document.addEventListener('keydown', onKeyDown);
    document.addEventListener('pointerdown', onPointerDown);

    return () => {
      document.removeEventListener('keydown', onKeyDown);
      document.removeEventListener('pointerdown', onPointerDown);
    };
  }, [open, onClose]);

  const setDrag = useCallback((event: React.ChangeEvent<HTMLInputElement>) => {
    updateSettings({ knobDragPx: Number(event.target.value) });
  }, []);

  if (!open) return null;

  return (
    <div className="gn-settings__panel" ref={panelRef} role="dialog" aria-label="Settings">
      <header className="gn-settings__head">
        <span className="gn-settings__title">Settings</span>
        <button
          className="gn-settings__reset"
          type="button"
          onClick={resetSettings}
          title="Back to defaults, including the system's reduced-motion preference"
        >
          Reset
        </button>
      </header>

      <SettingRow label="Theme" hint="Acid and Ember are single-accent; Dream is the two-tone one.">
        <div className="gn-settings__segmented">
          {THEMES.map((name) => (
            <button
              key={name}
              type="button"
              data-active={settings.theme === name}
              onClick={() => updateSettings({ theme: name })}
            >
              {THEME_LABELS[name]}
            </button>
          ))}
        </div>
      </SettingRow>

      <SettingRow
        label="Animations"
        hint="Transitions on hover and selection. Off is instant, not stepped."
      >
        <Toggle
          value={settings.motion}
          onChange={(value) => updateSettings({ motion: value })}
          compact
        />
      </SettingRow>

      <SettingRow
        label="Hover glow"
        hint="The glow under the pointer. Glow that marks what is ON stays either way."
      >
        <Toggle
          value={settings.glow}
          onChange={(value) => updateSettings({ glow: value })}
          compact
        />
      </SettingRow>

      <SettingRow
        label="Help text"
        hint="The one-line explanations under a panel's controls."
      >
        <Toggle
          value={settings.hints}
          onChange={(value) => updateSettings({ hints: value })}
          compact
        />
      </SettingRow>

      <SettingRow
        label="Knob travel"
        hint="Drag distance for a full sweep. Shift is fine, Ctrl finer, whatever this is."
      >
        <div className="gn-settings__slider">
          <input
            type="range"
            min={MIN_KNOB_DRAG_PX}
            max={MAX_KNOB_DRAG_PX}
            step={10}
            value={settings.knobDragPx}
            onChange={setDrag}
            aria-label="Knob travel in pixels"
          />
          <span className="gn-settings__value">{settings.knobDragPx} px</span>
        </div>
      </SettingRow>

      <footer className="gn-settings__foot">
        {/* The version is only shown when the plugin actually reported one.
            In the browser preview it comes back as the literal string
            "unknown", and "GNARL unknown" reads like a failure rather than
            like a preview. */}
        <span>{info.isMock ? 'GNARL' : `GNARL ${info.pluginVersion}`}</span>
        <span>
          {info.isMock ? 'browser preview' : `engine connected · state v${info.stateVersion}`}
        </span>
      </footer>
    </div>
  );
}

export { GearIcon };
