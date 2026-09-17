#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"

#include <set>
#include <string>

using namespace gnarl;

/*
    Guards the C++ / TypeScript parameter-ID mirror.

    ui/src/bridge/parameterIds.ts has to carry the same IDs as the C++ layout.
    When it drifts, nothing fails loudly: JUCE's relay simply never connects,
    so the control renders, moves, and changes nothing. That is an expensive
    bug to find by hand, and a cheap one to find here.

    GNARL_UI_PARAMETER_IDS_PATH is passed in by tests/CMakeLists.txt.
*/

namespace
{
#if defined (GNARL_UI_PARAMETER_IDS_PATH)
    juce::File getMirrorFile()
    {
        return juce::File { juce::String { GNARL_UI_PARAMETER_IDS_PATH } };
    }

    /** Extracts the leading quoted string from a line, or an empty string. */
    juce::String quotedValue (const juce::String& text)
    {
        if (! (text.startsWith ("'") || text.startsWith ("\"")))
            return {};

        const auto quote = text.substring (0, 1);
        return text.substring (1).upToFirstOccurrenceOf (quote, false, false);
    }

    /** Collects every parameter ID literal in the mirror file.

        Deliberately a dumb scan rather than a TypeScript parser: the file is
        generated and holds only object literals and string arrays, and a test
        that needs a real parser is a test nobody keeps working.

        Two shapes appear, and BOTH must be handled - missing the second is how
        this scanner first gave a false failure on the macro list:
            keyed:  `tablePos: 'osc1_table_pos',`
            bare:   `'macro1',`   (inside a string array) */
    std::set<std::string> readMirroredIDs (const juce::String& source)
    {
        std::set<std::string> ids;

        const auto add = [&ids] (const juce::String& value)
        {
            // Skip the type-only lines and any string that is plainly not an
            // ID, so a stray literal in a comment cannot register as one.
            if (value.isNotEmpty() && ! value.containsChar (' '))
                ids.insert (value.toStdString());
        };

        for (const auto& rawLine : juce::StringArray::fromLines (source))
        {
            const auto line = rawLine.trim();

            if (line.startsWith ("//") || line.startsWith ("*") || line.startsWith ("/*"))
                continue;

            if (line.contains (":"))
                add (quotedValue (line.fromFirstOccurrenceOf (":", false, false).trim()));

            // Bare array entry.
            add (quotedValue (line));
        }

        return ids;
    }
#endif
}

#if defined (GNARL_UI_PARAMETER_IDS_PATH)

TEST_CASE ("The TypeScript parameter mirror is readable", "[params][mirror]")
{
    const auto file = getMirrorFile();

    INFO ("expected at: " << file.getFullPathName());
    REQUIRE (file.existsAsFile());
    CHECK (file.loadFileAsString().isNotEmpty());
}

TEST_CASE ("Every C++ parameter is mirrored in TypeScript", "[params][mirror]")
{
    const auto mirrored = readMirroredIDs (getMirrorFile().loadFileAsString());
    REQUIRE_FALSE (mirrored.empty());

    GnarlProcessor processor;

    for (auto* param : processor.getParameters())
    {
        auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (param);
        REQUIRE (withID != nullptr);

        const auto id = withID->paramID.toStdString();

        INFO ("missing from ui/src/bridge/parameterIds.ts: " << id);
        CHECK (mirrored.count (id) == 1);
    }
}

TEST_CASE ("Every mirrored TypeScript ID exists in C++", "[params][mirror]")
{
    const auto mirrored = readMirroredIDs (getMirrorFile().loadFileAsString());

    GnarlProcessor processor;
    auto& apvts = processor.getValueTreeState();

    for (const auto& id : mirrored)
    {
        // A TS-only ID is a control bound to nothing, which is the same silent
        // failure in the other direction.
        INFO ("declared in TypeScript but not in C++: " << id);
        CHECK (apvts.getParameter (juce::String { id }) != nullptr);
    }
}

#endif
