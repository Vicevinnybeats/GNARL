#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"
#include "preset/PresetFormat.h"
#include "preset/PresetManager.h"

#include <cmath>
#include <vector>

using namespace gnarl;
using Catch::Approx;

namespace
{
    constexpr double kSampleRate = 48000.0;

    std::unique_ptr<GnarlProcessor> preparedProcessor()
    {
        auto processor = std::make_unique<GnarlProcessor>();
        processor->setPlayConfigDetails (0, 2, kSampleRate, 256);
        processor->prepareToPlay (kSampleRate, 256);
        return processor;
    }

    void setValue (GnarlProcessor& processor, const char* id, float value)
    {
        auto* parameter = processor.getValueTreeState().getParameter (id);
        REQUIRE (parameter != nullptr);

        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
        REQUIRE (ranged != nullptr);
        ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
    }

    float readValue (GnarlProcessor& processor, const char* id)
    {
        auto* value = processor.getValueTreeState().getRawParameterValue (id);
        REQUIRE (value != nullptr);
        return value->load();
    }

    preset::Metadata sampleMetadata()
    {
        preset::Metadata metadata;
        metadata.name = "Test Growl";
        metadata.author = "GNARL";
        metadata.category = "Growl";
        metadata.description = "A triplet growl for the tests.";
        metadata.tags = { "growl", "heavy" };
        return metadata;
    }
}

// ---------------------------------------------------------------------------
// The format
// ---------------------------------------------------------------------------

TEST_CASE ("A preset round trips through text", "[preset]")
{
    auto processor = preparedProcessor();

    setValue (*processor, pid::filter[0].cutoff, 440.0f);
    setValue (*processor, pid::fxReverb.mix, 0.75f);

    const auto tree = processor->getPresets().capture (sampleMetadata());
    const auto text = preset::toText (tree);

    REQUIRE (text.isNotEmpty());

    const auto parsed = preset::fromText (text);

    REQUIRE (preset::isPreset (parsed));

    const auto metadata = preset::readMetadata (parsed);

    CHECK (metadata.name == "Test Growl");
    CHECK (metadata.author == "GNARL");
    CHECK (metadata.category == "Growl");
    CHECK (metadata.description == "A triplet growl for the tests.");
    CHECK (metadata.tags.size() == 2);
    CHECK (metadata.tags[0] == "growl");
    CHECK (metadata.created.isNotEmpty());
    CHECK (preset::readState (parsed).isValid());
}

TEST_CASE ("A preset recalls every parameter it was saved with", "[preset]")
{
    /*  THE POINT OF THE WHOLE PHASE. 430 parameters, a fourteen-effect chain
        and the modulation state have to come back exactly - not nearly. This
        sweeps EVERY parameter to a distinctive value rather than checking a
        handful, because the ones that get forgotten are always the ones
        nobody thought to check. */
    auto source = preparedProcessor();

    // A different value per parameter, derived from its index, so a preset
    // that recalls the right NUMBER of parameters in the wrong ORDER fails.
    auto index = 0;

    for (auto* parameter : source->getParameters())
    {
        const auto fraction = static_cast<float> ((index * 37) % 101) / 100.0f;
        parameter->setValueNotifyingHost (fraction);
        ++index;
    }

    const auto tree = source->getPresets().capture (sampleMetadata());

    /*  COMPARED ON THE VALUE THE ENGINE READS, not on getValue().

        getValue() is the NORMALISED value the host sees, and for a stepped or
        boolean parameter it can legitimately differ across a round trip: the
        tree stores the quantised real value, so a boolean set to 0.7
        normalised comes back as 1.0. That is the tree being more consistent
        than the live parameter, not a recall failure - and the quantity that
        decides whether the patch SOUNDS the same is the raw value every
        cached atomic pointer in the engine reads. */
    std::vector<std::pair<juce::String, float>> expected;

    for (auto* parameter : source->getParameters())
    {
        auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter);
        REQUIRE (withID != nullptr);

        auto* raw = source->getValueTreeState().getRawParameterValue (withID->paramID);
        REQUIRE (raw != nullptr);

        expected.emplace_back (withID->paramID, raw->load());
    }

    auto target = preparedProcessor();
    REQUIRE (target->getPresets().apply (tree));

    for (const auto& [id, value] : expected)
    {
        auto* raw = target->getValueTreeState().getRawParameterValue (id);
        REQUIRE (raw != nullptr);

        INFO ("parameter " << id);

        // Exact: a state round trip must recall bit-identically, which is why
        // the test target turns -Wfloat-equal off (CLAUDE.md section 8).
        CHECK (raw->load() == value);
    }
}

