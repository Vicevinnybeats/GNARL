/* Copyright 2013-2019 Matt Tytel
 *
 * vital is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * vital is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with vital.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Modified by Gnarl Audio, 2026: the wobble macro (see lfo_module.h).
 */

#include "lfo_module.h"

#include <cmath>

#include "line_generator.h"
#include "synth_lfo.h"

namespace vital {

  LfoModule::LfoModule(const std::string& prefix, LineGenerator* line_generator, const Output* beats_per_second) :
      SynthModule(kNumInputs, kNumOutputs), prefix_(prefix), beats_per_second_(beats_per_second) {
    lfo_ = new SynthLfo(line_generator);
    addProcessor(lfo_);

    setControlRate(true);
  }

  void LfoModule::init() {
    Output* free_frequency = createPolyModControl(prefix_ + "_frequency");
    Output* phase = createPolyModControl(prefix_ + "_phase");
    Output* fade = createPolyModControl(prefix_ + "_fade_time");
    Output* delay = createPolyModControl(prefix_ + "_delay_time");
    Output* stereo_phase = createPolyModControl(prefix_ + "_stereo");
    Value* sync_type = createBaseControl(prefix_ + "_sync_type");
    Value* smooth_mode = createBaseControl(prefix_ + "_smooth_mode");
    Output* smooth_time = createPolyModControl(prefix_ + "_smooth_time");

    Output* frequency = createTempoSyncSwitch(prefix_, free_frequency->owner, beats_per_second_, true, input(kMidi));
    lfo_->useInput(input(kNoteTrigger), SynthLfo::kNoteTrigger);
    lfo_->useInput(input(kNoteCount), SynthLfo::kNoteCount);

    lfo_->useOutput(output(kValue), SynthLfo::kValue);
    lfo_->useOutput(output(kOscPhase), SynthLfo::kOscPhase);
    lfo_->useOutput(output(kOscFrequency), SynthLfo::kOscFrequency);
    lfo_->plug(frequency, SynthLfo::kFrequency);
    lfo_->plug(phase, SynthLfo::kPhase);
    lfo_->plug(stereo_phase, SynthLfo::kStereoPhase);
    lfo_->plug(sync_type, SynthLfo::kSyncType);
    lfo_->plug(smooth_mode, SynthLfo::kSmoothMode);
    lfo_->plug(fade, SynthLfo::kFade);
    lfo_->plug(smooth_time, SynthLfo::kSmoothTime);
    lfo_->plug(delay, SynthLfo::kDelay);
  }

  void LfoModule::correctToTime(double seconds) {
    lfo_->correctToTime(seconds);
  }

  void LfoModule::setControlRate(bool control_rate) {
    Processor::setControlRate(control_rate);
    lfo_->setControlRate(control_rate);
  }

  constexpr mono_float WobbleRate::kCyclesPerBeat[WobbleRate::kNumRates];

  void WobbleRate::process(int num_samples) {
    // utils::toInt ROUNDS TO NEAREST-EVEN (it is the SSE/NEON conversion), it
    // does not truncate - so the obvious `toInt(rate + 0.5f)` turns 1 into 2
    // and 3 into 4, and index 4 reads past the end of the table. That is not
    // hypothetical: it is what the first build did, and the rate test measured
    // 1/8 at 7.0 Hz and 1/16 at 17.5 Hz, a number from outside the table.
    // The parameter is an integer, so rounding it is already right; the index
    // is clamped AFTER conversion as well, because the table read must be in
    // bounds whatever arrives.
    poly_float rate = utils::clamp(input(kRate)->at(0), 0.0f, kNumRates - 1);
    poly_int index = utils::toInt(rate);
    poly_float cycles_per_beat = 0.0f;
    for (int i = 0; i < poly_float::kSize; ++i) {
      int lane = std::min(std::max(static_cast<int>(index[i]), 0), kNumRates - 1);
      cycles_per_beat.set(i, kCyclesPerBeat[lane]);
    }

    output()->buffer[0] = cycles_per_beat * input(kBeatsPerSecond)->at(0);
  }

