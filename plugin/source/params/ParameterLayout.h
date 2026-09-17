#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace gnarl::params
{

/**
    Builds GNARL's complete AudioProcessorValueTreeState layout.

    The WHOLE layout is declared here in Phase 1, including parameters whose
    features do not land until Phase 4. This is deliberate: a parameter added
    after release cannot be inserted without breaking preset compatibility, so
    there is no opportunity to add one later.

    Parameter creation ORDER is also frozen. It sets the index a host shows in
    its automation list, and reordering would move every lane a customer has
    already drawn.
*/
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Number of parameters the layout declares. Used by tests to catch an
    accidental addition or removal. */
int getDeclaredParameterCount();

} // namespace gnarl::params