TEST_CASE ("A preset carries the FX chain order and the mod state", "[preset]")
{
    /*  Neither of these is a parameter, so neither is covered by the sweep
        above - they live in the ValueTree and are exactly what a preset
        format that enumerated parameters would lose. */
    auto source = preparedProcessor();

    std::array<choices::FxSlot, dsp::FxOrder::kNumSlots> order {};

    for (std::size_t i = 0; i < order.size(); ++i)
        order[i] = static_cast<choices::FxSlot> (order.size() - 1 - i);

    REQUIRE (source->getFxOrder().setOrder (order));
    source->getModState().setDestination (3, pid::filter[1].cutoff);

    // What the SOURCE resolved that ID to. Comparing the two snapshots rather
    // than asserting a particular enum value keeps this test about the
    // preset carrying the destination, not about how the bridge maps IDs.
    const auto expected = source->getModState().getSnapshot().destinations[3];
    REQUIRE (expected != dsp::ModDestination::none);

    const auto tree = source->getPresets().capture (sampleMetadata());

    auto target = preparedProcessor();
    REQUIRE (target->getPresets().apply (tree));

    const auto& recalled = target->getFxOrder().getOrder();

    for (std::size_t i = 0; i < order.size(); ++i)
    {
        INFO ("chain position " << i);
        CHECK (recalled.getSlot (i) == order[i]);
    }

    CHECK (target->getModState().getSnapshot().destinations[3] == expected);
}

TEST_CASE ("Anything that is not a preset is refused", "[preset]")
{
    /*  A file with the right extension is not a promise. Every one of these
        has to leave the loaded patch ALONE rather than half-applying: a
        browser that resets the patch to defaults when you click a broken file
        has lost the user's work. */
    auto processor = preparedProcessor();

    setValue (*processor, pid::filter[0].cutoff, 660.0f);
    const auto before = readValue (*processor, pid::filter[0].cutoff);

    auto& presets = processor->getPresets();

    CHECK_FALSE (presets.apply ({}));
    CHECK_FALSE (presets.apply (juce::ValueTree { "SOMETHING_ELSE" }));

    // Well-formed XML that is not a preset.
    CHECK_FALSE (preset::isPreset (preset::fromText ("<OTHER a='1'/>")));
    CHECK_FALSE (preset::fromText ("").isValid());
    CHECK_FALSE (preset::fromText ("not xml at all {").isValid());

    // A preset element with metadata but NO state: applying it would silently
    // reset every parameter to its default.
    juce::ValueTree metaOnly { preset::kPresetType };
    metaOnly.appendChild (juce::ValueTree { preset::kMetaType }, nullptr);

    CHECK_FALSE (presets.apply (metaOnly));

    CHECK (readValue (*processor, pid::filter[0].cutoff) == before);
}

TEST_CASE ("A preset from an unknown state version still loads", "[preset]")
{
    /*  Refusing would be safer for us and worse for the person who just
        bought the thing. APVTS ignores IDs it does not know and leaves absent
        ones at their defaults, so a patch from a build with more parameters
        comes back as close to right as it can be. */
    auto source = preparedProcessor();
    setValue (*source, pid::filter[0].cutoff, 880.0f);

    auto tree = source->getPresets().capture (sampleMetadata());
    tree.setProperty (preset::kStateVersionProperty, 999, nullptr);

    auto state = preset::readState (tree);
    state.setProperty ("an_id_from_the_future", 0.5f, nullptr);

    auto target = preparedProcessor();
    REQUIRE (target->getPresets().apply (tree));

    CHECK (readValue (*target, pid::filter[0].cutoff) == Approx (880.0f).margin (0.5f));
}

TEST_CASE ("A missing state version reads as version 1", "[preset]")
{
    // A file with no version is from before the property existed, which is
    // version 1 by definition - not version 0.
    juce::ValueTree bare { preset::kPresetType };

    CHECK (preset::readStateVersion (bare) == 1);
}

TEST_CASE ("Filenames are sanitised without collapsing distinct names",
           "[preset]")
{
    // A name with a path separator in it must save rather than failing or
    // landing somewhere nobody asked for.
    CHECK (preset::sanitiseFilename ("Bass / Growl #1") == "Bass _ Growl #1");
    CHECK (preset::sanitiseFilename ("a:b*c?d\"e<f>g|h") == "a_b_c_d_e_f_g_h");

    // Replaced rather than removed, so two names that differ only in
    // punctuation do not become the same file.
    CHECK (preset::sanitiseFilename ("A/B") != preset::sanitiseFilename ("AB"));

    // A name that was nothing but illegal characters still has to produce a
    // file.
    CHECK (preset::sanitiseFilename ("///").isNotEmpty());
    CHECK (preset::sanitiseFilename ("   ").isNotEmpty());
}

