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
 * Modified by Gnarl Audio, 2026: the licence check (Phase 7, CMake build only).
 */

#include "synth_plugin.h"
#include "synth_editor.h"
#include "sound_engine.h"
#include "load_save.h"

#if GNARL_LICENSING
#include "licence/licence_client.h"
#endif

SynthPlugin::SynthPlugin() {
  last_seconds_time_ = 0.0;

  int num_params = vital::Parameters::getNumParameters();
  for (int i = 0; i < num_params; ++i) {
    const vital::ValueDetails* details = vital::Parameters::getDetails(i);
    if (controls_.count(details->name) == 0)
      continue;

    ValueBridge* bridge = new ValueBridge(details->name, controls_[details->name]);
    bridge->setListener(this);
    bridge_lookup_[details->name] = bridge;
    addParameter(bridge);
  }

  bypass_parameter_ = bridge_lookup_["bypass"];

#if GNARL_LICENSING
  startLicensing();
#endif
}

#if GNARL_LICENSING
// NOTHING HERE TOUCHES AUDIO: the manager decides one thing, whether preset
// files may be saved, and only the message thread asks.
void SynthPlugin::startLicensing() {
  using namespace gnarl::licence;
  licence_ = std::make_unique<LicenceManager>();

#if GNARL_PERSONAL_BUILD
  // Somebody's own instrument, built from source: nothing to check.
  licence_->setPersonal();
#else
  String endpoint = GNARL_LICENCE_ENDPOINT;
  if (endpoint.isEmpty()) {
    // No endpoint configured: this build cannot check, and says so.
    licence_->setUnenforced();
    return;
  }

  licence_key_ = loadKey();
  Time last_verified;
  bool ever_verified = false;
  loadVerification(last_verified, ever_verified);
  licence_->restore(last_verified, ever_verified);
  // A success is kept with the machine's settings, so going offline later
  // starts the grace period from the right day.
  licence_->setListener([this](const State& state) {
    if (state.status == kLicensed)
      saveVerification(licence_->getLastVerified(), true);
  });
  licence_->setVerifier(httpVerifier(endpoint, licence_key_));
  licence_->verify();
#endif
}

void SynthPlugin::setLicenceKey(const String& key) {
  using namespace gnarl::licence;
  licence_key_ = key.trim();
  saveKey(licence_key_);
  String endpoint = GNARL_LICENCE_ENDPOINT;
  if (endpoint.isEmpty() || licence_->getState().status == kPersonal)
    return;
  licence_->setVerifier(httpVerifier(endpoint, licence_key_));
  licence_->verify();
}

bool SynthPlugin::presetSavingAllowed() {
  licence_->refreshGrace();
  gnarl::licence::State state = licence_->getState();
  if (state.featuresAllowed())
    return true;

  // Said where the save was asked for, not silently dropped: the classic
  // editor's save dialog has no other way to learn why nothing happened.
  if (MessageManager::existsAndIsCurrentThread())
    AlertWindow::showMessageBoxAsync(MessageBoxIconType::InfoIcon, "Preset not saved", state.message);
  return false;
}
#endif

SynthPlugin::~SynthPlugin() {
  midi_manager_ = nullptr;
  keyboard_state_ = nullptr;
}

SynthGuiInterface* SynthPlugin::getGuiInterface() {
  AudioProcessorEditor* editor = getActiveEditor();
  if (editor)
    return dynamic_cast<SynthGuiInterface*>(editor);
  return nullptr;
}

void SynthPlugin::beginChangeGesture(const std::string& name) {
  if (bridge_lookup_.count(name))
    bridge_lookup_[name]->beginChangeGesture();
}

void SynthPlugin::endChangeGesture(const std::string& name) {
  if (bridge_lookup_.count(name))
    bridge_lookup_[name]->endChangeGesture();
}

void SynthPlugin::setValueNotifyHost(const std::string& name, vital::mono_float value) {
  if (bridge_lookup_.count(name)) {
    vital::mono_float plugin_value = bridge_lookup_[name]->convertToPluginValue(value);
    bridge_lookup_[name]->setValueNotifyHost(plugin_value);
  }
}

const CriticalSection& SynthPlugin::getCriticalSection() {
  return getCallbackLock();
}

void SynthPlugin::pauseProcessing(bool pause) {
  suspendProcessing(pause);
}

const String SynthPlugin::getName() const {
  return JucePlugin_Name;
}

const String SynthPlugin::getInputChannelName(int channel_index) const {
  return String(channel_index + 1);
}

