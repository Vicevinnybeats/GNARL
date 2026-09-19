#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"
#include "license/LicenseManager.h"

#include <atomic>

using namespace gnarl;
using namespace gnarl::license;

/*
    The licence policy from CLAUDE.md section 9.

    EVERY RULE HERE IS A PROMISE TO A CUSTOMER rather than an implementation
    detail, which is why they are tested as rules and not as code paths. The
    one that matters most - audio never stops - is the one that is hardest to
    test by poking at an implementation and easiest to state directly.

    The verifier and the clock are both injected, and that is what makes this
    testable at all: "the server hangs" cannot be arranged against a real
    server, and a thirty-day grace period tested against the real clock is a
    test that takes thirty days.
*/

namespace
{
    struct Harness
    {
        LicenseManager manager;
        std::atomic<int> notifications { 0 };
        State latest;

        juce::Time now { juce::Time::getCurrentTime() };

        Harness()
        {
            manager.setClock ([this] { return now; });
            manager.setListener ([this] (const State& state)
            {
                latest = state;
                notifications.fetch_add (1);
            });
        }

        void answer (LicenseManager::Reply reply)
        {
            manager.setVerifier ([reply] { return reply; });
        }

        /*  Delivers the result synchronously rather than sleeping and hoping.
            The manager normally publishes through an AsyncUpdater, which needs
            a message loop to dispatch, and a plugin test host does not run
            one - so a test that slept would pass or fail depending on the
            machine. */
        bool check (int expected)
        {
            manager.verify();
            manager.waitForPendingCheck (15000);

            return notifications.load() >= expected;
        }
    };
}

TEST_CASE ("Audio is allowed in every licence state", "[license]")
{
    /*  THE RULE THE WHOLE DESIGN EXISTS FOR (CLAUDE.md section 9): the licence
        check never silences the plugin. Stated as a compile-time constant so
        that anyone looking for the code path that stops the audio finds this
        instead of finding a bug. */
    static_assert (State::audioAllowed(), "audio must never be gated on a licence");

    for (const auto status : { Status::unlicensed, Status::licensed,
                               Status::offline, Status::expired,
                               Status::invalid })
    {
        State state;
        state.status = status;

        CHECK (state.audioAllowed());
    }
}

TEST_CASE ("Only preset saving and the AI features are gated", "[license]")
{
    const auto allowed = [] (Status status)
    {
        State state;
        state.status = status;
        return state.featuresAllowed();
    };

    CHECK (allowed (Status::licensed));

    // OFFLINE IS FULLY LICENSED. That is what the grace period is: a producer
    // in a studio with no wifi keeps working.
    CHECK (allowed (Status::offline));

    CHECK_FALSE (allowed (Status::unlicensed));
    CHECK_FALSE (allowed (Status::expired));
    CHECK_FALSE (allowed (Status::invalid));
}

TEST_CASE ("A valid reply licenses the plugin", "[license]")
{
    Harness harness;
    harness.answer (LicenseManager::Reply::valid);

    REQUIRE (harness.check (1));

    CHECK (harness.latest.status == Status::licensed);
    CHECK (harness.latest.featuresAllowed());
    CHECK_FALSE (harness.latest.shouldWarn());

    // Nothing to say when everything is fine. A banner that is always up is a
    // banner nobody reads.
    CHECK (harness.latest.message.isEmpty());
}

TEST_CASE ("An unreachable server does not revoke a licence", "[license]")
{
    /*  THE CASE THIS IS ALL FOR. The plugin was licensed, the connection is
        gone, and the customer is in the middle of a take. */
    Harness harness;

    harness.answer (LicenseManager::Reply::valid);
    REQUIRE (harness.check (1));

    harness.answer (LicenseManager::Reply::unreachable);
    REQUIRE (harness.check (2));

    CHECK (harness.latest.status == Status::offline);
    CHECK (harness.latest.featuresAllowed());
    CHECK (harness.latest.graceDaysRemaining == kGraceDays);

    // The message says how long is left and does NOT suggest anything is
    // wrong, because nothing is.
    CHECK (harness.latest.message.contains ("Offline"));
}

