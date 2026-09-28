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
    constexpr auto kQuarterT =
        static_cast<float> (choices::LfoRateDivision::quarterTriplet);
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

    constexpr auto kGraintable = static_cast<float> (choices::OscMode::graintable);

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
    /*  Every patch that is meant to be loud ends the same way - except that
        an OTT is not always wanted. It is a compressor, so on a patch whose
        whole character is a 1 ms transient it is a transient SQUASHER: the
        pluck archetype measured 0.014-0.314 peak with one, against 0.153 for
        the hand-written "Metal Pluck", which has no OTT at all. */
    void addGlue (std::vector<Setting>& s, Rng& rng, float ottDepth,
                  bool ott = true, bool limiter = true)
    {
        if (ott)
        {
            s.push_back ({ pid::ott.enabled, 1.0f });
            s.push_back ({ pid::ott.depth, ottDepth });
            s.push_back ({ pid::ott.mix, rng.range (0.65f, 1.0f) });
        }

        if (limiter)
            s.push_back ({ pid::fxLimiter.enabled, 1.0f });
    }

    /*  THE SUB IS THE PATCH, on the evidence. It carries 50-81% of the energy
        in every reference track measured, so a bass patch here ships with it
        on and loud rather than treating it as a garnish under the growl. */
    /*  THE SUB OCTAVE IS -1, NOT -2, AND THAT IS ARITHMETIC RATHER THAN
        TASTE.

        A Bass, Growl or Sub patch is played at MIDI 36 - 65.4 Hz, which is
        where the genre sits and what `getAuditionNote` returns. Two octaves
        below that is 16.35 Hz: under the 20 Hz bottom of the sub band, under
        the bottom of human hearing, and gone on every speaker ever built. It
        is not a quiet sub, it is no sub, and it still costs the headroom.

        A quarter of the generated presets were drawing it, and so was
        "Chainsaw Riddim" - which is why that patch measured 0.3% of its
        energy in the sub band while the reference tracks measure 58-85%.
        The patch had a sub the whole time; it was an octave and a half below
        anything that could reproduce it.

        One octave down is 32.7 Hz, which is low and audible. */
    void addSub (std::vector<Setting>& s, Rng& rng, float lo, float hi)
    {
        s.push_back ({ pid::sub.enabled, 1.0f });
        s.push_back ({ pid::sub.level, rng.range (lo, hi) });
        s.push_back ({ pid::sub.octave, -1.0f });
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

    /*  ARTICULATION SITS AT 1/4 AND 1/8, AND THIS IS A CORRECTION.

        The first measurement said 1/8 and 1/8 triplet, 4.7-8.2 Hz, and the
        table was weighted there: 34% eighth-triplet, 18% sixteenth, 12%
        sixteenth-triplet - so nearly two thirds of the growls wobbled at
        7.2 Hz or faster.

        Twelve reference tracks measured at the DROP (from 0:35, where the
        client pointed) put the 220-1200 Hz band's modulation at 2.35 Hz or
        4.65 Hz in ten of them. At the 140-150 BPM these run, those are a
        QUARTER (2.4 Hz) and an EIGHTH (4.8 Hz). Exactly one track reaches
        eighth-triplet, and none is faster.

        So the bank was running at roughly double the genre's articulation,
        which is the single measurable reason a correct-sounding patch does
        not sound like the reference: everything else can be right and a
        growl at twice the rate is a different sound. It reads as busy and
        thin where the material is slow and heavy.

        The two clusters are even, because the measurement is even: five
        tracks each. The faster divisions stay, as the minority they now
        actually are rather than as the majority they were. */
    float growlDivision (Rng& rng)
    {
        const auto roll = rng.unit();

        if (roll < 0.34f) return kQuarter;        // 2.4 Hz  - 5 of 12 tracks
        if (roll < 0.68f) return kEighth;         // 4.8 Hz  - 5 of 12
        if (roll < 0.80f) return kQuarterT;       // 3.6 Hz  - the 3.50 Hz one
        if (roll < 0.91f) return kEighthT;        // 7.2 Hz  - the 7.10 Hz one
        return kSixteenth;                        // 9.6 Hz  - none, kept sparse
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

        /*  UNISON 3-7 AT AROUND A TENTH OF DETUNE, which is where the
            technique writing on this genre consistently lands - six or seven
            voices at ~0.1 - rather than the two or three a formant patch
            needs on its own. The detune is what stops a square-ish growl
            reading as a single tone being chopped. */
        s.push_back ({ pid::osc[0].enabled, 1.0f });
        s.push_back ({ pid::osc[0].wavetable, table });
        s.push_back ({ pid::osc[0].tablePos, rng.quantised (0.15f, 0.70f, 0.05f) });
        s.push_back ({ pid::osc[0].level, rng.range (0.80f, 0.92f) });
        s.push_back ({ pid::osc[0].unisonVoices, static_cast<float> (3 + rng.index (5)) });
        s.push_back ({ pid::osc[0].unisonDetune, rng.range (0.06f, 0.16f) });
        s.push_back ({ pid::osc[0].unisonBlend, rng.range (0.3f, 0.7f) });
        s.push_back ({ pid::osc[0].sendFilter1, 1.0f });

        /*  A GRAINWAVE VARIANT, for a quarter of them. The instrument most
            associated with this sound in Reason is a grainwave synth -
            granular and wavetable at once - and GNARL's graintable mode is
            our own equivalent. Grains are windowed, so this costs about 5 dB
            against wavetable mode and the level is lifted to match. */
        const auto grain = rng.chance (0.25f);

        if (grain)
        {
            s.push_back ({ pid::osc[0].mode, kGraintable });
            s.push_back ({ pid::osc[0].grainSize, rng.range (28.0f, 90.0f) });
            s.push_back ({ pid::osc[0].grainDensity, rng.range (8.0f, 22.0f) });
            s.push_back ({ pid::osc[0].grainPosJitter, rng.range (0.05f, 0.30f) });
            s.push_back ({ pid::osc[0].level, 0.95f });
        }

        addSub (s, rng, 0.55f, 0.82f);

        s.push_back ({ pid::filter[0].enabled, 1.0f });
        s.push_back ({ pid::filter[0].type, kFormant });
        s.push_back ({ pid::filter[0].cutoff, rng.range (700.0f, 1300.0f) });
        s.push_back ({ pid::filter[0].resonance, rng.range (0.32f, 0.62f) });
        s.push_back ({ pid::filter[0].formantX, rng.range (0.20f, 0.60f) });
        s.push_back ({ pid::filter[0].formantY, rng.range (0.40f, 0.75f) });
        s.push_back ({ pid::filter[0].formantThroat, rng.range (0.18f, 0.48f) });
        s.push_back ({ pid::filter[0].mix, 1.0f });

        /*  A SQUARE LFO IS A FIRST-CLASS SHAPE HERE, not only the drawn
            curve. The stuttered, blocky articulation this genre is built on
            is what a square gives for free, and a drawn curve is the thing
            you reach for when you want a specific rhythm rather than an even
            one. Roughly a third square, the rest drawn. */
        addLfo (s, 0, rng.chance (0.34f) ? kSquare : kCustom, division);
        addSlot (s, 0, kLfo1, rng.range (0.62f, 0.92f));

        std::vector<Routing> routings { { 0, pid::filter[0].formantX } };

        /*  THE SAME FAST LFO ALSO MOVES THE TABLE POSITION. Every account of
            how this sound is built modulates the wavetable position and the
            filter TOGETHER and at the same speed - moving the formant alone
            is a filter on a static tone, and it is the table moving under it
            that gives the growl its watery, notched quality. The depth is
            lower than the formant's so it colours rather than takes over. */
        addSlot (s, 1, kLfo1, rng.range (0.22f, 0.50f));
        routings.push_back ({ 1, pid::osc[0].tablePos });

        // And a slower second hand, so the patch moves over a bar as well as
        // within a beat.
        if (rng.chance (0.5f))
        {
            addLfo (s, 1, kSine, rng.chance (0.5f) ? kWhole : kHalf);
            addSlot (s, 2, kLfo2, rng.range (0.16f, 0.34f));
            routings.push_back ({ 2, pid::filter[0].cutoff });
        }

        /*  CHORUS OR FLANGER ON ABOUT HALF. Both are named repeatedly as
            characteristic of the genre rather than as garnish, and on a
            mono-ish growl they are what makes the sound occupy a stereo
            field the sub cannot. */
        if (rng.chance (0.5f))
        {
            if (rng.chance (0.5f))
            {
                s.push_back ({ pid::fxChorus.enabled, 1.0f });
                s.push_back ({ pid::fxChorus.rate, rng.range (0.3f, 1.4f) });
                s.push_back ({ pid::fxChorus.depth, rng.range (0.25f, 0.6f) });
                s.push_back ({ pid::fxChorus.mix, rng.range (0.18f, 0.38f) });
            }
            else
            {
                s.push_back ({ pid::fxFlanger.enabled, 1.0f });
                s.push_back ({ pid::fxFlanger.rate, rng.range (0.15f, 0.9f) });
                s.push_back ({ pid::fxFlanger.depth, rng.range (0.3f, 0.7f) });
                s.push_back ({ pid::fxFlanger.feedback, rng.range (0.2f, 0.5f) });
                s.push_back ({ pid::fxFlanger.mix, rng.range (0.16f, 0.34f) });
            }
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
        //  -1 only: see addSub above. At a 65.4 Hz root, -2 is 16 Hz.
        s.push_back ({ pid::sub.octave, -1.0f });
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
        /*  0.82-0.95, raised from 0.72-0.88. A screech has no sub under it
            and gets its level from the oscillator alone, so it has the least
            margin of any archetype here - one variant measured -31.7 dBFS
            against the bank's -30 floor. The floor is not a mix decision;
            it is the line under which a patch is not usable at all. */
        s.push_back ({ pid::osc[0].level, rng.range (0.82f, 0.95f) });
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
        // Dry path kept for the comb-null reason given under the hyper leads.
        s.push_back ({ pid::fxHyper.mix, rng.range (0.5f, 0.75f) });
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

        //  More OTT than before for the same reason: it lifts the quiet
        //  part of a bright, transient sound, which is where a screech's
        //  perceived level lives.
        addGlue (s, rng, rng.range (0.44f, 0.66f));

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
        /*  Louder, and the wet effects pulled back. This archetype failed
            the audibility test at -32 to -35 dBFS with its modulation both
            present and removed, so the modulation was never its problem: it
            is the only patch here with no sub AND two wet stages diluting the
            dry signal. */
        s.push_back ({ pid::osc[0].level, rng.range (0.88f, 0.96f) });
        s.push_back ({ pid::osc[0].unisonVoices, static_cast<float> (5 + rng.index (4)) });
        /*  NARROWER THAN IT WAS. Detune spreads a fixed amount of energy over
            more partials, and the hyper's own detune spreads it again - at
            the wide end of both ranges at once the patch measured 34 dB under
            the bank while every other draw from the same archetype was fine.
            An axis whose extreme is only reachable in combination with
            another's is an axis that needs narrowing, not a threshold that
            needs lowering. */
        s.push_back ({ pid::osc[0].unisonDetune, rng.range (0.10f, 0.20f) });
        s.push_back ({ pid::osc[0].unisonBlend, rng.range (0.5f, 0.8f) });
        s.push_back ({ pid::osc[0].unisonSpread, rng.range (0.6f, 1.0f) });
        s.push_back ({ pid::osc[0].sendFilter1, 1.0f });

        s.push_back ({ pid::filter[0].enabled, 1.0f });
        s.push_back ({ pid::filter[0].type, kLp24 });
        s.push_back ({ pid::filter[0].cutoff, rng.range (2400.0f, 5200.0f) });
        s.push_back ({ pid::filter[0].resonance, rng.range (0.15f, 0.40f) });
        s.push_back ({ pid::filter[0].mix, 1.0f });

        s.push_back ({ pid::fxHyper.enabled, 1.0f });
        /*  A DRY PATH THROUGH THE HYPER, and the reason is already written
            down one floor up: the hyper is a multi-tap effect, and a single
            sustained tone through one measures a comb null rather than a
            level - "at 220 Hz one cycle is 4.5 ms, so the taps land two
            thirds of a cycle apart and partly CANCEL". The audibility test
            plays exactly that, one note held for three seconds, and one draw
            from this archetype sat in a null at C4 at -34 dBFS while every
            other draw was fine. Neither the detune range nor the modulation
            moved it, because neither was the cause.

            Leaving half the signal dry means a null can colour the patch but
            cannot take it, which is also what somebody would do by ear. */
        s.push_back ({ pid::fxHyper.mix, rng.range (0.45f, 0.68f) });
        s.push_back ({ pid::fxHyper.amount, rng.range (0.45f, 0.75f) });
        s.push_back ({ pid::fxHyper.voices, static_cast<float> (3 + rng.index (4)) });
        s.push_back ({ pid::fxHyper.detune, rng.range (0.15f, 0.35f) });
        s.push_back ({ pid::fxHyper.width, rng.range (0.7f, 1.0f) });

        s.push_back ({ pid::fxDimension.enabled, 1.0f });
        s.push_back ({ pid::fxDimension.amount, rng.range (0.2f, 0.45f) });

        /*  Also off the cutoff, for the reason spelled out under the plucks:
            this is the other archetype here with no sub, and it failed the
            same way at -32 and -35 dBFS. */
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
        s.push_back ({ pid::fxDistortion[0].drive, rng.range (10.0f, 18.0f) });
        s.push_back ({ pid::fxDistortion[0].tone, rng.range (0.45f, 0.70f) });

        /*  MORE OTT, AND A MASTER TRIM, because this archetype has the
            least level of any here and one variant measured -31.7 dBFS
            against the bank's -30 floor.

            It is not the oscillator - that already runs at 0.88-0.96. It is
            what happens after: a band-pass takes the low end off, and the
            hyper spreads the signal over several detuned taps whose partial
            cancellation is the effect working as intended (CLAUDE.md's
            root-n note). Raising the source would only clip the peaks
            before the losses.

            OTT lifts the quiet part of a bright sustained sound, which is
            where a lead's perceived level lives, and the master trim covers
            the rest without touching the balance inside the patch. */
        addGlue (s, rng, rng.range (0.56f, 0.72f));
        s.push_back ({ pid::masterGain, rng.range (2.0f, 3.5f) });   // dB

        cached.push_back ({ name (kScreechFirst, kScreechSecond, 100 + i),
                            "Lead",
                            text.add ("Wide unison through the hyper and the dimension "
                                      "widener - the supersaw end of the instrument."),
                            "lead,hyper,wide,supersaw",
                            std::move (s),
                            { { 0, pid::osc[0].tablePos } } });
    }

    // --- plucks -------------------------------------------------------------
    for (int i = 0; i < 10; ++i)
    {
        std::vector<Setting> s;

        s.push_back ({ pid::osc[0].enabled, 1.0f });
        s.push_back ({ pid::osc[0].wavetable, rng.pick (kBrightTables) });
        s.push_back ({ pid::osc[0].tablePos, rng.quantised (0.25f, 0.80f, 0.05f) });
        s.push_back ({ pid::osc[0].level, 0.96f });
        s.push_back ({ pid::osc[0].unisonVoices, static_cast<float> (2 + rng.index (3)) });
        s.push_back ({ pid::osc[0].unisonDetune, rng.range (0.02f, 0.10f) });
        s.push_back ({ pid::osc[0].sendFilter1, 1.0f });

        /*  A LITTLE SUB UNDER IT. This is the thinnest patch in the bank -
            one oscillator, no sub, and an envelope that is over in a tenth of
            a second - and it kept producing a quiet tail whichever way the
            draws fell: different plucks failed the audibility test on every
            reseed, at -33 to -44 dBFS. Body is what it was missing, and a
            pluck with a touch of sub under it is what anybody would build
            anyway. */
        addSub (s, rng, 0.38f, 0.58f);

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
        /*  NO MOD SLOT AT ALL, which is what the measurement says rather
            than what the design wanted.

            This archetype measured -30 to -44 dBFS with an envelope routed to
            the filter cutoff. Making the slot unipolar did not fix it; moving
            the destination to the table position did not fix it either - it
            came back at -44.7. Removing the slot entirely fixed it outright,
            and every pluck passed. So it is the modulation itself on a patch
            this thin, not the destination or the polarity, and I have not
            isolated the mechanism beyond that.

            The hand-written "Metal Pluck" has no modulation either and sits
            at 0.153 peak, so this is the shape that is known to work here. A
            pluck's character is its amplitude envelope; the timbral snap can
            come back when somebody has worked out what the slot is actually
            doing to the level. */

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

        /*  THE OTT STAYS ON, against the guess that it was squashing the
            transient. Taking it off made this archetype QUIETER, not louder -
            one pluck went from -33 to -44 dBFS - because its upward
            compression was lifting the decay, which is most of what a short
            patch has. Measured, not reasoned. */
        addGlue (s, rng, rng.range (0.28f, 0.44f));

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

        /*  A TEXTURE HAS NO DRIVE STAGE, so it has no gain staging either -
            the same reason the graintable pack arrived 40 dB under the
            riddim bank, and the same reason the pluck archetype needed a
            sub. Nothing here is a bug: a reverb at 25-50% wet spreads the
            energy in time, the band-pass takes the extremes off, and the
            legitimate costs stack until "Drone Haze 1" measures -35 dBFS.

            More OTT and a master trim, for the reason they work on the
            hyper lead above: OTT lifts the quiet part of a sustained sound,
            and the trim covers the rest without changing the balance
            inside the patch. A texture is quiet BY DESIGN relative to a
            growl; the floor is about being usable, not about being loud. */
        addGlue (s, rng, rng.range (0.34f, 0.52f));
        /*  5.5-7.5, not 3.5-5.5. The first range fixed the worst variant
            and left "Ether Field 1" at -30.33 dBFS - a third of a decibel
            under, because the trim is drawn per patch and that one drew
            the bottom of the range. A floor has to hold for the WHOLE
            range, not for its median, which is the same reason the drive
            stage's loose threshold hid a +15 dB bug. */
        s.push_back ({ pid::masterGain, rng.range (5.5f, 7.5f) });   // dB

        cached.push_back ({ name (kTextureFirst, kTextureSecond, i + 1),
                            "Pad",
                            text.add ("A slow bed: long attack, wide reverb, and the "
                                      "table drifting under it."),
                            "pad,texture,ambient,slow",
                            std::move (s),
                            { { 0, pid::osc[0].tablePos } } });
    }

    // --- rack growls --------------------------------------------------------
    /*  THE REASON RACK, REBUILT FROM ITS SIGNAL FLOW.

        The client sent a Combinator patch - "OGOG_square4_sweep_r13" - and
        said the growls should sound like it. Its device chain reads straight
        out of the file:

            Malstrom (both oscillators on one swept square graintable)
              -> Audiomatic (a retro/lofi transformer)
              -> BV-511 Vocoder
              -> Scream 4 distortion
              -> PH-90 phaser
              -> ECF-42 envelope-controlled filter
              -> RV-7 reverb

        WHAT IS COPIED IS THE ORDER, WHICH IS NOT THE SOUND. A signal chain is
        information architecture - the same thing CLAUDE.md section 9 permits
        of Serum's category names - and nothing here is taken from
        Propellerhead: no graintable, no sample, no spectrum. The oscillators
        run GNARL's own "Pulse Width" table, generated by this plugin, because
        a swept pulse width IS a sweeping square and we have one.

        THE VOCODER IS THE PART THAT MATTERS. Every other device in that rack
        has an obvious counterpart, and the temptation is to read the vocoder
        as one more effect. It is not: a vocoder imposes moving formants, and
        moving formants are what makes a growl read as a voice rather than as
        a filter sweep. So the formant filter carries the modulation here and
        the second slot is the envelope-controlled filter, in that order -
        which is also the order the rack has them.

        BOTH OSCILLATORS ON THE SAME TABLE, detuned, as the Malstrom is. Two
        different tables would be a richer patch and a different one; the
        thickness in that rack comes from two copies of one shape beating
        against each other, not from two shapes. */
    for (int i = 0; i < 10; ++i)
    {
        std::vector<Setting> s;

        const auto division = growlDivision (rng);
        const auto curve = kGrowlCurves[rng.index (std::size (kGrowlCurves))];

        //  "Pulse Width": the square family, swept. Table 9.
        constexpr float kPulseWidthTable = 9.0f;

        for (int osc = 0; osc < 2; ++osc)
        {
            const auto o = static_cast<std::size_t> (osc);

            s.push_back ({ pid::osc[o].enabled, 1.0f });
            s.push_back ({ pid::osc[o].mode, kGraintable });
            s.push_back ({ pid::osc[o].wavetable, kPulseWidthTable });
            s.push_back ({ pid::osc[o].tablePos, rng.quantised (0.15f, 0.55f, 0.05f) });
            s.push_back ({ pid::osc[o].level, rng.range (0.62f, 0.76f) });

            /*  Grains long enough to keep a square's edge. Grain size and
                density move the level by 25 dB across their ranges, with
                long grains at high density far quieter - the graintable
                pack's lesson, and the reason these sit mid-range rather
                than at an extreme. */
            s.push_back ({ pid::osc[o].grainSize, rng.range (0.34f, 0.58f) });
            s.push_back ({ pid::osc[o].grainDensity, rng.range (0.40f, 0.62f) });

            s.push_back ({ pid::osc[o].unisonVoices, static_cast<float> (2 + rng.index (3)) });
            s.push_back ({ pid::osc[o].unisonDetune, rng.range (0.06f, 0.16f) });
            s.push_back ({ pid::osc[o].sendFilter1, 1.0f });
            s.push_back ({ pid::osc[o].sendFilter2, 1.0f });
        }

        //  The second oscillator a few cents off the first: this is where the
        //  rack's thickness comes from.
        s.push_back ({ pid::osc[1].pitchFine, rng.range (-9.0f, 9.0f) });

        addSub (s, rng, 0.62f, 0.86f);

        //  --- the vocoder's job -------------------------------------------
        s.push_back ({ pid::filter[0].enabled, 1.0f });
        s.push_back ({ pid::filter[0].type, kFormant });
        s.push_back ({ pid::filter[0].formantX, rng.range (0.25f, 0.70f) });
        s.push_back ({ pid::filter[0].formantY, rng.range (0.30f, 0.65f) });
        s.push_back ({ pid::filter[0].formantThroat, rng.range (0.35f, 0.70f) });
        s.push_back ({ pid::filter[0].mix, 1.0f });

        //  --- the ECF-42 ---------------------------------------------------
        s.push_back ({ pid::filter[1].enabled, 1.0f });
        s.push_back ({ pid::filter[1].type, rng.chance (0.5f) ? kBp24 : kBp12 });
        s.push_back ({ pid::filter[1].cutoff, rng.range (0.30f, 0.58f) });
        s.push_back ({ pid::filter[1].resonance, rng.range (0.25f, 0.55f) });
        s.push_back ({ pid::filter[1].mix, rng.range (0.55f, 0.85f) });

        //  --- Audiomatic, then Scream 4 -------------------------------------
        s.push_back ({ pid::fxDistortion[0].enabled, 1.0f });
        s.push_back ({ pid::fxDistortion[0].type, rng.chance (0.45f) ? kBitcrush : kTube });
        s.push_back ({ pid::fxDistortion[0].drive, rng.range (0.38f, 0.68f) });
        s.push_back ({ pid::fxDistortion[0].tone, rng.range (0.30f, 0.60f) });
        s.push_back ({ pid::fxDistortion[0].mix, rng.range (0.55f, 0.85f) });

        //  --- PH-90 ---------------------------------------------------------
        s.push_back ({ pid::fxPhaser.enabled, 1.0f });
        s.push_back ({ pid::fxPhaser.mix, rng.range (0.30f, 0.55f) });
        s.push_back ({ pid::fxPhaser.rate, rng.range (0.10f, 0.32f) });
        s.push_back ({ pid::fxPhaser.depth, rng.range (0.45f, 0.80f) });
        //  An N-stage all-pass chain gives N/2 notches, so the stage count is
        //  how MANY notches there are, not how deep they are.
        s.push_back ({ pid::fxPhaser.stages, static_cast<float> (4 + 2 * rng.index (3)) });
        s.push_back ({ pid::fxPhaser.feedback, rng.range (0.25f, 0.55f) });

        //  --- RV-7, kept short: a growl drowns in a long tail ---------------
        s.push_back ({ pid::fxReverb.enabled, 1.0f });
        s.push_back ({ pid::fxReverb.mix, rng.range (0.08f, 0.20f) });
        s.push_back ({ pid::fxReverb.size, rng.range (0.25f, 0.50f) });
        s.push_back ({ pid::fxReverb.decay, rng.range (0.20f, 0.45f) });

        //  --- the modulation, which is the whole patch ----------------------
        addLfo (s, 0, kCustom, division);
        addSlot (s, 0, kLfo1, rng.range (0.55f, 0.85f));
        addSlot (s, 1, kLfo1, rng.range (0.20f, 0.45f));

        std::vector<Routing> routings {
            { 0, pid::filter[0].formantX },   // the vocoder moving
            { 1, pid::osc[0].tablePos },      // the square sweeping
        };

        addGlue (s, rng, rng.range (0.52f, 0.72f));

        cached.push_back ({ name (kGrowlFirst, kGrowlSecond, 200 + i),
                            "Growl",
                            text.add ("The Reason rack's signal flow: two graintable "
                                      "squares, formants moving like a vocoder, then "
                                      "crush, phase and a short room."),
                            "growl,rack,graintable,square,formant,phaser,riddim",
                            std::move (s),
                            std::move (routings),
                            { { 0, *curve } } });
    }

    return cached;
}

} // namespace gnarl::preset
