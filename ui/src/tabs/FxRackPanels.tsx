import { Dropdown } from '../components/Dropdown';
import { Knob } from '../components/Knob';
import { Row } from '../components/Panel';
import { Toggle } from '../components/Toggle';
import {
  FX_CHORUS, FX_DELAY, FX_DIMENSION, FX_DISTORTION, FX_EQ, FX_FILTER,
  FX_FLANGER, FX_HYPER, FX_LIMITER, FX_PHASER, FX_REVERB,
} from '../bridge/parameterIds';
import {
  FX_DISTORTION_TYPE, FX_FILTER_TYPE, LFO_RATE_DIVISION,
} from '../bridge/choices';
import { useParameter } from '../bridge/useParameter';
import { useChoiceParameter, useToggleParameter } from '../bridge/useDiscreteParameter';
import type { ParameterId } from '../bridge/parameterIds';

/**
 * The parameter panels for the fourteen rack instances.
 *
 * ONE COMPONENT PER EFFECT, not one generic panel driven by a table. A table
 * would be shorter and would lay every effect out identically, which is
 * exactly wrong: what makes a dense synth readable is that the control that
 * matters is the biggest one, and which control that is differs per effect.
 * The distortion's is drive, the delay's is feedback, the reverb's is decay.
 *
 * Every control here is a real host parameter reached through a relay, so it
 * keeps automation, undo and gesture handling. Only the chain ORDER is not -
 * see bridge/fxOrder.ts.
 */

/** Wraps a Knob with its parameter, because the six-prop spread is noise
    repeated ninety times otherwise. */
function FxKnob({
  id, label, size = 34, defaultValue,
}: { id: ParameterId; label: string; size?: number; defaultValue?: number }) {
  const parameter = useParameter(id);

  return (
    <Knob
      label={label}
      value={parameter.normalised}
      readout={parameter.text}
      onChange={parameter.setNormalised}
      onGestureStart={parameter.beginGesture}
      onGestureEnd={parameter.endGesture}
      size={size}
      defaultValue={defaultValue}
    />
  );
}

function FxChoice({
  id, label, options, wide,
}: { id: ParameterId; label: string; options: readonly string[]; wide?: boolean }) {
  const parameter = useChoiceParameter(id, options.length);

  return (
    <Dropdown
      label={label}
      value={parameter.index}
      options={options}
      onChange={parameter.setIndex}
      wide={wide}
    />
  );
}

function FxToggle({ id, label }: { id: ParameterId; label: string }) {
  const parameter = useToggleParameter(id);

  return (
    <label className="gn-fx__toggle">
      <span>{label}</span>
      <Toggle value={parameter.value} onChange={parameter.setValue} compact />
    </label>
  );
}

function DistortionPanel({ index }: { index: number }) {
  const ids = FX_DISTORTION[index];
  if (!ids) return null;

  return (
    <>
      {/* Drive biggest: it is the control this effect exists for, and the
          reason there are two instances of it in the rack. */}
      <Row>
        <FxKnob id={ids.drive} label="Drive" size={56} />
        <div className="gn-fx__stack">
          <FxChoice id={ids.type} label="Curve" options={FX_DISTORTION_TYPE} wide />
          {/* One row rather than two: the trim controls belong beside the
              curve's own controls, and a second row left the panel with a
              band of dead space under it. */}
          <Row gap="tight">
            <FxKnob id={ids.tone} label="Tone" defaultValue={0.5} />
            <FxKnob id={ids.bias} label="Bias" defaultValue={0.5} />
            <FxKnob id={ids.output} label="Out" defaultValue={0.5} />
            <FxKnob id={ids.mix} label="Mix" />
          </Row>
        </div>
      </Row>
      <p className="gn-fx__hint">
        Tone is a PRE-tilt: it changes what the curve is given, which is not
        something an EQ afterwards can reproduce.
      </p>
    </>
  );
}

