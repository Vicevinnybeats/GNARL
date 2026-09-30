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
 * Modified by Gnarl Audio, 2026: WobbleRate and WobbleModule
 * added (the riddim wobble macro, docs/design/phase2-01-wobble.md).
 */

#pragma once

#include "synth_module.h"

class LineGenerator;

namespace vital {
  class SynthLfo;

  class LfoModule : public SynthModule {
    public:
      enum {
        kNoteTrigger,
        kNoteCount,
        kMidi,
        kNumInputs
      };

      enum {
        kValue,
        kOscPhase,
        kOscFrequency,
        kNumOutputs
      };

      LfoModule(const std::string& prefix, LineGenerator* line_generator, const Output* beats_per_second);
      virtual ~LfoModule() { }

      void init() override;
      virtual Processor* clone() const override { return new LfoModule(*this); }
      void correctToTime(double seconds) override;
      void setControlRate(bool control_rate) override;

    protected:
      std::string prefix_;
      SynthLfo* lfo_;
      const Output* beats_per_second_;

      JUCE_LEAK_DETECTOR(LfoModule)
  };

  // GNARL: the wobble rate is ONE parameter, because Vital's tempo sync is two
  // (a division index and a triplet/dotted mode) and 1/8T needs both. A macro
  // turning the division alone steps 1/4 -> 1/8 -> 1/16 and skips the triplet.
  // Block-rate: one frequency per block, like TempoChooser.
  class WobbleRate : public Processor {
    public:
      enum {
        kRate,
        kBeatsPerSecond,
        kNumInputs
      };

      // Cycles per BEAT for each wobble_rate index: 1/4, 1/8, 1/8T, 1/16.
      // APPEND ONLY - a preset stores the index, so inserting a rate would
      // repoint every saved patch that uses a later one.
      static constexpr int kNumRates = 4;
      static constexpr mono_float kCyclesPerBeat[kNumRates] = { 1.0f, 2.0f, 3.0f, 4.0f };

      WobbleRate() : Processor(kNumInputs, 1, true) { }
      virtual Processor* clone() const override { return new WobbleRate(*this); }
      void process(int num_samples) override;

    private:
      JUCE_LEAK_DETECTOR(WobbleRate)
  };

  // GNARL: a tempo-synced LFO whose phase is taken from the host transport,
  // so every note's wobble lands on the grid rather than restarting wherever
  // the note began. Its shape is a LineGenerator, drawn like any LFO's.
  class WobbleModule : public SynthModule {
    public:
      enum {
        kNoteTrigger,
        kNoteCount,
        kNumInputs
      };

      enum {
        kValue,
        kOscPhase,
        kOscFrequency,
        kNumOutputs
      };

      WobbleModule(LineGenerator* line_generator, const Output* beats_per_second);
      virtual ~WobbleModule() { }

      void init() override;
      virtual Processor* clone() const override { return new WobbleModule(*this); }
      void correctToTime(double seconds) override;

      // MUST forward to the inner SynthLfo, exactly as LfoModule does. The
      // first build did not: setControlRate(true) in the constructor reached
      // the module and not the LFO inside it, so the LFO ran at audio rate
      // while the route read only the first sample of each block - a
      // sample-and-hold stepping once per block. Measured: the wobble matched
      // Vital's own LFO to -59 dB at block 1 and to only -11 dB at block 128.
      void setControlRate(bool control_rate) override;

    protected:
      SynthLfo* lfo_;
      const Output* beats_per_second_;

      JUCE_LEAK_DETECTOR(WobbleModule)
  };
} // namespace vital