const String SynthPlugin::getOutputChannelName(int channel_index) const {
  return String(channel_index + 1);
}

bool SynthPlugin::isInputChannelStereoPair(int index) const {
  return true;
}

bool SynthPlugin::isOutputChannelStereoPair(int index) const {
  return true;
}

bool SynthPlugin::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
  return true;
#else
  return false;
#endif
}

bool SynthPlugin::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
  return true;
#else
  return false;
#endif
}

bool SynthPlugin::silenceInProducesSilenceOut() const {
  return false;
}

double SynthPlugin::getTailLengthSeconds() const {
  return 0.0;
}

const String SynthPlugin::getProgramName(int index) {
  SynthGuiInterface* editor = getGuiInterface();
  if (editor == nullptr || editor->getSynth() == nullptr)
    return "";

  return editor->getSynth()->getPresetName();
}

void SynthPlugin::prepareToPlay(double sample_rate, int buffer_size) {
  engine_->setSampleRate(sample_rate);
  engine_->updateAllModulationSwitches();
  midi_manager_->setSampleRate(sample_rate);
}

void SynthPlugin::releaseResources() {
}

void SynthPlugin::processBlock(AudioSampleBuffer& buffer, MidiBuffer& midi_messages) {
  // GNARL: flush denormals to zero for the whole block. Upstream had no
  // protection at all, and GCC 13 no longer adds it through -ffast-math for
  // a shared library. A decaying effect tail otherwise sits in the
  // denormal range for as long as the plugin idles on a track. RAII:
  // restores the host's floating-point mode on return.
  ScopedNoDenormals no_denormals;

  static constexpr double kSecondsPerMinute = 60.0f;

  if (bypass_parameter_->getValue()) {
    processBlockBypassed(buffer, midi_messages);
    return;
  }

  int total_samples = buffer.getNumSamples();
  int num_channels = getTotalNumOutputChannels();
  AudioPlayHead* play_head = getPlayHead();
  if (play_head) {
    play_head->getCurrentPosition(position_info_);
    if (position_info_.bpm)
      engine_->setBpm(position_info_.bpm);

    if (position_info_.isPlaying) {
      double bps = position_info_.bpm / kSecondsPerMinute;
      last_seconds_time_ = position_info_.ppqPosition / bps;
    }
  }

  processModulationChanges();
  if (total_samples)
    processKeyboardEvents(midi_messages, total_samples);

  double sample_time = 1.0 / AudioProcessor::getSampleRate();
  for (int sample_offset = 0; sample_offset < total_samples;) {
    int num_samples = std::min<int>(total_samples - sample_offset, vital::kMaxBufferSize);

    engine_->correctToTime(last_seconds_time_);
    processMidi(midi_messages, sample_offset, sample_offset + num_samples);
    processAudio(&buffer, num_channels, num_samples, sample_offset);

    last_seconds_time_ += num_samples * sample_time;
    sample_offset += num_samples;
  }
}

bool SynthPlugin::hasEditor() const {
  return true;
}

AudioProcessorEditor* SynthPlugin::createEditor() {
  return new SynthEditor(*this);
}

void SynthPlugin::parameterChanged(std::string name, vital::mono_float value) {
  valueChangedExternal(name, value);
}

void SynthPlugin::getStateInformation(MemoryBlock& dest_data) {
  json data = LoadSave::stateToJson(this, getCallbackLock());
  data["tuning"] = getTuning()->stateToJson();

  String data_string = data.dump();
  MemoryOutputStream stream;
  stream.writeString(data_string);
  dest_data.append(stream.getData(), stream.getDataSize());
}

void SynthPlugin::setStateInformation(const void* data, int size_in_bytes) {
  MemoryInputStream stream(data, size_in_bytes, false);
  String data_string = stream.readEntireStreamAsString();

  pauseProcessing(true);
  try {
    json json_data = json::parse(data_string.toStdString());
    LoadSave::jsonToState(this, save_info_, json_data);

    if (json_data.count("tuning"))
      getTuning()->jsonToState(json_data["tuning"]);
  }
  catch (const json::exception& e) {
    std::string error = "There was an error open the preset. Preset file is corrupted.";
    AlertWindow::showNativeDialogBox("Error opening preset", error, false);
  }
  pauseProcessing(false);

  SynthGuiInterface* editor = getGuiInterface();
  if (editor)
    editor->updateFullGui();
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new SynthPlugin();
}
