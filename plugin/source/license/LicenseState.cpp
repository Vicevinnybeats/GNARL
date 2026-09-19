#include "LicenseState.h"

namespace gnarl::license
{

juce::String describe (const State& state)
{
    switch (state.status)
    {
        case Status::licensed:
            return {};

        case Status::offline:
            /*  Says how long is left, and does NOT say anything is wrong,
                because nothing is: the licence is valid and the machine is
                offline. A warning here would train people to ignore the
                banner. */
            return "Offline - licence re-checks when you reconnect ("
                 + juce::String (state.graceDaysRemaining)
                 + (state.graceDaysRemaining == 1 ? " day" : " days") + " left).";

        case Status::expired:
            // Names what still works first. Someone reading this is worried
            // about losing a session, and the first clause answers that.
            return "Audio still works. Connect to the internet to restore "
                   "preset saving and the AI features.";

        case Status::invalid:
            return "This licence could not be verified. Audio still works; "
                   "preset saving and the AI features are disabled.";

        case Status::unenforced:
            /*  Deliberately conspicuous. A development build saying nothing
                is a development build that gets shipped. */
            return "Development build - licence checking is not configured.";

        case Status::unlicensed:
        default:
            return "Unlicensed - audio works, preset saving and the AI "
                   "features are disabled.";
    }
}

} // namespace gnarl::license