TEST_CASE ("The grace period is thirty days and counts down", "[license]")
{
    Harness harness;

    harness.answer (LicenseManager::Reply::valid);
    REQUIRE (harness.check (1));

    /*  Anchor every case to the moment the licence was actually verified,
        absolutely. Advancing the clock by a difference computed from the
        previous iteration compounds the flooring in `inDays()` and lands a
        day or two short of the day being named - which is exactly the kind
        of off-by-one the day-30 boundary case exists to catch. */
    const auto verifiedAt = harness.manager.getLastVerified();

    harness.answer (LicenseManager::Reply::unreachable);

    // One real check to move out of `licensed`: `refreshGrace` deliberately
    // does nothing to a licensed state, because waiting does not expire a
    // licence - only a check that cannot reach the server does.
    REQUIRE (harness.check (2));

    struct Case { int daysLater; int expectedRemaining; Status expected; };

    for (const auto& testCase : { Case { 0,  kGraceDays,      Status::offline },
                                  Case { 1,  kGraceDays - 1,  Status::offline },
                                  Case { 15, kGraceDays - 15, Status::offline },
                                  Case { 29, 1,               Status::offline },
                                  // Day 30 is when it runs out, not day 31.
                                  Case { 30, 0,               Status::expired },
                                  Case { 90, 0,               Status::expired } })
    {
        harness.now = verifiedAt + juce::RelativeTime::days (testCase.daysLater);

        harness.manager.refreshGrace();

        INFO ("after " << testCase.daysLater << " days offline");
        CHECK (harness.latest.graceDaysRemaining == testCase.expectedRemaining);
        CHECK (harness.latest.status == testCase.expected);

        // AND AUDIO STILL WORKS, on every single one of those days.
        CHECK (harness.latest.audioAllowed());
    }
}

TEST_CASE ("An expired grace period disables features and nothing else",
           "[license]")
{
    Harness harness;

    harness.answer (LicenseManager::Reply::valid);
    REQUIRE (harness.check (1));

    harness.now = harness.now + juce::RelativeTime::days (45);
    harness.answer (LicenseManager::Reply::unreachable);
    REQUIRE (harness.check (2));

    CHECK (harness.latest.status == Status::expired);
    CHECK_FALSE (harness.latest.featuresAllowed());
    CHECK (harness.latest.audioAllowed());

    // The message names what still works FIRST. Somebody reading it is
    // worried about losing a session.
    CHECK (harness.latest.message.startsWith ("Audio still works"));
}

TEST_CASE ("A rejection does not get a grace period", "[license]")
{
    /*  Grace exists for a machine that cannot ASK, not for one that asked and
        was told no. This is the one case where the thirty days do not apply -
        and audio still plays, because audio always plays. */
    Harness harness;

    harness.answer (LicenseManager::Reply::valid);
    REQUIRE (harness.check (1));

    harness.answer (LicenseManager::Reply::rejected);
    REQUIRE (harness.check (2));

    CHECK (harness.latest.status == Status::invalid);
    CHECK (harness.latest.graceDaysRemaining == 0);
    CHECK_FALSE (harness.latest.featuresAllowed());
    CHECK (harness.latest.audioAllowed());
}

TEST_CASE ("Reconnecting restores a licence that had expired", "[license]")
{
    Harness harness;

    harness.answer (LicenseManager::Reply::valid);
    REQUIRE (harness.check (1));

    harness.now = harness.now + juce::RelativeTime::days (60);
    harness.answer (LicenseManager::Reply::unreachable);
    REQUIRE (harness.check (2));
    REQUIRE (harness.latest.status == Status::expired);

    // Back online. The grace period resets in full - it is thirty days from
    // the last successful check, not thirty days ever.
    harness.answer (LicenseManager::Reply::valid);
    REQUIRE (harness.check (3));

    CHECK (harness.latest.status == Status::licensed);
    CHECK (harness.latest.graceDaysRemaining == kGraceDays);
    CHECK (harness.latest.featuresAllowed());
}

TEST_CASE ("A plugin that has never been licensed is not in grace",
           "[license]")
{
    // Grace is for a licence that exists. Handing thirty days to a machine
    // that has never activated would be handing out a free month.
    Harness harness;

    harness.answer (LicenseManager::Reply::unreachable);
    REQUIRE (harness.check (1));

    CHECK (harness.latest.status == Status::unlicensed);
    CHECK (harness.latest.graceDaysRemaining == 0);
    CHECK_FALSE (harness.latest.featuresAllowed());
    CHECK (harness.latest.audioAllowed());
}

TEST_CASE ("A verifier that hangs is abandoned, not waited on", "[license]")
{
    /*  THE CASE THAT CANNOT BE ARRANGED AGAINST A REAL SERVER, and the reason
        the verifier is injected at all. A captive portal that accepts the
        connection and never answers, or a socket with no timeout set, would
        otherwise keep the thread alive indefinitely - and then every plugin
        instance being closed would wait on it.

        The verifier here takes longer than kTimeoutMs, and the manager must
        treat the answer as unreachable however cheerful it eventually is. */
    Harness harness;

    harness.answer (LicenseManager::Reply::valid);
    REQUIRE (harness.check (1));
    REQUIRE (harness.latest.status == Status::licensed);

    harness.manager.setVerifier ([]
    {
        juce::Thread::sleep (kTimeoutMs + 200);
        return LicenseManager::Reply::valid;
    });

    REQUIRE (harness.check (2));

    // Late "valid" is not valid: the user has already been waiting longer
    // than the timeout allows.
    CHECK (harness.latest.status == Status::offline);
    CHECK (harness.latest.featuresAllowed());
}

