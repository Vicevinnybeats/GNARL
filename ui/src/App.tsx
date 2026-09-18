import { useCallback, useEffect, useState } from 'react';

import { Dropdown } from './components/Dropdown';
import { Knob } from './components/Knob';
import { Meter } from './components/Meter';
import { FxTab } from './tabs/FxTab';
import { ModTab } from './tabs/ModTab';
import { OscTab } from './tabs/OscTab';
import { PlaceholderTab } from './tabs/PlaceholderTab';
import { OVERSAMPLING, POLY_MODE } from './bridge/choices';
import { GLOBAL } from './bridge/parameterIds';
import { getPluginInfo } from './bridge/pluginInfo';
import { useArtwork } from './bridge/artwork';
import { useParameter } from './bridge/useParameter';
import { useChoiceParameter } from './bridge/useDiscreteParameter';
import './App.css';

const TABS = ['OSC', 'MOD', 'FX', 'AI'] as const;
type Tab = (typeof TABS)[number];

/** Matches the theme blocks in styles/tokens.css. */
const THEMES = ['acid', 'ember', 'dream'] as const;
type Theme = (typeof THEMES)[number];

export function App() {
  const info = getPluginInfo();

  const [tab, setTab] = useState<Tab>('OSC');
  // Dream is the DEFAULT, on the client's direction: they asked for the dreamy
  // look as the instrument's face, not as a third option behind two clicks.
  // Acid and Ember stay available on the theme button.
  const [theme, setTheme] = useState<Theme>('dream');
  const [status, setStatus] = useState('');

  // Publishes the embedded background artwork to CSS, or leaves the procedural
  // gradient in place when the slot still holds its placeholder.
  useArtwork();

  const master = useParameter(GLOBAL.masterGain);
  const voices = useParameter(GLOBAL.maxVoices);
  const glide = useParameter(GLOBAL.glideTime);
  const polyMode = useChoiceParameter(GLOBAL.polyMode, POLY_MODE.length);
  const oversampling = useChoiceParameter(GLOBAL.oversampling, OVERSAMPLING.length);

  useEffect(() => {
    document.documentElement.dataset.theme = theme;
  }, [theme]);

  /**
   * Hovering any control writes a one-line description here. This is how a
   * dense synth stays learnable without hiding controls behind menus.
   */
  const describe = useCallback((text: string) => () => setStatus(text), []);
  const clearStatus = useCallback(() => setStatus(''), []);

  return (
    <div className="gn-app">
      <header className="gn-header">
        <div className="gn-logo">GNARL</div>

        <div className="gn-preset">
          <button className="gn-preset__arrow" type="button" aria-label="Previous preset">
            ‹
          </button>
          <button className="gn-preset__name" type="button">
            Init
          </button>
          <button className="gn-preset__arrow" type="button" aria-label="Next preset">
            ›
          </button>
        </div>

        <nav className="gn-tabs" role="tablist">
          {TABS.map((name) => (
            <button
              key={name}
              className="gn-tab"
              role="tab"
              aria-selected={name === tab}
              data-active={name === tab}
              type="button"
              onClick={() => setTab(name)}
            >
              {name}
            </button>
          ))}
        </nav>

        <div className="gn-header__right">
          <div
            className="gn-header__group"
            onMouseEnter={describe('Voice count, portamento time, and mono/legato behaviour.')}
            onMouseLeave={clearStatus}
          >
            <Knob label="Voices" value={voices.normalised} readout={voices.text} onChange={voices.setNormalised} onGestureStart={voices.beginGesture} onGestureEnd={voices.endGesture} size={28} />
            <Knob label="Glide" value={glide.normalised} readout={glide.text} onChange={glide.setNormalised} onGestureStart={glide.beginGesture} onGestureEnd={glide.endGesture} size={28} />
            <Dropdown value={polyMode.index} options={POLY_MODE} onChange={polyMode.setIndex} />
          </div>

          <div
            className="gn-header__group"
            onMouseEnter={describe('Oversampling. Higher settings reduce aliasing from the drive stages at the cost of CPU.')}
            onMouseLeave={clearStatus}
          >
            <Dropdown label="OS" value={oversampling.index} options={OVERSAMPLING} onChange={oversampling.setIndex} />
          </div>

          <Meter label="Out" value={0.0} />

          <div
            onMouseEnter={describe('Master output level. Sits after OTT, so riding it does not change how hard the compressor works.')}
            onMouseLeave={clearStatus}
          >
            <Knob label="Master" value={master.normalised} readout={master.text} onChange={master.setNormalised} onGestureStart={master.beginGesture} onGestureEnd={master.endGesture} size={32} />
          </div>

          <button
            className="gn-theme"
            type="button"
            aria-label="Switch theme"
            title={`Theme: ${theme}`}
            onClick={() =>
              setTheme(THEMES[(THEMES.indexOf(theme) + 1) % THEMES.length] ?? 'acid')
            }
          >
            <span className="gn-theme__swatch" />
          </button>
        </div>
      </header>

      <main className="gn-body">
        {tab === 'OSC' && <OscTab />}

        {tab === 'MOD' && <ModTab />}

        {tab === 'FX' && <FxTab />}

        {tab === 'AI' && (
          <PlaceholderTab
            title="AI"
            phase="Phase 8"
            summary="Server-side inference, called from a background thread. Nothing AI-related touches the audio thread, and if the backend is unreachable this panel disables itself while the rest of the plugin works normally."
            items={[
              'Text to patch, validated against the preset schema',
              'Patch doctor: sends a feature vector, never your audio',
              'Eight variations on a grid, half computed locally',
              'Plain-language preset search over the factory bank',
              'Every AI action undoable in one click',
              'Remaining quota shown in the UI',
            ]}
          />
        )}
      </main>

      <footer className="gn-statusbar">
        <span className="gn-statusbar__text">{status}</span>
        <span className="gn-statusbar__meta">
          {info.isMock ? 'browser preview · no audio engine' : `v${info.pluginVersion} · ${info.platform}`}
        </span>
      </footer>
    </div>
  );
}
