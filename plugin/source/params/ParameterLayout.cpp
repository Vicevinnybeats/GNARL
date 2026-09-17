#include "ParameterLayout.h"

#include "ParameterChoices.h"
#include "ParameterIDs.h"
#include "ParameterRanges.h"

namespace gnarl::params
{

namespace
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
    using FloatAttributes = juce::AudioParameterFloatAttributes;
    using IntAttributes = juce::AudioParameterIntAttributes;

    juce::ParameterID makeID (const char* id)
    {
        // kParameterVersionHint, NOT kStateVersion: see ParameterIDs.h.
        return juce::ParameterID { id, pid::kParameterVersionHint };
    }

    void addFloat (Layout& layout,
                   const char* id,
                   const juce::String& name,
                   juce::NormalisableRange<float> range,
                   float defaultValue,
                   juce::String (*formatter) (float, int) = nullptr)
    {
        auto attributes = FloatAttributes {};

        if (formatter != nullptr)
            attributes = attributes.withStringFromValueFunction (formatter);

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            makeID (id), name, range, defaultValue, attributes));
    }

    void addChoice (Layout& layout,
                    const char* id,
                    const juce::String& name,
                    const juce::StringArray& choices,
                    int defaultIndex)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            makeID (id), name, choices, defaultIndex));
    }

    void addBool (Layout& layout, const char* id, const juce::String& name, bool defaultValue)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (
            makeID (id), name, defaultValue));
    }

    void addInt (Layout& layout,
                 const char* id,
                 const juce::String& name,
                 int min,
                 int max,
                 int defaultValue)
    {
        layout.add (std::make_unique<juce::AudioParameterInt> (
            makeID (id), name, min, max, defaultValue));
    }

    /** "Osc 1 " etc. Display names may change freely; only IDs are frozen. */
    juce::String prefixed (const juce::String& section, int oneBasedIndex, const juce::String& name)
    {
        return section + " " + juce::String (oneBasedIndex) + " " + name;
    }

    // --- Sections ----------------------------------------------------------

    void addOscillator (Layout& layout, std::size_t index)
    {
        const auto& p = pid::osc[index];
        const auto n = static_cast<int> (index) + 1;
        const auto name = [n] (const juce::String& s) { return prefixed ("Osc", n, s); };

        // Osc 1 on by default so a fresh patch makes a sound as soon as the
        // engine exists; Osc 2 off so it is an addition, not a surprise.
        addBool   (layout, p.enabled, name ("On"), index == 0);
        addChoice (layout, p.mode, name ("Mode"), choices::oscMode,
                   static_cast<int> (choices::OscMode::wavetable));

        // Which of the shipped wavetables is loaded. An index, not a path: a
        // preset must not depend on a file living at an absolute location.
        // Phase 5 extends this with a reference to a user table.
        addInt (layout, p.wavetable, name ("Table"), 0, 255, 0);

        addFloat (layout, p.tablePos, name ("Position"), ranges::unipolar(), 0.0f,
                  ranges::formatPercent);
        addFloat (layout, p.pitchSemi, name ("Semi"), ranges::pitchSemitones(), 0.0f,
                  ranges::formatSemitones);
        addFloat (layout, p.pitchFine, name ("Fine"), ranges::pitchCents(), 0.0f,
                  ranges::formatCents);
        addFloat (layout, p.phase, name ("Phase"), ranges::unipolar(), 0.0f,
                  ranges::formatPercent);
        addFloat (layout, p.phaseRandom, name ("Rand Phase"), ranges::unipolar(), 0.0f,
                  ranges::formatPercent);
        addFloat (layout, p.pan, name ("Pan"), ranges::bipolar(), 0.0f, ranges::formatPan);
        addFloat (layout, p.level, name ("Level"), ranges::unipolar(), index == 0 ? 0.8f : 0.0f,
                  ranges::formatPercent);

        addInt   (layout, p.unisonVoices, name ("Unison"), 1, pid::kMaxUnisonVoices, 1);
        addFloat (layout, p.unisonDetune, name ("Detune"), ranges::unipolar(), 0.25f,
                  ranges::formatPercent);
        addFloat (layout, p.unisonBlend, name ("Blend"), ranges::unipolar(), 0.5f,
                  ranges::formatPercent);
        addFloat (layout, p.unisonSpread, name ("Spread"), ranges::unipolar(), 0.5f,
                  ranges::formatPercent);

        addChoice (layout, p.warpMode, name ("Warp"), choices::warpMode,
                   static_cast<int> (choices::WarpMode::off));
        addFloat  (layout, p.warpAmount, name ("Warp Amt"), ranges::bipolar(), 0.0f,
                   ranges::formatSignedPercent);

        addFloat (layout, p.grainSize, name ("Grain Size"), ranges::grainSizeMs(), 40.0f,
                  ranges::formatMilliseconds);
        addFloat (layout, p.grainDensity, name ("Grain Density"), ranges::grainDensity(), 20.0f,
                  ranges::formatHertz);
        addFloat (layout, p.grainPosJitter, name ("Grain Pos Jit"), ranges::unipolar(), 0.0f,
                  ranges::formatPercent);
        addFloat (layout, p.grainPitchJitter, name ("Grain Pitch Jit"), ranges::unipolar(), 0.0f,
                  ranges::formatPercent);

        // Independent send levels rather than a routing dropdown, so a source
        // can feed both filters at once - which is how parallel filter growls
        // are built.
        addFloat (layout, p.sendFilter1, name ("To F1"), ranges::unipolar(), 1.0f,
                  ranges::formatPercent);
        addFloat (layout, p.sendFilter2, name ("To F2"), ranges::unipolar(), 0.0f,
                  ranges::formatPercent);
        addFloat (layout, p.sendDirect, name ("Direct"), ranges::unipolar(), 0.0f,
                  ranges::formatPercent);
    }

    void addSubOscillator (Layout& layout)
    {
        const auto& p = pid::sub;

        addBool   (layout, p.enabled, "Sub On", false);
        addChoice (layout, p.waveform, "Sub Wave", choices::subWaveform,
                   static_cast<int> (choices::SubWaveform::sine));
        addInt    (layout, p.octave, "Sub Octave", -3, 1, -1);
        addFloat  (layout, p.pitchFine, "Sub Fine", ranges::pitchCents(), 0.0f,
                   ranges::formatCents);
        addFloat  (layout, p.phase, "Sub Phase", ranges::unipolar(), 0.0f, ranges::formatPercent);
        addFloat  (layout, p.pan, "Sub Pan", ranges::bipolar(), 0.0f, ranges::formatPan);
        addFloat  (layout, p.level, "Sub Level", ranges::unipolar(), 0.6f, ranges::formatPercent);

        // Sub defaults to bypassing the filters: the whole point of a sub in
        // this genre is a clean fundamental under the growl.
        addFloat (layout, p.sendFilter1, "Sub To F1", ranges::unipolar(), 0.0f,
                  ranges::formatPercent);
        addFloat (layout, p.sendFilter2, "Sub To F2", ranges::unipolar(), 0.0f,
                  ranges::formatPercent);
        addFloat (layout, p.sendDirect, "Sub Direct", ranges::unipolar(), 1.0f,
                  ranges::formatPercent);
    }

    void addNoise (Layout& layout)
    {
        const auto& p = pid::noise;

        addBool   (layout, p.enabled, "Noise On", false);
        addChoice (layout, p.type, "Noise Type", choices::noiseType,
                   static_cast<int> (choices::NoiseType::white));
        addFloat  (layout, p.level, "Noise Level", ranges::unipolar(), 0.3f,
                   ranges::formatPercent);
        addFloat  (layout, p.pan, "Noise Pan", ranges::bipolar(), 0.0f, ranges::formatPan);
        addFloat  (layout, p.pitchSemi, "Noise Semi", ranges::pitchSemitones(), 0.0f,
                   ranges::formatSemitones);
        addFloat  (layout, p.pitchFine, "Noise Fine", ranges::pitchCents(), 0.0f,
                   ranges::formatCents);
        addFloat  (layout, p.phaseRandom, "Noise Rand Phase", ranges::unipolar(), 1.0f,
                   ranges::formatPercent);
        addFloat  (layout, p.sendFilter1, "Noise To F1", ranges::unipolar(), 1.0f,
                   ranges::formatPercent);
        addFloat  (layout, p.sendFilter2, "Noise To F2", ranges::unipolar(), 0.0f,
                   ranges::formatPercent);
        addFloat  (layout, p.sendDirect, "Noise Direct", ranges::unipolar(), 0.0f,
                   ranges::formatPercent);
    }

    void addFilter (Layout& layout, std::size_t index)
    {
        const auto& p = pid::filter[index];
        const auto n = static_cast<int> (index) + 1;
        const auto name = [n] (const juce::String& s) { return prefixed ("Filter", n, s); };

        addBool   (layout, p.enabled, name ("On"), index == 0);
        addChoice (layout, p.type, name ("Type"), choices::filterType,
                   static_cast<int> (choices::FilterType::lowPass24));

        // Defaults to wide open: a filter that starts closed makes a new patch
        // sound broken rather than plain.
        addFloat (layout, p.cutoff, name ("Cutoff"), ranges::cutoff(), 20000.0f,
                  ranges::formatHertz);
        addFloat (layout, p.resonance, name ("Res"), ranges::unipolar(), 0.1f,
                  ranges::formatPercent);
        addFloat (layout, p.drive, name ("Drive"), ranges::unipolar(), 0.0f,
                  ranges::formatPercent);
        addChoice (layout, p.driveCurve, name ("Drive Curve"), choices::driveCurve,
                   static_cast<int> (choices::DriveCurve::tanh));
        addFloat (layout, p.mix, name ("Mix"), ranges::unipolar(), 1.0f, ranges::formatPercent);
        addFloat (layout, p.keyTrack, name ("Key Track"), ranges::bipolar(), 0.0f,
                  ranges::formatSignedPercent);

        // Formant filter: the X/Y pad position and the throat shift. Centre of
        // the pad so the puck starts somewhere neutral.
        addFloat (layout, p.formantX, name ("Formant X"), ranges::unipolar(), 0.5f,
                  ranges::formatPercent);
        addFloat (layout, p.formantY, name ("Formant Y"), ranges::unipolar(), 0.5f,
                  ranges::formatPercent);
        addFloat (layout, p.formantThroat, name ("Throat"), ranges::bipolar(), 0.0f,
                  ranges::formatSignedPercent);

        addFloat (layout, p.combFeedback, name ("Comb FB"), ranges::unipolar(), 0.5f,
                  ranges::formatPercent);
        addFloat (layout, p.combDamping, name ("Comb Damp"), ranges::unipolar(), 0.3f,
                  ranges::formatPercent);
    }

    void addEnvelope (Layout& layout, std::size_t index)
    {
        const auto& p = pid::envelope[index];
        const auto n = static_cast<int> (index) + 1;
        const auto name = [n] (const juce::String& s) { return prefixed ("Env", n, s); };

        addChoice (layout, p.mode, name ("Mode"), choices::envelopeMode,
                   static_cast<int> (choices::EnvelopeMode::adsr));

        addFloat (layout, p.delay, name ("Delay"), ranges::holdTime(), 0.0f,
                  ranges::formatSeconds);
        addFloat (layout, p.attack, name ("Attack"), ranges::attackTime(), 0.002f,
                  ranges::formatSeconds);
        addFloat (layout, p.hold, name ("Hold"), ranges::holdTime(), 0.0f,
                  ranges::formatSeconds);
        addFloat (layout, p.decay, name ("Decay"), ranges::decayTime(), 0.4f,
                  ranges::formatSeconds);
        addFloat (layout, p.sustain, name ("Sustain"), ranges::unipolar(), 1.0f,
                  ranges::formatPercent);
        addFloat (layout, p.release, name ("Release"), ranges::decayTime(), 0.05f,
                  ranges::formatSeconds);

        // Curve per segment: 0 is linear, positive is exponential, negative is
        // logarithmic. A linear-only envelope cannot do a convincing pluck.
        addFloat (layout, p.attackCurve, name ("Attack Curve"), ranges::bipolar(), 0.0f,
                  ranges::formatSignedPercent);
        addFloat (layout, p.decayCurve, name ("Decay Curve"), ranges::bipolar(), 0.0f,
                  ranges::formatSignedPercent);
        addFloat (layout, p.releaseCurve, name ("Release Curve"), ranges::bipolar(), 0.0f,
                  ranges::formatSignedPercent);

        // Env 1 is the amp envelope, so it tracks velocity by default.
        addFloat (layout, p.velocityAmount, name ("Velocity"), ranges::unipolar(),
                  index == 0 ? 1.0f : 0.0f, ranges::formatPercent);
    }

    void addLfo (Layout& layout, std::size_t index)
    {
        const auto& p = pid::lfo[index];
        const auto n = static_cast<int> (index) + 1;
        const auto name = [n] (const juce::String& s) { return prefixed ("LFO", n, s); };

        // Default shape is the drawn curve: the drawable LFO is the feature,
        // not an option buried behind the built-in shapes.
        addChoice (layout, p.shape, name ("Shape"), choices::lfoShape,
                   static_cast<int> (choices::LfoShape::custom));

        // Tempo sync on by default. In this genre an unsynced wobble is the
        // exception.
        addBool   (layout, p.syncEnabled, name ("Sync"), true);
        addFloat  (layout, p.rateHz, name ("Rate"), ranges::lfoRateHz(), 2.0f,
                   ranges::formatHertz);
        addChoice (layout, p.rateDivision, name ("Division"), choices::lfoRateDivision,
                   static_cast<int> (choices::LfoRateDivision::eighth));
        addChoice (layout, p.mode, name ("Mode"), choices::lfoMode,
                   static_cast<int> (choices::LfoMode::trigger));
        addFloat  (layout, p.phase, name ("Phase"), ranges::unipolar(), 0.0f,
                   ranges::formatPercent);

        // Slew, to stop a steep drawn step from clicking.
        addFloat  (layout, p.smooth, name ("Smooth"), ranges::unipolar(), 0.0f,
                   ranges::formatPercent);
        addChoice (layout, p.gridDivision, name ("Grid"), choices::gridDivision,
                   static_cast<int> (choices::GridDivision::sixteenth));
        addBool   (layout, p.bipolar, name ("Bipolar"), false);
    }

    void addModSlot (Layout& layout, std::size_t index)
    {
        const auto& p = pid::modSlot[index];
        const auto n = static_cast<int> (index) + 1;
        const auto name = [n] (const juce::String& s) { return prefixed ("Mod", n, s); };

        // The DESTINATION is not here: it is a parameter-ID string in the
        // ValueTree, because an index into a list of targets cannot stay
        // stable across releases. See ParameterIDs.h.
        addBool   (layout, p.enabled, name ("On"), false);
        addChoice (layout, p.source, name ("Source"), choices::modSource,
                   static_cast<int> (choices::ModSource::none));
        addFloat  (layout, p.depth, name ("Depth"), ranges::bipolar(), 0.0f,
                   ranges::formatSignedPercent);
        addChoice (layout, p.curve, name ("Curve"), choices::modCurve,
                   static_cast<int> (choices::ModCurve::linear));

        // Secondary modulator scaling this slot's own depth - how an LFO gets
        // faded in by an envelope without burning a second slot.
        addChoice (layout, p.auxSource, name ("Aux"), choices::modSource,
                   static_cast<int> (choices::ModSource::none));
        addFloat  (layout, p.auxAmount, name ("Aux Amt"), ranges::unipolar(), 0.0f,
                   ranges::formatPercent);
        addBool   (layout, p.bipolar, name ("Bipolar"), true);
    }

    void addMacros (Layout& layout)
    {
        static const char* macroNames[] = { "GROWL", "Macro 2", "Macro 3", "Macro 4" };

        for (std::size_t i = 0; i < pid::kNumMacros; ++i)
            addFloat (layout, pid::macro[i], macroNames[i], ranges::unipolar(), 0.0f,
                      ranges::formatPercent);
    }

    void addGlobal (Layout& layout)
    {
        addFloat (layout, pid::masterGain, "Master", ranges::masterGainDb(), 0.0f,
                  ranges::formatDecibels);
        addBool  (layout, pid::bypass, "Bypass", false);

        addInt    (layout, pid::maxVoices, "Voices", 1, pid::kMaxVoices, pid::kMaxVoices);
        addChoice (layout, pid::polyMode, "Poly Mode", choices::polyMode,
                   static_cast<int> (choices::PolyMode::poly));
        addFloat  (layout, pid::glideTime, "Glide", ranges::glideTime(), 0.0f,
                   ranges::formatSeconds);

        // "Glide always" glides even between non-overlapping notes; off means
        // glide only applies when notes overlap, which is the behaviour most
        // players expect from a legato line.
        addBool (layout, pid::glideAlways, "Glide Always", false);

        addInt    (layout, pid::pitchBendRange, "Bend Range", 0, 24, 2);
        addChoice (layout, pid::oversampling, "Oversampling", choices::oversampling,
                   static_cast<int> (choices::Oversampling::twoTimes));
        addChoice (layout, pid::velocityCurve, "Velocity Curve", choices::velocityCurve,
                   static_cast<int> (choices::VelocityCurve::linear));

        // Per-voice detune and drift, seeded per voice, so stacked notes are
        // not phase-identical. Small by default: this is glue, not an effect.
        addFloat (layout, pid::analogDrift, "Drift", ranges::unipolar(), 0.15f,
                  ranges::formatPercent);

        addChoice (layout, pid::filterRouting, "Filter Routing", choices::filterRouting,
                   static_cast<int> (choices::FilterRouting::series));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    Layout layout;

    // ORDER IS FROZEN. It determines the parameter index a host shows in its
    // automation list; reordering moves every lane a customer has drawn.
    // Append new sections at the end, never in the middle.
    for (std::size_t i = 0; i < pid::kNumOscillators; ++i)
        addOscillator (layout, i);

    addSubOscillator (layout);
    addNoise (layout);

    for (std::size_t i = 0; i < pid::kNumFilters; ++i)
        addFilter (layout, i);

    for (std::size_t i = 0; i < pid::kNumEnvelopes; ++i)
        addEnvelope (layout, i);

    for (std::size_t i = 0; i < pid::kNumLfos; ++i)
        addLfo (layout, i);

    for (std::size_t i = 0; i < pid::kNumModSlots; ++i)
        addModSlot (layout, i);

    addMacros (layout);
    addGlobal (layout);

    return layout;
}

int getDeclaredParameterCount()
{
    // Kept in step by ParameterLayoutTests, which counts what the layout
    // actually produced. Update this when a parameter is added on purpose.
    return 299;
}

} // namespace gnarl::params
