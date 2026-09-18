import { useCallback, useEffect, useMemo, useState } from 'react';

import { DepthSlider } from '../components/DepthSlider';
import { Dropdown } from '../components/Dropdown';
import { EnvelopeDisplay } from '../components/EnvelopeDisplay';
import { Knob } from '../components/Knob';
import { LfoEditor } from '../components/LfoEditor';
import { Panel, Row } from '../components/Panel';
import { Toggle } from '../components/Toggle';
import {
  ENVELOPE_MODE,
  GRID_DIVISION,
  LFO_MODE,
  LFO_RATE_DIVISION,
  LFO_SHAPE,
  MACRO_NAMES,
  MOD_CURVE,
  MOD_SOURCE,
} from '../bridge/choices';
import { ENV, LFO, MACRO, MOD } from '../bridge/parameterIds';
import { defaultRamp, type CurvePoint } from '../dsp/lfoCurve';
import { fetchModState, pushLfoCurve, pushModDestination, type DestinationOption } from '../bridge/modState';
import { setPreviewCurves, setPreviewDestinations } from '../bridge/previewEngine';
import { useModulationFrame } from '../bridge/useModulation';
import { useParameter } from '../bridge/useParameter';
import { useChoiceParameter, useToggleParameter } from '../bridge/useDiscreteParameter';
import './ModTab.css';

/**
 * The modulation tab: four LFOs, four envelopes, sixteen mod slots, four
 * macros.
 *
 * ONE LFO IS SHOWN AT A TIME, selected by a row of tabs. Four editors side by
 * side would each be about 260 px wide, which is not enough to draw a
 * breakpoint curve into - and the drawable curve is the feature. The other
 * three LFOs keep running; the tab chooses what is on screen, not what is
 * playing.
 */
export function ModTab() {
  const [lfoIndex, setLfoIndex] = useState(0);
  const [curves, setCurves] = useState<CurvePoint[][]>(() =>
    Array.from({ length: LFO.length }, () => defaultRamp()),
  );
  const [destinations, setDestinations] = useState<string[]>(() =>
    Array.from({ length: MOD.length }, () => ''),
  );
  const [available, setAvailable] = useState<DestinationOption[]>([]);

  // The curves and destinations are ValueTree state, not parameters, so they
  // are fetched once rather than arriving through a relay. A preset load
  // republishes them, which Phase 5 will have to re-fetch on.
  useEffect(() => {
    let cancelled = false;

    void fetchModState().then((state) => {
      if (cancelled) return;

      setCurves(state.curves);
      setDestinations(state.destinations);
      setAvailable(state.available);
      setPreviewCurves(state.curves);
      setPreviewDestinations(state.destinations);
    });

    return () => {
      cancelled = true;
    };
  }, []);

  const handleCurveChange = useCallback(
    (index: number, points: CurvePoint[]) => {
      setCurves((previous) => {
        const next = [...previous];
        next[index] = points;
        setPreviewCurves(next);
        return next;
      });

      // Fire-and-forget, like every other write from this UI: the editor must
      // never wait on C++ between two frames of a drag.
      void pushLfoCurve(index, points);
    },
    [],
  );

  const handleDestinationChange = useCallback(
    (slot: number, parameterId: string) => {
      setDestinations((previous) => {
        const next = [...previous];
        next[slot] = parameterId;
        setPreviewDestinations(next);
        return next;
      });

      void pushModDestination(slot, parameterId);
    },
    [],
  );

  return (
    <div className="gn-mod-tab">
      <LfoSection
        index={lfoIndex}
        ids={LFO[lfoIndex] ?? LFO[0]}
        onSelect={setLfoIndex}
        points={curves[lfoIndex] ?? defaultRamp()}
        onPointsChange={(points) => handleCurveChange(lfoIndex, points)}
      />

      <div className="gn-mod-tab__envelopes">
        {ENV.map((ids, index) => (
          <EnvelopePanel key={index} index={index} ids={ids} />
        ))}
      </div>

      <div className="gn-mod-tab__bottom">
        <ModMatrix
          destinations={destinations}
          available={available}
          onDestinationChange={handleDestinationChange}
        />
        <MacroPanel />
      </div>
    </div>
  );
}

