#pragma once

#include "Wavetable.h"

#include <array>
#include <memory>

namespace gnarl::dsp
{

/**
    GNARL's factory wavetables.

    All 20 are GENERATED FROM HARMONIC FORMULAE at load time, not shipped as
    sample data. Two reasons, and the first is the one that matters:

    1. LICENSING. A wavetable ripped from another synth is that synth's
       copyrighted content. Generating ours from published additive, FM and
       formant maths means there is nothing to dispute and nothing to license.

    2. Band-limiting is exact. A generator hands over a harmonic series, so
       every mip level is built by truncating that series rather than by
       filtering samples and hoping.

    The set is chosen for this genre: heavy odd-harmonic content, vowel-like
    formant peaks, and FM-derived metallic tables. A general-purpose synth would
    ship more sine-ish tables; those are not what a growl is made of.

    Tables are generated lazily and cached, because generating all 20 up front
    would cost about 80 MB of RAM to hold tables no patch is using.
*/
class WavetableLibrary
{
public:
    static constexpr int kNumFactoryTables = 20;

    WavetableLibrary();
    ~WavetableLibrary();

    /** Generates the table if it has not been built yet, then returns it.
        MESSAGE THREAD ONLY - this allocates and runs FFTs. */
    const Wavetable& getTable (int index);

    /** Returns the table only if it is already built, else nullptr.
        Real-time safe, for the audio thread to check before use. */
    const Wavetable* getTableIfLoaded (int index) const noexcept;

    /** Builds every factory table. For tests and for the preset browser's
        audition, never from the audio thread. */
    void generateAll();

    static juce::StringArray getTableNames();
    static juce::String getTableName (int index);

private:
    static void fillTable (int index, Wavetable& table);

    std::array<std::unique_ptr<Wavetable>, kNumFactoryTables> tables;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WavetableLibrary)
};

} // namespace gnarl::dsp
