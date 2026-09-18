#include "PresetFormat.h"

#include <juce_core/juce_core.h>

namespace gnarl::preset
{

juce::ValueTree build (const Metadata& metadata, const juce::ValueTree& state)
{
    juce::ValueTree preset { kPresetType };

    preset.setProperty (kFormatVersionProperty, kFormatVersion, nullptr);
    preset.setProperty (kStateVersionProperty, pid::kStateVersion, nullptr);

    juce::ValueTree meta { kMetaType };

    meta.setProperty (kNameProperty, metadata.name, nullptr);
    meta.setProperty (kAuthorProperty, metadata.author, nullptr);
    meta.setProperty (kCategoryProperty, metadata.category, nullptr);
    meta.setProperty (kDescriptionProperty, metadata.description, nullptr);
    meta.setProperty (kTagsProperty, metadata.tags.joinIntoString (","), nullptr);
    meta.setProperty (kCreatedProperty,
                      metadata.created.isEmpty() ? timestampNow() : metadata.created,
                      nullptr);
    meta.setProperty (kPluginVersionProperty, metadata.pluginVersion, nullptr);

    // The metadata goes FIRST, so a lister can stop reading early.
    preset.appendChild (meta, nullptr);

    if (state.isValid())
        preset.appendChild (state.createCopy(), nullptr);

    return preset;
}

Metadata readMetadata (const juce::ValueTree& preset)
{
    Metadata metadata;

    const auto meta = preset.getChildWithName (kMetaType);

    if (! meta.isValid())
        return metadata;

    const auto text = [&meta] (const juce::Identifier& id, const juce::String& fallback)
    {
        const auto value = meta.getProperty (id).toString();
        return value.isEmpty() ? fallback : value;
    };

    metadata.name = text (kNameProperty, metadata.name);
    metadata.author = text (kAuthorProperty, {});
    metadata.category = text (kCategoryProperty, metadata.category);
    metadata.description = text (kDescriptionProperty, {});
    metadata.created = text (kCreatedProperty, {});
    metadata.pluginVersion = text (kPluginVersionProperty, {});

    // Empty tokens dropped and whitespace trimmed: "growl, , heavy," is a
    // thing people type, and it should give two tags rather than four.
    metadata.tags = juce::StringArray::fromTokens (
        meta.getProperty (kTagsProperty).toString(), ",", "");
    metadata.tags.trim();
    metadata.tags.removeEmptyStrings();

    return metadata;
}

juce::ValueTree readState (const juce::ValueTree& preset)
{
    // The state is whichever child is NOT the metadata. Finding it by
    // elimination rather than by name means the APVTS's own element name can
    // change without invalidating every preset ever saved - it is JUCE's to
    // choose, not ours.
    for (auto child : preset)
        if (! child.hasType (kMetaType))
            return child;

    return {};
}

bool isPreset (const juce::ValueTree& tree)
{
    return tree.isValid() && tree.hasType (kPresetType);
}

int readStateVersion (const juce::ValueTree& preset)
{
    // 1 rather than 0 when absent: a file with no version is from before the
    // property existed, which is version 1 by definition.
    return static_cast<int> (preset.getProperty (kStateVersionProperty, 1));
}

juce::String toText (const juce::ValueTree& preset)
{
    if (auto xml = preset.createXml())
        return xml->toString();

    return {};
}

juce::ValueTree fromText (const juce::String& text)
{
    if (text.isEmpty())
        return {};

    auto xml = juce::parseXML (text);

    if (xml == nullptr)
        return {};

    auto tree = juce::ValueTree::fromXml (*xml);

    // Well-formed XML that is not a preset is not a preset. Returning it
    // anyway would hand the caller something that parses and then behaves
    // like an empty patch.
    return isPreset (tree) ? tree : juce::ValueTree {};
}

juce::String timestampNow()
{
    return juce::Time::getCurrentTime().toISO8601 (true);
}

juce::String sanitiseFilename (const juce::String& name)
{
    // Everything a path separator or a reserved character on any of the three
    // platforms, replaced rather than removed so two presets whose names
    // differ only in punctuation do not collide.
    static const juce::String illegal { "\\/:*?\"<>|" };

    juce::String cleaned;

    for (auto character : name)
    {
        /*  charToString, NOT `cleaned += character`. Iterating a juce::String
            yields juce_wchar, which is an integer type - so `+=` appends the
            character's CODE as digits, and "Bass" came out as
            "66971151115". The test caught it; the compiler was perfectly
            happy, because appending an int to a String is a legitimate thing
            to want. */
        cleaned += illegal.containsChar (character)
                 ? juce::String { "_" }
                 : juce::String::charToString (character);
    }

    cleaned = cleaned.trim();

    // A name that was nothing but illegal characters still has to produce a
    // file. "Untitled" is a worse name than the user's and better than none.
    return cleaned.isEmpty() ? juce::String { "Untitled" } : cleaned;
}

} // namespace gnarl::preset
