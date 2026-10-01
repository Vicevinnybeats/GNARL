/* Copyright 2026 Gnarl Audio
 *
 * GNARL is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See LICENSE.
 */

// The licence policy and client (src/plugin/licence/), CLAUDE.md §8.
//
//   cmake --build build-cmake --target gnarl_licence_tests
//   build-cmake/gnarl_licence_tests_artefacts/Release/gnarl_licence_tests
//
// Ported from the retired engine's LicenseTests / LicenseClientTests. Every
// rule here is a promise to a customer, so they are tested as rules. The
// verifier and the clock are injected: "the server hangs" cannot be arranged
// against a real server, and thirty days cannot be waited out.
// Exit status: the number of failures.

#include "JuceHeader.h"
#include "licence/licence_client.h"

#include <atomic>
#include <cstdio>

using namespace gnarl::licence;

namespace {
  int failures = 0;

  void check(bool ok, const String& message) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", message.toRawUTF8());
    if (!ok)
      failures++;
  }

  struct Harness {
    LicenceManager manager;
    State latest;
    int notifications = 0;
    Time now = Time::getCurrentTime();

    Harness() {
      manager.setClock([this] { return now; });
      manager.setListener([this](const State& state) {
        latest = state;
        notifications++;
      });
    }

    void answer(LicenceManager::Reply reply) {
      manager.setVerifier([reply] { return reply; });
    }

    // Runs one check to its result (no message loop in a test).
    State check() {
      manager.verify();
      manager.deliverPending(15000);
      return manager.getState();
    }

    void daysPass(int days) { now = now + RelativeTime::days(days); }
  };

  void policy() {
    static_assert(State::audioAllowed(), "audio must never be gated on a licence");

    for (int s = 0; s < kNumStatuses; ++s) {
      State state;
      state.status = static_cast<Status>(s);
      check(state.audioAllowed(), "audio allowed when " + statusName(state.status));
    }

    {
      State state;
      state.status = kExpired;
      check(!state.featuresAllowed() && state.shouldWarn(), "expired: preset saving off, banner up");
      state.status = kOffline;
      check(state.featuresAllowed() && state.shouldWarn(), "offline: saving on, banner says so");
      state.status = kLicensed;
      check(state.featuresAllowed() && !state.shouldWarn(), "licensed: saving on, no banner");
    }

    {
      Harness h;
      h.answer(LicenceManager::kValid);
      State state = h.check();
      check(state.status == kLicensed && state.message.isEmpty(), "a valid reply licenses, and says nothing");
    }

    {
      Harness h;
      h.answer(LicenceManager::kValid);
      h.check();
      h.answer(LicenceManager::kUnreachable);
      h.daysPass(3);
      State state = h.check();
      check(state.status == kOffline && state.featuresAllowed() && state.grace_days_remaining == 27,
            "unreachable after a licence: offline, saving on, 27 days left after 3 (" +
            String(state.grace_days_remaining) + ")");
    }

    {
      Harness h;
      h.answer(LicenceManager::kValid);
      h.check();
      h.answer(LicenceManager::kUnreachable);
      h.daysPass(29);
      State state = h.check();
      check(state.status == kOffline && state.grace_days_remaining == 1 && state.message.contains("1 day left"),
            "day 29: one day left, singular: \"" + state.message + "\"");
      h.daysPass(1);
      state = h.check();
      check(state.status == kExpired && !state.featuresAllowed() && state.message.startsWith("Audio still works"),
            "day 30: expired; the message names what still works first");
      h.answer(LicenceManager::kValid);
      state = h.check();
      check(state.status == kLicensed && state.featuresAllowed(), "reconnecting restores an expired licence");
    }

    {
      Harness h;
      h.answer(LicenceManager::kValid);
      h.check();
      h.answer(LicenceManager::kRejected);
      State state = h.check();
      check(state.status == kInvalid && !state.featuresAllowed() && state.grace_days_remaining == 0,
            "a rejection gets no grace period, even after a licence");
    }

    {
      Harness h;
      h.answer(LicenceManager::kUnreachable);
      State state = h.check();
      check(state.status == kUnlicensed && state.grace_days_remaining == 0,
            "never licensed and unreachable: unlicensed, not in grace");
    }

    {
      // The verifier hangs far past the timeout: the check is decided at the
      // timeout as unreachable, and the late answer is dropped.
      Harness h;
      h.manager.setTimeoutMs(300);
      h.answer(LicenceManager::kValid);
      h.check();
      std::atomic<bool> release { false };
      h.manager.setVerifier([&release] {
        while (!release.load())
          Thread::sleep(10);
        return LicenceManager::kRejected;
      });
      uint32 started = Time::getMillisecondCounter();
      State state = h.check();
      uint32 took = Time::getMillisecondCounter() - started;
      check(state.status == kOffline && took < 2000,
            "a hung verifier is decided at the timeout (" + String(took) + " ms) as unreachable: offline");
      release.store(true);
      Thread::sleep(100);
      check(h.manager.getState().status == kOffline, "and its late rejection is dropped");
    }

    {
      Harness h;
      h.answer(LicenceManager::kValid);
      h.check();
      Time verified = h.manager.getLastVerified();
      Harness later;
      later.now = verified + RelativeTime::days(10);
      later.manager.restore(verified, true);
      State state = later.manager.getState();
      check(state.status == kOffline && state.grace_days_remaining == 20,
            "a saved licence restored 10 days later: offline, 20 days left (" +
            String(state.grace_days_remaining) + ")");
      later.daysPass(25);
      later.manager.refreshGrace();
      check(later.manager.getState().status == kExpired, "and a month on, refreshGrace notices it expired");
    }

    {
      Harness h;
      std::atomic<int> calls { 0 };
      h.manager.setVerifier([&calls] {
        calls++;
        Thread::sleep(200);
        return LicenceManager::kValid;
      });
      h.manager.verify();
      h.manager.verify();
      h.manager.verify();
      h.manager.deliverPending(5000);
      check(calls.load() == 1, "three overlapping checks run the verifier once (" + String(calls.load()) + ")");
    }

    {
      Harness h;
      h.manager.setUnenforced();
      State state = h.manager.getState();
      check(state.status == kUnenforced && state.featuresAllowed() && state.shouldWarn() &&
            state.message.startsWith("Development build"),
            "unconfigured build: saving on, banner says development build");
      h.daysPass(400);
      h.manager.refreshGrace();
      check(h.manager.getState().status == kUnenforced, "waiting does not turn it into a licence or an expiry");
      h.manager.setPersonal();
      state = h.manager.getState();
      check(state.status == kPersonal && state.featuresAllowed() && !state.shouldWarn() && state.message.isEmpty(),
            "personal build: saving on, no banner");
    }
  }

  void client() {
    check(interpretResponse(503, "{\"status\":\"rejected\"}") == LicenceManager::kUnreachable,
          "a 503 is unreachable even if its body says rejected (the Worker's own outage)");
    check(interpretResponse(200, "{\"status\":\"valid\"}") == LicenceManager::kValid, "200 valid is valid");
    check(interpretResponse(200, "{\"status\":\"rejected\",\"reason\":\"revoked\"}") == LicenceManager::kRejected,
          "200 rejected is rejected");
    const char* not_decisions[][2] = {
      { "200", "<html><body>Sign in to the hotel wifi</body></html>" },
      { "200", "{\"status\":\"pending\"}" },
      { "200", "{}" },
      { "200", "" },
      { "401", "{\"status\":\"valid\"}" },
      { "302", "" },
      { "0", "" },
      { "500", "Internal Server Error" },
    };
    for (auto& pair : not_decisions) {
      check(interpretResponse(String(pair[0]).getIntValue(), pair[1]) == LicenceManager::kUnreachable,
            "unreachable: " + String(pair[0]) + " \"" + String(pair[1]).substring(0, 30) + "\"");
    }

    var body = JSON::parse(activationBody("GNARL-ABCD-1234", "f00d", "Studio Mac"));
    check(body["key"] == "GNARL-ABCD-1234" && body["machineId"] == "f00d" && body["machineLabel"] == "Studio Mac",
          "the body carries key, machine id and label");
    check(!JSON::parse(activationBody("k", "m", "")).getDynamicObject()->hasProperty("machineLabel"),
          "an empty label is left out, not sent empty");
    var tricky = JSON::parse(activationBody("a\"b\\c\n", "m", "x"));
    check(tricky["key"] == "a\"b\\c\n", "a key with quotes and backslashes survives as JSON");

    String fingerprint = machineFingerprint();
    String raw = SystemStats::getUniqueDeviceID();
    check(fingerprint.length() == 32 && fingerprint.containsOnly("0123456789abcdef") &&
          (raw.isEmpty() || !fingerprint.contains(raw)) && fingerprint == machineFingerprint(),
          "the fingerprint is 32 hex characters, stable, and not the raw device id");

    check(httpVerifier("", "KEY")() == LicenceManager::kUnreachable, "no endpoint: unreachable, not rejected");
    check(httpVerifier("https://example.invalid/activate", "")() == LicenceManager::kUnreachable,
          "no key entered yet: unreachable, not rejected");

    // The real settings file: put back whatever was there.
    String before = loadKey();
    saveKey("  TEST-KEY-42  ");
    bool round_trip = loadKey() == "TEST-KEY-42";
    saveKey("");
    bool cleared = loadKey().isEmpty();
    saveKey(before);
    check(round_trip && cleared, "a stored key round-trips trimmed, and an empty one clears it");
  }
}

int main() {
  ScopedJuceInitialiser_GUI juce_init;
  policy();
  client();
  std::printf("\n%d failure(s)\n", failures);
  return failures;
}
