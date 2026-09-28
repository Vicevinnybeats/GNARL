#include "FactoryGenerator.h"

#include "../params/ParameterChoices.h"
#include "../params/ParameterIDs.h"

#include <array>
#include <deque>
#include <string>

namespace gnarl::preset
{

namespace
{
    using Setting = FactoryBank::Setting;
    using Routing = FactoryBank::Routing;
    using Curve = FactoryBank::Curve;
    using CurvePoint = FactoryBank::CurvePoint;
    using Definition = FactoryBank::Definition;

    // --- names for the numbers ---------------------------------------------
    constexpr auto kFormant = static_cast<float> (choices::FilterType::formant);
    constexpr auto kLp24 = static_cast<float> (choices::FilterType::lowPass24);
    constexpr auto kLp12 = static_cast<float> (choices::FilterType::lowPass12);
    constexpr auto kBp12 = static_cast<float> (choices::FilterType::bandPass12);
    constexpr auto kBp24 = static_cast<float> (choices::FilterType::bandPass24);
    constexpr auto kComb = static_cast<float> (choices::FilterType::comb);
    constexpr auto kLadderLp = static_cast<float> (choices::FilterType::ladderLowPass);

    constexpr auto kCustom = static_cast<float> (choices::LfoShape::custom);
    constexpr auto kSine = static_cast<float> (choices::LfoShape::sine);
    constexpr auto kTriangle = static_cast<float> (choices::LfoShape::triangle);
    constexpr auto kSquare = static_cast<float> (choices::LfoShape::square);
    constexpr auto kRandomStep = static_cast<float> (choices::LfoShape::randomStep);

    constexpr auto kQuarter = static_cast<float> (choices::LfoRateDivision::quarter);
    constexpr auto kEighth = static_cast<float> (choices::LfoRateDivision::eighth);
    constexpr auto kEighthT = static_cast<float> (choices::LfoRateDivision::eighthTriplet);
    constexpr auto kEighthD = static_cast<float> (choices::LfoRateDivision::eighthDotted);
    constexpr auto kSixteenth = static_cast<float> (choices::LfoRateDivision::sixteenth);
    constexpr auto kSixteenthT =
        static_cast<float> (choices::LfoRateDivision::sixteenthTriplet);
    constexpr auto kHalf = static_cast<float> (choices::LfoRateDivision::half);
    constexpr auto kWhole = static_cast<float> (choices::LfoRateDivision::whole);

    constexpr auto kLfo1 = static_cast<float> (choices::ModSource::lfo1);
    constexpr auto kLfo2 = static_cast<float> (choices::ModSource::lfo2);
    constexpr auto kEnv2 = static_cast<float> (choices::ModSource::env2);

    constexpr auto kTanh = static_cast<float> (choices::FxDistortionType::tanh);
    constexpr auto kTube = static_cast<float> (choices::FxDistortionType::tube);
    constexpr auto kHardClip = static_cast<float> (choices::FxDistortionType::hardClip);
    constexpr auto kFold = static_cast<float> (choices::FxDistortionType::fold);
    constexpr auto kBitcrush = static_cast<float> (choices::FxDistortionType::bitcrush);

    constexpr auto kSubSine = static_cast<float> (choices::SubWaveform::sine);
    constexpr auto kSubTriangle = static_cast<float> (choices::SubWaveform::triangle);
    constexpr auto kSubSquare = static_cast<float> (choices::SubWaveform::square);

    /*  Table indices. The ORDER IS FROZEN (WavetableLibrary::kTableNames), for
        the same reason a choice list is: a preset stores the index. */
    constexpr float kGrowlTables[] = { 1.0f, 3.0f, 7.0f, 13.0f, 16.0f };
    constexpr float kBassTables[] = { 1.0f, 2.0f, 3.0f, 9.0f };
    constexpr float kBrightTables[] = { 2.0f, 10.0f, 11.0f, 15.0f, 16.0f };
    constexpr float kVocalTables[] = { 1.0f, 7.0f, 13.0f };

    // --- deterministic variation -------------------------------------------
    /*  A fixed sequence, not `rand()`. The bank must be byte-identical from
        build to build: a preset stores an INDEX into this list, so a bank that
        reshuffled itself between versions would silently repoint every saved
        reference to it - the same argument that freezes the choice lists. */
    struct Rng
    {
        std::uint32_t state;

        std::uint32_t next()
        {
            // xorshift32: small, fixed, and the same on every platform.
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return state;
        }

        /** 0..1. */
        float unit() { return static_cast<float> (next() % 100000u) / 100000.0f; }

        float range (float lo, float hi) { return lo + unit() * (hi - lo); }

        /** Rounded to `step`, so values read as decisions rather than as noise. */
        float quantised (float lo, float hi, float step)
        {
            const auto steps = static_cast<int> ((hi - lo) / step);
            return lo + static_cast<float> (static_cast<int> (next() % static_cast<std::uint32_t> (steps + 1))) * step;
        }

        int index (std::size_t n) { return static_cast<int> (next() % static_cast<std::uint32_t> (n)); }

        template <typename T, std::size_t N>
        T pick (const T (&options)[N]) { return options[next() % N]; }

        bool chance (float p) { return unit() < p; }
    };

    // --- drawn curves ------------------------------------------------------
    /*  THE RHYTHM IS THE CURVE, NOT THE RATE. The rate says how long one cycle
        lasts; what happens inside it is drawn, and that is where the genre
        lives. `step` holds a value until the next point instead of gliding to
        it, which is what makes the articulation bite. */

