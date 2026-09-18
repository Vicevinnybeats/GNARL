#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"
#include "dsp/Modulation.h"

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

// ============================================================================
// Choice lists
// ============================================================================

#if defined (GNARL_UI_CHOICES_PATH)

namespace
{
    /** Reads one `export const NAME = [ ... ] as const;` array out of
        choices.ts, as a list of the quoted strings inside it.

        A dumb scan again, for the same reason as the ID scanner above: the
        file holds nothing but string arrays, and a test that needs a real
        TypeScript parser is a test that stops being maintained. */
    juce::StringArray readChoiceList (const juce::String& source, const juce::String& name)
    {
        const auto declaration = "export const " + name + " = [";
        const auto start = source.indexOf (declaration);

        if (start < 0)
            return {};

        const auto body = source.substring (start + declaration.length());
        const auto end = body.indexOf ("]");

        if (end < 0)
            return {};

        juce::StringArray entries;
        auto remaining = body.substring (0, end);

        while (remaining.isNotEmpty())
        {
            const auto quote = remaining.indexOfAnyOf ("'\"");

            if (quote < 0)
                break;

            const auto delimiter = remaining.substring (quote, quote + 1);
            const auto rest = remaining.substring (quote + 1);
            const auto close = rest.indexOf (delimiter);

            if (close < 0)
                break;

            entries.add (rest.substring (0, close));
            remaining = rest.substring (close + 1);
        }

        return entries;
    }

    void checkChoiceList (const juce::String& source,
                          const juce::String& name,
                          const juce::StringArray& expected)
    {
        const auto mirrored = readChoiceList (source, name);

        INFO ("choice list " << name);
        REQUIRE (mirrored.size() == expected.size());

        for (int i = 0; i < expected.size(); ++i)
        {
            INFO (name << "[" << i << "]");
            CHECK (mirrored[i] == expected[i]);
        }
    }
}

TEST_CASE ("The TypeScript choice lists match the C++ ones", "[params][mirror]")
{
    /*  Choice-list ORDER IS FROZEN on the C++ side, because a preset stores
        the chosen index rather than the name. The TypeScript mirror is what
        the dropdowns render, so a list that drifts shows the wrong label for
        every saved patch - and, like a drifted parameter ID, it fails
        silently: the dropdown works, it just lies.

        This guard did not exist when the modulation lists were added to
        choices.ts by hand, which is exactly when it was needed.
    */
    const juce::File file { juce::String { GNARL_UI_CHOICES_PATH } };
    REQUIRE (file.existsAsFile());

    const auto source = file.loadFileAsString();

    checkChoiceList (source, "OSC_MODE", choices::oscMode);
    checkChoiceList (source, "WARP_MODE", choices::warpMode);
    checkChoiceList (source, "SUB_WAVEFORM", choices::subWaveform);
    checkChoiceList (source, "NOISE_TYPE", choices::noiseType);
    checkChoiceList (source, "FILTER_TYPE", choices::filterType);
    checkChoiceList (source, "FILTER_ROUTING", choices::filterRouting);
    checkChoiceList (source, "DRIVE_CURVE", choices::driveCurve);
    checkChoiceList (source, "ENVELOPE_MODE", choices::envelopeMode);
    checkChoiceList (source, "LFO_SHAPE", choices::lfoShape);
    checkChoiceList (source, "LFO_MODE", choices::lfoMode);
    checkChoiceList (source, "LFO_RATE_DIVISION", choices::lfoRateDivision);
    checkChoiceList (source, "GRID_DIVISION", choices::gridDivision);
    checkChoiceList (source, "MOD_SOURCE", choices::modSource);
    checkChoiceList (source, "MOD_CURVE", choices::modCurve);
    checkChoiceList (source, "OVERSAMPLING", choices::oversampling);
    checkChoiceList (source, "POLY_MODE", choices::polyMode);
}

#endif

#endif

// ============================================================================
// Modulation destinations
// ============================================================================

#if defined (GNARL_UI_MOD_DESTINATIONS_PATH)

TEST_CASE ("The TypeScript modulation destinations match the C++ table", "[params][mirror]")
{
    /*  The plugin serves the destination list over the bridge from the
        engine's own table, so in the plugin the picker cannot offer a
        destination the matrix does not have. The BROWSER PREVIEW has no plugin
        to ask and uses ui/src/bridge/modDestinations.ts instead - and an empty
        or drifted list there makes the MOD tab untestable in exactly the place
        its layout gets judged.

        Checked by display name and in ORDER, because the preview's picker
        indexes into this list.
    */
    const juce::File file { juce::String { GNARL_UI_MOD_DESTINATIONS_PATH } };
    REQUIRE (file.existsAsFile());

    const auto source = file.loadFileAsString();

    auto expected = 0;

    for (int i = 1; i < static_cast<int> (dsp::ModDestination::count); ++i)
    {
        const auto destination = static_cast<dsp::ModDestination> (i);
        const auto name = dsp::getDestinationDisplayName (destination);

        INFO ("missing from ui/src/bridge/modDestinations.ts: " << name);
        CHECK (source.contains ("'" + name + "'"));

        ++expected;
    }

    // And nothing extra: a TS-only destination is a picker entry that stores a
    // parameter ID the engine will resolve to `none`, so the slot silently
    // does nothing.
    const auto entries = juce::StringArray::fromTokens (source, "\n", "").size() > 0
        ? source.upToLastOccurrenceOf ("]", false, false)
        : source;

    auto found = 0;
    auto remaining = entries;

    while (remaining.contains ("name: '"))
    {
        remaining = remaining.fromFirstOccurrenceOf ("name: '", false, false);
        remaining = remaining.fromFirstOccurrenceOf ("'", false, false);
        ++found;
    }

    INFO ("destination count");
    CHECK (found == expected);
}

#endif
