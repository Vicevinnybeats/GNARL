#include <catch2/catch_test_macros.hpp>

#include "license/LicenseClient.h"

using namespace gnarl;
using namespace gnarl::license;

/*
    The HTTP side of the licence check.

    EVERYTHING ASSERTED HERE IS PURE. Building the request and interpreting
    the response are where the three-answer contract is actually enforced,
    and both are ordinary functions with no IO in them - which is the only
    reason this file can cover the cases that matter. A test that needed a
    server could not check what happens when the server answers with a
    captive portal's login page, and that is exactly the case that decides
    whether a customer keeps working.
*/

TEST_CASE ("A 503 is not a rejection", "[license][client]")
{
    /*  THE SINGLE MOST IMPORTANT ASSERTION IN THIS FILE.

        503 is what the Worker returns when its OWN database fails. Reading
        that as `rejected` would disable a paying customer's preset saving
        because of an outage that was never theirs - and `unreachable`
        instead puts them in the 30-day grace period, which is the entire
        reason the grace period exists (CLAUDE.md section 9). */
    CHECK (interpretResponse (503, R"({"error":"temporarily unavailable"})")
             == LicenseManager::Reply::unreachable);

    // And not even if the body somehow carries a decision. The status code
    // decides first; a 503 is not an answer whatever it is carrying.
    CHECK (interpretResponse (503, R"({"status":"rejected"})")
             == LicenseManager::Reply::unreachable);
}

TEST_CASE ("Only a 200 carrying a known status is a decision", "[license][client]")
{
    CHECK (interpretResponse (200, R"({"status":"valid","reason":"new activation"})")
             == LicenseManager::Reply::valid);

    CHECK (interpretResponse (200, R"({"status":"rejected","reason":"unknown key"})")
             == LicenseManager::Reply::rejected);

    // A refusal IS a 200 - it is an answer. The client has to be able to
    // tell it apart from the service being down.
    CHECK (interpretResponse (200, R"({"status":"rejected","reason":"activation limit reached"})")
             == LicenseManager::Reply::rejected);
}

TEST_CASE ("Anything that is not a decision opens the grace period",
           "[license][client]")
{
    struct Case { int status; const char* body; const char* what; };

    for (const auto& testCase : {
             Case { 0,   "",                              "the request never completed" },
             Case { 200, "",                              "an empty body" },
             Case { 200, "not json at all",               "a body that is not JSON" },
             Case { 200, "<html><body>Sign in</body>",    "a captive portal's login page" },
             Case { 200, R"({"status":"maybe"})",         "a status we do not know" },
             Case { 200, R"({"reason":"unknown key"})",   "a reason with no status" },
             Case { 200, R"(["valid"])",                  "JSON that is not an object" },
             Case { 200, R"({"status":"VALID"})",         "the right word, wrong case" },
             Case { 401, R"({"status":"rejected"})",      "an auth error from a proxy" },
             Case { 404, R"({"status":"rejected"})",      "the endpoint having moved" },
             Case { 500, "",                              "a crash" },
         })
    {
        INFO (testCase.what << " (HTTP " << testCase.status << ")");

        CHECK (interpretResponse (testCase.status, testCase.body)
                 == LicenseManager::Reply::unreachable);
    }
}

TEST_CASE ("The request body carries the key and the machine", "[license][client]")
{
    const auto body = buildActivationBody ("GNARL-ABCD-1234", "fingerprint", "Studio PC");
    const auto parsed = juce::JSON::parse (body);

    REQUIRE (parsed.isObject());

    CHECK (parsed.getProperty ("key", {}).toString() == "GNARL-ABCD-1234");
    CHECK (parsed.getProperty ("machineId", {}).toString() == "fingerprint");
    CHECK (parsed.getProperty ("machineLabel", {}).toString() == "Studio PC");
}

