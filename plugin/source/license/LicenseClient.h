#pragma once

#include "LicenseManager.h"

#include <juce_data_structures/juce_data_structures.h>

namespace gnarl::license
{

/**
    The HTTP side of the licence check, split from the policy on purpose.

    `LicenseManager` owns the POLICY - grace periods, what a rejection
    disables, what audio is allowed to do - and takes its verifier by
    injection so that policy can be tested against a function rather than
    against a server. This file is the verifier it gets in a real build.

    THE TWO INTERESTING FUNCTIONS HERE ARE PURE, and that is deliberate.
    Building the request and interpreting the response are where the
    three-answer contract is actually enforced, so they are ordinary
    functions with no IO in them and `LicenseClientTests` covers them
    exhaustively. Only `makeHttpVerifier` touches the network, and it has
    nothing in it but the call.
*/

/** A stable, non-identifying machine fingerprint.

    A SHA-256 of the platform's device id, hex, truncated. The RAW id never
    leaves the machine: the server only ever needs to tell two machines
    apart, and it has no business being able to tell WHICH machine. Sending
    the hash costs nothing and means a leaked database cannot be joined
    against anything. */
juce::String getMachineFingerprint();

/** Something a person can recognise in an activation list - "MacBook Pro",
    the host name - so that deactivating the right machine does not require
    matching hex strings. Best effort, and never required. */
juce::String getMachineLabel();

/** The request body. Pure, so it can be asserted on. */
juce::String buildActivationBody (const juce::String& licenceKey,
                                  const juce::String& machineId,
                                  const juce::String& machineLabel);

/** Turns an HTTP result into one of the three answers.

    THE WHOLE CONTRACT IS IN THIS FUNCTION. `valid` and `rejected` are
    decisions the server made; EVERYTHING ELSE is `unreachable`, which opens
    the 30-day grace period rather than disabling anything:

      - a non-200 status, including the 503 the Worker returns when its own
        database fails - a fault of ours must not cost a customer their
        session;
      - a body that is not JSON, or JSON without a recognised `status`,
        which is what a captive portal's login page looks like;
      - an empty body.

    A `statusCode` of 0 means the request never completed at all. */
LicenseManager::Reply interpretResponse (int statusCode, const juce::String& body);

/** Where the licence key is stored, and where it is NOT.

    NOT in the ValueTree, and this is the one decision here that is not
    reversible later without hurting somebody. The ValueTree travels with
    the patch: a key kept there would be written into every `.gnarl` preset
    and every host session file, so the first time a customer shared a patch
    they would be sharing their licence with it - and the first time they
    posted one publicly, with everyone.

    So it lives with the machine's own settings, beside the view
    preferences, for the same reason those do (CLAUDE.md section 6): it
    belongs to the person at this computer, not to the patch. */
juce::PropertiesFile::Options getSettingsOptions();

/** The stored key, or empty. MESSAGE THREAD. */
juce::String loadLicenceKey();

/** Stores it, or clears it when given an empty string. MESSAGE THREAD. */
void saveLicenceKey (const juce::String& licenceKey);

/** The verifier `LicenseManager` runs on its background thread.

    Called with no arguments by the manager, so the endpoint and key are
    captured. Returns `unreachable` on any failure, per `interpretResponse`. */
LicenseManager::Verifier makeHttpVerifier (juce::String endpoint,
                                           juce::String licenceKey);

} // namespace gnarl::license
