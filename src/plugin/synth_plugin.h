/* Copyright 2013-2019 Matt Tytel
 *
 * pylon is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * pylon is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with pylon.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Modified by Gnarl Audio, 2026: getBridge() for the web panel; the licence
 * check (Phase 7, CMake build only).
 */

#pragma once

#include "JuceHeader.h"

#include "synth_base.h"
#include "value_bridge.h"

// GNARL: the licence check (Phase 7) is in the CMake / JUCE 8 build only,
// whose panel shows its banner; CMakeLists.txt defines this. The Projucer
// builds compile none of it.
#if GNARL_LICENSING
#include "licence/licence_manager.h"
#endif

class ValueBridge;

class SynthPlugin : public SynthBase, public AudioProcessor, public ValueBridge::Listener {
  public:
    static constexpr int kSetProgramWaitMilliseconds = 500;

    SynthPlugin();
    virtual ~SynthPlugin();

    SynthGuiInterface* getGuiInterface() override;
    void beginChangeGesture(const std::string& name) override;
    void endChangeGesture(const std::string& name) override;
    void setValueNotifyHost(const std::string& name, vital::mono_float value) override;
    const CriticalSection& getCriticalSection() override;
    void pauseProcessing(bool pause) override;

    void prepareToPlay(double sample_rate, int buffer_size) override;
    void releaseResources() override;
    void processBlock(AudioSampleBuffer&, MidiBuffer&) override;

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const String getName() const override;
    bool supportsMPE() const override { return true; }

    const String getInputChannelName(int channel_index) const override;
    const String getOutputChannelName(int channel_index) const override;
    bool isInputChannelStereoPair(int index) const override;
    bool isOutputChannelStereoPair(int index) const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool silenceInProducesSilenceOut() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override { }
    const String getProgramName(int index) override;
    void changeProgramName(int index, const String& new_name) override { }

    void getStateInformation(MemoryBlock& destData) override;
    void setStateInformation(const void* data, int size_in_bytes) override;
    AudioProcessorParameter* getBypassParameter() const override { return bypass_parameter_; }

    void parameterChanged(std::string name, vital::mono_float value) override;

    // GNARL: the host-facing parameter for an engine control, or nullptr. The
    // web panel (web_panel.cpp) converts and displays values through it, so
    // it shows exactly what the DAW shows.
    ValueBridge* getBridge(const std::string& name) {
      auto found = bridge_lookup_.find(name);
      return found == bridge_lookup_.end() ? nullptr : found->second;
    }

#if GNARL_LICENSING
    // MESSAGE THREAD. The panel reads the state for its banner and hands a
    // typed key here; nothing on the audio thread touches any of it.
    gnarl::licence::State getLicenceState() const { return licence_->getState(); }
    bool hasLicenceKey() const { return licence_key_.isNotEmpty(); }
    void setLicenceKey(const String& key);
    bool presetSavingAllowed() override;
#endif

  private:
    ValueBridge* bypass_parameter_;
    double last_seconds_time_;

    AudioPlayHead::CurrentPositionInfo position_info_;

    std::map<std::string, ValueBridge*> bridge_lookup_;

#if GNARL_LICENSING
    void startLicensing();

    std::unique_ptr<gnarl::licence::LicenceManager> licence_;
    String licence_key_;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthPlugin)
};

