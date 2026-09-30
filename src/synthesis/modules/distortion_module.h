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

#pragma once

#include "synth_constants.h"
#include "synth_module.h"

namespace vital {

  class Distortion;
  class DigitalSvf;

  class DistortionModule : public SynthModule {
    public:
      DistortionModule();
      virtual ~DistortionModule();

      virtual void init() override;
      virtual void setSampleRate(int sample_rate) override;
      virtual void processWithInput(const poly_float* audio_in, int num_samples) override;
      virtual Processor* clone() const override { return new DistortionModule(*this); }
      virtual void hardReset() override;

      // GNARL: the slot holds a chain - Vital's drive stage (distortion_on),
      // then FOLD, then CRUSH - and runs while ANY of them is on. The effect
      // chain tells the module whether the drive stage itself is on.
      void setDriveStageOn(bool on) { drive_stage_on_ = on; }
      bool chainStageOn() const { return fold_on_->value() != 0.0f || crush_on_->value() != 0.0f; }

      // CRUSH's longest hold, in engine samples: 64 is a sample rate of
      // 1.4 kHz at 88.2 kHz (2x oversampling) - the grit end of a riddim
      // crush - reached at rate 100%. Rate 0% holds 1 sample: no effect.
      static constexpr mono_float kMaxCrushHold = 64.0f;
      static constexpr mono_float kMinCrushBits = 1.0f;
      static constexpr mono_float kMaxCrushBits = 16.0f;

    protected:
      void processFold(int num_samples);
      void processCrush(int num_samples);

      Distortion* distortion_;
      Value* filter_order_;
      DigitalSvf* filter_;
      Output* distortion_mix_;
      poly_float mix_;

      bool drive_stage_on_;
      Value* fold_on_;
      Value* fold_type_;
      Value* fold_distortion_type_;
      Distortion* fold_;
      Output* fold_mix_;
      poly_float fold_mix_value_;
      Value* crush_on_;
      Output* crush_bits_;
      Output* crush_rate_;
      mono_float crush_step_;
      mono_float crush_period_;
      poly_float crush_hold_;
      mono_float crush_count_;

      JUCE_LEAK_DETECTOR(DistortionModule)
  };
} // namespace vital

