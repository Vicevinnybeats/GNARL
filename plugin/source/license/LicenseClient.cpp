#include "LicenseClient.h"

#include <juce_cryptography/juce_cryptography.h>

namespace gnarl::license
{

namespace
{
    /** Long enough that a collision is not a concern, short enough to read
        in a support email. */
    constexpr int kFingerprintChars = 32;
}

juce::String getMachineFingerprint()
{
    /*  Hashed, never sent raw. The server needs to tell two machines apart
        and has no business telling WHICH machine - so what crosses the wire
        cannot be joined against anything if the database ever leaks.

        The salt makes the hash specific to this product: the same device
        running two of our things should not produce the same identifier in
        both databases. */
    const auto raw = "GNARL-v1:" + juce::SystemStats::getUniqueDeviceID();

    return juce::SHA256 (raw.toRawUTF8(), raw.getNumBytesAsUTF8())
        .toHexString()
        .substring (0, kFingerprintChars);
}

juce::String getMachineLabel()
{
    auto label = juce::SystemStats::getComputerName();

    if (label.isEmpty())
        label = juce::SystemStats::getOperatingSystemName();

    return label.substring (0, 120);
}

juce::PropertiesFile::Options getSettingsOptions()
{
    juce::PropertiesFile::Options options;

    options.applicationName = "GNARL";
    options.filenameSuffix = "settings";
    options.folderName = "GNARL";
    options.osxLibrarySubFolder = "Application Support";

    return options;
}

namespace
{
    /** The property name. Frozen once shipped, like a parameter ID: renaming
        it would silently lose every customer's activation and present as the
        plugin asking them to enter a key they already entered. */
    const juce::String kLicenceKeyProperty { "licenceKey" };
}

juce::String loadLicenceKey()
{
    juce::PropertiesFile file (getSettingsOptions());
    return file.getValue (kLicenceKeyProperty);
}

void saveLicenceKey (const juce::String& licenceKey)
{
    juce::PropertiesFile file (getSettingsOptions());

    const auto trimmed = licenceKey.trim();

    if (trimmed.isEmpty())
        file.removeValue (kLicenceKeyProperty);
    else
        file.setValue (kLicenceKeyProperty, trimmed);

    file.saveIfNeeded();
}

juce::String buildActivationBody (const juce::String& licenceKey,
                                  const juce::String& machineId,
                                  const juce::String& machineLabel)
{
    auto* root = new juce::DynamicObject();

    root->setProperty ("key", licenceKey);
    root->setProperty ("machineId", machineId);

    if (machineLabel.isNotEmpty())
        root->setProperty ("machineLabel", machineLabel);

    return juce::JSON::toString (juce::var (root), true);
}

LicenseManager::Reply interpretResponse (int statusCode, const juce::String& body)
{
    /*  ANYTHING THAT IS NOT A DECISION IS `unreachable`. That is not
        defensive coding, it is the policy: `rejected` disables preset saving
        and `unreachable` opens a 30-day grace period, so guessing wrong in
        this direction costs a paying customer their session over a fault
        that was never theirs.

        A 503 in particular is what the Worker returns when its OWN database
        fails. Reading that as a rejection would be the single worst bug this
        file could contain. */
    if (statusCode != 200)
        return LicenseManager::Reply::unreachable;

    const auto parsed = juce::JSON::parse (body);

    if (! parsed.isObject())
        return LicenseManager::Reply::unreachable;

    const auto status = parsed.getProperty ("status", juce::var()).toString();

    if (status == "valid")
        return LicenseManager::Reply::valid;

    if (status == "rejected")
        return LicenseManager::Reply::rejected;

    // A 200 carrying something we do not recognise is a proxy, a captive
    // portal, or a version of the service we do not know. None of those is
    // the customer's fault.
    return LicenseManager::Reply::unreachable;
}

LicenseManager::Verifier makeHttpVerifier (juce::String endpoint,
                                           juce::String licenceKey)
{
    return [endpoint = std::move (endpoint),
            licenceKey = std::move (licenceKey)]() -> LicenseManager::Reply
    {
        if (endpoint.isEmpty() || licenceKey.isEmpty())
            return LicenseManager::Reply::unreachable;

        const auto body = buildActivationBody (licenceKey,
                                               getMachineFingerprint(),
                                               getMachineLabel());

        const auto url = juce::URL (endpoint).withPOSTData (body);

        int statusCode = 0;

        /*  A timeout here as well as the one the manager enforces on its own
            thread. This one stops the socket waiting; the manager's stops a
            verifier that hangs for any other reason. Neither is redundant -
            see LicenseManager::run. */
        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                           .withExtraHeaders ("Content-Type: application/json")
                           .withConnectionTimeoutMs (kTimeoutMs)
                           .withStatusCode (&statusCode);

        auto stream = url.createInputStream (options);

        if (stream == nullptr)
            return LicenseManager::Reply::unreachable;

        return interpretResponse (statusCode, stream->readEntireStreamAsString());
    };
}

} // namespace gnarl::license
