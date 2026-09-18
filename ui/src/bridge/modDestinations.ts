import { FILTER, NOISE, OSC, SUB } from './parameterIds';

/**
 * The modulation destinations, mirroring the table in
 * plugin/source/dsp/Modulation.cpp.
 *
 * The plugin serves this list over the bridge, from the engine's own table, so
 * a destination cannot exist in the picker and not in the matrix. This copy is
 * for the BROWSER PREVIEW, where there is no plugin to ask - without it the
 * destination picker is empty and the whole MOD tab is unusable in the
 * preview, which is where its layout gets judged.
 *
 * Guarded by tests/ParameterMirrorTests.cpp: an entry that drifts from the C++
 * table fails the build rather than quietly offering a destination the engine
 * does not have.
 */
export interface ModDestinationOption {
  /** The parameter ID, which is what gets stored in a preset. */
  id: string;
  /** The label the picker shows. */
  name: string;
}

export const MOD_DESTINATIONS: readonly ModDestinationOption[] = [
  { id: OSC[0].tablePos, name: 'Osc 1 Position' },
  { id: OSC[0].warpAmount, name: 'Osc 1 Warp' },
  { id: OSC[0].level, name: 'Osc 1 Level' },
  { id: OSC[0].pan, name: 'Osc 1 Pan' },
  { id: OSC[0].pitchSemi, name: 'Osc 1 Semi' },
  { id: OSC[0].pitchFine, name: 'Osc 1 Fine' },
  { id: OSC[0].unisonDetune, name: 'Osc 1 Detune' },
  { id: OSC[0].grainSize, name: 'Osc 1 Grain Size' },
  { id: OSC[0].grainDensity, name: 'Osc 1 Grain Density' },
  { id: OSC[1].tablePos, name: 'Osc 2 Position' },
  { id: OSC[1].warpAmount, name: 'Osc 2 Warp' },
  { id: OSC[1].level, name: 'Osc 2 Level' },
  { id: OSC[1].pan, name: 'Osc 2 Pan' },
  { id: OSC[1].pitchSemi, name: 'Osc 2 Semi' },
  { id: OSC[1].pitchFine, name: 'Osc 2 Fine' },
  { id: OSC[1].unisonDetune, name: 'Osc 2 Detune' },
  { id: SUB.level, name: 'Sub Level' },
  { id: NOISE.level, name: 'Noise Level' },
  { id: FILTER[0].cutoff, name: 'Filter 1 Cutoff' },
  { id: FILTER[0].resonance, name: 'Filter 1 Res' },
  { id: FILTER[0].drive, name: 'Filter 1 Drive' },
  { id: FILTER[0].mix, name: 'Filter 1 Mix' },
  { id: FILTER[0].formantX, name: 'Filter 1 Vowel X' },
  { id: FILTER[0].formantY, name: 'Filter 1 Vowel Y' },
  { id: FILTER[0].formantThroat, name: 'Filter 1 Throat' },
  { id: FILTER[1].cutoff, name: 'Filter 2 Cutoff' },
  { id: FILTER[1].resonance, name: 'Filter 2 Res' },
  { id: FILTER[1].drive, name: 'Filter 2 Drive' },
  { id: FILTER[1].mix, name: 'Filter 2 Mix' },
];