function EqPanel({ index }: { index: number }) {
  const ids = FX_EQ[index];
  if (!ids) return null;

  return (
    <>
      {/* Laid out left to right in FREQUENCY order, which is how the response
          reads - a flat list of fourteen knobs would expose the same controls
          and be unusable. */}
      <Row gap="tight">
        <FxKnob id={ids.highPassFreq} label="HP" />
        <FxKnob id={ids.lowShelfFreq} label="LS Hz" />
        <FxKnob id={ids.lowShelfGain} label="LS dB" defaultValue={0.5} />
        <FxKnob id={ids.lowPassFreq} label="LP" />
      </Row>
      <Row gap="tight">
        <FxKnob id={ids.band1Freq} label="B1 Hz" />
        <FxKnob id={ids.band1Gain} label="B1 dB" defaultValue={0.5} />
        <FxKnob id={ids.band1Q} label="B1 Q" />
        <FxKnob id={ids.highShelfFreq} label="HS Hz" />
      </Row>
      <Row gap="tight">
        <FxKnob id={ids.band2Freq} label="B2 Hz" />
        <FxKnob id={ids.band2Gain} label="B2 dB" defaultValue={0.5} />
        <FxKnob id={ids.band2Q} label="B2 Q" />
        <FxKnob id={ids.highShelfGain} label="HS dB" defaultValue={0.5} />
      </Row>
      <Row gap="tight">
        <FxKnob id={ids.mix} label="Mix" />
      </Row>
    </>
  );
}

function FilterPanel({ index }: { index: number }) {
  const ids = FX_FILTER[index];
  if (!ids) return null;

  return (
    <>
      <Row>
        <FxKnob id={ids.cutoff} label="Cutoff" size={56} />
        <div className="gn-fx__stack">
          <FxChoice id={ids.type} label="Type" options={FX_FILTER_TYPE} wide />
          <Row gap="tight">
            <FxKnob id={ids.resonance} label="Res" />
            <FxKnob id={ids.drive} label="Drive" />
          </Row>
        </div>
      </Row>
      <Row gap="tight">
        <FxKnob id={ids.mix} label="Mix" />
      </Row>
      <p className="gn-fx__hint">
        Runs on the summed signal, so a sweep here moves the whole chord. The
        two VOICE filters sweep per note, which is what makes a growl.
      </p>
    </>
  );
}

function DelayPanel() {
  const sync = useToggleParameter(FX_DELAY.syncEnabled);

  return (
    <>
      <Row>
        <FxKnob id={FX_DELAY.feedback} label="Feedback" size={56} />
        <div className="gn-fx__stack">
          <FxToggle id={FX_DELAY.syncEnabled} label="Sync" />
          {/* Only the control that is actually in use is shown. Two time
              controls side by side, one of them inert, is a control that
              looks broken. */}
          {sync.value
            ? <FxChoice id={FX_DELAY.division} label="Division" options={LFO_RATE_DIVISION} wide />
            : <Row gap="tight"><FxKnob id={FX_DELAY.timeMs} label="Time" /></Row>}
        </div>
      </Row>
      <Row gap="tight">
        <FxKnob id={FX_DELAY.lowCut} label="Low Cut" />
        <FxKnob id={FX_DELAY.highCut} label="High Cut" />
        <FxKnob id={FX_DELAY.width} label="Width" />
        <FxKnob id={FX_DELAY.mix} label="Mix" />
      </Row>
      <Row gap="tight">
        <FxKnob id={FX_DELAY.modRate} label="Mod Rate" />
        <FxKnob id={FX_DELAY.modDepth} label="Mod" />
        <FxToggle id={FX_DELAY.pingPong} label="Ping-Pong" />
      </Row>
      <p className="gn-fx__hint">
        The cuts are INSIDE the feedback loop, so repeats lose their top end as
        they decay rather than repeating brightly forever.
      </p>
    </>
  );
}

