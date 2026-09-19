#include "PresetManager.h"

#include "../PluginProcessor.h"

namespace gnarl::preset
{

PresetManager::PresetManager (GnarlProcessor& processorToUse)
    : processor (processorToUse)
{
    current.name = "Init";
    current.pluginVersion = JucePlugin_VersionString;
}

juce::ValueTree PresetManager::capture (const Metadata& metadata) const
{
    // The plugin's own state, exactly as a session would store it - including
    // the MODSTATE and FXORDER branches, which is the whole point of going
    // through the same tree rather than enumerating parameters here.
    auto state = processor.getValueTreeState().copyState();

    auto withVersion = metadata;

    if (withVersion.pluginVersion.isEmpty())
        withVersion.pluginVersion = JucePlugin_VersionString;

    return build (withVersion, state);
}

bool PresetManager::apply (const juce::ValueTree& presetTree)
{
    if (! isPreset (presetTree))
        return false;

    const auto state = readState (presetTree);

    // A preset with no state is a metadata file, and applying it would
    // silently reset the patch to defaults. Refusing leaves what is loaded
    // alone, which is what someone who clicked a broken file wants.
    if (! state.isValid())
        return false;

    // Everything below happens inside the processor, which already knows how
    // to republish the mod state and the FX order and how to schedule the
    // wavetables. Duplicating that here is how the two paths drift.
    processor.applyPresetState (state);

    current = readMetadata (presetTree);
    modified = false;

    return true;
}

juce::File PresetManager::save (const Metadata& metadata)
{
    auto directory = getUserDirectory();

    if (! directory.exists() && ! directory.createDirectory().wasOk())
        return {};

    auto file = directory.getChildFile (sanitiseFilename (metadata.name)
                                        + "." + kFileExtension);

    auto withTimestamp = metadata;
    withTimestamp.created = timestampNow();

    const auto text = toText (capture (withTimestamp));

    if (text.isEmpty() || ! file.replaceWithText (text))
        return {};

    current = withTimestamp;
    modified = false;

    return file;
}

bool PresetManager::load (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    return apply (fromText (file.loadFileAsString()));
}

bool PresetManager::loadByIndex (int index)
{
    const auto tree = getTreeByIndex (index);

    return tree.isValid() && apply (tree);
}

juce::ValueTree PresetManager::getTreeByIndex (int index) const
{
    const auto entries = list();

    if (index < 0 || index >= static_cast<int> (entries.size()))
        return {};

    const auto& entry = entries[static_cast<std::size_t> (index)];

    if (entry.isFactory)
    {
        // The factory rows come first and in the same order as the bank, which
        // list() guarantees by sorting factory-first and stably.
        for (std::size_t i = 0, seen = 0; i < factory.size(); ++i)
        {
            juce::ignoreUnused (seen);

            if (readMetadata (factory[i]).name == entry.metadata.name)
                return factory[i];
        }

        return {};
    }

    return fromText (entry.file.loadFileAsString());
}

bool PresetManager::remove (int index)
{
    const auto entries = list();

    if (index < 0 || index >= static_cast<int> (entries.size()))
        return false;

    const auto& entry = entries[static_cast<std::size_t> (index)];

    // The bank ships in the binary, so deleting a factory row would be a
    // button that appears to work and does not.
    if (entry.isFactory || ! entry.file.existsAsFile())
        return false;

    return entry.file.deleteFile();
}

std::vector<Entry> PresetManager::list() const
{
    std::vector<Entry> entries;

    for (const auto& tree : factory)
    {
        Entry entry;
        entry.metadata = readMetadata (tree);
        entry.isFactory = true;
        entries.push_back (std::move (entry));
    }

    auto directory = getUserDirectory();

    if (directory.isDirectory())
    {
        for (const auto& file : directory.findChildFiles (juce::File::findFiles, true,
                                                          kFileWildcard))
        {
            // Only the metadata is parsed here. Reading each file's 430
            // parameters to list a bank of several hundred would make opening
            // the browser cost the whole bank.
            const auto tree = fromText (file.loadFileAsString());

            if (! isPreset (tree))
                continue;

            Entry entry;
            entry.file = file;
            entry.metadata = readMetadata (tree);
            entry.isFactory = false;
            entries.push_back (std::move (entry));
        }
    }

    std::stable_sort (entries.begin(), entries.end(), [] (const Entry& a, const Entry& b)
    {
        // Factory first, then by name. compareNatural so "Growl 10" sorts
        // after "Growl 9" rather than between "Growl 1" and "Growl 2".
        if (a.isFactory != b.isFactory)
            return a.isFactory;

        return a.metadata.name.compareNatural (b.metadata.name) < 0;
    });

    return entries;
}

juce::File PresetManager::getUserDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("GNARL")
        .getChildFile ("Presets");
}

void PresetManager::setFactoryPresets (std::vector<juce::ValueTree> presets)
{
    factory = std::move (presets);
}

} // namespace gnarl::preset