// --- LFOs -------------------------------------------------------------------

interface LfoSectionProps {
  index: number;
  ids: (typeof LFO)[number];
  onSelect: (index: number) => void;
  points: CurvePoint[];
  onPointsChange: (points: CurvePoint[]) => void;
}

function LfoSection({ index, ids, onSelect, points, onPointsChange }: LfoSectionProps) {
  const shape = useChoiceParameter(ids.shape, LFO_SHAPE.length);
  const mode = useChoiceParameter(ids.mode, LFO_MODE.length);
  const division = useChoiceParameter(ids.rateDivision, LFO_RATE_DIVISION.length);
  const grid = useChoiceParameter(ids.gridDivision, GRID_DIVISION.length);
  const sync = useToggleParameter(ids.syncEnabled);
  const bipolar = useToggleParameter(ids.bipolar);
  const rate = useParameter(ids.rateHz);
  const phase = useParameter(ids.phase);
  const smooth = useParameter(ids.smooth);

  const frame = useModulationFrame(120);
  const value = frame.lfoValues[index] ?? 0;

  return (
    <Panel
      title="LFO"
      grow
      headerRight={
        <div className="gn-lfo__tabs">
          {LFO.map((_unused, i) => (
            <button
              key={i}
              type="button"
              className="gn-lfo__tab"
              data-active={i === index}
              onClick={() => onSelect(i)}
            >
              {i + 1}
            </button>
          ))}

          {/* The live output, as a number and a bar. A wobble you can see the
              shape of but not the current value of is only half a display. */}
          <span className="gn-lfo__value" title="Live LFO output">
            <span
              className="gn-lfo__value-fill"
              style={{ width: `${Math.round(Math.abs(value) * 100)}%` }}
            />
            <span className="gn-lfo__value-text">{value.toFixed(2)}</span>
          </span>
        </div>
      }
    >
      <div className="gn-lfo__body">
        <LfoEditor
          lfoIndex={index}
          points={points}
          shapeIndex={shape.index}
          gridIndex={grid.index}
          bipolar={bipolar.value}
          onChange={onPointsChange}
        />

        <Row gap="tight">
          <Dropdown label="Shape" value={shape.index} options={LFO_SHAPE} onChange={shape.setIndex} />
          <Dropdown label="Mode" value={mode.index} options={LFO_MODE} onChange={mode.setIndex} />

          <span className="gn-mod-tab__divider" />

          <Toggle label="Sync" value={sync.value} onChange={sync.setValue} compact />

          {/* Only one of the two rate controls is meaningful at a time, so the
              other is not shown at all rather than shown greyed. A disabled
              control the user has to learn to ignore is clutter. */}
          {sync.value ? (
            <Dropdown
              label="Rate"
              value={division.index}
              options={LFO_RATE_DIVISION}
              onChange={division.setIndex}
              wide
            />
          ) : (
            <Knob
              label="Rate"
              value={rate.normalised}
              readout={rate.text}
              onChange={rate.setNormalised}
              onGestureStart={rate.beginGesture}
              onGestureEnd={rate.endGesture}
              size={32}
            />
          )}

          <Dropdown label="Grid" value={grid.index} options={GRID_DIVISION} onChange={grid.setIndex} />

          <span className="gn-mod-tab__divider" />

          <Knob
            label="Phase"
            value={phase.normalised}
            readout={phase.text}
            onChange={phase.setNormalised}
            onGestureStart={phase.beginGesture}
            onGestureEnd={phase.endGesture}
            size={32}
          />
          <Knob
            label="Smooth"
            value={smooth.normalised}
            readout={smooth.text}
            onChange={smooth.setNormalised}
            onGestureStart={smooth.beginGesture}
            onGestureEnd={smooth.endGesture}
            size={32}
          />

          <Toggle label="Bipolar" value={bipolar.value} onChange={bipolar.setValue} compact />
        </Row>
      </div>
    </Panel>
  );
}

// --- Envelopes --------------------------------------------------------------

