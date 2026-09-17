import { Knob } from './components/Knob';
import { SectionLabel } from './components/SectionLabel';
import { GLOBAL } from './bridge/parameterIds';
import { getPluginInfo } from './bridge/pluginInfo';
import { useParameter } from './bridge/useParameter';
import './App.css';

const TABS = ['OSC', 'MOD', 'FX', 'AI'] as const;

/**
 * Phase 0 shell: the header bar, the four-tab frame, and one live parameter.
 *
 * The master knob is here to prove the whole chain end to end — pointer event
 * -> WebSliderRelay -> AudioProcessorValueTreeState -> processBlock. Phase 6
 * replaces this placeholder with the real tab layouts.
 */
export function App() {
  const info = getPluginInfo();
  const master = useParameter(GLOBAL.masterGain);

  return (
    <div className="gn-app">
      <header className="gn-header">
        <div className="gn-logo">GNARL</div>

        <nav className="gn-tabs">
          {TABS.map((tab, i) => (
            <button key={tab} className="gn-tab" data-active={i === 0} type="button">
              {tab}
            </button>
          ))}
        </nav>

        <div className="gn-header__right">
          <Knob
            label="Master"
            value={master.normalised}
            readout={`${master.scaled.toFixed(1)} dB`}
            onChange={master.setNormalised}
            onGestureStart={master.beginGesture}
            onGestureEnd={master.endGesture}
            size={36}
          />
        </div>
      </header>

      <main className="gn-body">
        <div className="gn-placeholder">
          <SectionLabel>Phase 0 — scaffold</SectionLabel>
          <p>
            Audio engine runs and outputs silence. Voice architecture lands in
            Phase 1, the sound engine in Phase 2.
          </p>
          <p className="gn-placeholder__meta">
            v{info.pluginVersion} · state v{info.stateVersion}
            {info.isMock ? ' · browser preview (no audio engine)' : ''}
          </p>
        </div>
      </main>

      {/* Hovering any control writes its one-line description here. */}
      <footer className="gn-statusbar" />
    </div>
  );
}
