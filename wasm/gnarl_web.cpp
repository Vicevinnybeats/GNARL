/* Copyright 2026 Gnarl Audio
 *
 * GNARL is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See LICENSE.
 */

// The engine's host in the browser (wasm/README.md, docs/design/phase2-09-
// mobile.md): what SynthBase, ValueBridge and WebPanel do for the plugin,
// without JUCE. It runs inside an AudioWorklet, so every call below happens
// on the audio thread, between blocks - the queue SynthBase needs between
// the UI and the audio thread is the worklet's message port instead.
//
// Allocation happens only in gnarl_init and in gnarl_route (a connection's
// processor, as Vital's matrix allocates one); the per-block calls do not.

#include <emscripten/emscripten.h>

#include "json/json.h"
#include "line_generator.h"
#include "load_save.h"
#include "modulation_connection_processor.h"
#include "sample_source.h"
#include "sound_engine.h"
#include "synth_constants.h"
#include "synth_parameters.h"
#include "synth_types.h"
#include "tuning.h"
#include "wavetable_creator.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {
  // One control as the page sees it: the host parameter of the same name.
  struct Control {
    std::string name;
    vital::Value* value;
    vital::ValueDetails details;
    float span;
  };

  // As WebPanel: the scope's points and the wobble curve's.
  constexpr int kScopePoints = 256;
  constexpr int kCurvePoints = 64;
  constexpr int kScratchFloats = 1024;
  constexpr int kTextSize = 64;
  // As SynthBase.
  constexpr float kOutputWindowMinNote = 16.0f;
  constexpr float kOutputWindowMaxNote = 128.0f;

  std::unique_ptr<vital::SoundEngine> engine;
  std::unique_ptr<WavetableCreator> wavetable_creators[vital::kNumOscillators];
  std::vector<Control> controls;
  std::vector<vital::ModulationConnection*> connections;
  std::string preset_name = "Init";
  std::string preset_json;

  float output[2 * vital::kMaxBufferSize];
  float scratch[kScratchFloats];
  char text[kTextSize];
  char name_buffer[256];

  // SynthBase::updateMemoryOutput's state: the oscilloscope memory, synced to
  // the played note's period.
  vital::poly_float scope_memory[2 * vital::kOscilloscopeMemoryResolution];
  vital::poly_float scope_memory_write[2 * vital::kOscilloscopeMemoryResolution];
  float memory_reset_period = vital::kOscilloscopeMemoryResolution;
  float memory_input_offset = 0.0f;
  int memory_index = 0;
  float last_played_note = 0.0f;
  int last_num_pressed = 0;

  // ValueBridge::convertToEngineValue.
  float toEngine(const Control& control, float host) {
    float value = host * control.span + control.details.min;
    if (control.details.value_scale == vital::ValueDetails::kIndexed)
      return std::round(value);
    return value;
  }

  // ValueBridge::convertToPluginValue.
  float toHost(const Control& control, float value) {
    return (value - control.details.min) / control.span;
  }

  // ValueBridge::skewValue.
  float skew(const vital::ValueDetails& details, float value) {
    switch (details.value_scale) {
      case vital::ValueDetails::kQuadratic:
        return value * value;
      case vital::ValueDetails::kCubic:
        return value * value * value;
      case vital::ValueDetails::kQuartic:
        value *= value;
        return value * value;
      case vital::ValueDetails::kExponential:
        if (details.display_invert)
          return 1.0f / powf(2.0f, value);
        return powf(2.0f, value);
      case vital::ValueDetails::kSquareRoot:
        return sqrtf(value);
      default:
        return value;
    }
  }

  // The wheels also go to the engine's MIDI state, as Vital's own wheels do
  // (BendSection::sliderValueChanged); oversampling is applied on change, as
  // SynthBase::checkOversampling is.
  void applyValue(Control& control, float value) {
    control.value->set(value);
    if (control.name == "pitch_wheel")
      engine->setZonedPitchWheel(value, 0, vital::kNumMidiChannels - 1);
    else if (control.name == "mod_wheel")
      engine->setModWheelAllChannels(value);
    else if (control.name == "oversampling")
      engine->checkOversampling();
  }

  vital::ModulationConnection* findConnection(const std::string& source, const std::string& destination) {
    for (vital::ModulationConnection* connection : connections) {
      if (connection->source_name == source && connection->destination_name == destination)
        return connection;
    }
    return nullptr;
  }

  // SynthBase::createModulationChange.
  vital::modulation_change createChange(vital::ModulationConnection* connection) {
    vital::modulation_change change;
    change.source = engine->getModulationSource(connection->source_name);
    change.mono_destination = engine->getMonoModulationDestination(connection->destination_name);
    change.mono_modulation_switch = engine->getMonoModulationSwitch(connection->destination_name);
    change.destination_scale = vital::Parameters::getParameterRange(connection->destination_name);
    change.poly_modulation_switch = engine->getPolyModulationSwitch(connection->destination_name);
    change.poly_destination = engine->getPolyModulationDestination(connection->destination_name);
    change.modulation_processor = connection->modulation_processor.get();

    int num_audio_rate = 0;
    vital::ModulationConnectionBank& bank = engine->getModulationBank();
    for (int i = 0; i < vital::kMaxModulationConnections; ++i) {
      if (bank.atIndex(i)->source_name == connection->source_name &&
          bank.atIndex(i)->destination_name != connection->destination_name &&
          !bank.atIndex(i)->modulation_processor->isControlRate()) {
        num_audio_rate++;
      }
    }
    change.num_audio_rate = num_audio_rate;
    change.disconnecting = false;
    return change;
  }

  int connectionIndex(vital::ModulationConnection* connection) {
    vital::ModulationConnectionBank& bank = engine->getModulationBank();
    for (int i = 0; i < vital::kMaxModulationConnections; ++i) {
      if (bank.atIndex(i) == connection)
        return i;
    }
    return -1;
  }

  float amountOf(const std::string& name) {
    for (Control& control : controls) {
      if (control.name == name)
        return control.value->value();
    }
    return 0.0f;
  }

  vital::Value* amountControl(int index) {
    std::string name = "modulation_" + std::to_string(index + 1) + "_amount";
    for (Control& control : controls) {
      if (control.name == name)
        return control.value;
    }
    return nullptr;
  }

  // SynthBase::clearModulations.
  void clearModulations() {
    for (vital::ModulationConnection* connection : connections) {
      vital::modulation_change change = createChange(connection);
      change.disconnecting = true;
      engine->disconnectModulation(change);
      connection->source_name = "";
      connection->destination_name = "";
    }
    connections.clear();
    vital::ModulationConnectionBank& bank = engine->getModulationBank();
    for (int i = 0; i < static_cast<int>(bank.numConnections()); ++i)
      bank.atIndex(i)->modulation_processor->lineMapGenerator()->initLinear();
    engine->disableUnnecessaryModSources();
  }

  // A connection in a given slot: a preset's modulation N is slot N, because
  // its amount is the host parameter modulation_N+1_amount.
  bool connectAt(int index, const std::string& source, const std::string& destination) {
    if (engine->getModulationSource(source) == nullptr || engine->getMonoModulationDestination(destination) == nullptr)
      return false;
    vital::ModulationConnection* connection = engine->getModulationBank().atIndex(index);
    connection->source_name = source;
    connection->destination_name = destination;
    vital::modulation_change change = createChange(connection);
    if (change.poly_destination && change.poly_destination->router() == change.modulation_processor) {
      connection->source_name = "";
      connection->destination_name = "";
      return false;
    }
    engine->connectModulation(change);
    connections.push_back(connection);
    return true;
  }

  // LoadSave::stateToJson, without SynthBase.
  json saveState(const std::string& name) {
    json settings;
    for (Control& control : controls)
      settings[control.name] = control.value->value();
    settings["sample"] = engine->getSample()->stateToJson();

    json modulations;
    vital::ModulationConnectionBank& bank = engine->getModulationBank();
    for (int i = 0; i < vital::kMaxModulationConnections; ++i) {
      vital::ModulationConnection* connection = bank.atIndex(i);
      json modulation;
      modulation["source"] = connection->source_name;
      modulation["destination"] = connection->destination_name;
      LineGenerator* mapping = connection->modulation_processor->lineMapGenerator();
      if (!mapping->linear())
        modulation["line_mapping"] = mapping->stateToJson();
      modulations.push_back(modulation);
    }
    settings["modulations"] = modulations;

    json wavetables;
    for (int i = 0; i < vital::kNumOscillators; ++i)
      wavetables.push_back(wavetable_creators[i]->stateToJson());
    settings["wavetables"] = wavetables;

    json lfos;
    for (int i = 0; i < vital::kNumLfos; ++i)
      lfos.push_back(engine->getLfoSource(i)->stateToJson());
    settings["lfos"] = lfos;
    settings["wobble_shape"] = engine->getWobbleSource()->stateToJson();

    json data;
    data["synth_version"] = ProjectInfo::versionString;
    data["preset_name"] = name;
    data["author"] = "";
    data["comments"] = "Saved by GNARL's mobile version.";
    data["preset_style"] = "";
    for (int i = 0; i < vital::kNumMacros; ++i)
      data["macro" + std::to_string(i + 1)] = "";
    data["settings"] = settings;
    return data;
  }

  // The version as a number per dot-separated part, for "older than us".
  int compareVersions(const std::string& a, const std::string& b) {
    return LoadSave::compareVersionStrings(a, b);
  }

  // LoadSave::jsonToState for a patch in the current format. 0 loaded; 1 not
  // a patch; 2 from a newer GNARL; 3 from an older version, whose migration
  // (LoadSave::updateFromOldVersion) only the plugin has.
  int loadState(json& data) {
    if (!data.is_object() || !data.count("settings") || !data["settings"].is_object() ||
        !data.count("synth_version") || !data["synth_version"].is_string())
      return 1;
    std::string version = data["synth_version"];
    int compared = compareVersions(version, ProjectInfo::versionString);
    // A newer Vital 1.x patch that uses nothing this engine lacks loads as it
    // is (LoadSave::readableNewerPatch), as in the plugin.
    if (LoadSave::compareFeatureVersionStrings(version, ProjectInfo::versionString) > 0 &&
        !LoadSave::readableNewerPatch(data))
      return 2;
    json& settings = data["settings"];
    if (compared < 0 || settings.count("sub_octave"))
      return 3;

    engine->allSoundsOff();
    for (Control& control : controls) {
      if (settings.count(control.name) && settings[control.name].is_number())
        control.value->set(settings[control.name].get<float>());
      else
        control.value->set(control.details.default_value);
    }
    engine->setModWheelAllChannels(amountOf("mod_wheel"));

    clearModulations();
    if (settings.count("modulations") && settings["modulations"].is_array()) {
      int index = 0;
      for (json& modulation : settings["modulations"]) {
        if (index >= vital::kMaxModulationConnections)
          break;
        int slot = index++;
        if (!modulation.is_object() || !modulation["source"].is_string() || !modulation["destination"].is_string())
          continue;
        std::string source = modulation["source"];
        std::string destination = modulation["destination"];
        LineGenerator* mapping = engine->getModulationBank().atIndex(slot)->modulation_processor->lineMapGenerator();
        if (!source.empty() && !destination.empty())
          connectAt(slot, source, destination);
        if (modulation.count("line_mapping"))
          mapping->jsonToState(modulation["line_mapping"]);
        else
          mapping->initLinear();
      }
    }

    if (settings.count("sample") && settings["sample"].is_object())
      engine->getSample()->jsonToState(settings["sample"]);
    if (settings.count("wavetables") && settings["wavetables"].is_array()) {
      int i = 0;
      for (json& wavetable : settings["wavetables"]) {
        if (i >= vital::kNumOscillators)
          break;
        wavetable_creators[i]->jsonToState(wavetable);
        wavetable_creators[i]->render();
        i++;
      }
    }
    if (settings.count("lfos") && settings["lfos"].is_array()) {
      int i = 0;
      for (json& lfo : settings["lfos"]) {
        if (i >= vital::kNumLfos)
          break;
        engine->getLfoSource(i)->jsonToState(lfo);
        engine->getLfoSource(i)->render();
        i++;
      }
    }
    LineGenerator* wobble = engine->getWobbleSource();
    if (settings.count("wobble_shape") && settings["wobble_shape"].is_object())
      wobble->jsonToState(settings["wobble_shape"]);
    else
      wobble->initTriangle();
    wobble->render();
    engine->checkOversampling();

    preset_name = data.count("preset_name") && data["preset_name"].is_string() ? data["preset_name"].get<std::string>() : "";
    if (preset_name.empty())
      preset_name = "Untitled";
    return 0;
  }

  // SynthBase::updateMemoryOutput, without the audio memory nothing reads.
  void updateScope(int samples, const vital::poly_float* audio) {
    float last_played = engine->getLastActiveNote();
    last_played = vital::utils::clamp(last_played, kOutputWindowMinNote, kOutputWindowMaxNote);

    int num_pressed = engine->getNumPressedNotes();
    int output_inc = std::max<int>(1, engine->getSampleRate() / vital::kOscilloscopeMemorySampleRate);
    int oscilloscope_samples = 2 * vital::kOscilloscopeMemoryResolution;

    if (last_played && (last_played_note != last_played || num_pressed > last_num_pressed)) {
      last_played_note = last_played;

      float frequency = vital::utils::midiNoteToFrequency(last_played_note);
      float period = engine->getSampleRate() / frequency;
      int window_length = output_inc * vital::kOscilloscopeMemoryResolution;

      memory_reset_period = period;
      while (memory_reset_period < window_length)
        memory_reset_period += memory_reset_period;

      memory_reset_period = std::min(memory_reset_period, 2.0f * window_length);
      memory_index = 0;
      vital::utils::copyBuffer(scope_memory, scope_memory_write, oscilloscope_samples);
    }
    last_num_pressed = num_pressed;

    for (; memory_input_offset < samples; memory_input_offset += output_inc) {
      int input_index = vital::utils::iclamp(memory_input_offset, 0, samples);
      memory_index = vital::utils::iclamp(memory_index, 0, oscilloscope_samples - 1);
      scope_memory_write[memory_index++] = audio[input_index];

      if (memory_index * output_inc >= memory_reset_period) {
        memory_input_offset += memory_reset_period - memory_index * output_inc;
        memory_index = 0;
        vital::utils::copyBuffer(scope_memory, scope_memory_write, oscilloscope_samples);
      }
    }

    memory_input_offset -= samples;
  }
}

