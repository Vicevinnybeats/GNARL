#include "FactoryBank.h"

#include "FactoryGenerator.h"

#include "../params/ParameterChoices.h"
#include "../params/ModStateBridge.h"
#include "../params/ParameterIDs.h"

namespace gnarl::preset
{

namespace
{
    /** Indices into the choice lists, so a patch reads as a name rather than
        as a number. */
    constexpr auto kFilterFormant = static_cast<float> (choices::FilterType::formant);
    constexpr auto kFilterLowPass24 = static_cast<float> (choices::FilterType::lowPass24);
    constexpr auto kFilterComb = static_cast<float> (choices::FilterType::comb);
    constexpr auto kFilterBandPass12 = static_cast<float> (choices::FilterType::bandPass12);

    constexpr auto kLfoCustom = static_cast<float> (choices::LfoShape::custom);
    constexpr auto kLfoSine = static_cast<float> (choices::LfoShape::sine);

    constexpr auto kEighthTriplet = static_cast<float> (choices::LfoRateDivision::eighthTriplet);
    constexpr auto kSixteenth = static_cast<float> (choices::LfoRateDivision::sixteenth);
    constexpr auto kQuarter = static_cast<float> (choices::LfoRateDivision::quarter);

    constexpr auto kGraintable = static_cast<float> (choices::OscMode::graintable);

    constexpr auto kDistTanh = static_cast<float> (choices::FxDistortionType::tanh);
    constexpr auto kDistBitcrush = static_cast<float> (choices::FxDistortionType::bitcrush);
    constexpr auto kDistFold = static_cast<float> (choices::FxDistortionType::fold);

    constexpr auto kSourceLfo1 = static_cast<float> (choices::ModSource::lfo1);
    constexpr auto kSourceLfo2 = static_cast<float> (choices::ModSource::lfo2);
    constexpr auto kSourceEnv2 = static_cast<float> (choices::ModSource::env2);
    constexpr auto kSourceEnv3 = static_cast<float> (choices::ModSource::env3);

    /*  Table indices by name, so a patch reads as a shape rather than as a
        number. The ORDER IS FROZEN (WavetableLibrary::kTableNames) for the
        same reason a choice list is: a preset stores the index. */
    constexpr auto kTableGrowlVowels = 1.0f;
    constexpr auto kTableMetallicFm = 2.0f;
    constexpr auto kTableHollowComb = 3.0f;
    constexpr auto kTableFormantSweep = 7.0f;
    constexpr auto kTableBitcrushSteps = 8.0f;
    constexpr auto kTablePulseWidth = 9.0f;
    constexpr auto kTableAdditiveStack = 10.0f;
    constexpr auto kTableRingModBell = 11.0f;
    constexpr auto kTableTalkBox = 13.0f;
    constexpr auto kTableGlassHarmonics = 15.0f;
    constexpr auto kTablePhaseDistortion = 16.0f;

    constexpr auto kHalf = static_cast<float> (choices::LfoRateDivision::half);
    constexpr auto kTwoBars = static_cast<float> (choices::LfoRateDivision::bars2);
    constexpr auto kFourBars = static_cast<float> (choices::LfoRateDivision::bars4);

    constexpr auto kDistTube = static_cast<float> (choices::FxDistortionType::tube);

    constexpr auto kSixteenthTriplet =
        static_cast<float> (choices::LfoRateDivision::sixteenthTriplet);
    constexpr auto kEighth = static_cast<float> (choices::LfoRateDivision::eighth);

    constexpr auto kFilterLowPass12 = static_cast<float> (choices::FilterType::lowPass12);
    constexpr auto kDistHardClip = static_cast<float> (choices::FxDistortionType::hardClip);

    /*  DRAWN CURVES, as shapes rather than as numbers.

        THE RHYTHM OF A GROWL IS THE CURVE, NOT THE LFO'S RATE. The rate picks
        how long one cycle lasts; what happens inside the cycle is drawn, and
        that is where the genre lives. A sine at 1/8 triplet is a wobble; six
        hard steps over the same cycle is a growl.

        `step` holds the value until the next point instead of interpolating,
        which is what makes the articulation bite rather than slide. */

    /** Six hard steps over one cycle: the classic stuttered triplet
        articulation. Alternating high and low, no glide between them. */
    const std::vector<FactoryBank::CurvePoint> kSteppedSix {
        { 0.000f, 1.00f, 0.0f, true },
        { 0.167f, 0.25f, 0.0f, true },
        { 0.333f, 0.85f, 0.0f, true },
        { 0.500f, 0.10f, 0.0f, true },
        { 0.667f, 0.70f, 0.0f, true },
        { 0.833f, 0.35f, 0.0f, true },
    };

    /** A vowel sweep that opens fast and closes slowly - the "yoi". The
        tension on the long segment is what stops it being a triangle. */
    const std::vector<FactoryBank::CurvePoint> kVowelSweep {
        { 0.00f, 0.15f,  0.0f, false },
        { 0.12f, 1.00f, -0.6f, false },
        { 0.55f, 0.35f,  0.4f, false },
        { 0.80f, 0.55f,  0.0f, false },
    };

    /** Gated: full, then hard down to nothing, twice per cycle. Used on
        level rather than on a filter, which is what makes the gap a GAP
        instead of a muffle. */
    const std::vector<FactoryBank::CurvePoint> kGate {
        { 0.00f, 1.0f, 0.0f, true },
        { 0.30f, 0.0f, 0.0f, true },
        { 0.50f, 1.0f, 0.0f, true },
        { 0.72f, 0.0f, 0.0f, true },
    };

    /** One slow rise and fall across the cycle, eased at both ends. The
        movement these patches are built on is a SWEEP, not an articulation -
        nothing steps, and the tension is what keeps it off a triangle. */
    const std::vector<FactoryBank::CurvePoint> kSlowSweep {
        { 0.00f, 0.05f,  0.35f, false },
        { 0.50f, 0.95f, -0.35f, false },
    };

    /** Two sweeps per cycle, offset - so two destinations driven from one
        LFO do not arrive together and the movement reads as a pair of
        things breathing rather than one. */
    const std::vector<FactoryBank::CurvePoint> kOffsetSweep {
        { 0.00f, 0.60f, -0.25f, false },
        { 0.35f, 0.15f,  0.30f, false },
        { 0.70f, 0.90f,  0.00f, false },
    };

