#include "SettingsReader.h"

#include "ParameterChoices.h"
#include "ParameterIDs.h"

namespace gnarl::params
{

namespace
{
    /** Index of the "Basic Shapes" factory table, whose frames morph
        sine -> triangle -> square -> saw. The sub oscillator reads it at a
        fixed position per waveform rather than owning five tables of its own,
        which reuses the whole band-limiting path for free. */
    constexpr int kBasicShapesTableIndex = 0;

    /** Table position for each sub waveform within Basic Shapes. */
    float getSubTablePosition (choices::SubWaveform waveform) noexcept
    {
        switch (waveform)
        {
            case choices::SubWaveform::sine:     return 0.0f;
            case choices::SubWaveform::triangle: return 0.33f;
            case choices::SubWaveform::square:   return 0.66f;
            case choices::SubWaveform::saw:      return 1.0f;

            // Pulse is the square frame with the PWM warp applied, which is
            // narrower than any frame in the table can be on its own.
            case choices::SubWaveform::pulse:    return 0.66f;

            case choices::SubWaveform::count:
            default:                             return 0.0f;
        }
    }

    float readValue (const std::atomic<float>* parameter) noexcept
    {
        return parameter != nullptr ? parameter->load() : 0.0f;
    }

    bool readBool (const std::atomic<float>* parameter) noexcept
    {
        return readValue (parameter) > 0.5f;
    }

