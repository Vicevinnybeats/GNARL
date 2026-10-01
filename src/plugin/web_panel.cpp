/* Copyright 2026 Gnarl Audio
 *
 * GNARL is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See LICENSE.
 */

#include "web_panel.h"

#include "GnarlUiData.h"
#include "line_generator.h"
#include "synth_constants.h"
#include "synth_module.h"
#include "synth_plugin.h"
#include "value_bridge.h"

// The page talks to this class through JUCE's native integration:
//
//   page -> here   gnarlConnect(names)          a native function; returns the
//                                               current value and display text
//                                               of every engine name the page
//                                               binds, and which are stepped
//                  gnarlSet {name, value}       value is the host's 0..1
//                  gnarlGesture {name, begin}   a knob grabbed / released
//                  gnarlNote {note, on}         play a note (hold the scope)
//                  gnarlWobbleShape {kind, points}
//                  gnarlRoute {source, destination, amount}   set a matrix
//                                               connection (amount -1..1), or
//                                               remove it with remove: true
//                  gnarlClassic {}              show Vital's editor
//
//   here -> page   gnarlValues {name: [value, text]}     only what changed
//                  gnarlRoutes [{source, destination, amount}]  the matrix,
//                                               whenever it changes
//                  gnarlFrame {scope, wobblePhase, preset, curve?}
//
// ui/src/bridge.ts is the other half.

namespace {
  const Identifier kSet("gnarlSet");
  const Identifier kGesture("gnarlGesture");
  const Identifier kNote("gnarlNote");
  const Identifier kWobbleShape("gnarlWobbleShape");
  const Identifier kClassic("gnarlClassic");
  const Identifier kRoute("gnarlRoute");
  const Identifier kRoutes("gnarlRoutes");
  const Identifier kValues("gnarlValues");
  const Identifier kFrame("gnarlFrame");

  // A value closer than this to the last one sent is not a change: it keeps
  // float noise from a smoothed control out of the page's traffic.
  constexpr float kValueEpsilon = 1.0e-5f;
}

WebPanel::WebPanel(SynthPlugin& synth, std::function<void()> show_classic_editor) :
    synth_(synth), show_classic_editor_(std::move(show_classic_editor)) {
  browser_ = std::make_unique<WebBrowserComponent>(makeOptions());
  addAndMakeVisible(browser_.get());
  browser_->goToURL(WebBrowserComponent::getResourceProviderRoot());
  startTimerHz(kFrameRateHz);
}

WebPanel::~WebPanel() {
  stopTimer();
  for (int note : held_notes_)
    synth_.getKeyboardState()->noteOff(1, note, 0.0f);
}

void WebPanel::resized() {
  browser_->setBounds(getLocalBounds());
}

WebBrowserComponent::Options WebPanel::makeOptions() {
  // WebView2 keeps its profile in a folder it must be able to write. The
  // default is beside the host's executable, which a plugin cannot assume.
  File user_data = File::getSpecialLocation(File::tempDirectory).getChildFile("GNARL-webview");

  return WebBrowserComponent::Options{}
      .withBackend(WebBrowserComponent::Options::Backend::webview2)
      .withWinWebView2Options(WebBrowserComponent::Options::WinWebView2{}
                                  .withUserDataFolder(user_data)
                                  .withStatusBarDisabled())
      .withNativeIntegrationEnabled()
      .withKeepPageLoadedWhenBrowserIsHidden()
      .withResourceProvider([this](const String& url) { return getResource(url); })
      .withNativeFunction("gnarlConnect", [this](const Array<var>& args,
                                                 WebBrowserComponent::NativeFunctionCompletion done) {
        done(connect(args));
      })
      .withEventListener(kSet, [this](const var& event) { setValue(event); })
      .withEventListener(kGesture, [this](const var& event) { gesture(event); })
      .withEventListener(kNote, [this](const var& event) { note(event); })
      .withEventListener(kWobbleShape, [this](const var& event) { setWobbleShape(event); })
      .withEventListener(kRoute, [this](const var& event) { route(event); })
      .withEventListener(kClassic, [this](const var&) {
        // Async: the listener runs inside the page's call, and switching
        // editors hides the browser that is making it.
        MessageManager::callAsync([callback = show_classic_editor_] { callback(); });
      });
}