TEST_CASE ("Metadata survives a file with almost nothing in it", "[preset]")
{
    // A hand-written preset with nothing but a name still has to list.
    juce::ValueTree tree { preset::kPresetType };
    juce::ValueTree meta { preset::kMetaType };
    meta.setProperty (preset::kNameProperty, "Hand Written", nullptr);
    tree.appendChild (meta, nullptr);

    const auto metadata = preset::readMetadata (tree);

    CHECK (metadata.name == "Hand Written");
    CHECK (metadata.category.isNotEmpty());   // falls back rather than empty
    CHECK (metadata.tags.isEmpty());
}

TEST_CASE ("Tag parsing drops the empties people actually type", "[preset]")
{
    juce::ValueTree tree { preset::kPresetType };
    juce::ValueTree meta { preset::kMetaType };
    meta.setProperty (preset::kTagsProperty, "growl, , heavy,", nullptr);
    tree.appendChild (meta, nullptr);

    const auto metadata = preset::readMetadata (tree);

    CHECK (metadata.tags.size() == 2);
    CHECK (metadata.tags[0] == "growl");
    CHECK (metadata.tags[1] == "heavy");
}

TEST_CASE ("The metadata comes before the state in the file", "[preset]")
{
    /*  Not cosmetic: the browser lists a bank by reading every file's
        metadata, and a bank is hundreds of files. Metadata at the top lets a
        lister stop after a few hundred bytes instead of parsing 430
        parameters per row. */
    auto processor = preparedProcessor();
    const auto tree = processor->getPresets().capture (sampleMetadata());

    // Asserted on the TREE, because the tree's child order is what determines
    // the file's element order and it cannot be confused by element names.
    REQUIRE (tree.getNumChildren() >= 2);
    CHECK (tree.getChild (0).hasType (preset::kMetaType));
    CHECK_FALSE (tree.getChild (1).hasType (preset::kMetaType));

    /*  And in the text. The needle here needs the trailing space that the
        attributes bring, because the APVTS's element is named "GNARL" and the
        preset's root is "GNARL_PRESET" - so a search for "<GNARL" finds the
        ROOT first, which is how the first version of this test failed while
        the format was correct. */
    const auto text = preset::toText (tree);
    const auto stateName = processor->getValueTreeState().state.getType().toString();

    const auto metaAt = text.indexOf ("<" + preset::kMetaType.toString());

    // Either form: an element with attributes opens "<GNARL ", one without
    // opens "<GNARL>". The state element has none, which is how the second
    // version of this test failed.
    const auto withAttributes = text.indexOf ("<" + stateName + " ");
    const auto withoutAttributes = text.indexOf ("<" + stateName + ">");
    const auto stateAt = withAttributes >= 0 ? withAttributes : withoutAttributes;

    REQUIRE (metaAt >= 0);
    REQUIRE (stateAt >= 0);
    CHECK (metaAt < stateAt);
}

// ---------------------------------------------------------------------------
// Loading behaviour
// ---------------------------------------------------------------------------

TEST_CASE ("Loading a preset does not block on wavetable generation",
           "[preset][audio]")
{
    /*  THE MEASUREMENT THIS EXISTS FOR. Generating one table costs tens of
        milliseconds, so a preset changing both oscillators' tables would
        freeze the window for over a tenth of a second - on every click
        through a browser, which is how people audition a bank.

        Timed rather than asserted structurally: a test that checked "the
        loader was called" would pass just as happily if the loader then
        waited. */
    auto processor = preparedProcessor();

    // A table the plugin has not built yet. Index 0 is loaded by
    // prepareToPlay, so pick one from the far end of the bank.
    const auto unusedTable = dsp::WavetableLibrary::kNumFactoryTables - 1;

    setValue (*processor, pid::osc[0].wavetable, static_cast<float> (unusedTable));
    setValue (*processor, pid::osc[1].wavetable, static_cast<float> (unusedTable - 1));

    const auto tree = processor->getPresets().capture (sampleMetadata());

    auto target = preparedProcessor();

    const auto start = juce::Time::getMillisecondCounterHiRes();
    REQUIRE (target->getPresets().apply (tree));
    const auto elapsed = juce::Time::getMillisecondCounterHiRes() - start;

    INFO ("applying the preset took " << elapsed << " ms");

    // Generously above what applying 430 parameters costs and far below what
    // generating two tables does, even in a Debug build where a table is
    // roughly 300 ms.
    CHECK (elapsed < 150.0);
}