function EnvelopePanel({ index, ids }: { index: number; ids: (typeof ENV)[number] }) {
  const mode = useChoiceParameter(ids.mode, ENVELOPE_MODE.length);
  const delay = useParameter(ids.delay);
  const attack = useParameter(ids.attack);
  const hold = useParameter(ids.hold);
  const decay = useParameter(ids.decay);
  const sustain = useParameter(ids.sustain);
  const release = useParameter(ids.release);
  const attackCurve = useParameter(ids.attackCurve);
  const decayCurve = useParameter(ids.decayCurve);
  const releaseCurve = useParameter(ids.releaseCurve);
  const velocity = useParameter(ids.velocityAmount);

  const dahdsr = mode.index === 1;

  return (
    <Panel
      title={index === 0 ? 'Env 1 · Amp' : `Env ${index + 1}`}
      grow
      headerRight={
        <Dropdown value={mode.index} options={ENVELOPE_MODE} onChange={mode.setIndex} />
      }
    >
      <div className="gn-env__body">
        <EnvelopeDisplay
          delay={delay.scaled}
          attack={attack.scaled}
          hold={hold.scaled}
          decay={decay.scaled}
          sustain={sustain.scaled}
          release={release.scaled}
          attackCurve={attackCurve.scaled}
          decayCurve={decayCurve.scaled}
          releaseCurve={releaseCurve.scaled}
          dahdsr={dahdsr}
          highlight={index === 0}
        />

        <Row gap="tight">
          {dahdsr && (
            <Knob label="Del" value={delay.normalised} readout={delay.text} onChange={delay.setNormalised} onGestureStart={delay.beginGesture} onGestureEnd={delay.endGesture} size={26} />
          )}
          <Knob label="A" value={attack.normalised} readout={attack.text} onChange={attack.setNormalised} onGestureStart={attack.beginGesture} onGestureEnd={attack.endGesture} size={26} />
          {dahdsr && (
            <Knob label="H" value={hold.normalised} readout={hold.text} onChange={hold.setNormalised} onGestureStart={hold.beginGesture} onGestureEnd={hold.endGesture} size={26} />
          )}
          <Knob label="D" value={decay.normalised} readout={decay.text} onChange={decay.setNormalised} onGestureStart={decay.beginGesture} onGestureEnd={decay.endGesture} size={26} />
          <Knob label="S" value={sustain.normalised} readout={sustain.text} onChange={sustain.setNormalised} onGestureStart={sustain.beginGesture} onGestureEnd={sustain.endGesture} size={26} />
          <Knob label="R" value={release.normalised} readout={release.text} onChange={release.setNormalised} onGestureStart={release.beginGesture} onGestureEnd={release.endGesture} size={26} />
        </Row>

        {/* The curve controls, which are what make a pluck sound like a pluck
            rather than like a pad with a short release. */}
        <Row gap="tight">
          <Knob label="A Crv" value={attackCurve.normalised} readout={attackCurve.text} onChange={attackCurve.setNormalised} onGestureStart={attackCurve.beginGesture} onGestureEnd={attackCurve.endGesture} defaultValue={0.5} size={24} />
          <Knob label="D Crv" value={decayCurve.normalised} readout={decayCurve.text} onChange={decayCurve.setNormalised} onGestureStart={decayCurve.beginGesture} onGestureEnd={decayCurve.endGesture} defaultValue={0.5} size={24} />
          <Knob label="R Crv" value={releaseCurve.normalised} readout={releaseCurve.text} onChange={releaseCurve.setNormalised} onGestureStart={releaseCurve.beginGesture} onGestureEnd={releaseCurve.endGesture} defaultValue={0.5} size={24} />
          <Knob label="Vel" value={velocity.normalised} readout={velocity.text} onChange={velocity.setNormalised} onGestureStart={velocity.beginGesture} onGestureEnd={velocity.endGesture} size={24} />
        </Row>
      </div>
    </Panel>
  );
}

// --- Mod matrix -------------------------------------------------------------

interface ModMatrixProps {
  destinations: string[];
  available: DestinationOption[];
  onDestinationChange: (slot: number, parameterId: string) => void;
}