std::optional<WebBrowserComponent::Resource> WebPanel::getResource(const String& url) {
  // The whole panel is one file (ui/scripts/inline.mjs), embedded at build
  // time. Anything else asked for does not exist.
  if (url != "/" && url != "/index.html")
    return std::nullopt;

  const auto* data = reinterpret_cast<const std::byte*>(GnarlUi::gnarlui_html);
  WebBrowserComponent::Resource resource;
  resource.data.assign(data, data + GnarlUi::gnarlui_htmlSize);
  // Without the charset WebKit reads the UTF-8 page as Latin-1: "—" arrives
  // as three Latin-1 characters.
  resource.mimeType = "text/html; charset=utf-8";
  return resource;
}

var WebPanel::valueEntry(ValueBridge* bridge, float value) const {
  Array<var> entry;
  entry.add(value);
  entry.add(bridge->getText(value, 32));
  return entry;
}

var WebPanel::connect(const Array<var>& args) {
  bound_.clear();
  DynamicObject::Ptr values = new DynamicObject();
  DynamicObject::Ptr steps = new DynamicObject();

  if (args.size() > 0 && args[0].isArray()) {
    for (const var& name_var : *args[0].getArray()) {
      std::string name = name_var.toString().toStdString();
      ValueBridge* bridge = synth_.getBridge(name);
      if (bridge == nullptr)
        continue;

      float value = bridge->getValue();
      bound_[name] = { bridge, value };
      values->setProperty(String(name), valueEntry(bridge, value));
      if (bridge->isDiscrete())
        steps->setProperty(String(name), bridge->getNumSteps());
    }
  }

  connected_ = true;
  curve_changed_ = true;

  DynamicObject::Ptr result = new DynamicObject();
  result->setProperty("version", ProjectInfo::versionString);
  result->setProperty("values", var(values.get()));
  result->setProperty("steps", var(steps.get()));
  result->setProperty("routes", routes());
  last_routes_ = JSON::toString(routes(), true);
  return var(result.get());
}

void WebPanel::setValue(const var& event) {
  std::string name = event["name"].toString().toStdString();
  auto found = bound_.find(name);
  if (found == bound_.end())
    return;

  float value = jlimit(0.0f, 1.0f, (float) event["value"]);
  // Exactly the path a Vital knob takes: set the control, notify the host.
  synth_.valueChangedInternal(name, found->second.bridge->convertToEngineValue(value));
  // Not echoed back: the page already shows what it sent. The text is, since
  // only the engine knows its units.
  found->second.last_value = found->second.bridge->getValue();
  DynamicObject::Ptr changed = new DynamicObject();
  changed->setProperty(String(name), valueEntry(found->second.bridge, found->second.last_value));
  browser_->emitEventIfBrowserIsVisible(kValues, var(changed.get()));
}

void WebPanel::gesture(const var& event) {
  std::string name = event["name"].toString().toStdString();
  if (bound_.count(name) == 0)
    return;

  if ((bool) event["begin"])
    synth_.beginChangeGesture(name);
  else
    synth_.endChangeGesture(name);
}

void WebPanel::note(const var& event) {
  int midi_note = jlimit(0, 127, (int) event["note"]);
  auto held = std::find(held_notes_.begin(), held_notes_.end(), midi_note);

  if ((bool) event["on"]) {
    if (held == held_notes_.end())
      held_notes_.push_back(midi_note);
    synth_.getKeyboardState()->noteOn(1, midi_note, 1.0f);
  }
  else {
    if (held != held_notes_.end())
      held_notes_.erase(held);
    synth_.getKeyboardState()->noteOff(1, midi_note, 0.0f);
  }
}

void WebPanel::setWobbleShape(const var& event) {
  LineGenerator* line = synth_.getWobbleSource();
  if (line == nullptr)
    return;

  String kind = event["kind"].toString();
  if (kind == "sine") {
    line->initSin();
  }
  else if (kind == "square") {
    line->initSquare();
    line->setSmooth(true);
  }
  else if (kind == "draw" && event["points"].isArray()) {
    const Array<var>& points = *event["points"].getArray();
    int num_steps = std::min<int>(points.size(), LineGenerator::kMaxPoints - 1);
    if (num_steps < 2)
      return;

    // A closed loop: one point per step and the first repeated at x = 1, so
    // the curve joins itself at the cycle boundary. LineGenerator's y is
    // inverted (0 is the top), the page's is not.
    line->setNumPoints(num_steps + 1);
    for (int i = 0; i <= num_steps; ++i) {
      float value = jlimit(0.0f, 1.0f, (float) points[i % num_steps]);
      line->setPoint(i, { (1.0f * i) / num_steps, 1.0f - value });
      line->setPower(i, 0.0f);
    }
    line->setSmooth(false);
    line->render();
  }
  else {
    return;
  }

  curve_changed_ = true;
}