TEST_CASE ("A preset load keeps the engine running and finite", "[preset][audio]")
{
    /*  The patch arrives in two steps - parameters now, tables a moment
        later - so the interesting question is what comes out of processBlock
        in between. Silence would be a bug; a NaN would be worse. */
    auto processor = preparedProcessor();

    setValue (*processor, pid::osc[0].wavetable,
              static_cast<float> (dsp::WavetableLibrary::kNumFactoryTables - 1));
    setValue (*processor, pid::fxReverb.enabled, 1.0f);

    const auto tree = processor->getPresets().capture (sampleMetadata());

    auto target = preparedProcessor();

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, 1.0f), 0);

    // Sounding before the load.
    for (int block = 0; block < 8; ++block)
    {
        buffer.clear();
        target->processBlock (buffer, midi);
        midi.clear();
    }

    REQUIRE (target->getPresets().apply (tree));

    auto peak = 0.0f;

    for (int block = 0; block < 64; ++block)
    {
        buffer.clear();
        target->processBlock (buffer, midi);

        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 256; ++i)
            {
                const auto sample = buffer.getReadPointer (channel)[i];
                REQUIRE (std::isfinite (sample));
                peak = juce::jmax (peak, std::abs (sample));
            }
    }

    // Still making sound while the new table is being built: the audio thread
    // keeps playing the table it already had rather than going quiet.
    INFO ("peak during and after the load: " << peak);
    CHECK (peak > 1.0e-4f);
}

TEST_CASE ("A preset and a session recall identically", "[preset][audio]")
{
    /*  WHY THERE IS ONE CODE PATH. A preset that sounds right from the browser
        and wrong after reopening the project is the failure this prevents, and
        it is one a customer meets rather than a test - unless the test
        compares the two paths directly. */
    auto source = preparedProcessor();

    auto index = 0;

    for (auto* parameter : source->getParameters())
    {
        parameter->setValueNotifyingHost (
            static_cast<float> ((index * 53) % 97) / 96.0f);
        ++index;
    }

    // Path one: the preset format.
    const auto presetTree = source->getPresets().capture (sampleMetadata());

    auto viaPreset = preparedProcessor();
    REQUIRE (viaPreset->getPresets().apply (presetTree));

    // Path two: the host's own state blob.
    juce::MemoryBlock blob;
    source->getStateInformation (blob);

    auto viaSession = preparedProcessor();
    viaSession->setStateInformation (blob.getData(), static_cast<int> (blob.getSize()));

    // Raw values again, for the same reason as above: it is what the engine
    // reads and therefore what decides whether the two paths sound alike.
    for (auto* parameter : viaPreset->getParameters())
    {
        auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter);
        REQUIRE (withID != nullptr);

        auto* fromPreset = viaPreset->getValueTreeState().getRawParameterValue (withID->paramID);
        auto* fromSession = viaSession->getValueTreeState().getRawParameterValue (withID->paramID);

        REQUIRE (fromPreset != nullptr);
        REQUIRE (fromSession != nullptr);

        INFO ("parameter " << withID->paramID);
        CHECK (fromPreset->load() == fromSession->load());
    }

    // And the state that is not a parameter.
    for (std::size_t slot = 0; slot < dsp::FxOrder::kNumSlots; ++slot)
        CHECK (viaPreset->getFxOrder().getOrder().getSlot (slot)
               == viaSession->getFxOrder().getOrder().getSlot (slot));
}

TEST_CASE ("Saving and loading a file round trips", "[preset]")
{
    auto processor = preparedProcessor();

    setValue (*processor, pid::filter[0].cutoff, 1234.0f);

    // Written into a temporary directory rather than the real user folder: a
    // test must not drop files into somebody's preset bank.
    auto directory = juce::File::createTempFile ("gnarl-preset-test");
    directory.deleteFile();
    REQUIRE (directory.createDirectory().wasOk());

    auto file = directory.getChildFile ("Round Trip." + preset::kFileExtension);

    auto metadata = sampleMetadata();
    metadata.name = "Round Trip";

    REQUIRE (file.replaceWithText (
        preset::toText (processor->getPresets().capture (metadata))));

    auto target = preparedProcessor();
    REQUIRE (target->getPresets().load (file));

    CHECK (readValue (*target, pid::filter[0].cutoff) == Approx (1234.0f).margin (0.5f));
    CHECK (target->getPresets().getCurrent().name == "Round Trip");

    // Loading clears the modified mark: the patch now IS the file.
    CHECK_FALSE (target->getPresets().isModified());

    directory.deleteRecursively();
}

TEST_CASE ("Loading a file that is not there fails quietly", "[preset]")
{
    auto processor = preparedProcessor();

    CHECK_FALSE (processor->getPresets().load (
        juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("gnarl-does-not-exist.gnarl")));
}