  WobbleModule::WobbleModule(LineGenerator* line_generator, const Output* beats_per_second) :
      SynthModule(kNumInputs, kNumOutputs), beats_per_second_(beats_per_second) {
    lfo_ = new SynthLfo(line_generator);
    addProcessor(lfo_);

    setControlRate(true);
  }

  void WobbleModule::init() {
    Value* rate = createBaseControl("wobble_rate");

    WobbleRate* frequency = new WobbleRate();
    frequency->plug(rate, WobbleRate::kRate);
    frequency->plug(beats_per_second_, WobbleRate::kBeatsPerSecond);
    addProcessor(frequency);

    // Fixed, not parameters: fade-in, delay and per-channel phase have no use
    // on a transport-locked wobble, and every parameter added is one more
    // name frozen forever by the first preset. PHASE and SMOOTH became
    // parameters on 2026-10-01 (the panel's knobs), defaulting to exactly the
    // constants they replace.
    //
    // Each constant is the value an LFO's DEFAULT parameter delivers to
    // SynthLfo - in the engine's units, not the parameter's. That matters for
    // smoothing: lfo_N_smooth_mode defaults ON and lfo_N_smooth_time is
    // kExponential at -7.5, so an LFO receives 2^-7.5 s = 5.5 ms. The first
    // build fixed smoothing OFF, and it was measurable: an unsmoothed
    // control-rate LFO steps once per block, so the render depended on the
    // block size by -14 dB, against -64 dB for Vital's own LFO on the same
    // destination - and did not match that LFO at all (-16 dB).
    // wobble_phase: where on the grid the cycle starts, 0..1 of a cycle.
    // wobble_smooth_time: Vital's LFO smoothing, 2^x seconds. Block-rate.
    Output* phase = createPolyModControl("wobble_phase");
    Output* smooth_time = createPolyModControl("wobble_smooth_time");
    cr::Value* fade = new cr::Value(0.0f);
    cr::Value* delay = new cr::Value(0.0f);
    cr::Value* stereo_phase = new cr::Value(0.0f);
    cr::Value* sync_type = new cr::Value(SynthLfo::kSync);
    cr::Value* smooth_mode = new cr::Value(1.0f);
    for (Processor* constant : { (Processor*)fade, (Processor*)delay,
                                 (Processor*)stereo_phase, (Processor*)sync_type,
                                 (Processor*)smooth_mode })
      addIdleProcessor(constant);

    lfo_->useInput(input(kNoteTrigger), SynthLfo::kNoteTrigger);
    lfo_->useInput(input(kNoteCount), SynthLfo::kNoteCount);

    lfo_->useOutput(output(kValue), SynthLfo::kValue);
    lfo_->useOutput(output(kOscPhase), SynthLfo::kOscPhase);
    lfo_->useOutput(output(kOscFrequency), SynthLfo::kOscFrequency);
    lfo_->plug(frequency, SynthLfo::kFrequency);
    lfo_->plug(phase, SynthLfo::kPhase);
    lfo_->plug(stereo_phase, SynthLfo::kStereoPhase);
    lfo_->plug(sync_type, SynthLfo::kSyncType);
    lfo_->plug(smooth_mode, SynthLfo::kSmoothMode);
    lfo_->plug(fade, SynthLfo::kFade);
    lfo_->plug(smooth_time, SynthLfo::kSmoothTime);
    lfo_->plug(delay, SynthLfo::kDelay);

    SynthModule::init();
  }

  void WobbleModule::correctToTime(double seconds) {
    lfo_->correctToTime(seconds);
  }

  void WobbleModule::setControlRate(bool control_rate) {
    Processor::setControlRate(control_rate);
    lfo_->setControlRate(control_rate);
  }

} // namespace vital
