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
 * Modified by Gnarl Audio, 2026: the drive chain - fold and crush stages
 * after Vital's drive stage (docs/design/phase2-07-drive-chain.md).
 */

#include "distortion_module.h"

#include "distortion.h"
#include "digital_svf.h"

#include <cmath>

namespace vital {

  DistortionModule::DistortionModule() :
      SynthModule(0, 1), distortion_(nullptr), filter_(nullptr), mix_(0.0f),
      drive_stage_on_(true), fold_on_(nullptr), fold_type_(nullptr), fold_distortion_type_(nullptr),
      fold_(nullptr), fold_mix_(nullptr), fold_mix_value_(0.0f), crush_on_(nullptr), crush_bits_(nullptr),
      crush_rate_(nullptr), crush_step_(0.0f), crush_period_(1.0f), crush_hold_(0.0f), crush_count_(0.0f) { }

  DistortionModule::~DistortionModule() {
  }

  void DistortionModule::init() {
    distortion_ = new Distortion();
    distortion_->useOutput(output());
    addIdleProcessor(distortion_);

    Value* distortion_type = createBaseControl("distortion_type");
    Output* distortion_drive = createMonoModControl("distortion_drive", true, true);
    distortion_mix_ = createMonoModControl("distortion_mix");

    distortion_->plug(distortion_type, Distortion::kType);
    distortion_->plug(distortion_drive, Distortion::kDrive);

    filter_order_ = createBaseControl("distortion_filter_order");
    Output* midi_cutoff = createMonoModControl("distortion_filter_cutoff", true, true);
    Output* resonance = createMonoModControl("distortion_filter_resonance");
    Output* blend = createMonoModControl("distortion_filter_blend");

    filter_ = new DigitalSvf();
    filter_->useOutput(output());
    filter_->plug(midi_cutoff, DigitalSvf::kMidiCutoff);
    filter_->plug(resonance, DigitalSvf::kResonance);
    filter_->plug(blend, DigitalSvf::kPassBlend);
    filter_->setDriveCompensation(false);
    filter_->setBasic(true);
    addIdleProcessor(filter_);

    // GNARL: FOLD is Vital's own fold (the same Distortion processor, type
    // sine or linear fold), with its own drive and mix. Rates as the drive
    // stage's: drive audio-rate smoothed, mix block-rate ramped.
    fold_on_ = createBaseControl("distortion_fold_on");
    fold_type_ = createBaseControl("distortion_fold_type");
    fold_distortion_type_ = new Value(Distortion::kSinFold);
    addIdleProcessor(fold_distortion_type_);
    fold_ = new Distortion();
    fold_->plug(fold_distortion_type_, Distortion::kType);
    fold_->plug(createMonoModControl("distortion_fold_drive", true, true), Distortion::kDrive);
    addIdleProcessor(fold_);
    fold_mix_ = createMonoModControl("distortion_fold_mix");

    // CRUSH: bits and a sample hold, block-rate, ramped across the block.
    crush_on_ = createBaseControl("distortion_crush_on");
    crush_bits_ = createMonoModControl("distortion_crush_bits");
    crush_rate_ = createMonoModControl("distortion_crush_rate");

    SynthModule::init();
  }

  void DistortionModule::setSampleRate(int sample_rate) {
    SynthModule::setSampleRate(sample_rate);
    distortion_->setSampleRate(sample_rate);
    filter_->setSampleRate(sample_rate);
    fold_->setSampleRate(sample_rate);
  }

  void DistortionModule::hardReset() {
    SynthModule::hardReset();
    crush_hold_ = 0.0f;
    crush_count_ = 0.0f;
  }

  void DistortionModule::processWithInput(const poly_float* audio_in, int num_samples) {
    SynthModule::process(num_samples);
    poly_float* audio_out = output()->buffer;

    // GNARL: with only FOLD or CRUSH on, the drive stage passes the audio on
    // untouched.
    if (!drive_stage_on_)
      utils::copyBuffer(audio_out, audio_in, num_samples);
    else {
      if (filter_order_->output()->buffer[0][0] < 1.0f)
        distortion_->processWithInput(audio_in, num_samples);
      else if (filter_order_->output()->buffer[0][0] > 1.0f) {
        distortion_->processWithInput(audio_in, num_samples);
        filter_->processWithInput(output()->buffer, num_samples);
      }
      else {
        filter_->processWithInput(audio_in, num_samples);
        distortion_->processWithInput(output()->buffer, num_samples);
      }

      poly_float current_mix = mix_;
      mix_ = utils::clamp(distortion_mix_->buffer[0], 0.0f, 1.0f);
      poly_float delta_mix = (mix_ - current_mix) * (1.0f / num_samples);

      for (int i = 0; i < num_samples; ++i) {
        current_mix += delta_mix;
        audio_out[i] = utils::interpolate(audio_in[i], audio_out[i], current_mix);
      }
    }

    if (fold_on_->value())
      processFold(num_samples);
    if (crush_on_->value())
      processCrush(num_samples);
  }

  // The same steps as the drive stage above, so FOLD alone renders exactly
  // as Vital's single stage does with a fold type (tests/test_drive_chain.py).
  void DistortionModule::processFold(int num_samples) {
    fold_distortion_type_->set(fold_type_->value() ? Distortion::kLinearFold : Distortion::kSinFold);
    poly_float* audio = output()->buffer;
    fold_->processWithInput(audio, num_samples);
    const poly_float* folded = fold_->output(Distortion::kAudioOut)->buffer;

    poly_float current_mix = fold_mix_value_;
    fold_mix_value_ = utils::clamp(fold_mix_->buffer[0], 0.0f, 1.0f);
    poly_float delta_mix = (fold_mix_value_ - current_mix) * (1.0f / num_samples);
    for (int i = 0; i < num_samples; ++i) {
      current_mix += delta_mix;
      audio[i] = utils::interpolate(audio[i], folded[i], current_mix);
    }
  }

  // Quantise to 2^bits levels over -1..1, then hold each quantised sample
  // for `period` engine samples. Bits and period ramp across the block; the
  // hold counter carries over, so the result does not depend on block size.
  void DistortionModule::processCrush(int num_samples) {
    mono_float bits = utils::clamp(crush_bits_->buffer[0][0], kMinCrushBits, kMaxCrushBits);
    mono_float rate = utils::clamp(crush_rate_->buffer[0][0], 0.0f, 1.0f);
    mono_float target_step = std::exp2(1.0f - bits);
    mono_float target_period = std::exp2(rate * std::log2(kMaxCrushHold));
    // First use: start at the targets rather than ramping from nothing,
    // which would make the first block's sound depend on its length.
    if (crush_step_ <= 0.0f) {
      crush_step_ = target_step;
      crush_period_ = target_period;
    }

    mono_float delta_step = (target_step - crush_step_) / num_samples;
    mono_float delta_period = (target_period - crush_period_) / num_samples;
    mono_float step = crush_step_;
    mono_float period = crush_period_;
    mono_float count = crush_count_;
    poly_float hold = crush_hold_;
    poly_float* audio = output()->buffer;

    for (int i = 0; i < num_samples; ++i) {
      step += delta_step;
      period += delta_period;
      count += 1.0f;
      if (count >= period) {
        count -= period;
        hold = utils::round(audio[i] * (1.0f / step)) * step;
      }
      audio[i] = hold;
    }

    crush_step_ = target_step;
    crush_period_ = target_period;
    crush_count_ = count;
    crush_hold_ = hold;
  }
} // namespace vital
