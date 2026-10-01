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
 */

#pragma once

#include "synth_constants.h"
#include "synth_module.h"

#include "delay.h"
#include "operators.h"

namespace vital {

  // GNARL: the delay's STEPS. Divides a tempo-synced delay frequency by the
  // step count, so the delay time is that many note values; a free (Hz)
  // frequency passes untouched. Block-rate, as the frequency it scales.
  class DelayStepsScale : public Operator {
    public:
      enum {
        kFrequency,
        kSteps,
        kSync,
        kNumInputs
      };

      // 16 steps of 1/16 is a bar, the longest a step delay line counts.
      static constexpr mono_float kMaxSteps = 16.0f;

      DelayStepsScale() : Operator(kNumInputs, 1, true) { }

      virtual Processor* clone() const override { return new DelayStepsScale(*this); }

      void process(int num_samples) override {
        poly_float frequency = input(kFrequency)->at(0);
        poly_float steps = utils::clamp(utils::round(input(kSteps)->at(0)), 1.0f, kMaxSteps);
        poly_mask free_mask = poly_float::equal(input(kSync)->at(0), TempoChooser::kFrequencyMode);
        output()->buffer[0] = utils::maskLoad(frequency / steps, frequency, free_mask);
      }

    private:
      JUCE_LEAK_DETECTOR(DelayStepsScale)
  };

  class DelayModule : public SynthModule {
    public:
      static constexpr mono_float kMaxDelayTime = 4.0f;
    
      DelayModule(const Output* beats_per_second);
      virtual ~DelayModule();

      virtual void init() override;
      virtual void hardReset() override { delay_->hardReset(); }
      virtual void enable(bool enable) override {
        SynthModule::enable(enable);
        process(1);
        if (!enable)
          delay_->hardReset();
      }
      virtual void setSampleRate(int sample_rate) override;
      virtual void setOversampleAmount(int oversample) override;
      virtual void processWithInput(const poly_float* audio_in, int num_samples) override;
      virtual Processor* clone() const override { return new DelayModule(*this); }
    
    protected:
      const Output* beats_per_second_;
      StereoDelay* delay_;

      JUCE_LEAK_DETECTOR(DelayModule)
  };
} // namespace vital

