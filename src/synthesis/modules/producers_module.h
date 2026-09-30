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
 * Modified by Gnarl Audio, 2026: SubOscillator added (the clean mono sub,
 * docs/design/phase2-05-mono-sub.md).
 */

#pragma once

#include "synth_module.h"
#include "oscillator_module.h"
#include "sample_module.h"

namespace vital {
  class ProducersModule : public SynthModule {
    public:
      enum {
        kReset,
        kRetrigger,
        kMidi,
        kActiveVoices,
        kNoteCount,
        kNumInputs
      };

      enum {
        kToFilter1,
        kToFilter2,
        kRawOut,
        kDirectOut,
        kNumOutputs
      };

      static force_inline int getFirstModulationIndex(int index) {
        return index == 0 ? 1 : 0;
      }

      static force_inline int getSecondModulationIndex(int index) {
        return index == 1 ? 2 : (getFirstModulationIndex(index) + 1);
      }

      ProducersModule();
      virtual ~ProducersModule() { }

      void process(int num_samples) override;
      void init() override;
      virtual Processor* clone() const override { return new ProducersModule(*this); }

      Wavetable* getWavetable(int index) {
        return oscillators_[index]->getWavetable();
      }

      Sample* getSample() { return sampler_->getSample(); }
      Output* samplePhaseOutput() { return sampler_->getPhaseOutput(); }
      void setFilter1On(const Value* on) { filter1_on_ = on; }
      void setFilter2On(const Value* on) { filter2_on_ = on; }

    protected:
      bool isFilter1On() { return filter1_on_ == nullptr || filter1_on_->value() != 0.0f; }
      bool isFilter2On() { return filter2_on_ == nullptr || filter2_on_->value() != 0.0f; }
      OscillatorModule* oscillators_[kNumOscillators];
      Value* oscillator_destinations_[kNumOscillators];
      Value* sample_destination_;
      SampleModule* sampler_;

      const Value* filter1_on_;
      const Value* filter2_on_;

      JUCE_LEAK_DETECTOR(ProducersModule)
  };

  // GNARL: the clean mono sub. A sine at the played note, one or two octaves
  // down, ADDED to the producers' direct output - which the voice handler
  // multiplies by the amp envelope and SoundEngine sums AFTER the effect
  // chain. So the sub rides env 1 like an oscillator but no filter or effect
  // touches it, and both channels carry identical samples.
  //
  // Rates: the sine is sample-rate; pitch, level and drive are block-rate,
  // interpolated across the block; the phase resets at the exact sample a
  // note starts.
  class SubOscillator : public Processor {
    public:
      enum {
        kAudio,
        kOn,
        kLevel,
        kOctave,
        kDrive,
        kMidi,
        kReset,
        kNumInputs
      };

      // Drive shapes with tanh(k x) / tanh(k), k = kMaxDriveK * drive. At 7
      // a sine is close to a rounded square: a strong 3rd harmonic, which
      // is what lets a sub be heard on a small speaker, without the
      // buzz of a hard clip. Measured in tests/test_sub.py.
      static constexpr mono_float kMaxDriveK = 7.0f;
      // Below this k the shaper is skipped: tanh(k x) / tanh(k) -> x, and a
      // ratio of two tiny numbers is noise.
      static constexpr mono_float kMinDriveK = 1.0e-3f;
      // Entries in the table of RMS make-up gains over drive 0..1. The gain
      // curve is smooth, and linear interpolation over 64 steps is within
      // 0.01 dB of it.
      static constexpr int kGainTableSize = 65;

      SubOscillator();
      virtual Processor* clone() const override { return new SubOscillator(*this); }
      void process(int num_samples) override;
      bool hasState() const override { return true; }

    private:
      mono_float makeUpGain(mono_float drive) const;

      poly_float phase_;
      poly_float phase_delta_;
      poly_float level_;
      poly_float drive_;
      poly_float gain_;
      mono_float gain_table_[kGainTableSize];

      JUCE_LEAK_DETECTOR(SubOscillator)
  };
} // namespace vital

