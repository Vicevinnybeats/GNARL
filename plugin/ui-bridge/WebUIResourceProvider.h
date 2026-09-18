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

    /** The MIME type for a path's extension.

        Public because it is a pure function worth testing directly: a wrong
        MIME type is one of the two ways this class fails SILENTLY - the
        browser refuses to execute a bundle served as octet-stream, or simply
        never paints an image, and neither writes anything to any log. Testing
        it through get() would only cover the extensions that happen to be
        embedded today. */
    static juce::String mimeTypeFor (const juce::String& path);
};

} // namespace gnarl
