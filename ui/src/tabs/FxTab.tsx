import { useCallback, useEffect, useState } from 'react';
import { Knob } from '../components/Knob';
import { Meter } from '../components/Meter';
import { Panel, Row } from '../components/Panel';
import { Toggle } from '../components/Toggle';
import { OTT } from '../bridge/parameterIds';
import { FX_SLOT_NAME } from '../bridge/choices';
import {
  defaultOrder, fetchFxOrder, pushFxOrder, withMoved,
} from '../bridge/fxOrder';
import { FX_SLOT_ENABLE_IDS, FxSlotPanel } from './FxRackPanels';
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
        {/* ONE row of six, not two of four and two. The OTT had the whole tab
            to itself before the rack existed; now it shares the vertical
            budget with fourteen chain rows, and a second stacked row of knobs
            cost exactly the height that was clipping the band panels below. */}
        <Row gap="tight">
          <Knob label="Time" value={time.normalised} readout={time.text} onChange={time.setNormalised} onGestureStart={time.beginGesture} onGestureEnd={time.endGesture} size={34} defaultValue={0.5} />
          <Knob label="Mix" value={mix.normalised} readout={mix.text} onChange={mix.setNormalised} onGestureStart={mix.beginGesture} onGestureEnd={mix.endGesture} size={34} />
          <Knob label="In" value={inGain.normalised} readout={inGain.text} onChange={inGain.setNormalised} onGestureStart={inGain.beginGesture} onGestureEnd={inGain.endGesture} size={34} defaultValue={0.5} />
          <Knob label="Out" value={outGain.normalised} readout={outGain.text} onChange={outGain.setNormalised} onGestureStart={outGain.beginGesture} onGestureEnd={outGain.endGesture} size={34} defaultValue={0.5} />
          <Knob label="X-Low" value={xLow.normalised} readout={xLow.text} onChange={xLow.setNormalised} onGestureStart={xLow.beginGesture} onGestureEnd={xLow.endGesture} size={34} />
          <Knob label="X-High" value={xHigh.normalised} readout={xHigh.text} onChange={xHigh.setNormalised} onGestureStart={xHigh.beginGesture} onGestureEnd={xHigh.endGesture} size={34} />
        </Row>
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

/**
 * One row of the chain list.
 *
 * Its own component because it needs its enable parameter, and a hook cannot
 * be called in a loop in the parent.
 */
function ChainRow({
  slot, position, selected, dragging, onSelect, onDragStart, onDragOver, onDrop, onDragEnd,
}: {
  slot: number;
  position: number;
  selected: boolean;
  dragging: boolean;
  onSelect: () => void;
  onDragStart: () => void;
  onDragOver: () => void;
  onDrop: () => void;
  onDragEnd: () => void;
}) {
  const enabled = useToggleParameter(FX_SLOT_ENABLE_IDS[slot]!);

  return (
    <div
      className="gn-fx-chain__row"
      data-selected={selected}
      data-dragging={dragging}
      data-off={!enabled.value}
      draggable
      onClick={onSelect}
      onDragStart={onDragStart}
      onDragOver={(event) => {
        // Without this the drop is never accepted and the row snaps back.
        event.preventDefault();
        onDragOver();
      }}
      onDrop={(event) => {
        event.preventDefault();
        onDrop();
      }}
      onDragEnd={onDragEnd}
    >
      <span className="gn-fx-chain__grip" aria-hidden>⠿</span>
      <span className="gn-fx-chain__index">{position + 1}</span>
      <span className="gn-fx-chain__name">{FX_SLOT_NAME[slot]}</span>
      <span
        className="gn-fx-chain__toggle"
        onClick={(event) => {
          // The toggle must not also select the row: reaching for an enable
          // and having the panel change under you is the kind of thing that
          // makes a dense UI feel unpredictable.
          event.stopPropagation();
        }}
      >
        <Toggle value={enabled.value} onChange={enabled.setValue} compact />
      </span>
    </div>
  );
}

/**
 * The rack: the chain on the left, the selected effect's controls on the right.
 *
 * THE ORDER IS A DRAG-REORDERABLE LIST, which is the whole reason the roster is
 * fixed rather than generic. Each row is one named instance with its own
 * parameters, so dragging it changes where it runs and nothing else - whereas
 * a slot whose TYPE was a parameter would silently change the meaning of every
 * automation lane pointing into it. See docs/fx-architecture.md.
 *
 * The order is not a parameter, so it does not arrive through a relay: the list
 * is fetched once and then kept locally, with each change pushed to the plugin.
 * Optimistic, because the UI never blocks on C++ - and reconciled from the
 * plugin's answer, so a rejected reorder snaps back rather than leaving the
 * list disagreeing with what the engine runs.
 */
function FxRackPanel() {
  const [order, setOrder] = useState<number[]>(defaultOrder);
  const [selected, setSelected] = useState(0);
  const [dragFrom, setDragFrom] = useState<number | null>(null);

  useEffect(() => {
    let cancelled = false;

    void fetchFxOrder().then((fetched) => {
      if (!cancelled) setOrder(fetched);
    });

    return () => { cancelled = true; };
  }, []);

  /*  COMMITTED ON DROP, AS A WHOLE ORDER, not as a move. The dragover handler
      reorders the list live - which is what makes a drag readable - and
      advances the drag's own index as it goes, so by the time the drag ends
      there is no single "from and to" left to send: the first version of this
      computed one and it was always a no-op, so the reorder looked right in
      the list and never reached the plugin at all.

      Sending the finished order instead is both simpler and what the UI
      actually knows. The plugin rejects anything that is not a permutation, so
      a rejected push is re-read rather than trusted - otherwise the list would
      keep showing a chain the engine is not running. */
  const commitOrder = useCallback((committed: readonly number[]) => {
    void pushFxOrder(committed).then(async (accepted) => {
      if (!accepted) setOrder(await fetchFxOrder());
    });
  }, []);

  return (
    <div className="gn-fx-rack">
      <Panel title="Chain">
        <div className="gn-fx-chain">
          {order.map((slot, position) => (
            <ChainRow
              key={slot}
              slot={slot}
              position={position}
              selected={slot === selected}
              dragging={dragFrom === position}
              onSelect={() => setSelected(slot)}
              onDragStart={() => setDragFrom(position)}
              onDragOver={() => {
                if (dragFrom !== null && dragFrom !== position) {
                  setOrder((current) => withMoved(current, dragFrom, position));
                  setDragFrom(position);
                }
              }}
              onDrop={() => setDragFrom(null)}
              onDragEnd={() => {
                setDragFrom(null);
                commitOrder(order);
              }}
            />
          ))}
        </div>
      </Panel>

      <Panel title={FX_SLOT_NAME[selected]} grow>
        <FxSlotPanel slot={selected} />
      </Panel>
    </div>
  );
}

export function FxTab() {
  return (
    <div className="gn-fx-tab">
      <OttPanel />
      <FxRackPanel />
    </div>
  );
}