TEST_CASE ("A missing label is omitted rather than sent empty", "[license][client]")
{
    // The server stores the label for a human to read in an activations
    // list. An empty string there is worse than nothing: it looks like a
    // machine whose name failed to load rather than one that never had one.
    const auto parsed = juce::JSON::parse (buildActivationBody ("K", "M", ""));

    REQUIRE (parsed.isObject());
    CHECK (! parsed.hasProperty ("machineLabel"));
}

TEST_CASE ("The body is valid JSON even when the key contains punctuation",
           "[license][client]")
{
    /*  A key format is not frozen yet, so the encoder has to survive
        whatever one turns out to be - and a hand-built string with quotes
        in it would produce a body the server rejects as malformed, which
        the client would then read as... a rejection. Building it through
        JUCE's encoder rather than by concatenation is what prevents that. */
    const auto awkward = R"(quote" backslash\ newline)" "\n" R"(tab	end)";

    const auto parsed = juce::JSON::parse (buildActivationBody (awkward, "M", "L"));

    REQUIRE (parsed.isObject());
    CHECK (parsed.getProperty ("key", {}).toString() == awkward);
}

TEST_CASE ("The machine fingerprint is a hash, not the device id",
           "[license][client]")
{
    const auto fingerprint = getMachineFingerprint();

    CHECK (fingerprint.isNotEmpty());

    // Hex only, and a fixed width: it is a truncated SHA-256.
    CHECK (fingerprint.length() == 32);
    CHECK (fingerprint.containsOnly ("0123456789abcdefABCDEF"));

    /*  THE PRIVACY PROPERTY, asserted rather than assumed. The raw device id
        must not appear in what is sent - the server needs to tell two
        machines apart and has no business telling WHICH machine, so a
        leaked database cannot be joined against anything. */
    const auto raw = juce::SystemStats::getUniqueDeviceID();

    if (raw.isNotEmpty())
        CHECK (! fingerprint.containsIgnoreCase (raw));

    // Stable across calls, or every launch would burn an activation.
    CHECK (getMachineFingerprint() == fingerprint);
}

TEST_CASE ("A verifier with nothing to ask reports unreachable",
           "[license][client]")
{
    /*  No endpoint configured, or no key entered yet. Neither is a licence
        failure: an unconfigured build has not been told to check, and a
        customer who has not typed their key in has not been rejected. Both
        must avoid `rejected`, which would disable features. */
    CHECK (makeHttpVerifier ("", "GNARL-ABCD")() == LicenseManager::Reply::unreachable);
    CHECK (makeHttpVerifier ("https://example.invalid/activate", "")()
             == LicenseManager::Reply::unreachable);
}

TEST_CASE ("The licence key is not stored in the patch", "[license][client]")
{
    /*  THE ONE DECISION HERE THAT CANNOT BE WALKED BACK. The ValueTree
        travels with the patch, so a key kept there would be written into
        every `.gnarl` preset and every host session file - and the first
        time somebody posted a patch publicly they would be posting their
        licence with it.

        Asserted by checking where the settings actually go: the machine's
        own settings folder, beside the view preferences, for the same
        reason those live there (CLAUDE.md section 6). */
    const auto options = getSettingsOptions();

    CHECK (options.applicationName == "GNARL");
    CHECK (options.filenameSuffix == "settings");

    const auto file = options.getDefaultFile();

    INFO ("settings file: " << file.getFullPathName());

    // Not in the user's preset folder, which is where a patch would put it.
    CHECK (! file.getFullPathName().containsIgnoreCase ("Presets"));
}

TEST_CASE ("A stored key round-trips and can be cleared", "[license][client]")
{
    const auto original = loadLicenceKey();

    saveLicenceKey ("GNARL-ROUNDTRIP-0001");
    CHECK (loadLicenceKey() == "GNARL-ROUNDTRIP-0001");

    // Whitespace trimmed: a key pasted from an email arrives with some.
    saveLicenceKey ("  GNARL-PADDED-0002\n");
    CHECK (loadLicenceKey() == "GNARL-PADDED-0002");

    saveLicenceKey ("");
    CHECK (loadLicenceKey().isEmpty());

    // Leave the machine as it was found.
    saveLicenceKey (original);
}
