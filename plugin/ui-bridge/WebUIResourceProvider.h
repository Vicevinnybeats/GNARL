#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <optional>

namespace gnarl
{

/**
    Serves the Vite-built React bundle out of the plugin binary.

    The UI is embedded as BinaryData rather than written to disk: a plugin that
    unpacks its own interface into a temp folder is a plugin that breaks under
    sandboxed hosts and gets flagged by antivirus.

    All calls happen on the message thread, from inside WebBrowserComponent.
*/
class WebUIResourceProvider
{
public:
    /** Returns the embedded resource for a request path such as "/" or
        "/assets/index.js", or nullopt if there is no such resource. */
    static std::optional<juce::WebBrowserComponent::Resource> get (const juce::String& path);

    /** The origin the WebBrowserComponent serves the embedded UI from. */
    static juce::String getOrigin();

private:
    static juce::String mimeTypeFor (const juce::String& path);
};

} // namespace gnarl
