/* Copyright 2026 Gnarl Audio
 *
 * GNARL is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See LICENSE.
 */

#include "licence_state.h"

namespace gnarl {
namespace licence {

  String describe(const State& state) {
    switch (state.status) {
      case kLicensed:
      case kPersonal:
        // Nothing to say. A banner that is always up is a banner nobody reads.
        return {};
      case kOffline:
        // Says how long is left and does NOT say anything is wrong, because
        // nothing is: the licence is valid and the machine is offline.
        return "Offline - the licence re-checks when you reconnect (" + String(state.grace_days_remaining) +
               (state.grace_days_remaining == 1 ? " day" : " days") + " left).";
      case kExpired:
        // Names what still works first: someone reading this is worried about
        // losing a session, and the first clause answers that.
        return "Audio still works. Connect to the internet to restore preset saving.";
      case kInvalid:
        return "This licence could not be verified. Audio still works; preset saving is disabled.";
      case kUnenforced:
        // Deliberately conspicuous: a development build that says nothing is
        // a development build that gets shipped.
        return "Development build - licence checking is not configured.";
      case kUnlicensed:
      default:
        return "Unlicensed - audio works; enter a licence key to save presets.";
    }
  }

  String statusName(Status status) {
    switch (status) {
      case kLicensed: return "licensed";
      case kOffline: return "offline";
      case kExpired: return "expired";
      case kInvalid: return "invalid";
      case kUnenforced: return "unenforced";
      case kPersonal: return "personal";
      case kUnlicensed:
      default: return "unlicensed";
    }
  }

} // namespace licence
} // namespace gnarl