    /** Three descending steps then a snap back - reads as a falling
        inflection, the "wah-ah-ah" that answers a growl. */
    const std::vector<FactoryBank::CurvePoint> kDescendingThree {
        { 0.00f, 0.95f, 0.0f, true },
        { 0.25f, 0.62f, 0.0f, true },
        { 0.50f, 0.30f, 0.0f, true },
        { 0.75f, 0.80f, 0.0f, false },
    };
}

std::vector<FactoryBank::Definition> FactoryBank::getDefinitions()
{
    /*  THE SIGNATURE HALF, written out. Each of these exists to exercise a
        specific part of the architecture and says in its comment what it is
        for. The generated half that follows provides COVERAGE of the space
        these map out - the same growl at eight articulations, the same sub at
        six weights - which is a thing to describe once rather than to type a
        hundred and twenty-four times. See FactoryGenerator.h. */
    std::vector<Definition> definitions {
        /*  THE PATCH THE PRODUCT EXISTS FOR. A wavetable through the formant
            filter, with a drawn LFO on the formant X at a triplet division -
            which is where the "triplet growl" in the brief comes from. The
            OTT is on because it is on in nearly every riddim patch, and the
            drive is in the FX rack rather than the filter so the harmonics
            are generated after the formant shaping rather than before it. */
        { "Triplet Growl", "Growl",
          "Formant filter under a drawn LFO at 1/8 triplet. The growl is the "
          "formant moving, not the filter sweeping.",
          "growl,triplet,formant,bass",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].wavetable, 3.0f },
              { pid::osc[0].tablePos, 0.35f },
              { pid::osc[0].level, 0.85f },
              { pid::osc[0].unisonVoices, 3.0f },
              { pid::osc[0].unisonDetune, 0.12f },
              { pid::osc[0].sendFilter1, 1.0f },
              { pid::sub.enabled, 1.0f },
              { pid::sub.level, 0.5f },
              { pid::sub.octave, -1.0f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterFormant },
              { pid::filter[0].cutoff, 900.0f },
              { pid::filter[0].resonance, 0.45f },
              { pid::filter[0].formantX, 0.35f },
              { pid::filter[0].formantY, 0.6f },
              { pid::filter[0].formantThroat, 0.3f },
              { pid::filter[0].mix, 1.0f },
              { pid::lfo[0].shape, kLfoCustom },
              { pid::lfo[0].syncEnabled, 1.0f },
              { pid::lfo[0].rateDivision, kEighthTriplet },
              { pid::modSlot[0].enabled, 1.0f },
              { pid::modSlot[0].source, kSourceLfo1 },
              { pid::modSlot[0].depth, 0.8f },
              { pid::ott.enabled, 1.0f },
              { pid::ott.depth, 0.45f },
              { pid::fxDistortion[0].enabled, 1.0f },
              { pid::fxDistortion[0].drive, 14.0f },
              { pid::fxDistortion[0].tone, 0.3f },
              { pid::fxLimiter.enabled, 1.0f },
          },
          // Block-rate: the formant coefficients are recomputed per chunk.
          { { 0, pid::filter[0].formantX } },
          { { 0, kSteppedSix } } },

        /*  The other half of the growl vocabulary: the formant held still and
            the CUTOFF wobbling, at a straight sixteenth. Slower, wider, and
            the one that sits under a drop rather than being the drop. */
        { "Sixteenth Wobble", "Bass",
          "A 24 dB low-pass wobbling at 1/16 with the formant held still. "
          "Sits under a drop rather than being it.",
          "wobble,bass,sixteenth",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].wavetable, 1.0f },
              { pid::osc[0].tablePos, 0.2f },
              { pid::osc[0].unisonVoices, 2.0f },
              { pid::osc[0].unisonDetune, 0.06f },
              { pid::sub.enabled, 1.0f },
              { pid::sub.level, 0.7f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterLowPass24 },
              { pid::filter[0].cutoff, 420.0f },
              { pid::filter[0].resonance, 0.55f },
              { pid::filter[0].drive, 0.35f },
              { pid::lfo[0].shape, kLfoSine },
              { pid::lfo[0].syncEnabled, 1.0f },
              { pid::lfo[0].rateDivision, kSixteenth },
              { pid::modSlot[0].enabled, 1.0f },
              { pid::modSlot[0].source, kSourceLfo1 },
              { pid::modSlot[0].depth, 0.65f },
              { pid::ott.enabled, 1.0f },
              { pid::ott.depth, 0.3f },
              { pid::fxLimiter.enabled, 1.0f },
          },
          // Block-rate. Cutoff, not formant - that is the whole difference
          // between this patch and the one above it.
          { { 0, pid::filter[0].cutoff } } },

        /*  A Reese: two oscillators detuned against each other through ONE
            filter, so the beating is in the source rather than in an effect.
            No FX at all on purpose - this is the foundation somebody builds
            on, and a foundation with a reverb on it is not one. */
        { "Reese Foundation", "Bass",
          "Two oscillators detuned against each other through one filter. No "
          "effects: this is a starting point, not a finished sound.",
          "reese,bass,foundation,clean",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].wavetable, 2.0f },
              { pid::osc[0].pitchFine, -14.0f },
              { pid::osc[0].level, 0.8f },
              { pid::osc[1].enabled, 1.0f },
              { pid::osc[1].wavetable, 2.0f },
              { pid::osc[1].pitchFine, 14.0f },
              { pid::osc[1].level, 0.8f },
              { pid::osc[1].sendFilter1, 1.0f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterLowPass24 },
              { pid::filter[0].cutoff, 1400.0f },
              { pid::filter[0].resonance, 0.2f },
              { pid::fxLimiter.enabled, 1.0f },
          } },

        /*  Screech: the top of the table, the hyper widening it, and a synced
            delay. The filter is a band-pass because a low-pass at this
            register removes the thing that makes it a screech. */
        { "Screech Lead", "Lead",
          "Top of the table through a band-pass, widened by the hyper and "
          "answered by a synced delay.",
          "screech,lead,bright,wide",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].wavetable, 6.0f },
              { pid::osc[0].tablePos, 0.85f },
              { pid::osc[0].pitchSemi, 12.0f },
              { pid::osc[0].unisonVoices, 5.0f },
              { pid::osc[0].unisonDetune, 0.25f },
              { pid::osc[0].unisonSpread, 0.7f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterBandPass12 },
              { pid::filter[0].cutoff, 2600.0f },
              { pid::filter[0].resonance, 0.35f },
              { pid::fxHyper.enabled, 1.0f },
              { pid::fxHyper.amount, 0.6f },
              { pid::fxHyper.width, 0.85f },
              { pid::fxDelay.enabled, 1.0f },
              { pid::fxDelay.mix, 0.28f },
              { pid::fxDelay.syncEnabled, 1.0f },
              { pid::fxDelay.division, kSixteenth },
              { pid::fxDelay.feedback, 0.4f },
              { pid::fxDelay.pingPong, 1.0f },
              { pid::fxLimiter.enabled, 1.0f },
          } },

        /*  The sub on its own, with a long release and nothing above it. The
            high cut is there because a sub drop with any top end on it fights
            whatever is playing over the top. */
        { "Sub Drop", "Bass",
          "The sub oscillator alone with a long release and the top end cut. "
          "Meant to sit under a mix, not in front of it.",
          "sub,drop,clean,low",
          {
              { pid::osc[0].enabled, 0.0f },
              { pid::sub.enabled, 1.0f },
              { pid::sub.level, 1.0f },
              { pid::sub.octave, -2.0f },
              { pid::envelope[0].attack, 0.004f },
              { pid::envelope[0].decay, 0.9f },
              { pid::envelope[0].sustain, 0.6f },
              { pid::envelope[0].release, 1.4f },
              { pid::fxEq[0].enabled, 1.0f },
              { pid::fxEq[0].lowPassFreq, 900.0f },
              { pid::fxLimiter.enabled, 1.0f },
          } },

        /*  A comb filter is a per-voice effect whose character comes from
            tracking the note, which is exactly why it lives in the voice
            filter and not in the rack. Short envelope, high feedback. */
        { "Metal Pluck", "Pluck",
          "Comb filter tracking the note, with a short envelope. The metallic "
          "ring is the comb's own resonance.",
          "pluck,comb,metal,short",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].wavetable, 0.0f },
              { pid::osc[0].level, 0.7f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterComb },
              { pid::filter[0].cutoff, 600.0f },
              { pid::filter[0].combFeedback, 0.8f },
              { pid::filter[0].combDamping, 0.25f },
              { pid::envelope[0].attack, 0.001f },
              { pid::envelope[0].decay, 0.28f },
              { pid::envelope[0].sustain, 0.0f },
              { pid::envelope[0].release, 0.2f },
              { pid::fxLimiter.enabled, 1.0f },
          } },

        /*  The one patch here that is not aggressive, and the one that shows
            the widener doing its job: a centred pad through the dimension,
            which is mono-safe by construction, plus the FDN reverb at a long
            decay. Slow attack, so it is played rather than triggered. */
        { "Dream Pad", "Pad",
          "Slow graintable pad through the mono-safe widener and a long "
          "reverb. The only patch in the bank that is not aggressive.",
          "pad,dream,wide,slow",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].mode, kGraintable },
              { pid::osc[0].wavetable, 8.0f },
              { pid::osc[0].grainSize, 90.0f },
              { pid::osc[0].grainDensity, 28.0f },
              { pid::osc[0].grainPosJitter, 0.3f },
              { pid::osc[0].unisonVoices, 4.0f },
              { pid::osc[0].unisonDetune, 0.18f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterLowPass24 },
              { pid::filter[0].cutoff, 3200.0f },
              { pid::envelope[0].attack, 0.85f },
              { pid::envelope[0].decay, 1.2f },
              { pid::envelope[0].sustain, 0.8f },
              { pid::envelope[0].release, 2.2f },
              { pid::fxChorus.enabled, 1.0f },
              { pid::fxChorus.mix, 0.35f },
              { pid::fxDimension.enabled, 1.0f },
              { pid::fxDimension.width, 0.8f },
              { pid::fxReverb.enabled, 1.0f },
              { pid::fxReverb.mix, 0.42f },
              { pid::fxReverb.size, 0.8f },
              { pid::fxReverb.decay, 0.75f },
              { pid::fxLimiter.enabled, 1.0f },
          } },

        /*  Bitcrush and downsample are the two curves that only make sense in
            the rack, because their whole character is the aliasing the voice
            filter's oversampled drive exists to remove. This is the patch
            that demonstrates why they live where they do. */
        { "Bitcrushed Stab", "FX",
          "Bitcrush and fold in series. These two curves alias on purpose, "
          "which is why they are in the rack and not in the voice filter.",
          "bitcrush,stab,dirty,fx",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].wavetable, 4.0f },
              { pid::osc[0].tablePos, 0.5f },
              { pid::envelope[0].attack, 0.001f },
              { pid::envelope[0].decay, 0.18f },
              { pid::envelope[0].sustain, 0.15f },
              { pid::envelope[0].release, 0.12f },
              { pid::fxDistortion[0].enabled, 1.0f },
              { pid::fxDistortion[0].type, kDistBitcrush },
              { pid::fxDistortion[0].drive, 22.0f },
              { pid::fxDistortion[0].mix, 0.8f },
              { pid::fxDistortion[1].enabled, 1.0f },
              { pid::fxDistortion[1].type, kDistFold },
              { pid::fxDistortion[1].drive, 8.0f },
              { pid::fxDistortion[1].mix, 0.45f },
              { pid::fxEq[0].enabled, 1.0f },
              { pid::fxEq[0].highPassFreq, 120.0f },
              { pid::fxLimiter.enabled, 1.0f },
          } },

        /*  Drive stacked on drive, which is the reason there are two
            distortion instances in the rack rather than one. The EQ between
            them is the whole point: the first tilts what the second is given,
            and no EQ after the fact can reproduce that. */
        { "Stacked Drive", "Growl",
          "Two distortions with an EQ between them. The EQ changes what the "
          "second curve is given, which an EQ afterwards cannot reproduce.",
          "drive,stack,growl,heavy",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].wavetable, 5.0f },
              { pid::osc[0].tablePos, 0.4f },
              { pid::osc[0].unisonVoices, 2.0f },
              { pid::sub.enabled, 1.0f },
              { pid::sub.level, 0.4f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterLowPass24 },
              { pid::filter[0].cutoff, 1100.0f },
              { pid::fxDistortion[0].enabled, 1.0f },
              { pid::fxDistortion[0].type, kDistTanh },
              { pid::fxDistortion[0].drive, 18.0f },
              { pid::fxDistortion[0].tone, 0.45f },
              { pid::fxEq[0].enabled, 1.0f },
              { pid::fxEq[0].band1Freq, 320.0f },
              { pid::fxEq[0].band1Gain, -6.0f },
              { pid::fxEq[0].band2Freq, 2400.0f },
              { pid::fxEq[0].band2Gain, 5.0f },
              { pid::fxDistortion[1].enabled, 1.0f },
              { pid::fxDistortion[1].type, kDistFold },
              { pid::fxDistortion[1].drive, 10.0f },
              { pid::ott.enabled, 1.0f },
              { pid::ott.depth, 0.5f },
              { pid::fxLimiter.enabled, 1.0f },
          } },

        /*  ------------------------------------------------------------------
            THE RIDDIM BANK.

            The patches above exercise the architecture - one per feature,
            roughly. These six are the genre, and they are built on one
            observation: in riddim the rhythm lives in the DRAWN CURVE, not
            in the LFO's rate. The rate says how long a cycle lasts. What
            happens inside it - six hard steps, a gate, a falling
            inflection - is what makes it a growl rather than a wobble, and
            it is why the drawable LFO exists at all.

            All six sit on a triplet or straight division at a tempo the host
            provides, so they lock to the project rather than to a number
            baked in here.

            THE SUB SITS AT 0.45, AND THAT NUMBER WAS MEASURED.
            These patches route the sub DIRECT so it skips the filter and the
            low end holds through the movement - which is right, and which
            also means the sub is the one voice the formant never touches. A
            formant filter attenuates, so the sub is what the vowel movement
            has to be heard over. Chasing that balance is what turned up the
            real bug: the formant filter was losing 12.5 dB it should not
            have been (see FormantFilter's kOutputScale), so the direct sub
            arrived 27 dB above the oscillator and sweeping the vowel across
            its whole range moved the patch by 1.4%. The growl was being
            computed correctly and buried.

            With the filter's loss compensated the balance is an ordinary
            mixing decision again, and 0.45 leaves the movement clearly
            audible over it - measured, not guessed. */

        /*  The "yoi". Two LFOs on the two formant axes at different
            divisions, so the vowel traces a path through the space instead
            of sliding along one axis - which is the difference between a
            voice and a filter sweep. X steps, Y glides. */
        { "Yoi Growl", "Growl",
          "Two LFOs on the two formant axes at different divisions, so the "
          "vowel traces a path rather than sliding along one axis. X steps, "
          "Y glides.",
          "riddim,growl,formant,triplet,vowel",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].wavetable, 5.0f },
              { pid::osc[0].tablePos, 0.45f },
              { pid::osc[0].level, 0.9f },
              { pid::osc[0].unisonVoices, 2.0f },
              { pid::osc[0].unisonDetune, 0.08f },
              { pid::osc[0].sendFilter1, 1.0f },
              { pid::sub.enabled, 1.0f },
              { pid::sub.level, 0.45f },
              { pid::sub.octave, -1.0f },
              { pid::sub.sendDirect, 1.0f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterFormant },
              { pid::filter[0].cutoff, 780.0f },
              { pid::filter[0].resonance, 0.5f },
              { pid::filter[0].formantThroat, 0.35f },
              { pid::filter[0].mix, 1.0f },
              { pid::lfo[0].shape, kLfoCustom },
              { pid::lfo[0].syncEnabled, 1.0f },
              { pid::lfo[0].rateDivision, kEighthTriplet },
              { pid::lfo[1].shape, kLfoCustom },
              { pid::lfo[1].syncEnabled, 1.0f },
              { pid::lfo[1].rateDivision, kQuarter },
              { pid::modSlot[0].enabled, 1.0f },
              { pid::modSlot[0].source, kSourceLfo1 },
              { pid::modSlot[0].depth, 0.85f },
              { pid::modSlot[1].enabled, 1.0f },
              { pid::modSlot[1].source, kSourceLfo2 },
              { pid::modSlot[1].depth, 0.6f },
              { pid::ott.enabled, 1.0f },
              { pid::ott.depth, 0.5f },
              { pid::fxDistortion[0].enabled, 1.0f },
              { pid::fxDistortion[0].drive, 16.0f },
              { pid::fxDistortion[0].tone, 0.25f },
              { pid::fxLimiter.enabled, 1.0f },
          },
          // Both block-rate.
          { { 0, pid::filter[0].formantX },
            { 1, pid::filter[0].formantY } },
          { { 0, kSteppedSix },
            { 1, kVowelSweep } } },

        /*  THE GAP IS THE POINT. A gated level, not a gated filter: closing
            a filter muffles, and riddim wants the sound to stop. The gate
            runs on osc 1's level while the SUB stays out of it and routes
            direct, so the low end holds through the holes - which is what
            keeps a gated patch from sounding thin on a big system. */
        { "Gap Chopper", "Growl",
          "A gated LEVEL, not a gated filter - closing a filter muffles, and "
          "this wants the sound to stop. The sub routes direct so the low "
          "end holds through the holes.",
          "riddim,gate,chop,stutter,triplet",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].wavetable, 3.0f },
              { pid::osc[0].tablePos, 0.6f },
              { pid::osc[0].level, 0.9f },
              { pid::osc[0].unisonVoices, 3.0f },
              { pid::osc[0].unisonDetune, 0.14f },
              { pid::osc[0].sendFilter1, 1.0f },
              { pid::sub.enabled, 1.0f },
              { pid::sub.level, 0.45f },
              { pid::sub.octave, -1.0f },
              { pid::sub.sendDirect, 1.0f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterLowPass24 },
              { pid::filter[0].cutoff, 2400.0f },
              { pid::filter[0].resonance, 0.4f },
              { pid::filter[0].drive, 0.4f },
              { pid::filter[0].mix, 1.0f },
              { pid::lfo[0].shape, kLfoCustom },
              { pid::lfo[0].syncEnabled, 1.0f },
              { pid::lfo[0].rateDivision, kEighth },
              { pid::modSlot[0].enabled, 1.0f },
              { pid::modSlot[0].source, kSourceLfo1 },
              { pid::modSlot[0].depth, 1.0f },
              { pid::ott.enabled, 1.0f },
              { pid::ott.depth, 0.55f },
              { pid::fxDistortion[0].enabled, 1.0f },
              { pid::fxDistortion[0].type, kDistHardClip },
              { pid::fxDistortion[0].drive, 12.0f },
              { pid::fxLimiter.enabled, 1.0f },
          },
          { { 0, pid::osc[0].level } },
          { { 0, kGate } } },

        /*  Fast. A 1/16 triplet is 18 steps a bar, which is past where the
            ear hears rhythm and into where it hears TIMBRE - the modulation
            stops being a pattern and becomes a buzz with a pitch of its own.
            That is the effect, and it is why the drive is lower here: there
            is already plenty of harmonic content from the modulation. */
        { "Chainsaw Riddim", "Growl",
          "A 1/16 triplet is past where the ear hears rhythm and into where "
          "it hears timbre - the modulation becomes a buzz with a pitch of "
          "its own. Less drive, because the movement already makes harmonics.",
          "riddim,fast,chainsaw,triplet,aggressive",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].wavetable, 7.0f },
              { pid::osc[0].tablePos, 0.5f },
              { pid::osc[0].level, 0.85f },
              { pid::osc[0].unisonVoices, 4.0f },
              { pid::osc[0].unisonDetune, 0.18f },
              { pid::osc[0].sendFilter1, 1.0f },
              { pid::sub.enabled, 1.0f },
              { pid::sub.level, 0.45f },
              { pid::sub.octave, -2.0f },
              { pid::sub.sendDirect, 1.0f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterFormant },
              { pid::filter[0].cutoff, 1100.0f },
              { pid::filter[0].resonance, 0.6f },
              { pid::filter[0].formantY, 0.4f },
              { pid::filter[0].formantThroat, 0.5f },
              { pid::filter[0].mix, 1.0f },
              { pid::lfo[0].shape, kLfoCustom },
              { pid::lfo[0].syncEnabled, 1.0f },
              { pid::lfo[0].rateDivision, kSixteenthTriplet },
              { pid::modSlot[0].enabled, 1.0f },
              { pid::modSlot[0].source, kSourceLfo1 },
              { pid::modSlot[0].depth, 0.9f },
              { pid::ott.enabled, 1.0f },
              { pid::ott.depth, 0.6f },
              { pid::fxDistortion[0].enabled, 1.0f },
              { pid::fxDistortion[0].drive, 8.0f },
              { pid::fxLimiter.enabled, 1.0f },
          },
          { { 0, pid::filter[0].formantX } },
          { { 0, kSteppedSix } } },

        /*  The answer phrase. A falling inflection rather than a repeating
            one, so it reads as a reply to a growl rather than as more of the
            same - which is how a riddim drop is actually arranged. The last
            step glides instead of stepping, which is the whole shape. */
        { "Answer Wah", "Growl",
          "A FALLING inflection rather than a repeating one, so it reads as "
          "a reply to a growl rather than more of the same. The last step "
          "glides instead of stepping - that is the whole shape.",
          "riddim,wah,answer,phrase,formant",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].wavetable, 4.0f },
              { pid::osc[0].tablePos, 0.3f },
              { pid::osc[0].level, 0.88f },
              { pid::osc[0].unisonVoices, 2.0f },
              { pid::osc[0].unisonDetune, 0.1f },
              { pid::osc[0].sendFilter1, 1.0f },
              { pid::sub.enabled, 1.0f },
              { pid::sub.level, 0.45f },
              { pid::sub.octave, -1.0f },
              { pid::sub.sendDirect, 1.0f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterFormant },
              { pid::filter[0].cutoff, 850.0f },
              { pid::filter[0].resonance, 0.55f },
              { pid::filter[0].formantY, 0.65f },
              { pid::filter[0].mix, 1.0f },
              { pid::lfo[0].shape, kLfoCustom },
              { pid::lfo[0].syncEnabled, 1.0f },
              { pid::lfo[0].rateDivision, kQuarter },
              { pid::modSlot[0].enabled, 1.0f },
              { pid::modSlot[0].source, kSourceLfo1 },
              { pid::modSlot[0].depth, 0.8f },
              { pid::ott.enabled, 1.0f },
              { pid::ott.depth, 0.45f },
              { pid::fxDistortion[0].enabled, 1.0f },
              { pid::fxDistortion[0].drive, 13.0f },
              { pid::fxLimiter.enabled, 1.0f },
          },
          { { 0, pid::filter[0].formantX } },
          { { 0, kDescendingThree } } },

        /*  Movement in the SOURCE rather than in the filter. The same
            stepped curve on the table position, so each step is a different
            waveform rather than the same waveform filtered differently -
            which sounds like a different instrument per step instead of one
            instrument being shaped. Worth having in the bank because it is
            the thing a filter cannot do. */
        { "Table Stepper", "Growl",
          "The stepped curve on the TABLE POSITION, so each step is a "
          "different waveform rather than the same one filtered differently. "
          "This is the thing a filter cannot do.",
          "riddim,wavetable,step,morph,triplet",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].wavetable, 6.0f },
              { pid::osc[0].tablePos, 0.2f },
              { pid::osc[0].level, 0.9f },
              { pid::osc[0].unisonVoices, 3.0f },
              { pid::osc[0].unisonDetune, 0.12f },
              { pid::osc[0].sendFilter1, 1.0f },
              { pid::sub.enabled, 1.0f },
              { pid::sub.level, 0.45f },
              { pid::sub.octave, -1.0f },
              { pid::sub.sendDirect, 1.0f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterLowPass12 },
              { pid::filter[0].cutoff, 5000.0f },
              { pid::filter[0].resonance, 0.25f },
              { pid::filter[0].mix, 1.0f },
              { pid::lfo[0].shape, kLfoCustom },
              { pid::lfo[0].syncEnabled, 1.0f },
              { pid::lfo[0].rateDivision, kEighthTriplet },
              { pid::modSlot[0].enabled, 1.0f },
              { pid::modSlot[0].source, kSourceLfo1 },
              { pid::modSlot[0].depth, 0.95f },
              { pid::ott.enabled, 1.0f },
              { pid::ott.depth, 0.5f },
              { pid::fxDistortion[0].enabled, 1.0f },
              { pid::fxDistortion[0].drive, 15.0f },
              { pid::fxDistortion[0].tone, 0.35f },
              { pid::fxLimiter.enabled, 1.0f },
          },
          { { 0, pid::osc[0].tablePos } },
          { { 0, kSteppedSix } } },

        /*  The one that does not repeat. Envelope 2 on the formant instead
            of an LFO, so the movement happens ONCE per note and the rhythm
            comes from how the notes are played rather than from a division.
            A bank of nothing but synced LFOs writes the same bar over and
            over; this is the patch that lets the player write the part. */
        { "Note Growl", "Growl",
          "Envelope 2 on the formant instead of an LFO, so the movement "
          "happens once per note and the rhythm comes from how you play "
          "rather than from a division.",
          "riddim,envelope,played,formant,expressive",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].wavetable, 5.0f },
              { pid::osc[0].tablePos, 0.4f },
              { pid::osc[0].level, 0.9f },
              { pid::osc[0].unisonVoices, 2.0f },
              { pid::osc[0].unisonDetune, 0.09f },
              { pid::osc[0].sendFilter1, 1.0f },
              { pid::sub.enabled, 1.0f },
              { pid::sub.level, 0.45f },
              { pid::sub.octave, -1.0f },
              { pid::sub.sendDirect, 1.0f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterFormant },
              { pid::filter[0].cutoff, 900.0f },
              { pid::filter[0].resonance, 0.5f },
              { pid::filter[0].formantY, 0.5f },
              { pid::filter[0].mix, 1.0f },
              // Fast in, slow out: the vowel opens on the attack and closes
              // across the note, which is the shape a mouth actually makes.
              { pid::envelope[1].attack, 0.004f },
              { pid::envelope[1].decay, 0.35f },
              { pid::envelope[1].sustain, 0.15f },
              { pid::envelope[1].release, 0.2f },
              { pid::modSlot[0].enabled, 1.0f },
              { pid::modSlot[0].source, kSourceEnv2 },
              { pid::modSlot[0].depth, 0.9f },
              { pid::ott.enabled, 1.0f },
              { pid::ott.depth, 0.45f },
              { pid::fxDistortion[0].enabled, 1.0f },
              { pid::fxDistortion[0].drive, 14.0f },
              { pid::fxLimiter.enabled, 1.0f },
          },
          // No curve: an envelope is not an LFO and has no drawn shape.
          { { 0, pid::filter[0].formantX } } },

        /*  ------------------------------------------------------------------
            THE GRAINTABLE PACK.

            A different instrument from the riddim bank above, built around
            the chain a Reason Combinator gets its character from: a
            graintable oscillator, made vocal by a vocoder, then phased,
            distorted and reverbed. GNARL has every one of those - graintable
            mode, the formant filter (which IS the vocoder character here),
            the phaser, the drive curves, the reverb - so the sound is
            reachable without anything being imported.

            AND NOTHING WAS. CLAUDE.md section 9 names Malstrom specifically:
            only tables we generated. Every patch below plays one of the
            twenty tables the plugin synthesises for itself. A Combinator
            patch file would not have helped anyway - it references its
            graintable by name and the audio lives in a soundbank - but the
            rule is the reason, not the convenience.

            WHAT MAKES THESE ONE FAMILY rather than ten unrelated patches:
            the movement is a SWEEP, not an articulation. The riddim bank
            steps hard six times a cycle; these breathe once every two or
            four bars. Same engine, opposite gesture.

            AND THE FIRST VERSION OF THIS PACK WAS INAUDIBLE, which is worth
            recording because none of it was a bug. Measured against the
            riddim bank's -23 to -31 dBFS, these arrived at -39 to -61, and
            "Grain Choir" peaked at 0.003 - fifty decibels down.

            Three costs stack, and each is legitimate on its own. Graintable
            mode is about 5 dB under wavetable mode, because grains are
            windowed. The formant filter costs another 8, which is its
            documented residual (section 3). And GRAIN SIZE AND DENSITY MOVE
            THE LEVEL BY 25 dB across their ranges - long grains at high
            density are far quieter than moderate ones, which is the opposite
            of what "more grains, more overlap" suggests. Every patch here
            now stays inside roughly 20-60 ms and 14-26.

            The last cost was the largest and the least obvious: these
            patches deliberately have no drive stages, and the riddim bank
            gets most of its level FROM its drive stages. A patch with no
            nonlinearity anywhere has no gain staging either. A gentle OTT
            does that job here without changing the character - which is also
            what the Combinator this pack takes its shape from does. */

        /*  The bones of the family: a pulse-width graintable swept slowly,
            read by the formant filter so the sweep comes out vocal, then
            phased. The grain size is the control that matters - at 40 ms the
            grains overlap into a tone, and shortening them is what turns it
            granular. */
        { "Square Sweeper", "Texture",
          "A pulse-width graintable swept slowly and read by the formant "
          "filter, so the sweep comes out vocal rather than merely bright.",
          "graintable,sweep,vocal,phaser,texture",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].mode, kGraintable },
              { pid::osc[0].wavetable, kTablePulseWidth },
              { pid::osc[0].tablePos, 0.2f },
              { pid::osc[0].grainSize, 40.0f },
              { pid::osc[0].grainDensity, 20.0f },
              { pid::osc[0].level, 0.85f },
              { pid::osc[0].unisonVoices, 2.0f },
              { pid::osc[0].unisonDetune, 0.06f },
              { pid::osc[0].sendFilter1, 1.0f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterFormant },
              { pid::filter[0].resonance, 0.4f },
              { pid::filter[0].formantY, 0.45f },
              { pid::filter[0].mix, 1.0f },
              { pid::lfo[0].shape, kLfoCustom },
              { pid::lfo[0].syncEnabled, 1.0f },
              { pid::lfo[0].rateDivision, kTwoBars },
              { pid::modSlot[0].enabled, 1.0f },
              { pid::modSlot[0].source, kSourceLfo1 },
              { pid::modSlot[0].depth, 0.75f },
              { pid::ott.enabled, 1.0f },
              { pid::ott.depth, 0.3f },
              { pid::fxPhaser.enabled, 1.0f },
              { pid::fxPhaser.mix, 0.5f },
              { pid::fxPhaser.stages, 6.0f },
              { pid::fxPhaser.feedback, 0.4f },
              { pid::fxReverb.enabled, 1.0f },
              { pid::fxReverb.mix, 0.3f },
              { pid::fxLimiter.enabled, 1.0f },
          },
          // Block-rate: the table position, so the sweep is in the SOURCE
          // and the formant filter reads whatever arrives.
          { { 0, pid::osc[0].tablePos } },
          { { 0, kSlowSweep } } },

        /*  The vocoder, taken literally. A talk-box table through the
            formant filter with both vowel axes moving - which is what a
            vocoder does that a filter sweep cannot: the two formants move
            independently, so the sound says something rather than just
            opening. */
        { "Vocoder Bloom", "Pad",
          "Both vowel axes moving independently, which is what a vocoder "
          "does that a filter sweep cannot - the sound says something "
          "rather than just opening.",
          "vocoder,formant,pad,graintable,vocal",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].mode, kGraintable },
              { pid::osc[0].wavetable, kTableTalkBox },
              { pid::osc[0].tablePos, 0.4f },
              { pid::osc[0].grainSize, 45.0f },
              { pid::osc[0].grainDensity, 24.0f },
              { pid::osc[0].level, 0.8f },
              { pid::osc[0].unisonVoices, 3.0f },
              { pid::osc[0].unisonDetune, 0.1f },
              { pid::osc[0].sendFilter1, 1.0f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterFormant },
              { pid::filter[0].resonance, 0.55f },
              { pid::filter[0].formantThroat, 0.3f },
              { pid::filter[0].mix, 1.0f },
              { pid::lfo[0].shape, kLfoCustom },
              { pid::lfo[0].syncEnabled, 1.0f },
              { pid::lfo[0].rateDivision, kTwoBars },
              { pid::lfo[1].shape, kLfoCustom },
              { pid::lfo[1].syncEnabled, 1.0f },
              { pid::lfo[1].rateDivision, kFourBars },
              { pid::modSlot[0].enabled, 1.0f },
              { pid::modSlot[0].source, kSourceLfo1 },
              { pid::modSlot[0].depth, 0.8f },
              { pid::modSlot[1].enabled, 1.0f },
              { pid::modSlot[1].source, kSourceLfo2 },
              { pid::modSlot[1].depth, 0.65f },
              { pid::ott.enabled, 1.0f },
              { pid::ott.depth, 0.32f },
              { pid::fxReverb.enabled, 1.0f },
              { pid::fxReverb.mix, 0.42f },
              { pid::fxReverb.decay, 0.7f },
              { pid::fxLimiter.enabled, 1.0f },
          },
          { { 0, pid::filter[0].formantX },
            { 1, pid::filter[0].formantY } },
          { { 0, kSlowSweep },
            { 1, kOffsetSweep } } },

        /*  The scream. A pulse-width graintable driven hard through the tube
            curve with the phaser after it - the order matters, because a
            phaser BEFORE distortion has its notches filled back in by the
            harmonics the drive generates. */
        { "Screamer Lead", "Lead",
          "Driven hard through the tube curve with the phaser AFTER it - a "
          "phaser before distortion has its notches filled back in by the "
          "harmonics the drive makes.",
          "lead,scream,distortion,phaser,graintable",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].mode, kGraintable },
              { pid::osc[0].wavetable, kTablePulseWidth },
              { pid::osc[0].tablePos, 0.65f },
              { pid::osc[0].grainSize, 25.0f },
              { pid::osc[0].grainDensity, 24.0f },
              { pid::osc[0].level, 0.85f },
              { pid::osc[0].unisonVoices, 4.0f },
              { pid::osc[0].unisonDetune, 0.15f },
              { pid::osc[0].sendFilter1, 1.0f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterBandPass12 },
              { pid::filter[0].cutoff, 1800.0f },
              { pid::filter[0].resonance, 0.5f },
              { pid::filter[0].mix, 1.0f },
              { pid::lfo[0].shape, kLfoCustom },
              { pid::lfo[0].syncEnabled, 1.0f },
              { pid::lfo[0].rateDivision, kHalf },
              { pid::modSlot[0].enabled, 1.0f },
              { pid::modSlot[0].source, kSourceLfo1 },
              { pid::modSlot[0].depth, 0.6f },
              { pid::fxDistortion[0].enabled, 1.0f },
              { pid::fxDistortion[0].type, kDistTube },
              { pid::fxDistortion[0].drive, 20.0f },
              { pid::fxDistortion[0].tone, 0.4f },
              { pid::fxPhaser.enabled, 1.0f },
              { pid::fxPhaser.mix, 0.55f },
              { pid::fxPhaser.stages, 8.0f },
              { pid::fxPhaser.rate, 0.3f },
              { pid::fxReverb.enabled, 1.0f },
              { pid::fxReverb.mix, 0.18f },
              { pid::fxLimiter.enabled, 1.0f },
          },
          { { 0, pid::filter[0].cutoff } },
          { { 0, kSlowSweep } } },

        /*  Long grains, high density, no drive at all. The one in the pack
            that proves the character is the GRAINTABLE and not the
            distortion - take every nonlinear stage away and it still sounds
            like this family. */
        { "Grain Choir", "Pad",
          "Long grains, high density, and no drive anywhere. Proves the "
          "character is the graintable rather than the distortion.",
          "pad,choir,graintable,clean,reverb",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].mode, kGraintable },
              { pid::osc[0].wavetable, kTableAdditiveStack },
              { pid::osc[0].tablePos, 0.3f },
              { pid::osc[0].grainSize, 55.0f },
              { pid::osc[0].grainDensity, 25.0f },
              { pid::osc[0].level, 1.0f },
              { pid::osc[0].unisonVoices, 3.0f },
              { pid::osc[0].unisonDetune, 0.13f },
              { pid::osc[0].unisonSpread, 0.8f },
              { pid::osc[0].sendFilter1, 1.0f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterFormant },
              { pid::filter[0].resonance, 0.3f },
              { pid::filter[0].formantY, 0.7f },
              { pid::filter[0].mix, 0.45f },
              { pid::envelope[0].attack, 0.6f },
              { pid::envelope[0].release, 1.4f },
              { pid::lfo[0].shape, kLfoCustom },
              { pid::lfo[0].syncEnabled, 1.0f },
              { pid::lfo[0].rateDivision, kFourBars },
              { pid::modSlot[0].enabled, 1.0f },
              { pid::modSlot[0].source, kSourceLfo1 },
              { pid::modSlot[0].depth, 0.5f },
              /*  OTT DOING ALL THE GAIN STAGING, at more depth than the
                  others need. This is the quietest construction in the bank
                  - graintable, through the formant filter, with no
                  nonlinearity anywhere to add level - and it is the patch
                  that found the whole problem. Upward compression is what
                  lifts it without putting drive into a patch whose entire
                  claim is that it has none.

                  The formant mix sits at 0.45 rather than 1.0 for the same
                  reason: the filter is the attenuator here, so letting some
                  of the dry graintable past is level AND keeps the vowel
                  audible as a colour rather than as the whole sound. */
              { pid::ott.enabled, 1.0f },
              { pid::ott.depth, 0.75f },
              { pid::ott.outputGain, 9.0f },
              { pid::fxDimension.enabled, 1.0f },
              { pid::fxDimension.width, 0.6f },
              { pid::fxReverb.enabled, 1.0f },
              { pid::fxReverb.mix, 0.55f },
              { pid::fxReverb.decay, 0.85f },
              { pid::fxReverb.size, 0.8f },
              { pid::fxLimiter.enabled, 1.0f },
          },
          { { 0, pid::filter[0].formantX } },
          { { 0, kSlowSweep } } },

        /*  Short grains at low density: the gaps between them are audible,
            which is the granular sound proper rather than a tone that
            happens to be made of grains. */
        { "Glass Drift", "Texture",
          "Short grains at LOW density, so the gaps between them are "
          "audible - granular proper, rather than a tone that happens to be "
          "made of grains.",
          "granular,glass,texture,sparse,phaser",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].mode, kGraintable },
              { pid::osc[0].wavetable, kTableGlassHarmonics },
              { pid::osc[0].tablePos, 0.5f },
              { pid::osc[0].grainSize, 22.0f },
              { pid::osc[0].grainDensity, 14.0f },
              { pid::osc[0].level, 0.8f },
              { pid::osc[0].unisonVoices, 3.0f },
              { pid::osc[0].unisonDetune, 0.25f },
              { pid::osc[0].unisonSpread, 0.9f },
              { pid::osc[0].sendFilter1, 1.0f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterLowPass12 },
              { pid::filter[0].cutoff, 6000.0f },
              { pid::filter[0].resonance, 0.2f },
              { pid::filter[0].mix, 1.0f },
              { pid::envelope[0].attack, 0.25f },
              { pid::envelope[0].release, 1.8f },
              { pid::lfo[0].shape, kLfoCustom },
              { pid::lfo[0].syncEnabled, 1.0f },
              { pid::lfo[0].rateDivision, kFourBars },
              { pid::modSlot[0].enabled, 1.0f },
              { pid::modSlot[0].source, kSourceLfo1 },
              { pid::modSlot[0].depth, 0.7f },
              { pid::ott.enabled, 1.0f },
              { pid::ott.depth, 0.35f },
              { pid::fxPhaser.enabled, 1.0f },
              { pid::fxPhaser.mix, 0.45f },
              { pid::fxPhaser.stages, 12.0f },
              { pid::fxReverb.enabled, 1.0f },
              { pid::fxReverb.mix, 0.6f },
              { pid::fxReverb.decay, 0.9f },
              { pid::fxLimiter.enabled, 1.0f },
          },
          { { 0, pid::osc[0].grainSize } },
          { { 0, kSlowSweep } } },

        /*  The phaser doing the talking instead of the filter. Twelve stages
            is six notches (see FxModulationTests), swept slowly - which on a
            harmonically dense source reads as movement through the sound
            rather than across it. */
        { "Phase Talker", "Texture",
          "The phaser doing the talking instead of the filter. Twelve "
          "stages is six notches, swept slowly across a dense source.",
          "phaser,texture,graintable,movement",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].mode, kGraintable },
              { pid::osc[0].wavetable, kTablePhaseDistortion },
              { pid::osc[0].tablePos, 0.55f },
              { pid::osc[0].grainSize, 35.0f },
              { pid::osc[0].grainDensity, 22.0f },
              { pid::osc[0].level, 0.85f },
              { pid::osc[0].unisonVoices, 2.0f },
              { pid::osc[0].unisonDetune, 0.08f },
              { pid::osc[0].sendFilter1, 1.0f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterLowPass24 },
              { pid::filter[0].cutoff, 4500.0f },
              { pid::filter[0].resonance, 0.25f },
              { pid::filter[0].mix, 1.0f },
              { pid::lfo[0].shape, kLfoCustom },
              { pid::lfo[0].syncEnabled, 1.0f },
              { pid::lfo[0].rateDivision, kTwoBars },
              { pid::modSlot[0].enabled, 1.0f },
              { pid::modSlot[0].source, kSourceLfo1 },
              { pid::modSlot[0].depth, 0.85f },
              { pid::fxPhaser.enabled, 1.0f },
              { pid::fxPhaser.mix, 0.7f },
              { pid::fxPhaser.stages, 12.0f },
              { pid::fxPhaser.feedback, 0.55f },
              { pid::fxPhaser.rate, 0.12f },
              { pid::fxReverb.enabled, 1.0f },
              { pid::fxReverb.mix, 0.28f },
              { pid::fxLimiter.enabled, 1.0f },
          },
          { { 0, pid::osc[0].tablePos } },
          { { 0, kOffsetSweep } } },

        /*  Bell-like, struck rather than swept - an envelope on the table
            position instead of an LFO, so the timbre decays with the note
            and repeating it does not sound identical every time. */
        { "Metal Bloom", "Pluck",
          "Struck rather than swept: an envelope on the table position, so "
          "the timbre decays with the note instead of cycling.",
          "bell,metallic,pluck,graintable,envelope",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].mode, kGraintable },
              { pid::osc[0].wavetable, kTableRingModBell },
              { pid::osc[0].tablePos, 0.15f },
              { pid::osc[0].grainSize, 20.0f },
              { pid::osc[0].grainDensity, 26.0f },
              { pid::osc[0].level, 0.85f },
              { pid::osc[0].unisonVoices, 2.0f },
              { pid::osc[0].unisonDetune, 0.05f },
              { pid::osc[0].sendFilter1, 1.0f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterBandPass12 },
              { pid::filter[0].cutoff, 2200.0f },
              { pid::filter[0].resonance, 0.45f },
              { pid::filter[0].mix, 1.0f },
              { pid::envelope[0].attack, 0.002f },
              { pid::envelope[0].decay, 0.9f },
              { pid::envelope[0].sustain, 0.0f },
              { pid::envelope[0].release, 0.7f },
              { pid::envelope[2].attack, 0.002f },
              { pid::envelope[2].decay, 0.5f },
              { pid::envelope[2].sustain, 0.1f },
              { pid::modSlot[0].enabled, 1.0f },
              { pid::modSlot[0].source, kSourceEnv3 },
              { pid::modSlot[0].depth, 0.8f },
              { pid::fxDelay.enabled, 1.0f },
              { pid::fxDelay.mix, 0.25f },
              { pid::fxReverb.enabled, 1.0f },
              { pid::fxReverb.mix, 0.4f },
              { pid::fxLimiter.enabled, 1.0f },
          },
          // No curve: an envelope has no drawn shape.
          { { 0, pid::osc[0].tablePos } } },

        /*  A comb filter tracking the note under a graintable, which is two
            resonators fighting - the comb's and the grain rate's. Detuning
            the grain density against the note is what makes it metallic
            rather than merely filtered. */
        { "Hollow Pulse", "Texture",
          "A comb tracking the note under a graintable: two resonators, the "
          "comb's and the grain rate's, deliberately not agreeing.",
          "comb,hollow,graintable,resonant,texture",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].mode, kGraintable },
              { pid::osc[0].wavetable, kTableHollowComb },
              { pid::osc[0].tablePos, 0.4f },
              { pid::osc[0].grainSize, 18.0f },
              { pid::osc[0].grainDensity, 24.0f },
              { pid::osc[0].level, 0.8f },
              { pid::osc[0].sendFilter1, 1.0f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterComb },
              { pid::filter[0].resonance, 0.6f },
              { pid::filter[0].mix, 0.85f },
              { pid::lfo[0].shape, kLfoCustom },
              { pid::lfo[0].syncEnabled, 1.0f },
              { pid::lfo[0].rateDivision, kTwoBars },
              { pid::modSlot[0].enabled, 1.0f },
              { pid::modSlot[0].source, kSourceLfo1 },
              { pid::modSlot[0].depth, 0.55f },
              { pid::fxPhaser.enabled, 1.0f },
              { pid::fxPhaser.mix, 0.4f },
              { pid::fxReverb.enabled, 1.0f },
              { pid::fxReverb.mix, 0.35f },
              { pid::fxLimiter.enabled, 1.0f },
          },
          { { 0, pid::osc[0].grainDensity } },
          { { 0, kOffsetSweep } } },

        /*  The slowest thing in the bank: four bars for one pass of the
            vowel. At that rate the formant stops reading as an effect and
            starts reading as the patch simply being alive. */
        { "Vowel Drift", "Pad",
          "Four bars for one pass of the vowel. At that rate the formant "
          "stops reading as an effect and starts reading as the patch being "
          "alive.",
          "pad,vowel,slow,formant,ambient",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].mode, kGraintable },
              { pid::osc[0].wavetable, kTableGrowlVowels },
              { pid::osc[0].tablePos, 0.25f },
              { pid::osc[0].grainSize, 50.0f },
              { pid::osc[0].grainDensity, 24.0f },
              { pid::osc[0].level, 0.75f },
              { pid::osc[0].unisonVoices, 4.0f },
              { pid::osc[0].unisonDetune, 0.14f },
              { pid::osc[0].unisonSpread, 0.7f },
              { pid::osc[0].sendFilter1, 1.0f },
              { pid::sub.enabled, 1.0f },
              { pid::sub.level, 0.25f },
              { pid::sub.sendDirect, 1.0f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterFormant },
              { pid::filter[0].resonance, 0.45f },
              { pid::filter[0].formantThroat, 0.25f },
              { pid::filter[0].mix, 1.0f },
              { pid::envelope[0].attack, 0.9f },
              { pid::envelope[0].release, 2.0f },
              { pid::lfo[0].shape, kLfoCustom },
              { pid::lfo[0].syncEnabled, 1.0f },
              { pid::lfo[0].rateDivision, kFourBars },
              { pid::modSlot[0].enabled, 1.0f },
              { pid::modSlot[0].source, kSourceLfo1 },
              { pid::modSlot[0].depth, 0.9f },
              { pid::fxDimension.enabled, 1.0f },
              { pid::fxDimension.width, 0.5f },
              { pid::fxReverb.enabled, 1.0f },
              { pid::fxReverb.mix, 0.5f },
              { pid::fxReverb.decay, 0.8f },
              { pid::fxLimiter.enabled, 1.0f },
          },
          { { 0, pid::filter[0].formantX } },
          { { 0, kSlowSweep } } },

        /*  The retro-transformer flavour: bitcrush into tube drive, which is
            a different dirt from the riddim bank's hard clip - quantisation
            noise rather than folded harmonics. Deliberately the only patch
            here with no reverb, so the grit has nothing to hide behind. */
        { "Retro Grit", "FX",
          "Bitcrush into tube drive - quantisation noise rather than folded "
          "harmonics. No reverb, so the grit has nothing to hide behind.",
          "retro,bitcrush,lofi,grit,graintable",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::osc[0].mode, kGraintable },
              { pid::osc[0].wavetable, kTableBitcrushSteps },
              { pid::osc[0].tablePos, 0.6f },
              { pid::osc[0].grainSize, 30.0f },
              { pid::osc[0].grainDensity, 22.0f },
              { pid::osc[0].level, 0.85f },
              { pid::osc[0].unisonVoices, 2.0f },
              { pid::osc[0].unisonDetune, 0.09f },
              { pid::osc[0].sendFilter1, 1.0f },
              { pid::filter[0].enabled, 1.0f },
              { pid::filter[0].type, kFilterLowPass12 },
              { pid::filter[0].cutoff, 3200.0f },
              { pid::filter[0].resonance, 0.35f },
              { pid::filter[0].mix, 1.0f },
              { pid::lfo[0].shape, kLfoCustom },
              { pid::lfo[0].syncEnabled, 1.0f },
              { pid::lfo[0].rateDivision, kHalf },
              { pid::modSlot[0].enabled, 1.0f },
              { pid::modSlot[0].source, kSourceLfo1 },
              { pid::modSlot[0].depth, 0.65f },
              { pid::fxDistortion[0].enabled, 1.0f },
              { pid::fxDistortion[0].type, kDistBitcrush },
              { pid::fxDistortion[0].drive, 10.0f },
              { pid::fxDistortion[1].enabled, 1.0f },
              { pid::fxDistortion[1].type, kDistTube },
              { pid::fxDistortion[1].drive, 14.0f },
              { pid::fxPhaser.enabled, 1.0f },
              { pid::fxPhaser.mix, 0.35f },
              { pid::fxLimiter.enabled, 1.0f },
          },
          { { 0, pid::filter[0].cutoff } },
          { { 0, kOffsetSweep } } },

        /*  The empty patch, and it earns its place: somebody who wants to
            build from nothing should not have to switch fourteen effects off
            first. Everything at its default except the limiter, which stays
            on because a blank patch that can clip is not a useful blank
            patch. */
        { "Init", "Keys",
          "Everything at its default, with the limiter on. A starting point "
          "that does not need fourteen effects switched off first.",
          "init,blank,default",
          {
              { pid::osc[0].enabled, 1.0f },
              { pid::fxLimiter.enabled, 1.0f },
          } },
    };

    /*  APPENDED, NEVER INTERLEAVED. A preset stores an index into this list,
        so inserting the generated ones among the hand-written ones would
        repoint every saved reference by however many landed before it. New
        presets go on the end, for the same reason a choice list is
        append-only (CLAUDE.md section 4). */
    const auto generated = generateVariations();
    definitions.insert (definitions.end(), generated.begin(), generated.end());

    return definitions;
}