    const std::vector<CurvePoint> kStepSix {
        { 0.000f, 1.00f, 0.0f, true }, { 0.167f, 0.22f, 0.0f, true },
        { 0.333f, 0.86f, 0.0f, true }, { 0.500f, 0.18f, 0.0f, true },
        { 0.667f, 0.94f, 0.0f, true }, { 0.833f, 0.30f, 0.0f, true },
        { 1.000f, 1.00f, 0.0f, true },
    };

    const std::vector<CurvePoint> kStepFour {
        { 0.000f, 0.95f, 0.0f, true }, { 0.250f, 0.20f, 0.0f, true },
        { 0.500f, 0.75f, 0.0f, true }, { 0.750f, 0.10f, 0.0f, true },
        { 1.000f, 0.95f, 0.0f, true },
    };

    /** Long-short-short: the gallop that carries most riddim bars. */
    const std::vector<CurvePoint> kGallop {
        { 0.000f, 1.00f, 0.0f, true }, { 0.500f, 0.15f, 0.0f, true },
        { 0.750f, 0.80f, 0.0f, true }, { 0.875f, 0.25f, 0.0f, true },
        { 1.000f, 1.00f, 0.0f, true },
    };

    /** A slide rather than a step: the wobble half of the vocabulary. */
    const std::vector<CurvePoint> kSlide {
        { 0.000f, 0.10f, 0.6f, false }, { 0.500f, 1.00f, -0.6f, false },
        { 1.000f, 0.10f, 0.0f, false },
    };

    /** Bitten off: a hard attack that decays, eight times a cycle. */
    const std::vector<CurvePoint> kChew {
        { 0.000f, 1.00f, -0.8f, false }, { 0.125f, 0.15f, 0.0f, true },
        { 0.250f, 1.00f, -0.8f, false }, { 0.375f, 0.15f, 0.0f, true },
        { 0.500f, 1.00f, -0.8f, false }, { 0.625f, 0.15f, 0.0f, true },
        { 0.750f, 1.00f, -0.8f, false }, { 0.875f, 0.15f, 0.0f, true },
        { 1.000f, 1.00f, 0.0f, false },
    };

    /*  THE RIBBIT. Not a wobble: a croak is a fast drop into the throat and a
        slower climb back out, with a flat hold at the bottom. The asymmetry is
        the whole sound - reversed it reads as a gulp rather than a croak, and
        made symmetrical it is just a triangle. */
    const std::vector<CurvePoint> kRibbit {
        { 0.000f, 0.92f, -0.9f, false }, { 0.120f, 0.08f, 0.0f, true },
        { 0.320f, 0.08f, 0.7f, false },  { 0.620f, 0.55f, 0.3f, false },
        { 0.780f, 0.20f, 0.0f, true },   { 0.880f, 0.70f, -0.4f, false },
        { 1.000f, 0.92f, 0.0f, false },
    };

    /** Two croaks a cycle, the second smaller: the double-ribbit. */
    const std::vector<CurvePoint> kRibbitDouble {
        { 0.000f, 0.90f, -0.9f, false }, { 0.100f, 0.06f, 0.0f, true },
        { 0.250f, 0.06f, 0.8f, false },  { 0.450f, 0.75f, -0.9f, false },
        { 0.550f, 0.12f, 0.0f, true },   { 0.700f, 0.12f, 0.6f, false },
        { 1.000f, 0.90f, 0.0f, false },
    };

    const std::vector<CurvePoint>* const kGrowlCurves[] {
        &kStepSix, &kStepFour, &kGallop, &kChew,
    };

    const std::vector<CurvePoint>* const kFrogCurves[] {
        &kRibbit, &kRibbitDouble,
    };

    // --- text --------------------------------------------------------------
    /*  A deque, because `Definition` holds `const char*` and the addresses
        have to outlive the call that built them. A deque never moves an
        element that is already in it, which a vector does on every growth. */
    struct TextPool
    {
        std::deque<std::string> owned;

        const char* add (std::string text)
        {
            owned.push_back (std::move (text));
            return owned.back().c_str();
        }
    };

    constexpr const char* kGrowlFirst[] {
        "Tar", "Iron", "Choke", "Rust", "Molar", "Gutter", "Static", "Brine",
        "Cinder", "Gravel", "Hollow", "Rotten", "Copper", "Grist", "Bramble",
    };
    constexpr const char* kGrowlSecond[] {
        "Growl", "Jaw", "Chatter", "Maw", "Snarl", "Grind", "Bite", "Throat",
    };

    constexpr const char* kFrogFirst[] {
        "Bog", "Marsh", "Pond", "Swamp", "Lily", "Reed", "Mire", "Croak",
        "Toad", "Fen", "Gully", "Slick",
    };
    constexpr const char* kFrogSecond[] {
        "Croak", "Ribbit", "Gulp", "Burp", "Warble", "Hop", "Chorus",
    };

    constexpr const char* kWobbleFirst[] {
        "Slow", "Heavy", "Deep", "Long", "Wide", "Soft", "Blunt", "Dark",
        "Warm", "Thick", "Loose", "Round",
    };
    constexpr const char* kWobbleSecond[] {
        "Wobble", "Rocker", "Sway", "Pulse", "Roll", "Lean", "Drift",
    };

    constexpr const char* kSubFirst[] {
        "Floor", "Cellar", "Bedrock", "Under", "Trench", "Anchor", "Mantle",
        "Basement", "Deep", "Weight", "Ballast", "Keel",
    };
    constexpr const char* kSubSecond[] {
        "Sub", "Sine", "Weight", "Bed", "Root", "Fundament",
    };

    constexpr const char* kReeseFirst[] {
        "Wide", "Split", "Beating", "Detuned", "Twin", "Parallel", "Drifting",
    };
    constexpr const char* kReeseSecond[] {
        "Reese", "Pair", "Spread", "Beat", "Foundation",
    };

