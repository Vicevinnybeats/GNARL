#pragma once

#include "Wavetable.h"

#include <array>
#include <atomic>
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

        NEVER FROM THE AUDIO THREAD - this allocates and runs FFTs, and it
        takes a lock. Safe to call from the message thread and from the
        preset TableLoader's background thread at the same time, which is why
        the lock is here: a plain check-then-act raced between those two and
        segfaulted while rendering the factory bank, intermittently, and not
        at all under a debugger. */
    const Wavetable& getTable (int index);

    /** Returns the table only if it is already built, else nullptr.

        REAL-TIME SAFE and lock-free, which is why the published view is a
        separate array of atomics rather than a read of the owning pointers:
        the audio thread may call this while a background thread is storing a
        newly generated table, and reading a std::unique_ptr that another
        thread is assigning is undefined behaviour however harmless it looks. */
    const Wavetable* getTableIfLoaded (int index) const noexcept;

    /** Builds every factory table. For tests and for the preset browser's
        audition, never from the audio thread. */
    void generateAll();

    static juce::StringArray getTableNames();
    static juce::String getTableName (int index);

private:
    static void fillTable (int index, Wavetable& table);

    /** Ownership. Only ever touched under `generationLock`. */
    std::array<std::unique_ptr<Wavetable>, kNumFactoryTables> tables;

    /** The lock-free view the audio thread reads. Published with release
        ordering after the table is fully built, so a reader that sees the
        pointer also sees the table behind it. */
    std::array<std::atomic<const Wavetable*>, kNumFactoryTables> published {};

    /** Guards generation between the message thread and the preset loader's
        background thread. The audio thread never reaches this. */
    juce::CriticalSection generationLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WavetableLibrary)
};

} // namespace gnarl::dsp
