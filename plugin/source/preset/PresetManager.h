#pragma once

#include "PresetFormat.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <vector>

namespace gnarl { class GnarlProcessor; }

namespace gnarl::preset
{

/** One entry in the browser's list: enough to show a row, without the cost of
    the 430 parameters behind it. */
struct Entry
{
    juce::File file;
    Metadata metadata;
    /** True for the read-only bank that ships in the binary. */
    bool isFactory = false;
};

/**
    Saving, loading and listing presets.

    MESSAGE THREAD ONLY. Every method here touches the filesystem or the
    parameter tree, and both are message-thread things: file IO has unbounded
    latency and the audio thread cannot wait on it (CLAUDE.md section 3).

    WHY LOADING DOES NOT BLOCK ON THE WAVETABLES. Generating one table costs
    about 21 ms at the old geometry and roughly 2.5x that at the shipped one,
    so a preset that changes both oscillators' tables would freeze the UI for
    something over a tenth of a second - on every click through a browser,
    which is how people audition. So the parameters are applied immediately and
    the tables are generated on a BACKGROUND thread; until a new table is
    ready the audio thread keeps playing the previously published one, which
    means the patch arrives in two steps instead of arriving late. The
    alternative - waiting - is a browser nobody scrolls through.

    THE USER'S DIRECTORY IS NOT THE FACTORY BANK. Factory presets ship inside
    the binary and are never written to; a customer's edits go to their own
    folder, and an edit to a factory preset is a new user preset rather than a
    change to the bank. A product that lets an update overwrite somebody's
    work loses their work exactly once.
*/
class PresetManager
{
public:
    explicit PresetManager (GnarlProcessor& processorToUse);

    /** The current patch as a preset tree, ready to write. */
    juce::ValueTree capture (const Metadata& metadata) const;

    /** Applies a preset to the plugin. Returns false for a tree that is not a
        preset or carries no state - and changes nothing in that case, rather
        than half-loading. */
    bool apply (const juce::ValueTree& preset);

    /** Saves the current patch into the user directory. The filename comes
        from the name, sanitised. Returns the file written, or an invalid file
        on failure. */
    juce::File save (const Metadata& metadata);

    /** Loads a file and applies it. False if it will not parse. */
    bool load (const juce::File& file);

    /** Loads by position in `list()`, which is what the browser has: a row
        index, covering the factory bank and the user's folder uniformly. The
        UI must not have to care which side of that line a row is on. */
    bool loadByIndex (int index);

    /** The preset trees for two rows, for morphing between them. An invalid
        tree for a row that does not exist. */
    juce::ValueTree getTreeByIndex (int index) const;

    /** Deletes a USER preset. Refuses a factory row: the bank ships in the
        binary and an update would bring it back anyway, so "deleting" one
        would be a button that appears to work and does not. */
    bool remove (int index);

    /** Every preset the browser can show: the factory bank first, then the
        user's, each sorted by name. Re-read from disk on every call, because
        a producer who drops a file into the folder expects it to appear. */
    std::vector<Entry> list() const;

    /** Where a customer's own presets live. Created on first use. */
    static juce::File getUserDirectory();

    /** The metadata of whatever is loaded, for the header's preset name. */
    const Metadata& getCurrent() const noexcept { return current; }

    void setCurrent (const Metadata& metadata) { current = metadata; }

    /** True once the patch has been changed since it was loaded or saved -
        which is what puts the asterisk next to the name. */
    bool isModified() const noexcept { return modified; }
    void markModified() noexcept { modified = true; }

    /** Called when a factory bank is compiled in. Kept as a hook rather than
        a hard dependency so the tests can run without the binary data. */
    void setFactoryPresets (std::vector<juce::ValueTree> presets);

private:
    GnarlProcessor& processor;

    Metadata current;
    bool modified = false;

    std::vector<juce::ValueTree> factory;
};

} // namespace gnarl::preset
