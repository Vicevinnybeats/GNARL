#pragma once

#include <juce_core/juce_core.h>

namespace gnarl::license
{

/**
    What the plugin knows about its licence, and what it is allowed to do.

    THE RULES HERE ARE FROM CLAUDE.md SECTION 9 AND ARE NOT NEGOTIABLE:

      - **The licence check never silences the plugin.** If verification fails,
        for any reason, audio keeps playing. Preset saving and the AI features
        disable and a banner appears, and that is the whole penalty.
      - **Thirty days of offline grace.** A producer in a studio with no wifi
        must not be locked out mid-take.
      - Verification runs on a background thread with a timeout, and never
        touches the audio thread or blocks the UI.

    WHY A SEPARATE STATE TYPE RATHER THAN A BOOL. "Licensed" is not one bit: a
    plugin can be activated and offline, activated and expired, never
    activated, or unreachable-and-within-grace, and those want different words
    in the banner and different behaviour from the feature gates. Collapsing
    them to a bool is how a product ends up telling a paying customer with a
    flaky connection that their licence is invalid.

    Trivially copyable, so the audio thread could read one if it ever needed
    to - it does not, because nothing here changes what comes out of
    processBlock. That is the point.
*/
enum class Status
{
    /** No licence has ever been activated on this machine. */
    unlicensed,

    /** Verified against the server within the grace period. */
    licensed,

    /** Verified once, but the server has not been reachable since. Still
        fully licensed - this is what the grace period IS. */
    offline,

    /** Verified once, and the grace period has now run out. */
    expired,

    /** The server answered and said no. */
    invalid
};

/** How many days offline before the grace period ends. NOT NEGOTIABLE (§9). */
inline constexpr int kGraceDays = 30;

/** How long a verification attempt may take before it is abandoned. A check
    that hangs is a check that would block whatever is waiting on it, and
    nothing here is worth waiting on. */
inline constexpr int kTimeoutMs = 8000;

struct State
{
    Status status = Status::unlicensed;

    /** Days left in the grace period. Only meaningful when offline. */
    int graceDaysRemaining = kGraceDays;

    /** When the server last confirmed the licence, or an invalid Time if it
        never has. */
    juce::Time lastVerified;

    /** For the banner. Empty when there is nothing to say. */
    juce::String message;

    /** AUDIO IS ALWAYS ALLOWED. This function exists to be read, not to be
        called: there is no code path that asks whether it may make sound,
        and if one ever appears this is what it should find. */
    static constexpr bool audioAllowed() noexcept { return true; }

    /** Saving presets and the AI features are the only things gated. */
    bool featuresAllowed() const noexcept
    {
        return status == Status::licensed || status == Status::offline;
    }

    /** True when the banner should be up. */
    bool shouldWarn() const noexcept
    {
        return status != Status::licensed;
    }
};

/** A human sentence for the banner. Kept here rather than in the UI so the
    wording is versioned with the policy it describes. */
juce::String describe (const State& state);

} // namespace gnarl::license
