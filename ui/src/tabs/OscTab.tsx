import { Knob } from '../components/Knob';
import { WavetableDisplay } from '../components/WavetableDisplay';
import { Dropdown } from '../components/Dropdown';
import { Panel, Row } from '../components/Panel';
import { Toggle } from '../components/Toggle';
import { XYPad } from '../components/XYPad';
import { DRIVE_CURVE, FILTER_ROUTING, FILTER_TYPE, NOISE_TYPE, OSC_MODE, SUB_WAVEFORM, VOWEL_ANCHORS, WARP_MODE, WAVETABLE_NAMES } from '../bridge/choices';
import { FILTER, GLOBAL, NOISE, OSC, SUB } from '../bridge/parameterIds';
import { useParameter } from '../bridge/useParameter';
import { useChoiceParameter, useToggleParameter } from '../bridge/useDiscreteParameter';
import './OscTab.css';

function OscillatorPanel({ index }: { index: 0 | 1 }) {
  const ids = OSC[index];
  const enabled = useToggleParameter(ids.enabled);
  const mode = useChoiceParameter(ids.mode, OSC_MODE.length);
  const table = useChoiceParameter(ids.wavetable, WAVETABLE_NAMES.length);
  const warp = useChoiceParameter(ids.warpMode, WARP_MODE.length);

  const tablePos = useParameter(ids.tablePos);
  const warpAmount = useParameter(ids.warpAmount);
  const semi = useParameter(ids.pitchSemi);
  const fine = useParameter(ids.pitchFine);
  const level = useParameter(ids.level);
  const pan = useParameter(ids.pan);
  const unison = useParameter(ids.unisonVoices);
  const detune = useParameter(ids.unisonDetune);
  const blend = useParameter(ids.unisonBlend);
  const spread = useParameter(ids.unisonSpread);
  const toF1 = useParameter(ids.sendFilter1);
  const toF2 = useParameter(ids.sendFilter2);
  const direct = useParameter(ids.sendDirect);

  const isGrain = mode.index === 1;

  return (
    <Panel
      title={`Osc ${index + 1}`}
      dimmed={!enabled.value}
      grow
      headerRight={<Toggle value={enabled.value} onChange={enabled.setValue} compact />}
    >
      <Row gap="tight">
        <Dropdown label="Table" value={table.index} options={WAVETABLE_NAMES} onChange={table.setIndex} wide />
        <Dropdown label="Mode" value={mode.index} options={OSC_MODE} onChange={mode.setIndex} />
      </Row>

      {/* The visual centrepiece. Moving the position control travels through
          the table rather than swapping one shape for another, which is what
          makes a wavetable synth feel alive. */}
      <div className="gn-osc__display">
        <WavetableDisplay
          tableIndex={table.index}
          position={tablePos.normalised}
          warpMode={warp.index}
          warpAmount={warpAmount.scaled}
          flat={isGrain}
          oscIndex={index}
        />
      </div>

      {/* Two knob rows, not three: at the design size there is not room for a
          third without it drawing over the panel below. */}
      <Row gap="tight">
        <Knob label="Pos" value={tablePos.normalised} readout={tablePos.text} onChange={tablePos.setNormalised} onGestureStart={tablePos.beginGesture} onGestureEnd={tablePos.endGesture} size={40} />
        <Knob label="Semi" value={semi.normalised} readout={semi.text} onChange={semi.setNormalised} onGestureStart={semi.beginGesture} onGestureEnd={semi.endGesture} defaultValue={0.5} size={32} />
        <Knob label="Fine" value={fine.normalised} readout={fine.text} onChange={fine.setNormalised} onGestureStart={fine.beginGesture} onGestureEnd={fine.endGesture} defaultValue={0.5} size={32} />
        <Knob label="Level" value={level.normalised} readout={level.text} onChange={level.setNormalised} onGestureStart={level.beginGesture} onGestureEnd={level.endGesture} size={32} />
        <Knob label="Pan" value={pan.normalised} readout={pan.text} onChange={pan.setNormalised} onGestureStart={pan.beginGesture} onGestureEnd={pan.endGesture} defaultValue={0.5} size={32} />
        <span className="gn-osc__divider" />
        <Dropdown label="Warp" value={warp.index} options={WARP_MODE} onChange={warp.setIndex} />
        <Knob label="Amt" value={warpAmount.normalised} readout={warpAmount.text} onChange={warpAmount.setNormalised} onGestureStart={warpAmount.beginGesture} onGestureEnd={warpAmount.endGesture} defaultValue={0.5} size={32} />
      </Row>

      <Row gap="tight">
        <Knob label="Uni" value={unison.normalised} readout={unison.text} onChange={unison.setNormalised} onGestureStart={unison.beginGesture} onGestureEnd={unison.endGesture} size={32} />
        <Knob label="Detune" value={detune.normalised} readout={detune.text} onChange={detune.setNormalised} onGestureStart={detune.beginGesture} onGestureEnd={detune.endGesture} size={32} />
        <Knob label="Blend" value={blend.normalised} readout={blend.text} onChange={blend.setNormalised} onGestureStart={blend.beginGesture} onGestureEnd={blend.endGesture} size={32} />
        <Knob label="Spread" value={spread.normalised} readout={spread.text} onChange={spread.setNormalised} onGestureStart={spread.beginGesture} onGestureEnd={spread.endGesture} size={32} />
        <span className="gn-osc__divider" />
        <Knob label="→F1" value={toF1.normalised} readout={toF1.text} onChange={toF1.setNormalised} onGestureStart={toF1.beginGesture} onGestureEnd={toF1.endGesture} size={30} />
        <Knob label="→F2" value={toF2.normalised} readout={toF2.text} onChange={toF2.setNormalised} onGestureStart={toF2.beginGesture} onGestureEnd={toF2.endGesture} size={30} />
        <Knob label="→Out" value={direct.normalised} readout={direct.text} onChange={direct.setNormalised} onGestureStart={direct.beginGesture} onGestureEnd={direct.endGesture} size={30} />
      </Row>

      {/* Grain controls appear only in graintable mode: showing dead knobs
          teaches the user nothing and makes the panel look broken. */}
      {isGrain && <GrainRow index={index} />}
    </Panel>
  );
}

