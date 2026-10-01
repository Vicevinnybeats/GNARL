/* Copyright 2026 Gnarl Audio
 *
 * GNARL is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See LICENSE.
 */

// What the plugin knows about its licence, and what it may do because of it
// (docs/design/phase7-01-licence.md). Ported from the retired engine
// (custom-engine-archive, plugin/source/license/), where the policy was
// written and tested; restyled to match Vital's code here.
//
// THE RULES ARE CLAUDE.md §8 AND ARE NOT NEGOTIABLE:
//   - the licence check never silences the plugin. If verification fails for
//     any reason, audio keeps playing; preset saving disables and a banner
//     appears, and that is the whole penalty;
//   - thirty days of offline grace;
//   - only an explicit valid / rejected answer is a decision; every error,
//     timeout, non-200 or captive-portal page opens the grace period.
//
// Not a bool, because "licensed" is not one bit: activated and offline,
// activated and expired, never activated, and rejected want different words
// in the banner and different behaviour from the gate.

#pragma once

#include "JuceHeader.h"

namespace gnarl {
namespace licence {

  enum Status {
    // No licence has ever been verified on this machine.
    kUnlicensed,
    // Verified against the server within the grace period.
    kLicensed,
    // Verified once; the server has not been reachable since. Still fully
    // licensed: this is what the grace period IS.
    kOffline,
    // Verified once, and the grace period has run out.
    kExpired,
    // The server answered and said no.
    kInvalid,
    // NOT A LICENCE STATE: a build with no endpoint configured does not check.
    // Features stay on (a build nobody can activate proves nothing by
    // refusing to save), and the banner says which build this is, so an
    // unconfigured build cannot quietly report kLicensed or be shipped unseen.
    kUnenforced,
    // ALSO NOT A LICENCE STATE: a build with no licensing by design - someone's
    // own instrument, built from source. Features on, no banner.
    kPersonal,
    kNumStatuses
  };

  // How many days offline before the grace period ends. CLAUDE.md §8.
  constexpr int kGraceDays = 30;
  // How long a verification may take before it is abandoned as unreachable:
  // a check that hangs would hold whatever waited on it.
  constexpr int kTimeoutMs = 8000;

  struct State {
    Status status = kUnlicensed;
    // Days left in the grace period; meaningful when offline.
    int grace_days_remaining = kGraceDays;
    // When the server last confirmed the licence, or an invalid Time.
    Time last_verified;
    // For the banner; empty when there is nothing to say.
    String message;

    // AUDIO IS ALWAYS ALLOWED. Nothing calls this: there is no code path that
    // asks whether it may make sound, and if one appears this is what it
    // should find.
    static constexpr bool audioAllowed() { return true; }

    // Saving presets is the only thing gated (the AI features are Phase 4,
    // which is not built).
    bool featuresAllowed() const {
      return status == kLicensed || status == kOffline || status == kUnenforced || status == kPersonal;
    }

    bool shouldWarn() const {
      return status != kLicensed && status != kPersonal;
    }
  };

  // The banner's sentence, versioned with the policy it describes.
  String describe(const State& state);

  // A stable name per status for the panel ("licensed", "offline", ...).
  // Frozen once shipped: the page compares them.
  String statusName(Status status);

} // namespace licence
} // namespace gnarl
