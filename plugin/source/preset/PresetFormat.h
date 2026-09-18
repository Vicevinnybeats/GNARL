#pragma once

#include "../params/ParameterIDs.h"

#include <juce_data_structures/juce_data_structures.h>

namespace gnarl::preset
{

/**
    The `.gnarl` preset format.

    A PRESET IS THE PLUGIN'S STATE PLUS METADATA, not a separate format. The
    same ValueTree that getStateInformation writes into a session is what a
    preset file holds, wrapped in an element that adds a name, an author and a
    category. That is deliberate: two serialisation paths drift, and the one
    that drifts is the one a customer notices - a patch that recalls correctly
    from the session and wrongly from the browser, or the reverse. One path
    means a preset and a session cannot disagree.

    THE METADATA IS A SIBLING OF THE STATE, NOT INSIDE IT. The browser lists a
    bank by reading every file's metadata, and a bank is hundreds of files; a
    format that required parsing 430 parameters to learn a preset's name would
    make opening the browser cost the whole bank. Keeping the metadata in its
    own element at the top lets the lister stop reading after a few hundred
    bytes.

    TWO VERSION NUMBERS, and they are not the same thing:

      - `formatVersion` is this FILE's shape - the element names, where the
        metadata sits. It changes when the container changes.
      - `stateVersion` is `pid::kStateVersion`, the meaning of the PARAMETERS
        inside. It changes when a parameter's meaning changes, and it is what
        migration keys off.

    A preset written by a newer build loads on a best-effort basis rather than
    being refused: APVTS ignores IDs it does not know and leaves absent ones at
    their defaults, so a patch from a version with more parameters comes back
    as close to right as it can be. Refusing would be safer for us and worse
    for the person who just bought the thing.
*/

/** What a preset says about itself. */
struct Metadata
{
    juce::String name { "Untitled" };
    juce::String author;
    /** One of `categories` below, by name. Free text is accepted on load -
        a bank from elsewhere must not fail to list because of a category we
        have not heard of. */
    juce::String category { "Bass" };
    juce::String description;
    /** Free-form, comma-separated on disk. */
    juce::StringArray tags;
    /** ISO-8601, UTC. Written on save, preserved on load. */
    juce::String created;
    /** The plugin version that wrote it, for support rather than for logic. */
    juce::String pluginVersion;
};

/**
    The category list.

    MATCHES SERUM'S INFORMATION ARCHITECTURE, which is a category convention
    and free to use - never its artwork, colours or typography (CLAUDE.md
    section 9). A producer who knows one synth should be able to find a bass
    patch in this one without learning a new vocabulary.

    Unlike a parameter choice list this is NOT frozen: the category is stored
    as a STRING in the file, so adding, renaming or reordering an entry cannot
    change the meaning of an existing preset. That is the whole reason it is a
    string and not an index.
*/
inline const juce::StringArray categories {
    "Bass", "Growl", "Lead", "Pluck", "Pad", "Keys",
    "Drums", "FX", "Sequence", "Texture"
};

/** The container element, and the version of its shape. */
inline const juce::Identifier kPresetType { "GNARL_PRESET" };
inline const juce::Identifier kMetaType { "META" };

/** Bumped only when the FILE's shape changes - not when a parameter's meaning
    does, which is what pid::kStateVersion is for. */
inline constexpr int kFormatVersion = 1;

inline const juce::Identifier kFormatVersionProperty { "formatVersion" };
inline const juce::Identifier kStateVersionProperty { "stateVersion" };

inline const juce::Identifier kNameProperty { "name" };
inline const juce::Identifier kAuthorProperty { "author" };
inline const juce::Identifier kCategoryProperty { "category" };
inline const juce::Identifier kDescriptionProperty { "description" };
inline const juce::Identifier kTagsProperty { "tags" };
inline const juce::Identifier kCreatedProperty { "created" };
inline const juce::Identifier kPluginVersionProperty { "pluginVersion" };

/** The file extension, without the dot. */
inline const juce::String kFileExtension { "gnarl" };
inline const juce::String kFileWildcard { "*.gnarl" };

/** Builds a preset tree from a plugin state tree and some metadata.

    `state` is copied, so the caller can keep using theirs. */
juce::ValueTree build (const Metadata& metadata, const juce::ValueTree& state);

/** The metadata out of a preset tree. Missing fields take their defaults, so
    a hand-written file with nothing but a name still lists. */
Metadata readMetadata (const juce::ValueTree& preset);

/** The plugin state out of a preset tree, or an invalid tree if there is
    none. The caller passes this to APVTS::replaceState. */
juce::ValueTree readState (const juce::ValueTree& preset);

/** True for a tree that is actually a preset. Checked before anything is
    read, because a file with the right extension is not a promise. */
bool isPreset (const juce::ValueTree& tree);

/** The state version a preset was written against, or 1 when it does not say
    - the version before the property existed. */
int readStateVersion (const juce::ValueTree& preset);

/** Serialises to the text that goes in a file. */
juce::String toText (const juce::ValueTree& preset);

/** Parses file text. Returns an invalid tree for anything that is not a
    preset, including empty input and XML that parses but is something else. */
juce::ValueTree fromText (const juce::String& text);

/** An ISO-8601 UTC timestamp, for `Metadata::created`. */
juce::String timestampNow();

/** Strips the characters a filename cannot hold, so a preset called
    "Bass / Growl #1" saves rather than failing or silently landing in a
    directory nobody asked for. */
juce::String sanitiseFilename (const juce::String& name);

} // namespace gnarl::preset