function ModMatrix({ destinations, available, onDestinationChange }: ModMatrixProps) {
  // The "-" entry is how a slot gets UNROUTED, and unrouting has to be as
  // reachable as routing.
  const options = useMemo(() => ['—', ...available.map((entry) => entry.name)], [available]);

  const activeCount = destinations.filter((entry) => entry !== '').length;

  return (
    <Panel title="Mod Matrix" grow headerRight={<span className="gn-matrix__count">{activeCount}/{MOD.length}</span>}>
      <div className="gn-matrix">
        <div className="gn-matrix__head">
          <span>On</span>
          <span>Source</span>
          <span>Destination</span>
          <span>Depth</span>
          <span>Curve</span>
          <span>Aux</span>
          <span>Amt</span>
          <span>Bi</span>
        </div>

        {MOD.map((ids, slot) => (
          <ModSlotRow
            key={slot}
            slot={slot}
            ids={ids}
            destination={destinations[slot] ?? ''}
            available={available}
            options={options}
            onDestinationChange={onDestinationChange}
          />
        ))}
      </div>
    </Panel>
  );
}

interface ModSlotRowProps {
  slot: number;
  ids: (typeof MOD)[number];
  destination: string;
  available: DestinationOption[];
  options: string[];
  onDestinationChange: (slot: number, parameterId: string) => void;
}

function ModSlotRow({ slot, ids, destination, available, options, onDestinationChange }: ModSlotRowProps) {
  const enabled = useToggleParameter(ids.enabled);
  const source = useChoiceParameter(ids.source, MOD_SOURCE.length);
  const depth = useParameter(ids.depth);
  const curve = useChoiceParameter(ids.curve, MOD_CURVE.length);
  const auxSource = useChoiceParameter(ids.auxSource, MOD_SOURCE.length);
  const auxAmount = useParameter(ids.auxAmount);
  const bipolar = useToggleParameter(ids.bipolar);

  const destinationIndex = destination
    ? available.findIndex((entry) => entry.id === destination) + 1
    : 0;

  const routed = enabled.value && source.index !== 0 && destination !== '';

  return (
    <div className="gn-matrix__row" data-routed={routed}>
      <Toggle value={enabled.value} onChange={enabled.setValue} compact />

      <Dropdown value={source.index} options={MOD_SOURCE} onChange={source.setIndex} />

      <Dropdown
        value={Math.max(0, destinationIndex)}
        options={options}
        onChange={(index) => onDestinationChange(slot, index === 0 ? '' : available[index - 1]?.id ?? '')}
        wide
      />

      <DepthSlider value={depth.normalised} readout={depth.text} title="Depth" onChange={depth.setNormalised} onGestureStart={depth.beginGesture} onGestureEnd={depth.endGesture} />

      <Dropdown value={curve.index} options={MOD_CURVE} onChange={curve.setIndex} />

      {/* The secondary modulator: how an LFO gets faded in by an envelope
          without burning a second of the sixteen slots. */}
      <Dropdown value={auxSource.index} options={MOD_SOURCE} onChange={auxSource.setIndex} />

      <DepthSlider value={auxAmount.normalised} readout={auxAmount.text} bipolar={false} title="Aux amount" onChange={auxAmount.setNormalised} onGestureStart={auxAmount.beginGesture} onGestureEnd={auxAmount.endGesture} />

      <Toggle value={bipolar.value} onChange={bipolar.setValue} compact />
    </div>
  );
}

// --- Macros -----------------------------------------------------------------

function MacroPanel() {
  return (
    <Panel title="Macros" grow>
      <div className="gn-macros">
        {MACRO.map((id, index) => (
          <MacroKnob key={id} id={id} name={MACRO_NAMES[index] ?? `Macro ${index + 1}`} primary={index === 0} />
        ))}
      </div>
    </Panel>
  );
}

function MacroKnob({ id, name, primary }: { id: (typeof MACRO)[number]; name: string; primary: boolean }) {
  const macro = useParameter(id);

  return (
    <div className="gn-macros__item" data-primary={primary}>
      <Knob
        label={name}
        value={macro.normalised}
        readout={macro.text}
        onChange={macro.setNormalised}
        onGestureStart={macro.beginGesture}
        onGestureEnd={macro.endGesture}
        size={34}
      />
    </div>
  );
}