function ReverbPanel() {
  return (
    <>
      <Row>
        <FxKnob id={FX_REVERB.decay} label="Decay" size={56} />
        <div className="gn-fx__stack">
          <Row gap="tight">
            <FxKnob id={FX_REVERB.size} label="Size" />
            <FxKnob id={FX_REVERB.damping} label="Damp" />
            <FxKnob id={FX_REVERB.preDelay} label="Pre" />
          </Row>
          <Row gap="tight">
            <FxKnob id={FX_REVERB.width} label="Width" />
            <FxKnob id={FX_REVERB.modDepth} label="Mod" />
            <FxKnob id={FX_REVERB.mix} label="Mix" />
          </Row>
        </div>
      </Row>
      <Row gap="tight">
        <FxKnob id={FX_REVERB.lowCut} label="Low Cut" />
        <FxKnob id={FX_REVERB.highCut} label="High Cut" />
      </Row>
      <p className="gn-fx__hint">
        Size and Decay are independent: the decay time means the same thing at
        every room size, so changing the room does not need it re-set.
      </p>
    </>
  );
}

function ChorusPanel() {
  return (
    <>
      <Row>
        <FxKnob id={FX_CHORUS.depth} label="Depth" size={56} />
        <div className="gn-fx__stack">
          <Row gap="tight">
            <FxKnob id={FX_CHORUS.rate} label="Rate" />
            <FxKnob id={FX_CHORUS.voices} label="Voices" />
          </Row>
          <Row gap="tight">
            <FxKnob id={FX_CHORUS.spread} label="Spread" />
            <FxKnob id={FX_CHORUS.feedback} label="Fdbk" />
          </Row>
        </div>
      </Row>
      <Row gap="tight">
        <FxKnob id={FX_CHORUS.mix} label="Mix" />
      </Row>
    </>
  );
}

function FlangerPanel() {
  return (
    <>
      <Row>
        <FxKnob id={FX_FLANGER.feedback} label="Feedback" size={56} defaultValue={0.5} />
        <div className="gn-fx__stack">
          <Row gap="tight">
            <FxKnob id={FX_FLANGER.rate} label="Rate" />
            <FxKnob id={FX_FLANGER.depth} label="Depth" />
          </Row>
          <Row gap="tight">
            <FxKnob id={FX_FLANGER.manual} label="Manual" />
            <FxKnob id={FX_FLANGER.stereo} label="Stereo" />
          </Row>
        </div>
      </Row>
      <Row gap="tight">
        <FxKnob id={FX_FLANGER.mix} label="Mix" />
      </Row>
      <p className="gn-fx__hint">
        Feedback is bipolar. A negative setting puts the nulls where a positive
        one puts the peaks, and the two sound nothing alike.
      </p>
    </>
  );
}

function PhaserPanel() {
  return (
    <>
      <Row>
        <FxKnob id={FX_PHASER.depth} label="Depth" size={56} />
        <div className="gn-fx__stack">
          <Row gap="tight">
            <FxKnob id={FX_PHASER.rate} label="Rate" />
            <FxKnob id={FX_PHASER.centre} label="Centre" />
          </Row>
          <Row gap="tight">
            <FxKnob id={FX_PHASER.stages} label="Stages" />
            <FxKnob id={FX_PHASER.feedback} label="Fdbk" defaultValue={0.5} />
          </Row>
        </div>
      </Row>
      <Row gap="tight">
        <FxKnob id={FX_PHASER.stereo} label="Stereo" />
        <FxKnob id={FX_PHASER.mix} label="Mix" />
      </Row>
      <p className="gn-fx__hint">
        Each PAIR of stages adds one notch, so 4 stages give 2 and 12 give 6.
        An odd count inverts the dry path, which is a usable sound.
      </p>
    </>
  );
}

