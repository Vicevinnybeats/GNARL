#include <catch2/catch_session.hpp>

#include <juce_events/juce_events.h>

/**
    Custom Catch2 main.

    JUCE needs its message manager and singletons initialised before any
    AudioProcessor is constructed, and shut down cleanly afterwards, or the
    leak detector fires on perfectly healthy code at exit.
*/
int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    return Catch::Session().run (argc, argv);
}
