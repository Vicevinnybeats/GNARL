/* Copyright 2026 Gnarl Audio
 *
 * GNARL is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See LICENSE.
 */

#include "licence_client.h"

namespace gnarl {
namespace licence {

  namespace {
    // Long enough that collisions do not matter, short enough to read out.
    constexpr int kFingerprintChars = 32;

    // Property names: frozen once shipped, like parameter names. Renaming one
    // loses every customer's activation.
    const char* const kKeyProperty = "licenceKey";
    const char* const kLastVerifiedProperty = "licenceLastVerified";
    const char* const kEverVerifiedProperty = "licenceEverVerified";

    PropertiesFile::Options settingsOptions() {
      PropertiesFile::Options options;
      options.applicationName = "GNARL";
      options.filenameSuffix = "settings";
      // On Linux JUCE puts folderName under the home directory itself, which
      // made a visible ~/GNARL; the XDG place is ~/.config. macOS adds
      // Application Support, Windows AppData.
#if JUCE_LINUX
      options.folderName = ".config/GNARL";
#else
      options.folderName = "GNARL";
#endif
      options.osxLibrarySubFolder = "Application Support";
      return options;
    }
  }

  String machineFingerprint() {
    // The salt keeps the same device from producing the same identifier in
    // two products' databases.
    String raw = "GNARL-v1:" + SystemStats::getUniqueDeviceID();
    return SHA256(raw.toRawUTF8(), raw.getNumBytesAsUTF8()).toHexString().substring(0, kFingerprintChars);
  }

  String machineLabel() {
    String label = SystemStats::getComputerName();
    if (label.isEmpty())
      label = SystemStats::getOperatingSystemName();
    return label.substring(0, 120);
  }

  String activationBody(const String& key, const String& machine_id, const String& machine_label) {
    DynamicObject::Ptr root = new DynamicObject();
    root->setProperty("key", key);
    root->setProperty("machineId", machine_id);
    if (machine_label.isNotEmpty())
      root->setProperty("machineLabel", machine_label);
    return JSON::toString(var(root.get()), true);
  }

  LicenceManager::Reply interpretResponse(int status_code, const String& body) {
    // Anything that is not a decision is unreachable. Not defensive coding,
    // the policy: guessing "rejected" wrongly costs a paying customer their
    // preset saving over a fault that was never theirs.
    if (status_code != 200)
      return LicenceManager::kUnreachable;

    var parsed = JSON::parse(body);
    if (!parsed.isObject())
      return LicenceManager::kUnreachable;

    String status = parsed.getProperty("status", var()).toString();
    if (status == "valid")
      return LicenceManager::kValid;
    if (status == "rejected")
      return LicenceManager::kRejected;
    return LicenceManager::kUnreachable;
  }

  LicenceManager::Verifier httpVerifier(String endpoint, String key) {
    return [endpoint, key]() -> LicenceManager::Reply {
      // No key entered yet is NOT a rejection: unreachable leaves features as
      // they were and the banner up, rather than punishing someone who has
      // not typed it in.
      if (endpoint.isEmpty() || key.isEmpty())
        return LicenceManager::kUnreachable;

      String body = activationBody(key, machineFingerprint(), machineLabel());
      URL url = URL(endpoint).withPOSTData(body);
      int status_code = 0;
      // The socket's own timeout as well as the manager's: this one stops the
      // socket waiting, the manager's stops waiting for anything else.
      auto options = URL::InputStreamOptions(URL::ParameterHandling::inPostData)
                         .withExtraHeaders("Content-Type: application/json")
                         .withConnectionTimeoutMs(kTimeoutMs)
                         .withStatusCode(&status_code);
      std::unique_ptr<InputStream> stream = url.createInputStream(options);
      if (stream == nullptr)
        return LicenceManager::kUnreachable;
      return interpretResponse(status_code, stream->readEntireStreamAsString());
    };
  }

  String loadKey() {
    PropertiesFile file(settingsOptions());
    return file.getValue(kKeyProperty);
  }

  void saveKey(const String& key) {
    PropertiesFile file(settingsOptions());
    String trimmed = key.trim();
    if (trimmed.isEmpty())
      file.removeValue(kKeyProperty);
    else
      file.setValue(kKeyProperty, trimmed);
    file.saveIfNeeded();
  }

  void loadVerification(Time& last_verified, bool& ever_verified) {
    PropertiesFile file(settingsOptions());
    ever_verified = file.getBoolValue(kEverVerifiedProperty, false);
    last_verified = Time(file.getValue(kLastVerifiedProperty, "0").getLargeIntValue());
  }

  void saveVerification(Time last_verified, bool ever_verified) {
    PropertiesFile file(settingsOptions());
    file.setValue(kEverVerifiedProperty, ever_verified);
    file.setValue(kLastVerifiedProperty, String(last_verified.toMilliseconds()));
    file.saveIfNeeded();
  }

} // namespace licence
} // namespace gnarl