function GrainRow({ index }: { index: 0 | 1 }) {
  const ids = OSC[index];
  const size = useParameter(ids.grainSize);
  const density = useParameter(ids.grainDensity);
  const posJitter = useParameter(ids.grainPosJitter);
  const pitchJitter = useParameter(ids.grainPitchJitter);

  return (
    <Row gap="tight">
      <span className="gn-osc__sends-label">Grain</span>
      <Knob label="Size" value={size.normalised} readout={size.text} onChange={size.setNormalised} onGestureStart={size.beginGesture} onGestureEnd={size.endGesture} size={32} />
      <Knob label="Dens" value={density.normalised} readout={density.text} onChange={density.setNormalised} onGestureStart={density.beginGesture} onGestureEnd={density.endGesture} size={32} />
      <Knob label="P Jit" value={posJitter.normalised} readout={posJitter.text} onChange={posJitter.setNormalised} onGestureStart={posJitter.beginGesture} onGestureEnd={posJitter.endGesture} size={32} />
      <Knob label="F Jit" value={pitchJitter.normalised} readout={pitchJitter.text} onChange={pitchJitter.setNormalised} onGestureStart={pitchJitter.beginGesture} onGestureEnd={pitchJitter.endGesture} size={32} />
    </Row>
  );
}

function SubPanel() {
  const enabled = useToggleParameter(SUB.enabled);
  const waveform = useChoiceParameter(SUB.waveform, SUB_WAVEFORM.length);
  const octave = useParameter(SUB.octave);
  const level = useParameter(SUB.level);
  const direct = useParameter(SUB.sendDirect);

  return (
    <Panel title="Sub" dimmed={!enabled.value} headerRight={<Toggle value={enabled.value} onChange={enabled.setValue} compact />}>
      <Row gap="tight">
        <Dropdown label="Wave" value={waveform.index} options={SUB_WAVEFORM} onChange={waveform.setIndex} />
        <Knob label="Oct" value={octave.normalised} readout={octave.text} onChange={octave.setNormalised} onGestureStart={octave.beginGesture} onGestureEnd={octave.endGesture} size={32} />
        <Knob label="Level" value={level.normalised} readout={level.text} onChange={level.setNormalised} onGestureStart={level.beginGesture} onGestureEnd={level.endGesture} size={32} />
        <Knob label="Dir" value={direct.normalised} readout={direct.text} onChange={direct.setNormalised} onGestureStart={direct.beginGesture} onGestureEnd={direct.endGesture} size={32} />
      </Row>
    </Panel>
  );
}

