#pragma once

#include "LicenseState.h"

#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>
#include <memory>

namespace gnarl::license
{

/**
    Runs the licence check and decides what the plugin may do.

    NOTHING HERE CAN AFFECT AUDIO, and that is a structural property rather
    than a promise: the audio thread never reads this class, and the only
    thing a verification result changes is two booleans the UI reads on the
    message thread. `State::audioAllowed()` is a compile-time `true` for the
    same reason - so that anyone looking for the code path that silences the
    plugin finds the constant instead of finding a bug.

    THE VERIFIER IS INJECTED. The real one will talk to Supabase over HTTPS
    (Phase 7); this class does not know that and must not, because the POLICY
    is what has to be right and the policy is testable only if the network can
    be replaced by a function that returns whatever a test wants - including
    "hangs", which is the case that matters most and the one that is
    impossible to arrange against a real server.

    THE CLOCK IS INJECTED TOO, for the same reason. A thirty-day grace period
    tested against the real clock is a test that takes thirty days.

    MESSAGE THREAD for everything public here. Verification itself runs on a
    background thread and comes back through an AsyncUpdater, which cancels
    its own pending callback on destruction - the same reasoning as
    preset/TableLoader.h.
*/
class LicenseManager : private juce::Thread,
                       private juce::AsyncUpdater
{
public:
    /** What a verifier returns. Deliberately not a bool: "the server said no"
        and "the server did not answer" lead to completely different
        behaviour, and a bool would force the caller to guess. */
    enum class Reply
    {
        valid,
        rejected,
        unreachable
    };

    /** Called on a BACKGROUND thread. Must not touch the UI or the audio
        thread, and must return within kTimeoutMs or be abandoned. */
    using Verifier = std::function<Reply()>;

    /** So the grace period can be tested without waiting a month. */
    using Clock = std::function<juce::Time()>;

    LicenseManager();
    ~LicenseManager() override;

    /** MESSAGE THREAD. Replaces the verifier. The default one reports
        `unreachable`, which is the honest answer before Phase 7 exists: there
        is no server yet, so the plugin behaves exactly as it would for a
        customer with no connection. */
    void setVerifier (Verifier verifier);

    void setClock (Clock clock);

    /** MESSAGE THREAD. Declares that this build does not check at all - see
        `Status::unenforced`. Publishes immediately, so the banner is right
        from the first frame rather than after a check that will never come.

        Separate from `setVerifier` on purpose: "there is no server to ask"
        is a property of the BUILD, and making it a verifier that returns
        some stand-in reply would put a fake licence decision in the same
        place real ones live. */
    void setUnenforced();

    /** MESSAGE THREAD. Called when the state changes, for the banner. */
    void setListener (std::function<void (const State&)> listener);

    /** MESSAGE THREAD. Starts a check. Returns immediately; the result
        arrives through the listener. A check already in flight is left
        alone rather than restarted - the answer is about to arrive anyway,
        and cancelling it would make a burst of calls slower than one. */
    void verify();

    /** MESSAGE THREAD. The current state. */
    State getState() const;

    /** MESSAGE THREAD. Restores what was saved, so a plugin that has been
        licensed and then opened offline knows it. */
    void restore (const juce::Time& lastVerified, bool everVerified);

    /** For persisting: the caller writes these wherever plugin settings go. */
    juce::Time getLastVerified() const;
    bool hasEverVerified() const;

    /** Recomputes the grace period against the clock, without contacting
        anything. Called when the editor opens, so a plugin left running for
        a month notices. */
    void refreshGrace();

    /** TESTS ONLY. Waits for an in-flight check and delivers its result
        synchronously.

        The result normally arrives through an AsyncUpdater, which needs the
        message loop to dispatch - and a plugin's test host does not run one.
        The alternative was for the tests to sleep and hope, which makes them
        pass or fail depending on the machine. This waits for the worker and
        then pumps the pending update, so a test asserts on a result that has
        definitely arrived. Returns false if nothing was pending. */
    bool waitForPendingCheck (int timeoutMs);

private:
    void run() override;
    void handleAsyncUpdate() override;

    void applyReply (Reply reply);
    void publish();

    mutable juce::CriticalSection stateLock;
    State state;

    Verifier verifier;
    Clock clock;
    std::function<void (const State&)> listener;

    juce::Time lastVerified;
    bool everVerified = false;

    std::atomic<bool> checking { false };
    std::atomic<int> pendingReply { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LicenseManager)
};

} // namespace gnarl::license
