/* Copyright 2026 Gnarl Audio
 *
 * GNARL is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See LICENSE.
 */

// Runs the licence check and decides what the plugin may do.
//
// NOTHING HERE CAN AFFECT AUDIO, structurally: the audio thread never reads
// this class. A verification changes a State the message thread reads.
//
// The verifier (the network) and the clock are injected, so the POLICY can be
// tested: "the server hangs" cannot be arranged against a real server, and a
// thirty-day grace period tested against the real clock takes thirty days.
//
// MESSAGE THREAD for everything public. The verifier runs on a background
// thread. The timeout is enforced HERE, on the message thread: at kTimeoutMs
// the check counts as unreachable whether or not the verifier has returned,
// and a late answer is dropped. (The retired engine's version waited for the
// verifier inside its thread, so a hung socket was never reported.)

#pragma once

#include "licence_state.h"

#include <atomic>
#include <functional>

namespace gnarl {
namespace licence {

  class LicenceManager : private Thread, private Timer {
    public:
      // Not a bool: "the server said no" and "the server did not answer" lead
      // to opposite behaviour.
      enum Reply {
        kValid,
        kRejected,
        kUnreachable
      };

      // Runs on a BACKGROUND thread.
      typedef std::function<Reply()> Verifier;
      typedef std::function<Time()> Clock;
      typedef std::function<void(const State&)> Listener;

      LicenceManager();
      ~LicenceManager() override;

      // The default verifier answers kUnreachable: what a machine with no
      // connection gets, which is the behaviour that most needs to be right.
      void setVerifier(Verifier verifier);
      void setClock(Clock clock);
      void setListener(Listener listener) { listener_ = std::move(listener); }
      // Tests only: a shorter timeout than kTimeoutMs.
      void setTimeoutMs(int timeout_ms) { timeout_ms_ = timeout_ms; }

      // This build does not check (kUnenforced) / has no licensing by design
      // (kPersonal). Publishes immediately.
      void setUnenforced();
      void setPersonal();

      // Starts a check and returns; the result arrives through the listener.
      // A check in flight is left alone rather than restarted.
      void verify();

      // What was saved: a plugin licensed once and opened offline knows it.
      void restore(Time last_verified, bool ever_verified);
      // Recomputes the grace period against the clock without asking anyone,
      // so a plugin left open for a month notices.
      void refreshGrace();

      State getState() const { return state_; }
      Time getLastVerified() const { return last_verified_; }
      bool hasEverVerified() const { return ever_verified_; }

      // Tests only: drives the timer by hand until the pending check is
      // decided (a test host runs no message loop). False if none was.
      bool deliverPending(int timeout_ms);

    private:
      static constexpr int kAbandoned = -2;
      static constexpr int kPollMs = 50;

      void run() override;
      void timerCallback() override;
      void applyReply(Reply reply);
      void publish();

      State state_;
      Verifier verifier_;
      Clock clock_;
      Listener listener_;
      int timeout_ms_;
      Time last_verified_;
      bool ever_verified_;

      bool checking_;
      uint32 check_started_ms_;
      // -1 nothing yet, a Reply, or kAbandoned once the timeout has decided.
      std::atomic<int> pending_reply_;

      JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenceManager)
  };

} // namespace licence
} // namespace gnarl