function HyperPanel() {
  return (
    <>
      <Row>
        <FxKnob id={FX_HYPER.amount} label="Amount" size={56} />
        <div className="gn-fx__stack">
          <Row gap="tight">
            <FxKnob id={FX_HYPER.detune} label="Detune" />
            <FxKnob id={FX_HYPER.voices} label="Voices" />
          </Row>
          <Row gap="tight">
            <FxKnob id={FX_HYPER.width} label="Width" />
            <FxKnob id={FX_HYPER.mix} label="Mix" />
          </Row>
        </div>
      </Row>
      <p className="gn-fx__hint">
        Unison for a signal that cannot be re-synthesised: the voices are taps
        whose delay is changing, and the rate of that change is the detune.
      </p>
    </>
  );
}

function DimensionPanel() {
  return (
    <>
      <Row>
        <FxKnob id={FX_DIMENSION.width} label="Width" size={56} />
        <div className="gn-fx__stack">
          <Row gap="tight">
            <FxKnob id={FX_DIMENSION.amount} label="Amount" />
            <FxKnob id={FX_DIMENSION.timeMs} label="Time" />
          </Row>
          <Row gap="tight">
            <FxKnob id={FX_DIMENSION.mix} label="Mix" />
          </Row>
        </div>
      </Row>
      <p className="gn-fx__hint">
        Mono-safe by construction: everything it adds lives in the side, so a
        mono sum gives the dry signal back exactly.
      </p>
    </>
  );
}

function LimiterPanel() {
  return (
    <>
      <Row>
        <FxKnob id={FX_LIMITER.threshold} label="Threshold" size={56} defaultValue={1} />
        <div className="gn-fx__stack">
          <Row gap="tight">
            <FxKnob id={FX_LIMITER.ceiling} label="Ceiling" />
            <FxKnob id={FX_LIMITER.release} label="Release" />
          </Row>
          <Row gap="tight">
            <FxKnob id={FX_LIMITER.mix} label="Mix" />
          </Row>
        </div>
      </Row>
      <p className="gn-fx__hint">
        Looks ahead, so a peak is already caught when it arrives. The default
        ceiling sits below 0 dBFS because a true peak of exactly zero clips in
        a lossy encoder.
      </p>
    </>
  );
}

/** The enable parameter for each slot, in FX_SLOT_NAME order. */
export const FX_SLOT_ENABLE_IDS: readonly ParameterId[] = [
  FX_DISTORTION[0]!.enabled, FX_EQ[0]!.enabled, FX_FILTER[0]!.enabled,
  FX_DISTORTION[1]!.enabled, FX_EQ[1]!.enabled, FX_FILTER[1]!.enabled,
  FX_CHORUS.enabled, FX_FLANGER.enabled, FX_PHASER.enabled,
  FX_HYPER.enabled, FX_DIMENSION.enabled, FX_DELAY.enabled,
  FX_REVERB.enabled, FX_LIMITER.enabled,
];

/**
 * The panel for one slot, chosen by its index into FX_SLOT_NAME.
 *
 * The index-to-effect mapping is the choice list's frozen order, which is the
 * same order the C++ FxSlot enum declares - ParameterMirrorTests fails the
 * build if the two disagree, so this switch cannot drift from the engine
 * without the build breaking.
 */
export function FxSlotPanel({ slot }: { slot: number }) {
  switch (slot) {
    case 0:  return <DistortionPanel index={0} />;
    case 1:  return <EqPanel index={0} />;
    case 2:  return <FilterPanel index={0} />;
    case 3:  return <DistortionPanel index={1} />;
    case 4:  return <EqPanel index={1} />;
    case 5:  return <FilterPanel index={1} />;
    case 6:  return <ChorusPanel />;
    case 7:  return <FlangerPanel />;
    case 8:  return <PhaserPanel />;
    case 9:  return <HyperPanel />;
    case 10: return <DimensionPanel />;
    case 11: return <DelayPanel />;
    case 12: return <ReverbPanel />;
    case 13: return <LimiterPanel />;
    default: return null;
  }
}