// The matrix as the page shows it: every connection with its amount, which
// lives in the host parameter modulation_N_amount for connection slot N.
var WebPanel::routes() const {
  Array<var> list;
  for (vital::ModulationConnection* connection : synth_.getModulationConnections()) {
    int index = synth_.getConnectionIndex(connection->source_name, connection->destination_name);
    if (index < 0)
      continue;
    DynamicObject::Ptr entry = new DynamicObject();
    entry->setProperty("source", String(connection->source_name));
    entry->setProperty("destination", String(connection->destination_name));
    std::string amount_name = "modulation_" + std::to_string(index + 1) + "_amount";
    entry->setProperty("amount", synth_.getControls()[amount_name]->value());
    list.add(var(entry.get()));
  }
  return list;
}

// Connect or disconnect the way Vital's own matrix does, and set the amount
// the way its amount slider does - valueChangedInternal, so the host records
// it and the slot's host parameter follows.
void WebPanel::route(const var& event) {
  std::string source = event["source"].toString().toStdString();
  std::string destination = event["destination"].toString().toStdString();
  if (source.empty() || destination.empty())
    return;

  if ((bool) event["remove"]) {
    synth_.disconnectModulation(source, destination);
    return;
  }

  synth_.connectModulation(source, destination);
  int index = synth_.getConnectionIndex(source, destination);
  if (index < 0)
    return;
  float amount = jlimit(-1.0f, 1.0f, (float) event["amount"]);
  synth_.valueChangedInternal("modulation_" + std::to_string(index + 1) + "_amount", amount);
}

Array<var> WebPanel::wobbleCurve() {
  Array<var> curve;
  LineGenerator* line = synth_.getWobbleSource();
  if (line == nullptr)
    return curve;

  for (int i = 0; i < kCurvePoints; ++i)
    curve.add(line->valueAtPhase((1.0f * i) / kCurvePoints));
  return curve;
}

void WebPanel::timerCallback() {
  if (!connected_ || !isShowing())
    return;

  // Values the engine changed: automation, a preset load, Vital's editor.
  DynamicObject::Ptr changed = new DynamicObject();
  bool any_changed = false;
  for (auto& [name, bound] : bound_) {
    float value = bound.bridge->getValue();
    if (std::abs(value - bound.last_value) > kValueEpsilon) {
      bound.last_value = value;
      changed->setProperty(String(name), valueEntry(bound.bridge, value));
      any_changed = true;
    }
  }
  if (any_changed)
    browser_->emitEventIfBrowserIsVisible(kValues, var(changed.get()));

  DynamicObject::Ptr frame = new DynamicObject();

  // The output, as Vital's own oscilloscope sees it: the audio thread writes
  // this memory, synced to the played note's period, and the display reads
  // it without a lock, as Vital's editor does. Left channel.
  const vital::poly_float* memory = synth_.getOscilloscopeMemory();
  constexpr int kMemorySize = 2 * vital::kOscilloscopeMemoryResolution;
  constexpr int kStride = kMemorySize / kScopePoints;
  Array<var> scope;
  scope.ensureStorageAllocated(kScopePoints);
  for (int i = 0; i < kScopePoints; ++i)
    scope.add(memory[i * kStride][0]);
  frame->setProperty("scope", scope);

  // Where the wobble is, or -1 when no voice is playing. The status carries
  // the phase packed with a voice count, as LfoEditor reads it.
  float wobble_phase = -1.0f;
  if (const vital::StatusOutput* phase = synth_.getStatusOutput("wobble_phase")) {
    vital::poly_float encoded = phase->value();
    if (!phase->isClearValue(encoded))
      wobble_phase = vital::utils::decodePhaseAndVoice(encoded).first[0];
  }
  frame->setProperty("wobblePhase", wobble_phase);

  String preset_name = synth_.getPresetName();
  if (preset_name != last_preset_name_ || curve_changed_) {
    last_preset_name_ = preset_name;
    frame->setProperty("preset", preset_name.isEmpty() ? String("Init") : preset_name);
    // A preset carries its own wobble shape.
    curve_changed_ = true;
  }

  if (curve_changed_) {
    frame->setProperty("curve", wobbleCurve());
    curve_changed_ = false;
  }

  browser_->emitEventIfBrowserIsVisible(kFrame, var(frame.get()));

  // The matrix, when it changed: a preset load, Vital's editor, automation of
  // an amount. A handful of connections, compared as text.
  var current_routes = routes();
  String routes_text = JSON::toString(current_routes, true);
  if (routes_text != last_routes_) {
    last_routes_ = routes_text;
    browser_->emitEventIfBrowserIsVisible(kRoutes, current_routes);
  }
}