function NoisePanel() {
  const enabled = useToggleParameter(NOISE.enabled);
  const type = useChoiceParameter(NOISE.type, NOISE_TYPE.length);
  const level = useParameter(NOISE.level);
  const pan = useParameter(NOISE.pan);

  return (
    <Panel title="Noise" dimmed={!enabled.value} headerRight={<Toggle value={enabled.value} onChange={enabled.setValue} compact />}>
      <Row gap="tight">
        <Dropdown label="Type" value={type.index} options={NOISE_TYPE} onChange={type.setIndex} />
        <Knob label="Level" value={level.normalised} readout={level.text} onChange={level.setNormalised} onGestureStart={level.beginGesture} onGestureEnd={level.endGesture} size={32} />
        <Knob label="Pan" value={pan.normalised} readout={pan.text} onChange={pan.setNormalised} onGestureStart={pan.beginGesture} onGestureEnd={pan.endGesture} size={32} defaultValue={0.5} />
      </Row>
    </Panel>
  );
}

function FilterPanel({ index }: { index: 0 | 1 }) {
  const ids = FILTER[index];
  const enabled = useToggleParameter(ids.enabled);
  const type = useChoiceParameter(ids.type, FILTER_TYPE.length);
  const curve = useChoiceParameter(ids.driveCurve, DRIVE_CURVE.length);

  const cutoff = useParameter(ids.cutoff);
  const resonance = useParameter(ids.resonance);
  const drive = useParameter(ids.drive);
  const mix = useParameter(ids.mix);
  const formantX = useParameter(ids.formantX);
  const formantY = useParameter(ids.formantY);
  const throat = useParameter(ids.formantThroat);

  const isFormant = type.index === 11;

  return (
    <Panel
      title={`Filter ${index + 1}`}
      dimmed={!enabled.value}
      grow
      headerRight={<Toggle value={enabled.value} onChange={enabled.setValue} compact />}
    >
      <Row gap="tight">
        <Dropdown label="Type" value={type.index} options={FILTER_TYPE} onChange={type.setIndex} wide />
        <Dropdown label="Drive Curve" value={curve.index} options={DRIVE_CURVE} onChange={curve.setIndex} />
      </Row>

      <Row gap="tight">
        {/* The vowel pad replaces the cutoff knob for the formant type,
            because cutoff means nothing there — the pad IS the control. */}
        {isFormant ? (
          <>
            <XYPad
              label="Vowel"
              x={formantX.normalised}
              y={formantY.normalised}
              anchors={VOWEL_ANCHORS}
              onChange={(x, y) => {
                formantX.setNormalised(x);
                formantY.setNormalised(y);
              }}
              size={92}
            />
            <div className="gn-filter__column">
              <Knob label="Throat" value={throat.normalised} readout={throat.text} onChange={throat.setNormalised} onGestureStart={throat.beginGesture} onGestureEnd={throat.endGesture} defaultValue={0.5} />
              <Knob label="Res" value={resonance.normalised} readout={resonance.text} onChange={resonance.setNormalised} onGestureStart={resonance.beginGesture} onGestureEnd={resonance.endGesture} />
            </div>
          </>
        ) : (
          <>
            <Knob label="Cutoff" value={cutoff.normalised} readout={cutoff.text} onChange={cutoff.setNormalised} onGestureStart={cutoff.beginGesture} onGestureEnd={cutoff.endGesture} size={52} />
            <Knob label="Res" value={resonance.normalised} readout={resonance.text} onChange={resonance.setNormalised} onGestureStart={resonance.beginGesture} onGestureEnd={resonance.endGesture} />
          </>
        )}

        <div className="gn-filter__column">
          <Knob label="Drive" value={drive.normalised} readout={drive.text} onChange={drive.setNormalised} onGestureStart={drive.beginGesture} onGestureEnd={drive.endGesture} />
          <Knob label="Mix" value={mix.normalised} readout={mix.text} onChange={mix.setNormalised} onGestureStart={mix.beginGesture} onGestureEnd={mix.endGesture} />
        </div>
      </Row>

    </Panel>
  );
}

export function OscTab() {
  const routing = useChoiceParameter(GLOBAL.filterRouting, FILTER_ROUTING.length);

  return (
    <div className="gn-osc-tab">
      <div className="gn-osc-tab__oscillators">
        <OscillatorPanel index={0} />
        <OscillatorPanel index={1} />
      </div>

      <div className="gn-osc-tab__strip">
        <SubPanel />
        <NoisePanel />
      </div>

      <div className="gn-osc-tab__filters">
        <FilterPanel index={0} />
        <Panel title="Routing">
          <Dropdown value={routing.index} options={FILTER_ROUTING} onChange={routing.setIndex} />
        </Panel>
        <FilterPanel index={1} />
      </div>
    </div>
  );
}
