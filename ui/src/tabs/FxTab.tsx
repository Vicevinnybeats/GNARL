import { Knob } from '../components/Knob';
import { Meter } from '../components/Meter';
import { Panel, Row } from '../components/Panel';
import { Toggle } from '../components/Toggle';
import { OTT } from '../bridge/parameterIds';
import { useParameter } from '../bridge/useParameter';
import { useToggleParameter } from '../bridge/useDiscreteParameter';
import './FxTab.css';

/**
 * The OTT compressor.
 *
 * Laid out as three band columns rather than a flat list of seventeen knobs,
 * because the mental model is "three bands, each doing two things". A flat
 * list would technically expose the same controls and be unusable.
 */
function OttPanel() {
  const enabled = useToggleParameter(OTT.enabled);

  const depth = useParameter(OTT.depth);
  const time = useParameter(OTT.time);
  const mix = useParameter(OTT.mix);
  const inGain = useParameter(OTT.inputGain);
  const outGain = useParameter(OTT.outputGain);
  const xLow = useParameter(OTT.crossoverLow);
  const xHigh = useParameter(OTT.crossoverHigh);

  const lowGain = useParameter(OTT.lowGain);
  const midGain = useParameter(OTT.midGain);
  const highGain = useParameter(OTT.highGain);
  const lowUp = useParameter(OTT.lowUpward);
  const midUp = useParameter(OTT.midUpward);
  const highUp = useParameter(OTT.highUpward);
  const lowDown = useParameter(OTT.lowDownward);
  const midDown = useParameter(OTT.midDownward);
  const highDown = useParameter(OTT.highDownward);

  const bands = [
    { name: 'Low', gain: lowGain, up: lowUp, down: lowDown },
    { name: 'Mid', gain: midGain, up: midUp, down: midDown },
    { name: 'High', gain: highGain, up: highUp, down: highDown },
  ];

  return (
    <Panel
      title="OTT — 3-Band Up/Down Compressor"
      dimmed={!enabled.value}
      headerRight={<Toggle value={enabled.value} onChange={enabled.setValue} compact />}
    >
      {/* Depth first and largest: it is the control that actually gets used,
          and everything else is trim. */}
      <Row>
        <Knob label="Depth" value={depth.normalised} readout={depth.text} onChange={depth.setNormalised} onGestureStart={depth.beginGesture} onGestureEnd={depth.endGesture} size={56} />
        <div className="gn-ott__globals">
          <Row gap="tight">
            <Knob label="Time" value={time.normalised} readout={time.text} onChange={time.setNormalised} onGestureStart={time.beginGesture} onGestureEnd={time.endGesture} size={34} defaultValue={0.5} />
            <Knob label="Mix" value={mix.normalised} readout={mix.text} onChange={mix.setNormalised} onGestureStart={mix.beginGesture} onGestureEnd={mix.endGesture} size={34} />
            <Knob label="In" value={inGain.normalised} readout={inGain.text} onChange={inGain.setNormalised} onGestureStart={inGain.beginGesture} onGestureEnd={inGain.endGesture} size={34} defaultValue={0.5} />
            <Knob label="Out" value={outGain.normalised} readout={outGain.text} onChange={outGain.setNormalised} onGestureStart={outGain.beginGesture} onGestureEnd={outGain.endGesture} size={34} defaultValue={0.5} />
          </Row>
          <Row gap="tight">
            <Knob label="X-Low" value={xLow.normalised} readout={xLow.text} onChange={xLow.setNormalised} onGestureStart={xLow.beginGesture} onGestureEnd={xLow.endGesture} size={34} />
            <Knob label="X-High" value={xHigh.normalised} readout={xHigh.text} onChange={xHigh.setNormalised} onGestureStart={xHigh.beginGesture} onGestureEnd={xHigh.endGesture} size={34} />
          </Row>
        </div>
      </Row>

      <div className="gn-ott__bands">
        {bands.map((band) => (
          <div className="gn-ott__band" key={band.name}>
            <span className="gn-ott__band-name">{band.name}</span>
            <Meter label="GR" value={0} bipolar />
            <Row gap="tight">
              <Knob label="Up" value={band.up.normalised} readout={band.up.text} onChange={band.up.setNormalised} onGestureStart={band.up.beginGesture} onGestureEnd={band.up.endGesture} size={34} />
              <Knob label="Down" value={band.down.normalised} readout={band.down.text} onChange={band.down.setNormalised} onGestureStart={band.down.beginGesture} onGestureEnd={band.down.endGesture} size={34} />
              <Knob label="Gain" value={band.gain.normalised} readout={band.gain.text} onChange={band.gain.setNormalised} onGestureStart={band.gain.beginGesture} onGestureEnd={band.gain.endGesture} size={34} defaultValue={0.5} />
            </Row>
          </div>
        ))}
      </div>
    </Panel>
  );
}

const PLANNED_EFFECTS = [
  'Distortion', 'EQ', 'Delay', 'Reverb', 'Chorus / Flanger / Phaser',
  'Filter', 'Hyper / Dimension', 'Limiter',
];

export function FxTab() {
  return (
    <div className="gn-fx-tab">
      <OttPanel />

      <Panel title="FX Rack" grow>
        <p className="gn-fx__note">
          The reorderable ten-slot rack is Phase 4. OTT ships early and
          separately because it is the one effect that is on in nearly every
          riddim patch, so it is built in rather than occupying a slot.
        </p>
        <div className="gn-fx__planned">
          {PLANNED_EFFECTS.map((name) => (
            <span className="gn-fx__slot" key={name}>
              {name}
            </span>
          ))}
        </div>
      </Panel>
    </div>
  );
}
