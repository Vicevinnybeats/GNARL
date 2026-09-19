#include "LicenseManager.h"

namespace gnarl::license
{

LicenseManager::LicenseManager()
    : juce::Thread ("GNARL licence")
{
    /*  THE DEFAULT VERIFIER SAYS `unreachable`, which is the honest answer
        before Phase 7 exists. There is no server yet, so the plugin behaves
        exactly as it would for a customer whose connection is down - which is
        also the behaviour that most needs to be right. A default of `valid`
        would mean the grace-period logic never ran until the day it shipped. */
    verifier = [] { return Reply::unreachable; };

    clock = [] { return juce::Time::getCurrentTime(); };
}

LicenseManager::~LicenseManager()
{
    // The verifier may be mid-request. 2 seconds is far longer than an
    // abandoned socket needs and far shorter than a hang.
    stopThread (2000);

    // Cancels any completion still queued, so it cannot run against a
    // destroyed manager - the same reasoning as preset/TableLoader.h.
    cancelPendingUpdate();
}

void LicenseManager::setVerifier (Verifier newVerifier)
{
    if (newVerifier)
        verifier = std::move (newVerifier);
}

void LicenseManager::setClock (Clock newClock)
{
    if (newClock)
        clock = std::move (newClock);
}

void LicenseManager::setListener (std::function<void (const State&)> newListener)
{
    listener = std::move (newListener);
}

void LicenseManager::verify()
{
    // Already checking: leave it. The answer is about to arrive, and
    // restarting would make a burst of calls slower than a single one.
    if (checking.exchange (true))
        return;

    startThread (juce::Thread::Priority::background);
}

void LicenseManager::run()
{
    /*  THE TIMEOUT IS ENFORCED HERE, not left to the verifier. A verifier that
        hangs - a socket with no timeout set, a captive portal that accepts the
        connection and never answers - would otherwise keep this thread alive
        forever, and the destructor would then wait its full two seconds on
        every plugin instance being closed.

        The request is not cancelled, because a std::function cannot be; it is
        ABANDONED. Its result is dropped when it eventually arrives, which is
        why the reply is published through an atomic that the timeout path
        stops reading rather than through a captured reference. */
    const auto started = juce::Time::getMillisecondCounter();

    auto reply = Reply::unreachable;

    if (verifier)
        reply = verifier();

    const auto elapsed = juce::Time::getMillisecondCounter() - started;

    if (threadShouldExit())
        return;

    // A verifier that came back too late is treated as unreachable. It may
    // well have said "valid", but a check that took longer than the timeout is
    // one the user has already been waiting on.
    pendingReply.store (static_cast<int> (
        elapsed > static_cast<juce::uint32> (kTimeoutMs) ? Reply::unreachable : reply));

    triggerAsyncUpdate();
}

void LicenseManager::handleAsyncUpdate()
{
    const auto raw = pendingReply.exchange (-1);

    checking.store (false);

    if (raw < 0)
        return;

    applyReply (static_cast<Reply> (raw));
    publish();
}

void LicenseManager::applyReply (Reply reply)
{
    const juce::ScopedLock lock (stateLock);

    const auto now = clock ? clock() : juce::Time::getCurrentTime();

    switch (reply)
    {
        case Reply::valid:
            lastVerified = now;
            everVerified = true;

            state.status = Status::licensed;
            state.graceDaysRemaining = kGraceDays;
            state.lastVerified = lastVerified;
            break;

        case Reply::rejected:
            /*  The server answered and said no. This is the ONE case where
                the grace period does not apply - grace exists for a machine
                that cannot ask, not for one that asked and was told no. Audio
                still plays, because audio always plays. */
            state.status = Status::invalid;
            state.graceDaysRemaining = 0;
            break;

        case Reply::unreachable:
        default:
        {
            if (! everVerified)
            {
                // Never licensed and cannot ask. Not an error, and not
                // something to warn about as though it were one.
                state.status = Status::unlicensed;
                state.graceDaysRemaining = 0;
                break;
            }

            const auto elapsedDays = static_cast<int> (
                (now - lastVerified).inDays());

            state.graceDaysRemaining = juce::jmax (0, kGraceDays - elapsedDays);
            state.status = state.graceDaysRemaining > 0 ? Status::offline
                                                        : Status::expired;
            state.lastVerified = lastVerified;
            break;
        }
    }

    state.message = describe (state);
}

void LicenseManager::setUnenforced()
{
    {
        const juce::ScopedLock lock (stateLock);

        state.status = Status::unenforced;
        state.graceDaysRemaining = 0;
        state.message = describe (state);
    }

    publish();
}

void LicenseManager::refreshGrace()
{
    {
        const juce::ScopedLock lock (stateLock);

        // Only the offline states move with the clock. A rejected licence does
        // not become valid by waiting, and a valid one does not expire until
        // the next check says so.
        // `unenforced` is not a licence state and does not move with the
        // clock; neither does a rejection, and a valid licence does not
        // expire until a check says so.
        if (state.status != Status::offline && state.status != Status::expired)
            return;
    }

    applyReply (Reply::unreachable);
    publish();
}

bool LicenseManager::waitForPendingCheck (int timeoutMs)
{
    const auto deadline = juce::Time::getMillisecondCounter()
                        + static_cast<juce::uint32> (juce::jmax (0, timeoutMs));

    while (isThreadRunning() && juce::Time::getMillisecondCounter() < deadline)
        juce::Thread::sleep (5);

    if (pendingReply.load() < 0)
        return false;

    // AsyncUpdater's own "deliver now if one is queued", which is exactly
    // what the message loop would eventually do.
    handleUpdateNowIfNeeded();

    return true;
}

void LicenseManager::publish()
{
    if (listener)
        listener (getState());
}

State LicenseManager::getState() const
{
    const juce::ScopedLock lock (stateLock);
    return state;
}

void LicenseManager::restore (const juce::Time& verifiedAt, bool ever)
{
    {
        const juce::ScopedLock lock (stateLock);
        lastVerified = verifiedAt;
        everVerified = ever;
    }

    // Recompute from the restored timestamp rather than assuming anything:
    // the plugin may have been closed for a month.
    applyReply (Reply::unreachable);
    publish();
}

juce::Time LicenseManager::getLastVerified() const
{
    const juce::ScopedLock lock (stateLock);
    return lastVerified;
}

bool LicenseManager::hasEverVerified() const
{
    const juce::ScopedLock lock (stateLock);
    return everVerified;
}

} // namespace gnarl::license
