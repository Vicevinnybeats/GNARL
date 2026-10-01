/* Copyright 2026 Gnarl Audio
 *
 * GNARL is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See LICENSE.
 */

// GNARL's own panel inside the plugin: the page built from ui/ (TypeScript,
// one HTML file) in a web view, bound to the engine. JUCE 8 builds only
// (CMakeLists.txt defines GNARL_WEB_UI); docs/design/phase2-04-web-panel.md.
//
// Everything here runs on the MESSAGE thread. Values reach the engine the way
// Vital's own knobs send them - SynthBase::valueChangedInternal, which also
// tells the host - so the audio thread sees no new code path.

#pragma once

#include "JuceHeader.h"

#include <map>
#include <string>
#include <vector>

class SynthPlugin;
class ValueBridge;

class WebPanel : public Component, private Timer {
  public:
    // How often values, the scope and the wobble phase are pushed to the page.
    // 30 Hz: smooth enough for a scope, cheap enough beside a DAW's own UI.
    static constexpr int kFrameRateHz = 30;
    // Points of output waveform per frame; the page draws a line through them.
    static constexpr int kScopePoints = 256;
    // Points of the wobble's curve sent to the page when it changes.
    static constexpr int kCurvePoints = 64;

    WebPanel(SynthPlugin& synth, std::function<void()> show_classic_editor);
    ~WebPanel() override;

    void resized() override;

  private:
    struct Bound {
      ValueBridge* bridge = nullptr;
      float last_value = -1.0f;
    };

    WebBrowserComponent::Options makeOptions();
    std::optional<WebBrowserComponent::Resource> getResource(const String& url);

    var connect(const Array<var>& args);
    void setValue(const var& event);
    void gesture(const var& event);
    void note(const var& event);
    void setWobbleShape(const var& event);
    void route(const var& event);
    var routes() const;
    // The licence banner's state (Phase 7), and a key the page typed in.
    var licence() const;
    void setLicenceKey(const var& event);
    // The starting sounds (ui/src/web/factory.json) and the init patch.
    void loadFactory(const var& event);
    void loadInit();
    // OSC 1 / 2's table (ui/src/wavetables.ts), as wavetable JSON text.
    void loadWavetable(const var& event);
    String wavetableNames();

    void timerCallback() override;
    var valueEntry(ValueBridge* bridge, float value) const;
    Array<var> wobbleCurve();

    SynthPlugin& synth_;
    std::function<void()> show_classic_editor_;
    std::map<std::string, Bound> bound_;
    std::vector<int> held_notes_;
    String last_preset_name_;
    String last_routes_;
    String last_licence_;
    String last_tables_;
    bool curve_changed_ = true;
    bool connected_ = false;

    // Last, so it is destroyed first: the page must not call back into a
    // half-destroyed panel.
    std::unique_ptr<WebBrowserComponent> browser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WebPanel)
};