TEST_CASE ("Restoring a saved timestamp recomputes the grace period",
           "[license]")
{
    // A plugin that was licensed, then closed for six weeks, then opened
    // offline has to know it is expired without being told by a server.
    Harness harness;

    const auto sixWeeksAgo = harness.now - juce::RelativeTime::days (42);

    harness.manager.restore (sixWeeksAgo, true);

    CHECK (harness.latest.status == Status::expired);
    CHECK (harness.latest.audioAllowed());

    // And one closed for a week is still inside it.
    Harness fresh;
    fresh.manager.restore (fresh.now - juce::RelativeTime::days (7), true);

    CHECK (fresh.latest.status == Status::offline);
    CHECK (fresh.latest.graceDaysRemaining == kGraceDays - 7);
    CHECK (fresh.latest.featuresAllowed());
}

TEST_CASE ("Overlapping checks do not pile up", "[license]")
{
    // Clicking a retry button repeatedly should not start ten threads.
    Harness harness;

    std::atomic<int> calls { 0 };

    harness.manager.setVerifier ([&calls]
    {
        calls.fetch_add (1);
        juce::Thread::sleep (50);
        return LicenseManager::Reply::valid;
    });

    for (int i = 0; i < 10; ++i)
        harness.manager.verify();

    REQUIRE (harness.manager.waitForPendingCheck (15000));

    INFO ("the verifier was called " << calls.load() << " times for ten requests");
    CHECK (calls.load() < 10);
}

TEST_CASE ("An unconfigured build does not check, and says so", "[license]")
{
    /*  `unenforced` is NOT a licence state. It is what a build with no
        activation endpoint reports, and it exists so that such a build
        cannot quietly claim to be `licensed`.

        Features stay on, because disabling preset saving in a build nobody
        can activate would make the plugin untestable and prove nothing. The
        banner is what carries the difference - and `shouldWarn` is true, so
        there is always one. */
    Harness harness;

    harness.manager.setUnenforced();

    CHECK (harness.latest.status == Status::unenforced);
    CHECK (harness.latest.featuresAllowed());
    CHECK (harness.latest.audioAllowed());

    // Conspicuous on purpose. A development build that says nothing is a
    // development build that gets shipped.
    CHECK (harness.latest.shouldWarn());
    CHECK (harness.latest.message.containsIgnoreCase ("development build"));
}

TEST_CASE ("Waiting does not turn an unconfigured build into a licence", "[license]")
{
    /*  `refreshGrace` walks the clock forward for the OFFLINE states. An
        unenforced build is not one of them and must not be dragged into the
        grace machinery - the grace period is thirty days of a real licence,
        not thirty days of not having checked. */
    Harness harness;

    harness.manager.setUnenforced();

    harness.now = harness.now + juce::RelativeTime::days (400);
    harness.manager.refreshGrace();

    CHECK (harness.latest.status == Status::unenforced);
    CHECK (harness.latest.featuresAllowed());
}

TEST_CASE ("A real check overrides an unenforced build", "[license]")
{
    // Order must not matter: configuring an endpoint and getting an answer
    // decides, whatever the build reported before it.
    Harness harness;

    harness.manager.setUnenforced();
    REQUIRE (harness.latest.status == Status::unenforced);

    harness.answer (LicenseManager::Reply::rejected);
    REQUIRE (harness.check (2));

    CHECK (harness.latest.status == Status::invalid);
    CHECK_FALSE (harness.latest.featuresAllowed());

    // And audio still plays, which is the point of the whole design.
    CHECK (harness.latest.audioAllowed());
}

TEST_CASE ("The processor reports a licence without being asked to", "[license]")
{
    /*  Integration: the manager is wired into the plugin, not merely
        present. A policy that is correct and connected to nothing is the
        failure this whole file would otherwise miss - the same shape as a
        synth whose every unit test passes and which makes no sound.

        This build has no `GNARL_LICENCE_ENDPOINT`, so the expected answer is
        `unenforced`: features on, banner up, nothing pretending to be a
        verified licence. A build WITH an endpoint must never land here, and
        the assertion below is written so that it fails loudly if one does. */
    GnarlProcessor processor;

    const auto state = processor.getLicense().getState();

    if (juce::String (GNARL_LICENCE_ENDPOINT).isEmpty())
    {
        CHECK (state.status == Status::unenforced);
        CHECK (state.featuresAllowed());
        CHECK (state.shouldWarn());
    }
    else
    {
        CHECK (state.status != Status::unenforced);
    }

    // Whatever the build, and whatever the answer.
    CHECK (state.audioAllowed());
}