    template <typename EnumType>
    EnumType readChoice (const std::atomic<float>* parameter) noexcept
    {
        const auto index = juce::jlimit (0,
                                         static_cast<int> (EnumType::count) - 1,
                                         static_cast<int> (readValue (parameter)));
        return static_cast<EnumType> (index);
    }
}

SettingsReader::SettingsReader (juce::AudioProcessorValueTreeState& state,
                                dsp::WavetableLibrary& wavetableLibrary)
    : apvts (state), library (wavetableLibrary)
{
    const auto bind = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    for (std::size_t i = 0; i < pid::kNumOscillators; ++i)
    {
        const auto& ids = pid::osc[i];
        auto& target = oscillators[i];

        target.enabled          = bind (ids.enabled);
        target.mode             = bind (ids.mode);
        target.wavetable        = bind (ids.wavetable);
        target.tablePos         = bind (ids.tablePos);
        target.pitchSemi        = bind (ids.pitchSemi);
        target.pitchFine        = bind (ids.pitchFine);
        target.phase            = bind (ids.phase);
        target.phaseRandom      = bind (ids.phaseRandom);
        target.pan              = bind (ids.pan);
        target.level            = bind (ids.level);
        target.unisonVoices     = bind (ids.unisonVoices);
        target.unisonDetune     = bind (ids.unisonDetune);
        target.unisonBlend      = bind (ids.unisonBlend);
        target.unisonSpread     = bind (ids.unisonSpread);
        target.warpMode         = bind (ids.warpMode);
        target.warpAmount       = bind (ids.warpAmount);
        target.grainSize        = bind (ids.grainSize);
        target.grainDensity     = bind (ids.grainDensity);
        target.grainPosJitter   = bind (ids.grainPosJitter);
        target.grainPitchJitter = bind (ids.grainPitchJitter);
        target.sendFilter1      = bind (ids.sendFilter1);
        target.sendFilter2      = bind (ids.sendFilter2);
        target.sendDirect       = bind (ids.sendDirect);

        oscillatorTables[i].store (nullptr);
    }

    sub.enabled     = bind (pid::sub.enabled);
    sub.waveform    = bind (pid::sub.waveform);
    sub.octave      = bind (pid::sub.octave);
    sub.pitchFine   = bind (pid::sub.pitchFine);
    sub.phase       = bind (pid::sub.phase);
    sub.pan         = bind (pid::sub.pan);
    sub.level       = bind (pid::sub.level);
    sub.sendFilter1 = bind (pid::sub.sendFilter1);
    sub.sendFilter2 = bind (pid::sub.sendFilter2);
    sub.sendDirect  = bind (pid::sub.sendDirect);

    noise.enabled     = bind (pid::noise.enabled);
    noise.type        = bind (pid::noise.type);
    noise.level       = bind (pid::noise.level);
    noise.pan         = bind (pid::noise.pan);
    noise.pitchSemi   = bind (pid::noise.pitchSemi);
    noise.pitchFine   = bind (pid::noise.pitchFine);
    noise.phaseRandom = bind (pid::noise.phaseRandom);
    noise.sendFilter1 = bind (pid::noise.sendFilter1);
    noise.sendFilter2 = bind (pid::noise.sendFilter2);
    noise.sendDirect  = bind (pid::noise.sendDirect);

    for (std::size_t i = 0; i < pid::kNumFilters; ++i)
    {
        const auto& ids = pid::filter[i];
        auto& target = filters[i];

        target.enabled       = bind (ids.enabled);
        target.type          = bind (ids.type);
        target.cutoff        = bind (ids.cutoff);
        target.resonance     = bind (ids.resonance);
        target.drive         = bind (ids.drive);
        target.driveCurve    = bind (ids.driveCurve);
        target.mix           = bind (ids.mix);
        target.keyTrack      = bind (ids.keyTrack);
        target.formantX      = bind (ids.formantX);
        target.formantY      = bind (ids.formantY);
        target.formantThroat = bind (ids.formantThroat);
        target.combFeedback  = bind (ids.combFeedback);
        target.combDamping   = bind (ids.combDamping);
    }

    filterRouting = bind (pid::filterRouting);
    analogDrift = bind (pid::analogDrift);
}

std::array<int, pid::kNumOscillators> SettingsReader::getSelectedTableIndices() const noexcept
{
    std::array<int, pid::kNumOscillators> indices {};

    for (std::size_t i = 0; i < pid::kNumOscillators; ++i)
        indices[i] = juce::jlimit (0,
                                   dsp::WavetableLibrary::kNumFactoryTables - 1,
                                   static_cast<int> (readValue (oscillators[i].wavetable)));

    return indices;
}

void SettingsReader::ensureTablesLoaded()
{
    // MESSAGE THREAD. Generating a table takes tens of milliseconds, so this
    // must never be reached from processBlock.
    const auto indices = getSelectedTableIndices();

    for (std::size_t i = 0; i < pid::kNumOscillators; ++i)
        oscillatorTables[i].store (&library.getTable (indices[i]));

    subTable.store (&library.getTable (kBasicShapesTableIndex));
}

void SettingsReader::read (dsp::VoiceSettings& settings) const noexcept
{
    for (std::size_t i = 0; i < pid::kNumOscillators; ++i)
    {
        const auto& source = oscillators[i];
        auto& target = settings.oscillators[i];

        target.enabled = readBool (source.enabled);
        target.table = oscillatorTables[i].load();

        target.pitchOffsetSemitones = readValue (source.pitchSemi)
                                    + readValue (source.pitchFine) * 0.01f;

        auto& osc = target.settings;
        osc.mode = readChoice<choices::OscMode> (source.mode);
        osc.tablePosition = readValue (source.tablePos);
        osc.warpMode = readChoice<choices::WarpMode> (source.warpMode);
        osc.warpAmount = readValue (source.warpAmount);
        osc.unisonVoices = static_cast<int> (readValue (source.unisonVoices));
        osc.unisonDetune = readValue (source.unisonDetune);
        osc.unisonBlend = readValue (source.unisonBlend);
        osc.unisonSpread = readValue (source.unisonSpread);
        osc.pan = readValue (source.pan);
        osc.level = readValue (source.level);
        osc.grainSizeMs = readValue (source.grainSize);
        osc.grainDensityHz = readValue (source.grainDensity);
        osc.grainPosJitter = readValue (source.grainPosJitter);
        osc.grainPitchJitter = readValue (source.grainPitchJitter);

        target.sends.toFilter1 = readValue (source.sendFilter1);
        target.sends.toFilter2 = readValue (source.sendFilter2);
        target.sends.direct = readValue (source.sendDirect);
    }

    {
        auto& target = settings.sub;

        target.enabled = readBool (sub.enabled);
        target.table = subTable.load();

        const auto waveform = readChoice<choices::SubWaveform> (sub.waveform);

        target.pitchOffsetSemitones = readValue (sub.octave) * 12.0f
                                    + readValue (sub.pitchFine) * 0.01f;

        auto& osc = target.settings;
        osc = dsp::WavetableOscillator::Settings {};
        osc.mode = choices::OscMode::wavetable;
        osc.tablePosition = getSubTablePosition (waveform);
        osc.pan = readValue (sub.pan);
        osc.level = readValue (sub.level);
        osc.unisonVoices = 1;

        if (waveform == choices::SubWaveform::pulse)
        {
            osc.warpMode = choices::WarpMode::pwm;
            osc.warpAmount = 0.5f;
        }

        target.sends.toFilter1 = readValue (sub.sendFilter1);
        target.sends.toFilter2 = readValue (sub.sendFilter2);
        target.sends.direct = readValue (sub.sendDirect);
    }

    {
        auto& target = settings.noise;

        target.enabled = readBool (noise.enabled);
        target.type = readChoice<choices::NoiseType> (noise.type);
        target.level = readValue (noise.level);
        target.pan = readValue (noise.pan);
        target.pitchOffsetSemitones = readValue (noise.pitchSemi)
                                    + readValue (noise.pitchFine) * 0.01f;

        target.sends.toFilter1 = readValue (noise.sendFilter1);
        target.sends.toFilter2 = readValue (noise.sendFilter2);
        target.sends.direct = readValue (noise.sendDirect);
    }

    for (std::size_t i = 0; i < pid::kNumFilters; ++i)
    {
        const auto& source = filters[i];
        auto& target = settings.filters[i];

        settings.filterEnabled[i] = readBool (source.enabled);

        target.type = readChoice<choices::FilterType> (source.type);
        target.cutoffHz = readValue (source.cutoff);
        target.resonance = readValue (source.resonance);
        target.drive = readValue (source.drive);
        target.driveCurve = readChoice<choices::DriveCurve> (source.driveCurve);
        target.mix = readValue (source.mix);
        target.formantX = readValue (source.formantX);
        target.formantY = readValue (source.formantY);
        target.formantThroat = readValue (source.formantThroat);
        target.combFeedback = readValue (source.combFeedback);
        target.combDamping = readValue (source.combDamping);
    }

    settings.filterRouting = readChoice<choices::FilterRouting> (filterRouting);
    settings.analogDrift = readValue (analogDrift);
}

} // namespace gnarl::params
