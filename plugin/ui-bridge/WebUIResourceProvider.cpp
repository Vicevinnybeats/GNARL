#include "WebUIResourceProvider.h"

#include <GnarlWebUIData.h>

namespace gnarl
{

namespace
{
    /** Maps a request path to the BinaryData symbol name Vite/JUCE produced.
        juce_add_binary_data mangles paths: "assets/index.js" -> "index_js". */
    juce::String binaryDataNameFor (const juce::String& path)
    {
        auto clean = path.startsWith ("/") ? path.substring (1) : path;

        if (clean.isEmpty())
            clean = "index.html";

        // Strip any query string or fragment the webview appended.
        clean = clean.upToFirstOccurrenceOf ("?", false, false)
                     .upToFirstOccurrenceOf ("#", false, false);

        return clean.fromLastOccurrenceOf ("/", false, false)
                    .replaceCharacter ('.', '_')
                    .replaceCharacter ('-', '_');
    }
}

juce::String WebUIResourceProvider::getOrigin()
{
    // A localhost origin keeps the page in a normal secure-ish browsing context,
    // which fetch(), WebGL and ES modules all need. Nothing actually listens on
    // this port - JUCE intercepts the requests.
    return juce::WebBrowserComponent::getResourceProviderRoot();
}

juce::String WebUIResourceProvider::mimeTypeFor (const juce::String& path)
{
    if (path.endsWithIgnoreCase (".html")) return "text/html; charset=utf-8";
    if (path.endsWithIgnoreCase (".js")
     || path.endsWithIgnoreCase (".mjs"))  return "text/javascript; charset=utf-8";
    if (path.endsWithIgnoreCase (".css"))  return "text/css; charset=utf-8";
    if (path.endsWithIgnoreCase (".json")) return "application/json; charset=utf-8";
    if (path.endsWithIgnoreCase (".svg"))  return "image/svg+xml";
    if (path.endsWithIgnoreCase (".png"))  return "image/png";
    if (path.endsWithIgnoreCase (".woff2"))return "font/woff2";
    if (path.endsWithIgnoreCase (".wasm")) return "application/wasm";

    return "application/octet-stream";
}

std::optional<juce::WebBrowserComponent::Resource>
WebUIResourceProvider::get (const juce::String& path)
{
    const auto name = binaryDataNameFor (path);

    int size = 0;
    if (const auto* data = GnarlWebUI::getNamedResource (name.toRawUTF8(), size))
    {
        const auto* bytes = reinterpret_cast<const std::byte*> (data);

        return juce::WebBrowserComponent::Resource {
            std::vector<std::byte> (bytes, bytes + size),
            mimeTypeFor (name.replaceCharacter ('_', '.'))
        };
    }

    // Unknown path: fall through to index.html so client-side routing works.
    if (! path.containsChar ('.'))
        return get ("/index.html");

    return std::nullopt;
}

} // namespace gnarl