    constexpr const char* kScreechFirst[] {
        "Glass", "Razor", "Wire", "Shard", "Needle", "Chrome", "Splinter",
        "Halogen", "Scalpel", "Filament",
    };
    constexpr const char* kScreechSecond[] {
        "Screech", "Lead", "Cry", "Siren", "Shriek", "Edge",
    };

    constexpr const char* kPluckFirst[] {
        "Bone", "Tin", "Dry", "Short", "Clipped", "Struck", "Muted", "Cold",
    };
    constexpr const char* kPluckSecond[] {
        "Pluck", "Tap", "Stab", "Knock", "Tick",
    };

    constexpr const char* kTextureFirst[] {
        "Ash", "Fog", "Vapour", "Dust", "Drone", "Ether", "Smoke",
    };
    constexpr const char* kTextureSecond[] {
        "Bed", "Wash", "Field", "Haze", "Drift",
    };

    // --- shared skeletons ---------------------------------------------------
    /** Every patch that is meant to be loud ends the same way. */
    void addGlue (std::vector<Setting>& s, Rng& rng, float ottDepth, bool limiter = true)
    {
        s.push_back ({ pid::ott.enabled, 1.0f });
        s.push_back ({ pid::ott.depth, ottDepth });
        s.push_back ({ pid::ott.mix, rng.range (0.65f, 1.0f) });

        if (limiter)
            s.push_back ({ pid::fxLimiter.enabled, 1.0f });
    }

    /*  THE SUB IS THE PATCH, on the evidence. It carries 50-81% of the energy
        in every reference track measured, so a bass patch here ships with it
        on and loud rather than treating it as a garnish under the growl. */
    void addSub (std::vector<Setting>& s, Rng& rng, float lo, float hi)
    {
        s.push_back ({ pid::sub.enabled, 1.0f });
        s.push_back ({ pid::sub.level, rng.range (lo, hi) });
        s.push_back ({ pid::sub.octave, rng.chance (0.25f) ? -2.0f : -1.0f });
        s.push_back ({ pid::sub.waveform, rng.chance (0.7f) ? kSubSine : kSubTriangle });
        s.push_back ({ pid::sub.sendDirect, 1.0f });
    }

    /** One LFO set up to run, with whatever shape and division it was given. */
    void addLfo (std::vector<Setting>& s, int lfo, float shape, float division)
    {
        s.push_back ({ pid::lfo[static_cast<std::size_t> (lfo)].shape, shape });
        s.push_back ({ pid::lfo[static_cast<std::size_t> (lfo)].syncEnabled, 1.0f });
        s.push_back ({ pid::lfo[static_cast<std::size_t> (lfo)].rateDivision, division });
    }

    /** One mod slot wired to a source at a depth. The DESTINATION is a routing
        and not a setting - see `FactoryBank::Routing`. */
    void addSlot (std::vector<Setting>& s, int slot, float source, float depth)
    {
        s.push_back ({ pid::modSlot[static_cast<std::size_t> (slot)].enabled, 1.0f });
        s.push_back ({ pid::modSlot[static_cast<std::size_t> (slot)].source, source });
        s.push_back ({ pid::modSlot[static_cast<std::size_t> (slot)].depth, depth });
    }