// The voices call this only when a tuning is set (VoiceHandler checks), and
// the web build loads no tuning file, so it is never reached. tuning.cpp needs
// JUCE's files, so the engine's one reference to it is met here: 12-TET.
vital::mono_float Tuning::convertMidiNote(int note) const {
  return note;
}

extern "C" {
  // SynthBase's constructor and initEngine: the engine, the init wavetables,
  // triangle LFOs and the wobble, every control at its default.
  EMSCRIPTEN_KEEPALIVE int gnarl_init(int sample_rate) {
    engine = std::make_unique<vital::SoundEngine>();
    for (int i = 0; i < vital::kNumOscillators; ++i) {
      vital::Wavetable* wavetable = engine->getWavetable(i);
      if (wavetable) {
        wavetable_creators[i] = std::make_unique<WavetableCreator>(wavetable);
        wavetable_creators[i]->init();
      }
    }
    engine->getSample()->init();
    for (int i = 0; i < vital::kNumLfos; ++i)
      engine->getLfoSource(i)->initTriangle();
    engine->getWobbleSource()->initTriangle();

    controls.clear();
    for (auto& [name, value] : engine->getControls()) {
      vital::ValueDetails details = vital::Parameters::getDetails(name);
      value->set(details.default_value);
      float span = details.max - details.min;
      if (details.value_scale == vital::ValueDetails::kIndexed)
        span = std::round(span);
      controls.push_back({ name, value, details, span });
    }
    connections.reserve(vital::kMaxModulationConnections);
    engine->disableUnnecessaryModSources();
    engine->checkOversampling();
    engine->setSampleRate(sample_rate);
    engine->updateAllModulationSwitches();
    return static_cast<int>(controls.size());
  }

  // SynthBase::initEngine on the running engine: the init patch.
  EMSCRIPTEN_KEEPALIVE void gnarl_reset() {
    engine->allSoundsOff();
    clearModulations();
    for (int i = 0; i < vital::kNumOscillators; ++i)
      wavetable_creators[i]->init();
    engine->getSample()->init();
    for (int i = 0; i < vital::kNumLfos; ++i)
      engine->getLfoSource(i)->initTriangle();
    engine->getWobbleSource()->initTriangle();
    for (Control& control : controls)
      control.value->set(control.details.default_value);
    engine->checkOversampling();
    preset_name = "Init";
  }

  // A 256-byte buffer the page writes a name into before the calls that take one.
  EMSCRIPTEN_KEEPALIVE char* gnarl_name_buffer() { return name_buffer; }
  EMSCRIPTEN_KEEPALIVE float* gnarl_scratch() { return scratch; }

  // The index of the control named in the name buffer, or -1.
  EMSCRIPTEN_KEEPALIVE int gnarl_control_index() {
    for (size_t i = 0; i < controls.size(); ++i) {
      if (controls[i].name == name_buffer)
        return static_cast<int>(i);
    }
    return -1;
  }

  EMSCRIPTEN_KEEPALIVE float gnarl_get(int index) {
    return toHost(controls[index], controls[index].value->value());
  }

  // WebPanel::setValue: the host value 0..1, through the bridge's conversion.
  EMSCRIPTEN_KEEPALIVE void gnarl_set(int index, float host) {
    Control& control = controls[index];
    applyValue(control, toEngine(control, std::min(1.0f, std::max(0.0f, host))));
  }

  // A value in the engine's own units, as a preset stores it. For applying a
  // patch's settings (tools/web_render.mjs); the page sends host values.
  EMSCRIPTEN_KEEPALIVE void gnarl_set_value(int index, float value) {
    applyValue(controls[index], value);
  }

  // ValueBridge::getNumSteps for a discrete control, else 0.
  EMSCRIPTEN_KEEPALIVE int gnarl_steps(int index) {
    const Control& control = controls[index];
    constexpr int kMaxIndexedSteps = 300;
    if (control.details.value_scale == vital::ValueDetails::kIndexed && control.span < kMaxIndexedSteps)
      return 1 + static_cast<int>(control.span);
    return 0;
  }

  // ValueBridge::getText for the control's current value. The page tidies
  // the number (bridge.ts tidyText), so plain %g is enough here.
  EMSCRIPTEN_KEEPALIVE const char* gnarl_text(int index) {
    const Control& control = controls[index];
    float value = control.value->value();
    if (control.details.string_lookup) {
      int lookup = std::max<int>(0, std::min<float>(value, control.details.max));
      snprintf(text, kTextSize, "%s", control.details.string_lookup[lookup].c_str());
    }
    else {
      float display = control.details.display_multiply * skew(control.details, value) + control.details.post_offset;
      snprintf(text, kTextSize, "%g%s", display, control.details.display_units.c_str());
    }
    return text;
  }

  EMSCRIPTEN_KEEPALIVE void gnarl_note(int note, int on) {
    // The renderer's velocity and lift (SynthBase::renderAudioToFile), so a
    // browser render compares with a desktop one. (The plugin's keyboard
    // state sends velocity 1.0 from the panel.)
    if (on)
      engine->noteOn(note, 0.7f, 0, 0);
    else
      engine->noteOff(note, 0.5f, 0, 0);
  }

  EMSCRIPTEN_KEEPALIVE void gnarl_all_notes_off() {
    engine->allNotesOff(0, 0);
  }

  // The transport position, as the plugin passes a playing host's
  // (SoundEngine::correctToTime): tempo-synced modulation follows it.
  EMSCRIPTEN_KEEPALIVE void gnarl_time(double seconds) {
    engine->correctToTime(seconds);
  }

  EMSCRIPTEN_KEEPALIVE void gnarl_bpm(float bpm) {
    engine->setBpm(bpm);
  }

  // One block, at most kMaxBufferSize samples, interleaved left/right.
  EMSCRIPTEN_KEEPALIVE float* gnarl_process(int num_samples) {
    num_samples = std::min(num_samples, vital::kMaxBufferSize);
    engine->process(num_samples);
    const vital::poly_float* audio = engine->output(0)->buffer;
    for (int i = 0; i < num_samples; ++i) {
      output[2 * i] = audio[i][0];
      output[2 * i + 1] = audio[i][1];
    }
    updateScope(num_samples, audio);
    return output;
  }

  // The scope as WebPanel sends it: kScopePoints of the left channel.
  EMSCRIPTEN_KEEPALIVE float* gnarl_scope() {
    constexpr int kStride = 2 * vital::kOscilloscopeMemoryResolution / kScopePoints;
    for (int i = 0; i < kScopePoints; ++i)
      scratch[i] = scope_memory[i * kStride][0];
    return scratch;
  }

  // Where the wobble is, or -1 when no voice is playing (WebPanel).
  EMSCRIPTEN_KEEPALIVE float gnarl_wobble_phase() {
    const vital::StatusOutput* phase = engine->getStatusOutput("wobble_phase");
    if (phase == nullptr)
      return -1.0f;
    vital::poly_float encoded = phase->value();
    if (phase->isClearValue(encoded))
      return -1.0f;
    return vital::utils::decodePhaseAndVoice(encoded).first[0];
  }

  // WebPanel::setWobbleShape: 0 sine, 1 square, 2 drawn from scratch[0..n).
  EMSCRIPTEN_KEEPALIVE void gnarl_wobble_shape(int kind, int num_steps) {
    LineGenerator* line = engine->getWobbleSource();
    if (kind == 0)
      line->initSin();
    else if (kind == 1) {
      line->initSquare();
      line->setSmooth(true);
    }
    else if (kind == 2) {
      num_steps = std::min<int>(num_steps, LineGenerator::kMaxPoints - 1);
      if (num_steps < 2)
        return;
      line->setNumPoints(num_steps + 1);
      for (int i = 0; i <= num_steps; ++i) {
        float value = std::min(1.0f, std::max(0.0f, scratch[i % num_steps]));
        line->setPoint(i, { (1.0f * i) / num_steps, 1.0f - value });
        line->setPower(i, 0.0f);
      }
      line->setSmooth(false);
      line->render();
    }
  }

  // WebPanel::wobbleCurve, into scratch.
  EMSCRIPTEN_KEEPALIVE float* gnarl_wobble_curve() {
    LineGenerator* line = engine->getWobbleSource();
    for (int i = 0; i < kCurvePoints; ++i)
      scratch[i] = line->valueAtPhase((1.0f * i) / kCurvePoints);
    return scratch;
  }

  // WebPanel::route: the source and destination are the name buffer's two
  // NUL-separated strings. Connect (or retune) with an amount -1..1, or remove.
  EMSCRIPTEN_KEEPALIVE int gnarl_route(float amount, int remove) {
    std::string source = name_buffer;
    std::string destination = name_buffer + source.size() + 1;
    if (source.empty() || destination.empty())
      return -1;

    vital::ModulationConnection* connection = findConnection(source, destination);
    if (remove) {
      if (connection == nullptr)
        return -1;
      vital::modulation_change change = createChange(connection);
      change.disconnecting = true;
      engine->disconnectModulation(change);
      connection->source_name = "";
      connection->destination_name = "";
      connections.erase(std::find(connections.begin(), connections.end(), connection));
      return 0;
    }

    if (connection == nullptr) {
      if (engine->getModulationSource(source) == nullptr || engine->getMonoModulationDestination(destination) == nullptr)
        return -1;
      connection = engine->getModulationBank().createConnection(source, destination);
      if (connection == nullptr)
        return -1;
      vital::modulation_change change = createChange(connection);
      if (change.poly_destination && change.poly_destination->router() == change.modulation_processor) {
        connection->source_name = "";
        connection->destination_name = "";
        return -1;
      }
      engine->connectModulation(change);
      connections.push_back(connection);
    }

    int index = connectionIndex(connection);
    if (vital::Value* amount_value = amountControl(index))
      amount_value->set(std::min(1.0f, std::max(-1.0f, amount)));
    return index;
  }

  // The matrix, for the page: count, then each connection's names in the
  // name buffer (NUL-separated) and its amount as the return value.
  EMSCRIPTEN_KEEPALIVE int gnarl_route_count() {
    return static_cast<int>(connections.size());
  }

  EMSCRIPTEN_KEEPALIVE float gnarl_route_at(int i) {
    vital::ModulationConnection* connection = connections[i];
    size_t source_size = connection->source_name.size();
    size_t destination_size = connection->destination_name.size();
    if (source_size + destination_size + 2 > sizeof(name_buffer))
      return 0.0f;
    memcpy(name_buffer, connection->source_name.c_str(), source_size + 1);
    memcpy(name_buffer + source_size + 1, connection->destination_name.c_str(), destination_size + 1);
    vital::Value* amount_value = amountControl(connectionIndex(connection));
    return amount_value ? amount_value->value() : 0.0f;
  }

  // The patch as a .vital file's JSON (LoadSave::stateToJson's format), named
  // from the name buffer. The text stays valid until the next call. Builds a
  // few hundred kB of JSON: a pause the page asks for, between blocks.
  EMSCRIPTEN_KEEPALIVE const char* gnarl_save() {
    try {
      preset_name = name_buffer[0] ? name_buffer : "Untitled";
      preset_json = saveState(preset_name).dump();
    }
    catch (const std::exception&) {
      preset_json.clear();
    }
    return preset_json.c_str();
  }

  // A buffer of `size` bytes for the page to write a patch into.
  EMSCRIPTEN_KEEPALIVE char* gnarl_load_buffer(int size) {
    preset_json.assign(static_cast<size_t>(std::max(0, size)) + 1, '\0');
    return &preset_json[0];
  }

  // Loads the patch in the load buffer: 0 loaded; 1 not a patch; 2 from a
  // newer GNARL; 3 needs the plugin's migration. A file that breaks the
  // parser or the wavetable reader is caught here (this file and the
  // wavetable code are built with exceptions; the engine is not), and the
  // engine goes back to the init patch rather than half of one.
  EMSCRIPTEN_KEEPALIVE int gnarl_load() {
    try {
      json data = json::parse(preset_json.c_str(), nullptr, false);
      if (data.is_discarded())
        return 1;
      return loadState(data);
    }
    catch (const std::exception&) {
      gnarl_reset();
      return 1;
    }
  }

  EMSCRIPTEN_KEEPALIVE const char* gnarl_preset_name() {
    return preset_name.c_str();
  }

  // OSC `osc`'s table from the wavetable JSON in the load buffer
  // (ui/src/wavetables.ts): 0 loaded, 1 refused (the table is unchanged, or
  // the init table if the reader failed part-way).
  EMSCRIPTEN_KEEPALIVE int gnarl_load_wavetable(int osc) {
    if (osc < 0 || osc >= vital::kNumOscillators || !wavetable_creators[osc])
      return 1;
    json data = json::parse(preset_json.c_str(), nullptr, false);
    if (data.is_discarded() || !data.is_object())
      return 1;
    try {
      wavetable_creators[osc]->jsonToState(data);
      return 0;
    }
    catch (const std::exception&) {
      wavetable_creators[osc]->init();
      return 1;
    }
  }

  // OSC `osc`'s table name. Valid until the next call.
  EMSCRIPTEN_KEEPALIVE const char* gnarl_wavetable_name(int osc) {
    static std::string name;
    name = osc >= 0 && osc < vital::kNumOscillators && wavetable_creators[osc] ?
           wavetable_creators[osc]->getName() : std::string();
    return name.c_str();
  }
}