int FactoryBank::getAuditionNote (const juce::String& category)
{
    // A bass patch auditioned at C4 and a pad auditioned at C1 are both
    // misleading, and the second is what the renderer used to do to the
    // whole bank.
    /*  "Sub" belongs with these and not with the default. A sub patch
        auditioned at C4 is a patch auditioned an octave above anything it
        contains - the low-pass at 140-380 Hz removes the note - and the bank
        test would then fail it for being inaudible when the only thing wrong
        was the question. */
    if (category == "Bass" || category == "Growl" || category == "Sub")
        return 36;                          // C1 - where a riddim patch lives.

    if (category == "Pluck" || category == "FX" || category == "Sequence")
        return 48;                          // C2.

    // Pads, leads, keys and textures: C4, where somebody would play them.
    return 60;
}

int FactoryBank::getCount()
{
    return static_cast<int> (getDefinitions().size());
}

std::vector<juce::ValueTree> FactoryBank::build (
    const juce::AudioProcessorValueTreeState& state,
    const juce::ValueTree& defaultState)
{
    std::vector<juce::ValueTree> presets;

    if (! defaultState.isValid())
        return presets;

    for (const auto& definition : getDefinitions())
    {
        // A COMPLETE state each time: the default tree copied, then the
        // overrides written into it. See the class comment for why a partial
        // tree would make a factory preset sound different depending on what
        // was loaded before it.
        auto tree = defaultState.createCopy();

        for (const auto& setting : definition.settings)
        {
            auto* parameter = state.getParameter (setting.id);

            // A setting naming a parameter that does not exist is a typo in
            // the table above, and skipping it quietly is how that typo
            // survives. It cannot be asserted in a shipping build, so the
            // test asserts it instead.
            jassert (parameter != nullptr);

            if (parameter == nullptr)
                continue;

            // Stored DENORMALISED, exactly as the APVTS stores it, so the
            // value written here means what the table says rather than being
            // reinterpreted through the range twice.
            auto node = tree.getChildWithProperty ("id", setting.id);

            if (node.isValid())
                node.setProperty ("value", setting.value, nullptr);
        }

        /*  THE MODULATION, which is not parameters and therefore not
            covered by the loop above.

            Written as a MODSTATE child matching what ModStateBridge parses,
            rather than going through the bridge itself: `build` is static
            and has only a tree, and a preset is a tree. The element and
            property names are the bridge's - see params/ModStateBridge.cpp -
            and ParameterMirrorTests would be the place to notice if they
            ever diverge.

            Skipped entirely for a patch with no routings and no curves, so
            an unmodulated preset stays byte-identical to the default state
            plus its overrides. */
        if (! definition.routings.empty() || ! definition.curves.empty())
        {
            juce::ValueTree modState { params::ModStateBridge::getModStateType() };

            for (const auto& routing : definition.routings)
            {
                juce::ValueTree slot { params::ModStateBridge::getSlotType() };
                slot.setProperty ("index", routing.slot, nullptr);
                slot.setProperty ("destination", juce::String (routing.destination), nullptr);
                modState.appendChild (slot, nullptr);
            }

            for (const auto& curve : definition.curves)
            {
                juce::ValueTree curveTree { params::ModStateBridge::getCurveType() };
                curveTree.setProperty ("index", curve.lfo, nullptr);

                for (const auto& point : curve.points)
                {
                    juce::ValueTree pointTree { params::ModStateBridge::getPointType() };
                    pointTree.setProperty ("time", point.time, nullptr);
                    pointTree.setProperty ("value", point.value, nullptr);
                    pointTree.setProperty ("tension", point.tension, nullptr);
                    pointTree.setProperty ("shape", point.step ? 1 : 0, nullptr);
                    curveTree.appendChild (pointTree, nullptr);
                }

                modState.appendChild (curveTree, nullptr);
            }

            // Replace rather than append: the default state may already
            // carry one, and two MODSTATE children would leave which one
            // wins up to iteration order.
            tree.removeChild (tree.getChildWithName (params::ModStateBridge::getModStateType()),
                              nullptr);
            tree.appendChild (modState, nullptr);
        }

        Metadata metadata;
        metadata.name = definition.name;
        metadata.author = "GNARL";
        metadata.category = definition.category;
        metadata.description = definition.description;
        metadata.tags = juce::StringArray::fromTokens (definition.tags, ",", "");
        metadata.tags.trim();
        metadata.tags.removeEmptyStrings();
        metadata.pluginVersion = JucePlugin_VersionString;

        // Qualified: inside FactoryBank::build, an unqualified `build` would
        // resolve to this very function rather than to the format's.
        presets.push_back (preset::build (metadata, tree));
    }

    return presets;
}

} // namespace gnarl::preset
