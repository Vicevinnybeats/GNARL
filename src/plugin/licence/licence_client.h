/* Copyright 2026 Gnarl Audio
 *
 * GNARL is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See LICENSE.
 */

// The HTTP side of the licence check, and where its results are kept. Ported
// from the retired engine (custom-engine-archive) with backend/'s contract.
//
// The two functions that decide anything - building the request and reading
// the answer - are PURE, so tests/licence_tests.cpp covers the cases no live
// server can produce: a captive portal's login page, a 200 with a status we
// do not know, a proxy's 401, a body that is not JSON. Only httpVerifier
// touches the network.

#pragma once

#include "licence_manager.h"

namespace gnarl {
namespace licence {

  // A SHA-256 of the device id, salted per product, hex, truncated. The raw
  // id never leaves the machine (CLAUDE.md §8): the server must tell two
  // machines apart and has no business telling WHICH machine.
  String machineFingerprint();
  // Something a person recognises in an activation list. Best effort.
  String machineLabel();

  String activationBody(const String& key, const String& machine_id, const String& machine_label);

  // THE WHOLE CONTRACT. kValid and kRejected are decisions the server made;
  // everything else is kUnreachable, which opens the grace period instead of
  // disabling anything: any non-200 (including the 503 the Worker returns
  // when its own database fails - our fault must not cost a customer), a
  // body that is not JSON or has no recognised status (a captive portal), an
  // empty body. status_code 0: the request never completed.
  LicenceManager::Reply interpretResponse(int status_code, const String& body);

  // The verifier for LicenceManager; the endpoint and key are captured.
  LicenceManager::Verifier httpVerifier(String endpoint, String key);

  // Machine settings, NEVER the patch or the host's session: a key in the
  // state would travel inside every saved preset and every project, and the
  // first patch a customer shared would share their licence. MESSAGE THREAD.
  String loadKey();
  void saveKey(const String& key);
  // When a check last succeeded, kept beside the key so a machine that goes
  // offline starts its grace period from the right day.
  void loadVerification(Time& last_verified, bool& ever_verified);
  void saveVerification(Time last_verified, bool ever_verified);

} // namespace licence
} // namespace gnarl
