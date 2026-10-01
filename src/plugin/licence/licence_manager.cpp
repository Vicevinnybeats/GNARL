/* Copyright 2026 Gnarl Audio
 *
 * GNARL is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See LICENSE.
 */

#include "licence_manager.h"

namespace gnarl {
namespace licence {

  LicenceManager::LicenceManager() :
      Thread("GNARL licence"), timeout_ms_(kTimeoutMs), ever_verified_(false),
      checking_(false), check_started_ms_(0), pending_reply_(-1) {
    verifier_ = [] { return kUnreachable; };
    clock_ = [] { return Time::getCurrentTime(); };
  }

  LicenceManager::~LicenceManager() {
    stopTimer();
    // The verifier may be mid-request; its own socket times out at
    // kTimeoutMs. Two seconds is longer than an abandoned socket needs to
    // notice and shorter than a host will tolerate on closing a plugin.
    stopThread(2000);
  }

  void LicenceManager::setVerifier(Verifier verifier) {
    if (verifier)
      verifier_ = std::move(verifier);
  }

  void LicenceManager::setClock(Clock clock) {
    if (clock)
      clock_ = std::move(clock);
  }

  void LicenceManager::verify() {
    // A check in flight - or a verifier still stuck after its timeout - is
    // left alone: the thread is busy, and a second would race it.
    if (checking_ || isThreadRunning())
      return;

    checking_ = true;
    pending_reply_.store(-1);
    check_started_ms_ = Time::getMillisecondCounter();
    startThread(Thread::Priority::background);
    startTimer(kPollMs);
  }

  void LicenceManager::run() {
    int reply = verifier_ ? verifier_() : kUnreachable;
    // Only if the timeout has not already decided: a late answer is dropped.
    int expected = -1;
    pending_reply_.compare_exchange_strong(expected, reply);
  }

  void LicenceManager::timerCallback() {
    int reply = pending_reply_.load();
    if (reply < 0) {
      if (Time::getMillisecondCounter() - check_started_ms_ < static_cast<uint32>(timeout_ms_))
        return;
      // Timed out: unreachable, and whatever the verifier says later is
      // ignored (run() finds kAbandoned and stores nothing).
      int expected = -1;
      if (pending_reply_.compare_exchange_strong(expected, kAbandoned))
        reply = kUnreachable;
      else
        reply = pending_reply_.load();
    }

    stopTimer();
    checking_ = false;
    applyReply(static_cast<Reply>(reply < 0 ? kUnreachable : reply));
    publish();
  }

  bool LicenceManager::deliverPending(int timeout_ms) {
    if (!checking_)
      return false;
    uint32 deadline = Time::getMillisecondCounter() + static_cast<uint32>(timeout_ms);
    while (checking_ && Time::getMillisecondCounter() < deadline) {
      Thread::sleep(5);
      timerCallback();
    }
    return !checking_;
  }

  void LicenceManager::applyReply(Reply reply) {
    Time now = clock_();
    switch (reply) {
      case kValid:
        last_verified_ = now;
        ever_verified_ = true;
        state_.status = kLicensed;
        state_.grace_days_remaining = kGraceDays;
        state_.last_verified = last_verified_;
        break;
      case kRejected:
        // The server answered and said no: the one case grace does not cover.
        // Grace is for a machine that cannot ask, not one that was told no.
        state_.status = kInvalid;
        state_.grace_days_remaining = 0;
        break;
      case kUnreachable:
      default: {
        if (!ever_verified_) {
          // Never licensed and cannot ask: unlicensed, not an error.
          state_.status = kUnlicensed;
          state_.grace_days_remaining = 0;
          break;
        }
        int elapsed_days = static_cast<int>((now - last_verified_).inDays());
        state_.grace_days_remaining = std::max(0, kGraceDays - elapsed_days);
        state_.status = state_.grace_days_remaining > 0 ? kOffline : kExpired;
        state_.last_verified = last_verified_;
        break;
      }
    }
    state_.message = describe(state_);
  }

  void LicenceManager::setUnenforced() {
    state_.status = kUnenforced;
    state_.grace_days_remaining = 0;
    state_.message = describe(state_);
    publish();
  }

  void LicenceManager::setPersonal() {
    state_.status = kPersonal;
    state_.grace_days_remaining = 0;
    state_.message = describe(state_);
    publish();
  }

  void LicenceManager::restore(Time last_verified, bool ever_verified) {
    last_verified_ = last_verified;
    ever_verified_ = ever_verified;
    // From the saved time, assuming nothing: the plugin may have been closed
    // for a month.
    applyReply(kUnreachable);
    publish();
  }

  void LicenceManager::refreshGrace() {
    // Only the offline states move with the clock. A rejection does not
    // become valid by waiting, and a valid licence does not expire until a
    // check says so.
    if (state_.status != kOffline && state_.status != kExpired)
      return;
    applyReply(kUnreachable);
    publish();
  }

  void LicenceManager::publish() {
    if (listener_)
      listener_(state_);
  }

} // namespace licence
} // namespace gnarl