    /*  ARTICULATION SITS AT 1/8 AND 1/8 TRIPLET, measured: the 220-1200 Hz
        band of the reference tracks is modulated at 4.7-8.2 Hz, and at the
        ~144 BPM they run that is an eighth (4.8 Hz) and an eighth triplet
        (7.2 Hz). The faster divisions are here as the minority they are in
        the material rather than as an equal choice. */
    float growlDivision (Rng& rng)
    {
        const auto roll = rng.unit();

        if (roll < 0.34f) return kEighthT;
        if (roll < 0.62f) return kEighth;
        if (roll < 0.80f) return kSixteenth;
        if (roll < 0.92f) return kSixteenthT;
        return kEighthD;
    }
} // namespace

std::vector<Definition> generateVariations()
{
    static TextPool text;
    static std::vector<Definition> cached;

    if (! cached.empty())
        return cached;

    Rng rng { 0x9E3779B9u };   // any fixed seed; this one is the golden ratio

    const auto name = [&] (const auto& first, const auto& second, int n) {
        const auto a = first[rng.next() % std::size (first)];
        const auto b = second[rng.next() % std::size (second)];
        return text.add (std::string (a) + " " + b + " " + std::to_string (n));
    };

    // --- growls -------------------------------------------------------------
    /*  The formant filter under a drawn curve, which is the patch the product
        exists for, at every articulation and vowel position it supports. */
    for (int i = 0; i < 22; ++i)
    {
        std::vector<Setting> s;

        const auto table = rng.pick (kGrowlTables);
        const auto division = growlDivision (rng);
        const auto curve = kGrowlCurves[rng.index (std::size (kGrowlCurves))];

        s.push_back ({ pid::osc[0].enabled, 1.0f });
        s.push_back ({ pid::osc[0].wavetable, table });
        s.push_back ({ pid::osc[0].tablePos, rng.quantised (0.15f, 0.70f, 0.05f) });
        s.push_back ({ pid::osc[0].level, rng.range (0.80f, 0.92f) });
        s.push_back ({ pid::osc[0].unisonVoices, static_cast<float> (2 + rng.index (3)) });
        s.push_back ({ pid::osc[0].unisonDetune, rng.range (0.04f, 0.18f) });
        s.push_back ({ pid::osc[0].sendFilter1, 1.0f });

        addSub (s, rng, 0.55f, 0.82f);

        s.push_back ({ pid::filter[0].enabled, 1.0f });
        s.push_back ({ pid::filter[0].type, kFormant });
        s.push_back ({ pid::filter[0].cutoff, rng.range (700.0f, 1300.0f) });
        s.push_back ({ pid::filter[0].resonance, rng.range (0.32f, 0.62f) });
        s.push_back ({ pid::filter[0].formantX, rng.range (0.20f, 0.60f) });
        s.push_back ({ pid::filter[0].formantY, rng.range (0.40f, 0.75f) });
        s.push_back ({ pid::filter[0].formantThroat, rng.range (0.18f, 0.48f) });
        s.push_back ({ pid::filter[0].mix, 1.0f });

        addLfo (s, 0, kCustom, division);
        addSlot (s, 0, kLfo1, rng.range (0.62f, 0.92f));

        std::vector<Routing> routings { { 0, pid::filter[0].formantX } };

        // A second, slower hand on the table position: the patch then moves
        // over a bar as well as within a beat.
        if (rng.chance (0.55f))
        {
            addLfo (s, 1, kSine, rng.chance (0.5f) ? kWhole : kHalf);
            addSlot (s, 1, kLfo2, rng.range (0.18f, 0.40f));
            routings.push_back ({ 1, pid::osc[0].tablePos });
        }

        s.push_back ({ pid::fxDistortion[0].enabled, 1.0f });
        s.push_back ({ pid::fxDistortion[0].type,
                       rng.pick<float, 4> ({ kTanh, kTube, kHardClip, kFold }) });
        s.push_back ({ pid::fxDistortion[0].drive, rng.range (8.0f, 20.0f) });
        s.push_back ({ pid::fxDistortion[0].tone, rng.range (0.22f, 0.48f) });

        addGlue (s, rng, rng.range (0.35f, 0.58f));

        cached.push_back ({ name (kGrowlFirst, kGrowlSecond, i + 1),
                            "Growl",
                            text.add ("Formant under a drawn curve. The growl is the "
                                      "formant moving, not the filter sweeping."),
                            "growl,formant,bass,riddim",
                            std::move (s),
                            std::move (routings),
                            { { 0, *curve } } });
    }

    // --- froggy -------------------------------------------------------------
    /*  A CROAK IS NOT A GROWL. Three things separate them, and all three are
        needed or it is just a growl with a silly name:

        the FORMANT SITS LOW AND THE THROAT IS OPEN, so the resonances are
        vowel-like and close together rather than spread across the spectrum;

        a COMB FILTER in the second slot supplies the short fixed resonance a
        croak rings at - a throat is a tube, and a tube is a comb;

        and the CURVE IS ASYMMETRIC - a fast drop into the throat, a flat hold
        at the bottom, a slower climb back out. Reversed it reads as a gulp;
        made symmetrical it is a triangle wobble. */
    for (int i = 0; i < 16; ++i)
    {
        std::vector<Setting> s;

        const auto curve = kFrogCurves[rng.index (std::size (kFrogCurves))];

        s.push_back ({ pid::osc[0].enabled, 1.0f });
        s.push_back ({ pid::osc[0].wavetable, rng.pick (kVocalTables) });
        s.push_back ({ pid::osc[0].tablePos, rng.quantised (0.10f, 0.45f, 0.05f) });
        s.push_back ({ pid::osc[0].level, rng.range (0.78f, 0.90f) });
        s.push_back ({ pid::osc[0].unisonVoices, static_cast<float> (1 + rng.index (3)) });
        s.push_back ({ pid::osc[0].unisonDetune, rng.range (0.02f, 0.10f) });
        s.push_back ({ pid::osc[0].sendFilter1, 1.0f });
        s.push_back ({ pid::osc[0].sendFilter2, 1.0f });

        addSub (s, rng, 0.60f, 0.85f);

        // Low vowel, open throat.
        s.push_back ({ pid::filter[0].enabled, 1.0f });
        s.push_back ({ pid::filter[0].type, kFormant });
        s.push_back ({ pid::filter[0].cutoff, rng.range (520.0f, 880.0f) });
        s.push_back ({ pid::filter[0].resonance, rng.range (0.48f, 0.74f) });
        s.push_back ({ pid::filter[0].formantX, rng.range (0.08f, 0.32f) });
        s.push_back ({ pid::filter[0].formantY, rng.range (0.12f, 0.38f) });
        s.push_back ({ pid::filter[0].formantThroat, rng.range (0.55f, 0.88f) });
        s.push_back ({ pid::filter[0].mix, 1.0f });

        // The tube the croak rings in.
        s.push_back ({ pid::filter[1].enabled, 1.0f });
        s.push_back ({ pid::filter[1].type, kComb });
        s.push_back ({ pid::filter[1].cutoff, rng.range (110.0f, 320.0f) });
        s.push_back ({ pid::filter[1].combFeedback, rng.range (0.42f, 0.72f) });
        s.push_back ({ pid::filter[1].combDamping, rng.range (0.30f, 0.62f) });
        s.push_back ({ pid::filter[1].mix, rng.range (0.30f, 0.55f) });

        addLfo (s, 0, kCustom, rng.chance (0.6f) ? kEighthT : kEighth);
        addSlot (s, 0, kLfo1, rng.range (0.70f, 0.95f));

        // Both ends of the throat move together: the vowel AND the tube. One
        // without the other reads as a filter sweep over a static croak.
        std::vector<Routing> routings {
            { 0, pid::filter[0].formantY },
            { 1, pid::filter[1].cutoff },
        };
        addSlot (s, 1, kLfo1, rng.range (0.30f, 0.55f));

        s.push_back ({ pid::fxDistortion[0].enabled, 1.0f });
        s.push_back ({ pid::fxDistortion[0].type, rng.chance (0.3f) ? kBitcrush : kTube });
        s.push_back ({ pid::fxDistortion[0].drive, rng.range (6.0f, 15.0f) });
        s.push_back ({ pid::fxDistortion[0].tone, rng.range (0.15f, 0.35f) });

        addGlue (s, rng, rng.range (0.40f, 0.62f));

        cached.push_back ({ name (kFrogFirst, kFrogSecond, i + 1),
                            "Growl",
                            text.add ("A croak: low vowel, open throat, a comb for the "
                                      "tube, and a curve that drops fast and climbs slow."),
                            "froggy,croak,growl,formant,comb,riddim",
                            std::move (s),
                            std::move (routings),
                            { { 0, *curve } } });
    }

    // --- wobbles ------------------------------------------------------------
    /*  The other half of the vocabulary: the formant held still and the
        CUTOFF moving. Slower and wider - the thing that sits under a drop
        rather than being the drop. */
    for (int i = 0; i < 18; ++i)
    {
        std::vector<Setting> s;

        const auto slide = rng.chance (0.55f);

        s.push_back ({ pid::osc[0].enabled, 1.0f });
        s.push_back ({ pid::osc[0].wavetable, rng.pick (kBassTables) });
        s.push_back ({ pid::osc[0].tablePos, rng.quantised (0.10f, 0.55f, 0.05f) });
        s.push_back ({ pid::osc[0].level, rng.range (0.78f, 0.90f) });
        s.push_back ({ pid::osc[0].unisonVoices, static_cast<float> (1 + rng.index (3)) });
        s.push_back ({ pid::osc[0].unisonDetune, rng.range (0.03f, 0.14f) });
        s.push_back ({ pid::osc[0].sendFilter1, 1.0f });

        addSub (s, rng, 0.62f, 0.88f);

        s.push_back ({ pid::filter[0].enabled, 1.0f });
        s.push_back ({ pid::filter[0].type, rng.chance (0.3f) ? kLadderLp : kLp24 });
        s.push_back ({ pid::filter[0].cutoff, rng.range (300.0f, 780.0f) });
        s.push_back ({ pid::filter[0].resonance, rng.range (0.35f, 0.68f) });
        s.push_back ({ pid::filter[0].drive, rng.range (0.20f, 0.50f) });
        s.push_back ({ pid::filter[0].mix, 1.0f });

        addLfo (s, 0,
                slide ? kSine : kCustom,
                rng.pick<float, 4> ({ kEighth, kEighthT, kSixteenth, kQuarter }));
        addSlot (s, 0, kLfo1, rng.range (0.50f, 0.82f));

        addGlue (s, rng, rng.range (0.26f, 0.46f));

        std::vector<Curve> curves;
        if (! slide) curves.push_back ({ 0, kSlide });

        cached.push_back ({ name (kWobbleFirst, kWobbleSecond, i + 1),
                            "Bass",
                            text.add ("A low-pass wobbling with the formant held still. "
                                      "Sits under a drop rather than being it."),
                            "wobble,bass,riddim",
                            std::move (s),
                            { { 0, pid::filter[0].cutoff } },
                            std::move (curves) });
    }

    // --- subs ---------------------------------------------------------------
    /*  A CATEGORY BECAUSE OF THE MEASUREMENT. The sub band is 50-81% of the
        energy in the reference tracks, which makes "the sub" a patch somebody
        loads on its own rather than a control inside a growl. These are built
        to be exactly that: almost no upper content, no drive to speak of, and
        the oscillator present only to give the sub an edge to be heard on a
        phone speaker. */
    for (int i = 0; i < 18; ++i)
    {
        std::vector<Setting> s;

        s.push_back ({ pid::osc[0].enabled, 1.0f });
        s.push_back ({ pid::osc[0].wavetable, rng.pick (kBassTables) });
        s.push_back ({ pid::osc[0].tablePos, rng.quantised (0.05f, 0.30f, 0.05f) });
        s.push_back ({ pid::osc[0].level, rng.range (0.22f, 0.46f) });
        s.push_back ({ pid::osc[0].unisonVoices, 1.0f });
        s.push_back ({ pid::osc[0].sendFilter1, 1.0f });

        s.push_back ({ pid::sub.enabled, 1.0f });
        s.push_back ({ pid::sub.level, rng.range (0.86f, 1.0f) });
        s.push_back ({ pid::sub.octave, rng.chance (0.35f) ? -2.0f : -1.0f });
        s.push_back ({ pid::sub.waveform,
                       rng.chance (0.75f) ? kSubSine
                                          : (rng.chance (0.5f) ? kSubTriangle : kSubSquare) });
        s.push_back ({ pid::sub.sendDirect, 1.0f });

        s.push_back ({ pid::filter[0].enabled, 1.0f });
        s.push_back ({ pid::filter[0].type, kLp24 });
        s.push_back ({ pid::filter[0].cutoff, rng.range (140.0f, 380.0f) });
        s.push_back ({ pid::filter[0].resonance, rng.range (0.08f, 0.28f) });
        s.push_back ({ pid::filter[0].mix, 1.0f });

        std::vector<Routing> routings;
        std::vector<Curve> curves;

        /*  A third of them breathe. A sub that never moves is the right
            answer most of the time, so the movement is the minority here
            rather than the default it is everywhere else in the bank. */
        if (rng.chance (0.34f))
        {
            addLfo (s, 0, rng.chance (0.5f) ? kSine : kTriangle,
                    rng.chance (0.5f) ? kWhole : kHalf);
            addSlot (s, 0, kLfo1, rng.range (0.10f, 0.26f));
            routings.push_back ({ 0, pid::filter[0].cutoff });
        }

        if (rng.chance (0.4f))
        {
            s.push_back ({ pid::fxDistortion[0].enabled, 1.0f });
            s.push_back ({ pid::fxDistortion[0].type, kTube });
            s.push_back ({ pid::fxDistortion[0].drive, rng.range (3.0f, 8.0f) });
            s.push_back ({ pid::fxDistortion[0].tone, rng.range (0.10f, 0.25f) });
        }

        addGlue (s, rng, rng.range (0.18f, 0.34f));

        cached.push_back ({ name (kSubFirst, kSubSecond, i + 1),
                            "Sub",
                            text.add ("Sub weight with just enough oscillator on top to "
                                      "survive a phone speaker."),
                            "sub,bass,low,foundation",
                            std::move (s),
                            std::move (routings),
                            std::move (curves) });
    }

    // --- reeses -------------------------------------------------------------
    for (int i = 0; i < 12; ++i)
    {
        std::vector<Setting> s;

        const auto table = rng.pick (kBassTables);
        const auto detune = rng.range (8.0f, 24.0f);

        for (int o = 0; o < 2; ++o)
        {
            const auto n = static_cast<std::size_t> (o);
            s.push_back ({ pid::osc[n].enabled, 1.0f });
            s.push_back ({ pid::osc[n].wavetable, table });
            s.push_back ({ pid::osc[n].tablePos, rng.quantised (0.15f, 0.60f, 0.05f) });
            s.push_back ({ pid::osc[n].pitchFine, o == 0 ? -detune : detune });
            s.push_back ({ pid::osc[n].level, rng.range (0.72f, 0.86f) });
            s.push_back ({ pid::osc[n].sendFilter1, 1.0f });
        }

        addSub (s, rng, 0.40f, 0.62f);

        s.push_back ({ pid::filter[0].enabled, 1.0f });
        s.push_back ({ pid::filter[0].type, rng.chance (0.3f) ? kLp12 : kLp24 });
        s.push_back ({ pid::filter[0].cutoff, rng.range (800.0f, 2000.0f) });
        s.push_back ({ pid::filter[0].resonance, rng.range (0.12f, 0.38f) });
        s.push_back ({ pid::filter[0].mix, 1.0f });

        std::vector<Routing> routings;

        if (rng.chance (0.5f))
        {
            addLfo (s, 0, kSine, rng.chance (0.5f) ? kHalf : kWhole);
            addSlot (s, 0, kLfo1, rng.range (0.20f, 0.42f));
            routings.push_back ({ 0, pid::filter[0].cutoff });
        }

        if (rng.chance (0.45f))
        {
            s.push_back ({ pid::fxDimension.enabled, 1.0f });
            s.push_back ({ pid::fxDimension.amount, rng.range (0.25f, 0.55f) });
            s.push_back ({ pid::fxDimension.width, rng.range (0.4f, 0.8f) });
        }

        addGlue (s, rng, rng.range (0.22f, 0.40f));

        cached.push_back ({ name (kReeseFirst, kReeseSecond, i + 1),
                            "Bass",
                            text.add ("Two oscillators detuned against each other through "
                                      "one filter, so the beating is in the source."),
                            "reese,bass,detune,foundation",
                            std::move (s),
                            std::move (routings) });
    }

    // --- screeches ----------------------------------------------------------
    for (int i = 0; i < 12; ++i)
    {
        std::vector<Setting> s;

        s.push_back ({ pid::osc[0].enabled, 1.0f });
        s.push_back ({ pid::osc[0].wavetable, rng.pick (kBrightTables) });
        s.push_back ({ pid::osc[0].tablePos, rng.quantised (0.50f, 0.95f, 0.05f) });
        s.push_back ({ pid::osc[0].level, rng.range (0.72f, 0.88f) });
        s.push_back ({ pid::osc[0].unisonVoices, static_cast<float> (3 + rng.index (4)) });
        s.push_back ({ pid::osc[0].unisonDetune, rng.range (0.08f, 0.26f) });
        s.push_back ({ pid::osc[0].unisonSpread, rng.range (0.4f, 0.9f) });
        s.push_back ({ pid::osc[0].sendFilter1, 1.0f });

        /*  A BAND-PASS, not a low-pass. At this register a low-pass removes
            the thing that makes it a screech. */
        s.push_back ({ pid::filter[0].enabled, 1.0f });
        s.push_back ({ pid::filter[0].type, rng.chance (0.4f) ? kBp24 : kBp12 });
        s.push_back ({ pid::filter[0].cutoff, rng.range (1100.0f, 3400.0f) });
        s.push_back ({ pid::filter[0].resonance, rng.range (0.28f, 0.58f) });
        s.push_back ({ pid::filter[0].mix, 1.0f });

        addLfo (s, 0, rng.chance (0.4f) ? kRandomStep : kCustom, growlDivision (rng));
        addSlot (s, 0, kLfo1, rng.range (0.40f, 0.75f));

        s.push_back ({ pid::fxHyper.enabled, 1.0f });
        s.push_back ({ pid::fxHyper.amount, rng.range (0.35f, 0.75f) });
        s.push_back ({ pid::fxHyper.voices, static_cast<float> (3 + rng.index (4)) });
        s.push_back ({ pid::fxHyper.width, rng.range (0.5f, 1.0f) });

        if (rng.chance (0.6f))
        {
            s.push_back ({ pid::fxDelay.enabled, 1.0f });
            s.push_back ({ pid::fxDelay.syncEnabled, 1.0f });
            s.push_back ({ pid::fxDelay.division, rng.chance (0.5f) ? kEighthD : kEighthT });
            s.push_back ({ pid::fxDelay.feedback, rng.range (0.25f, 0.48f) });
            s.push_back ({ pid::fxDelay.mix, rng.range (0.18f, 0.36f) });
        }

        s.push_back ({ pid::fxDistortion[0].enabled, 1.0f });
        s.push_back ({ pid::fxDistortion[0].type, rng.chance (0.5f) ? kHardClip : kTanh });
        s.push_back ({ pid::fxDistortion[0].drive, rng.range (6.0f, 16.0f) });
        s.push_back ({ pid::fxDistortion[0].tone, rng.range (0.45f, 0.75f) });

        addGlue (s, rng, rng.range (0.30f, 0.52f));

        cached.push_back ({ name (kScreechFirst, kScreechSecond, i + 1),
                            "Lead",
                            text.add ("Top of the table through a band-pass, widened by "
                                      "the hyper and answered by a synced delay."),
                            "screech,lead,hyper,bright",
                            std::move (s),
                            { { 0, pid::osc[0].tablePos } },
                            { { 0, kStepFour } } });
    }

    // --- hyper leads --------------------------------------------------------
    for (int i = 0; i < 8; ++i)
    {
        std::vector<Setting> s;

        s.push_back ({ pid::osc[0].enabled, 1.0f });
        s.push_back ({ pid::osc[0].wavetable, rng.pick (kBrightTables) });
        s.push_back ({ pid::osc[0].tablePos, rng.quantised (0.35f, 0.85f, 0.05f) });
        s.push_back ({ pid::osc[0].level, rng.range (0.70f, 0.85f) });
        s.push_back ({ pid::osc[0].unisonVoices, static_cast<float> (5 + rng.index (4)) });
        s.push_back ({ pid::osc[0].unisonDetune, rng.range (0.14f, 0.34f) });
        s.push_back ({ pid::osc[0].unisonBlend, rng.range (0.4f, 0.8f) });
        s.push_back ({ pid::osc[0].unisonSpread, rng.range (0.6f, 1.0f) });
        s.push_back ({ pid::osc[0].sendFilter1, 1.0f });

        s.push_back ({ pid::filter[0].enabled, 1.0f });
        s.push_back ({ pid::filter[0].type, kLp24 });
        s.push_back ({ pid::filter[0].cutoff, rng.range (1800.0f, 5000.0f) });
        s.push_back ({ pid::filter[0].resonance, rng.range (0.15f, 0.40f) });
        s.push_back ({ pid::filter[0].mix, 1.0f });

        s.push_back ({ pid::fxHyper.enabled, 1.0f });
        s.push_back ({ pid::fxHyper.amount, rng.range (0.55f, 0.95f) });
        s.push_back ({ pid::fxHyper.voices, static_cast<float> (4 + rng.index (5)) });
        s.push_back ({ pid::fxHyper.detune, rng.range (0.2f, 0.6f) });
        s.push_back ({ pid::fxHyper.width, rng.range (0.7f, 1.0f) });

        s.push_back ({ pid::fxDimension.enabled, 1.0f });
        s.push_back ({ pid::fxDimension.amount, rng.range (0.3f, 0.7f) });

        if (rng.chance (0.5f))
        {
            s.push_back ({ pid::fxReverb.enabled, 1.0f });
            s.push_back ({ pid::fxReverb.mix, rng.range (0.12f, 0.28f) });
            s.push_back ({ pid::fxReverb.size, rng.range (0.4f, 0.8f) });
        }

        addLfo (s, 0, kTriangle, rng.chance (0.5f) ? kWhole : kHalf);
        addSlot (s, 0, kLfo1, rng.range (0.15f, 0.35f));

        /*  A DRIVE STAGE, because a patch with none has no gain staging. This
            archetype and the plucks were the only two here without one, and
            they were the only two the audibility test failed - at -31 and
            -33 dBFS against a -30 bar. Same lesson as the graintable pack,
            where four legitimate costs stacked into a preset 40 dB under the
            bank: nothing was a bug, and it was still inaudible. */
        s.push_back ({ pid::fxDistortion[0].enabled, 1.0f });
        s.push_back ({ pid::fxDistortion[0].type, kTanh });
        s.push_back ({ pid::fxDistortion[0].drive, rng.range (5.0f, 12.0f) });
        s.push_back ({ pid::fxDistortion[0].tone, rng.range (0.45f, 0.70f) });

        addGlue (s, rng, rng.range (0.34f, 0.50f));

        cached.push_back ({ name (kScreechFirst, kScreechSecond, 100 + i),
                            "Lead",
                            text.add ("Wide unison through the hyper and the dimension "
                                      "widener - the supersaw end of the instrument."),
                            "lead,hyper,wide,supersaw",
                            std::move (s),
                            { { 0, pid::filter[0].cutoff } } });
    }

    // --- plucks -------------------------------------------------------------
    for (int i = 0; i < 10; ++i)
    {
        std::vector<Setting> s;

        s.push_back ({ pid::osc[0].enabled, 1.0f });
        s.push_back ({ pid::osc[0].wavetable, rng.pick (kBrightTables) });
        s.push_back ({ pid::osc[0].tablePos, rng.quantised (0.25f, 0.80f, 0.05f) });
        s.push_back ({ pid::osc[0].level, rng.range (0.88f, 0.98f) });
        s.push_back ({ pid::osc[0].unisonVoices, static_cast<float> (1 + rng.index (3)) });
        s.push_back ({ pid::osc[0].unisonDetune, rng.range (0.02f, 0.12f) });
        s.push_back ({ pid::osc[0].sendFilter1, 1.0f });

        /*  Short and plucked, in SECONDS. Envelope 1 is the amplifier, so
            this is what makes it a pluck rather than a stab held under a
            gate. */
        s.push_back ({ pid::envelope[0].attack, 0.001f });
        s.push_back ({ pid::envelope[0].decay, rng.range (0.08f, 0.34f) });
        s.push_back ({ pid::envelope[0].sustain, rng.range (0.0f, 0.12f) });
        s.push_back ({ pid::envelope[0].release, rng.range (0.06f, 0.24f) });

        s.push_back ({ pid::filter[0].enabled, 1.0f });
        s.push_back ({ pid::filter[0].type, kLp24 });
        s.push_back ({ pid::filter[0].cutoff, rng.range (1300.0f, 3400.0f) });
        s.push_back ({ pid::filter[0].resonance, rng.range (0.20f, 0.52f) });
        s.push_back ({ pid::filter[0].mix, 1.0f });

        /*  The envelope on the cutoff is the pluck's bite, and it must be
            UNIPOLAR. A mod slot is bipolar by default, which is right for an
            LFO - it should swing either side of the value the knob is set to -
            and wrong for an envelope: at note-on the envelope reads 0, so a
            bipolar slot puts the cutoff a full depth BELOW its base, and the
            base is where the note has to get out.

            The cutoff is smoothed, so it cannot climb back from there inside
            the millisecond the amp envelope takes to peak. The transient - the
            entire audible part of a pluck - passes through an almost closed
            filter. The whole archetype measured around -30 dBFS and three of
            them failed the audibility test outright, while the hand-written
            "Metal Pluck", identical in envelope and audition note but with no
            cutoff modulation at all, passed comfortably. */
        s.push_back ({ pid::envelope[1].attack, 0.001f });
        s.push_back ({ pid::envelope[1].decay, rng.range (0.04f, 0.20f) });
        s.push_back ({ pid::envelope[1].sustain, 0.0f });
        addSlot (s, 0, kEnv2, rng.range (0.30f, 0.60f));
        s.push_back ({ pid::modSlot[0].bipolar, 0.0f });

        if (rng.chance (0.55f))
        {
            s.push_back ({ pid::fxDelay.enabled, 1.0f });
            s.push_back ({ pid::fxDelay.syncEnabled, 1.0f });
            s.push_back ({ pid::fxDelay.division, kEighthD });
            s.push_back ({ pid::fxDelay.feedback, rng.range (0.20f, 0.45f) });
            s.push_back ({ pid::fxDelay.mix, rng.range (0.15f, 0.32f) });
        }

        // The pluck's own drive stage - see the hyper leads above for why.
        s.push_back ({ pid::fxDistortion[0].enabled, 1.0f });
        s.push_back ({ pid::fxDistortion[0].type, rng.chance (0.4f) ? kTube : kTanh });
        s.push_back ({ pid::fxDistortion[0].drive, rng.range (4.0f, 10.0f) });
        s.push_back ({ pid::fxDistortion[0].tone, rng.range (0.40f, 0.65f) });

        addGlue (s, rng, rng.range (0.30f, 0.46f));

        cached.push_back ({ name (kPluckFirst, kPluckSecond, i + 1),
                            "Pluck",
                            text.add ("Short, bright and gone: a decay envelope on the "
                                      "amplifier and another on the cutoff."),
                            "pluck,short,bright",
                            std::move (s),
                            { { 0, pid::filter[0].cutoff } } });
    }

    // --- textures -----------------------------------------------------------
    for (int i = 0; i < 8; ++i)
    {
        std::vector<Setting> s;

        s.push_back ({ pid::osc[0].enabled, 1.0f });
        s.push_back ({ pid::osc[0].wavetable, rng.pick (kGrowlTables) });
        s.push_back ({ pid::osc[0].tablePos, rng.quantised (0.10f, 0.90f, 0.05f) });
        s.push_back ({ pid::osc[0].level, rng.range (0.72f, 0.88f) });
        s.push_back ({ pid::osc[0].unisonVoices, static_cast<float> (3 + rng.index (4)) });
        s.push_back ({ pid::osc[0].unisonDetune, rng.range (0.10f, 0.30f) });
        s.push_back ({ pid::osc[0].sendFilter1, 1.0f });

        s.push_back ({ pid::envelope[0].attack, rng.range (0.15f, 0.9f) });
        s.push_back ({ pid::envelope[0].sustain, rng.range (0.6f, 1.0f) });
        s.push_back ({ pid::envelope[0].release, rng.range (0.4f, 1.6f) });

        s.push_back ({ pid::filter[0].enabled, 1.0f });
        s.push_back ({ pid::filter[0].type, kLp24 });
        s.push_back ({ pid::filter[0].cutoff, rng.range (700.0f, 2600.0f) });
        s.push_back ({ pid::filter[0].resonance, rng.range (0.10f, 0.34f) });
        s.push_back ({ pid::filter[0].mix, 1.0f });

        addLfo (s, 0, kSine, rng.chance (0.5f) ? kWhole : kHalf);
        addSlot (s, 0, kLfo1, rng.range (0.20f, 0.45f));

        s.push_back ({ pid::fxReverb.enabled, 1.0f });
        s.push_back ({ pid::fxReverb.mix, rng.range (0.25f, 0.50f) });
        s.push_back ({ pid::fxReverb.size, rng.range (0.5f, 0.95f) });
        s.push_back ({ pid::fxReverb.decay, rng.range (0.5f, 0.9f) });

        s.push_back ({ pid::fxDimension.enabled, 1.0f });
        s.push_back ({ pid::fxDimension.amount, rng.range (0.3f, 0.7f) });

        addGlue (s, rng, rng.range (0.18f, 0.34f));

        cached.push_back ({ name (kTextureFirst, kTextureSecond, i + 1),
                            "Pad",
                            text.add ("A slow bed: long attack, wide reverb, and the "
                                      "table drifting under it."),
                            "pad,texture,ambient,slow",
                            std::move (s),
                            { { 0, pid::osc[0].tablePos } } });
    }

    return cached;
}

} // namespace gnarl::preset
