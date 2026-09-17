#pragma once

#include "CombFilter.h"
#include "FormantFilter.h"
#include "LadderFilter.h"
#include "Saturation.h"
#include "StateVariableFilter.h"
#include "../params/ParameterChoices.h"

namespace gnarl::dsp
{

/**
    One of GNARL's two filter slots: every filter type behind one interface.

    All the types are constructed, not switched in and out. Two filters per
    voice times sixteen voices is small enough that holding them all costs
    less than the alternative, and swapping filter type mid-note must not
    allocate on the audio thread.

    The drive stage runs BEFORE the filter, which is the order that produces
    the classic growl: saturate, then filter the harmonics the saturation
    added. Driving after the filter just distorts the filtered result, which
    sounds like a distortion pedal rather than like a filter with attitude.
*/
class FilterSlot
{
public:
    using Type = choices::FilterType;

    struct Settings
    {
        Type type = Type::lowPass24;
        float cutoffHz = 20000.0f;
        float resonance = 0.1f;
        float drive = 0.0f;
        choices::DriveCurve driveCurve = choices::DriveCurve::tanh;
        float mix = 1.0f;               // 0..1 dry/wet

        // Formant type only.
        float formantX = 0.5f;
        float formantY = 0.5f;
        float formantThroat = 0.0f;

        // Comb type only.
        float combFeedback = 0.5f;
        float combDamping = 0.3f;
    };

    void prepare (double sampleRate)
    {
        sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;

        // Every filter prepared up front, so changing type is free.
        svf1.prepare (sampleRate);
        svf2.prepare (sampleRate);
        ladder.prepare (sampleRate);
        comb.prepare (sampleRate);
        formant.prepare (sampleRate);

        // DC blocker for the asymmetric drive curves. 20 Hz, high enough to
        // remove offset and low enough to leave a sub alone.
        dcBlockerCoefficient = 1.0f
            - std::exp (-juce::MathConstants<float>::twoPi * 20.0f
                        / static_cast<float> (sampleRateHz));

        reset();
    }

    void reset() noexcept
    {
        svf1.reset();
        svf2.reset();
        ladder.reset();
        comb.reset();
        formant.reset();

        dcState = 0.0f;
    }

    /** BLOCK-RATE. */
    void setSettings (const Settings& newSettings) noexcept
    {
        settings = newSettings;

        driveStage.setDrive (settings.driveCurve, settings.drive);

        switch (settings.type)
        {
            case Type::lowPass12:
            case Type::lowPass24:
            case Type::highPass12:
            case Type::highPass24:
            case Type::bandPass12:
            case Type::bandPass24:
            case Type::notch12:
            case Type::notch24:
                svf1.setCutoff (settings.cutoffHz);
                svf1.setResonance (settings.resonance);
                svf2.setCutoff (settings.cutoffHz);
                svf2.setResonance (settings.resonance);
                break;

            case Type::ladderLowPass:
            case Type::ladderHighPass:
                ladder.setMode (settings.type == Type::ladderLowPass
                                    ? LadderFilter::Mode::lowPass
                                    : LadderFilter::Mode::highPass);
                ladder.setCutoff (settings.cutoffHz);
                ladder.setResonance (settings.resonance);
                ladder.setDrive (settings.drive);
                break;

            case Type::comb:
                comb.setFrequency (settings.cutoffHz);
                comb.setFeedback (settings.combFeedback);
                comb.setDamping (settings.combDamping);
                break;

            case Type::formant:
                formant.setPosition (settings.formantX, settings.formantY);
                formant.setThroat (settings.formantThroat);
                formant.setResonance (settings.resonance);
                formant.updateCoefficients();
                break;

            case Type::count:
            default:
                break;
        }
    }

    /** SAMPLE-RATE. */
    float processSample (float input) noexcept
    {
        const auto dry = input;

        // Drive first: saturate, then filter what the saturation added.
        auto x = driveStage.processSample (input);

        if (driveStage.producesDC())
            x = blockDC (x);

        auto wet = x;

        switch (settings.type)
        {
            case Type::lowPass12:   wet = svf1.processSample (x).lowPass; break;
            case Type::highPass12:  wet = svf1.processSample (x).highPass; break;
            case Type::bandPass12:  wet = svf1.processSample (x).bandPass; break;
            case Type::notch12:     wet = svf1.processSample (x).notch; break;

            // 24 dB is two 12 dB sections in series, which is what a cascade
            // of identical biquads gives - not a single section with a
            // steeper coefficient.
            case Type::lowPass24:
                wet = svf2.processSample (svf1.processSample (x).lowPass).lowPass;
                break;
            case Type::highPass24:
                wet = svf2.processSample (svf1.processSample (x).highPass).highPass;
                break;
            case Type::bandPass24:
                wet = svf2.processSample (svf1.processSample (x).bandPass).bandPass;
                break;
            case Type::notch24:
                wet = svf2.processSample (svf1.processSample (x).notch).notch;
                break;

            case Type::ladderLowPass:
            case Type::ladderHighPass:
                wet = ladder.processSample (x);
                break;

            case Type::comb:
                wet = comb.processSample (x);
                break;

            case Type::formant:
                wet = formant.processSample (x);
                break;

            case Type::count:
            default:
                break;
        }

        // Guards against a NaN that arrived from upstream poisoning the
        // filter state permanently. Recovering costs a compare; not
        // recovering costs the rest of the session.
        if (! std::isfinite (wet))
        {
            reset();
            wet = 0.0f;
        }

        const auto mix = juce::jlimit (0.0f, 1.0f, settings.mix);

        return dry + (wet - dry) * mix;
    }

    /** True when any sub-filter's state has gone non-finite. */
    bool hasBlownUp() const noexcept
    {
        return svf1.hasBlownUp() || svf2.hasBlownUp()
            || ladder.hasBlownUp() || comb.hasBlownUp() || formant.hasBlownUp();
    }

private:
    float blockDC (float x) noexcept
    {
        dcState += dcBlockerCoefficient * (x - dcState);
        return x - dcState;
    }

    double sampleRateHz = 44100.0;

    Settings settings {};

    DriveStage driveStage;

    StateVariableFilter svf1;
    StateVariableFilter svf2;
    LadderFilter ladder;
    CombFilter comb;
    FormantFilter formant;

    float dcBlockerCoefficient = 0.0f;
    float dcState = 0.0f;
};

} // namespace gnarl::dsp
