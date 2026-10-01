/* Copyright 2026 Gnarl Audio
 *
 * GNARL is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See LICENSE.
 */

// The part of src/common the browser build needs, as one unity file the way
// src/unity_build/common.cpp is: parameters, the LFO shapes, modulation
// connections and the wavetable creator (the init wavetable). Not the preset
// loader, MIDI, tuning files or SynthBase - they need JUCE (wasm/README.md).

#include "operators.h"  // synth_strings.h names vital::StereoEncoder

#include "synth_parameters.cpp"
#include "line_generator.cpp"
#include "synth_types.cpp"
#include "wavetable_component_factory.cpp"
#include "wavetable_keyframe.cpp"
#include "file_source.cpp"
#include "shepard_tone_source.cpp"
#include "frequency_filter_modifier.cpp"
#include "wave_fold_modifier.cpp"
#include "phase_modifier.cpp"
#include "wavetable_creator.cpp"
#include "wave_line_source.cpp"
#include "wave_source.cpp"
#include "wavetable_group.cpp"
#include "wave_window_modifier.cpp"
#include "wavetable_component.cpp"
#include "pitch_detector.cpp"
#include "wave_warp_modifier.cpp"
#include "slew_limit_modifier.cpp"
