import defaults from './parameterDefaults.json';
import { getSliderState } from '../juce/index.js';
import { getPluginInfo } from './pluginInfo';

interface ParameterInfo {
  name: string;
  label: string;
  default: number;
  steps: number;
  textAtDefault?: string;
  textAtMin?: string;
  textAtMax?: string;
  min?: number;
  max?: number;
  skew?: number;
  interval?: number;
}

/**
 * Recovers the unit from the C++ side's own formatted output.
 *
 * Most parameters set a formatter rather than a `label`, so the label field is
 * empty and the UI has nothing to go on - which is why cutoff read "20000"
 * instead of "20.0 kHz". Taking the trailing non-numeric text of the
 * already-formatted default gives the unit without reimplementing every
 * formatter in TypeScript.
 *
 * This is a stopgap. The real fix, in Phase 6, is to relay the C++ formatted
 * string over the bridge so there is exactly one formatter.
 */
function inferUnit(info: ParameterInfo): string {
  if (info.label) return info.label;

  const sample = info.textAtMax || info.textAtDefault || '';
  const match = sample.match(/[-+0-9.,\s]*(.*)$/);
  const unit = (match?.[1] ?? '').trim();

  // kHz is a presentation of an underlying value in Hz, so report Hz and let
  // the formatter decide which to show.
  if (unit.toLowerCase().endsWith('hz')) return 'Hz';

  return unit;
}

/**
 * Seeds the browser preview with the plugin's real parameter ranges and
 * defaults.
 *
 * Without this, JUCE's browser stand-in gives every control a 0..1 range and
 * a value of zero, so every knob sits at the bottom and every readout says
 * "0%". That is useless for judging layout and actively misleading in a
 * screenshot - it looks like a synth with nothing set up.
 *
 * The values come from tools/dump_parameter_defaults.cpp, which reads the real
 * AudioProcessorValueTreeState, so the preview and the plugin cannot disagree.
 *
 * Does nothing when a real plugin is behind the page: there the relay supplies
 * the true values, and overwriting them would reset the user's patch.
 */
export function seedMockBackend(): void {
  if (!getPluginInfo().isMock) return;

  const table = defaults as Record<string, ParameterInfo>;

  for (const [id, info] of Object.entries(table)) {
    try {
      const state = getSliderState(id);

      // Mutating properties directly is how the mock is meant to be primed;
      // the real backend pushes these over the relay instead.
      const properties = state.properties as unknown as Record<string, unknown>;
      properties.start = info.min ?? 0;
      properties.end = info.max ?? 1;
      properties.skew = info.skew ?? 1;
      properties.interval = info.interval ?? 0;
      properties.label = inferUnit(info);
      properties.name = info.name ?? id;
      properties.numSteps = info.steps ?? 100;

      state.setNormalisedValue(info.default);
    } catch {
      // A parameter the mock does not know about is not worth failing over:
      // the preview should still come up.
    }
  }
}
