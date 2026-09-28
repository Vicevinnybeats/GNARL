#include <string_view>
#include <catch2/catch_test_macros.hpp>

#include "WebUIResourceProvider.h"

#include <string>

using namespace gnarl;

/*
    These tests guard a silent failure mode.

    juce_add_binary_data mangles file paths into C identifiers
    ("assets/index.js" -> "index_js"). WebUIResourceProvider has to reproduce
    that mangling exactly. If it drifts, the plugin still builds, still loads,
    and simply shows a blank white window — with nothing in any log to say why.
*/

namespace
{
    std::string mimeOf (const juce::String& path)
    {
        auto resource = WebUIResourceProvider::get (path);
        REQUIRE (resource.has_value());
        return resource->mimeType.toStdString();
    }
}

TEST_CASE ("Every embedded UI file resolves", "[webui]")
{
    // Must match GNARL_UI_FILES in cmake/WebUI.cmake and the output names in
    // ui/vite.config.ts.
    const char* paths[] = { "/index.html", "/assets/index.js", "/assets/index.css",
                            "/assets/backdrop.webp" };

    for (const auto* path : paths)
    {
        auto resource = WebUIResourceProvider::get (path);

        REQUIRE (resource.has_value());
        CHECK (resource->data.size() > 0);
        CHECK (resource->mimeType.isNotEmpty());
    }
}

TEST_CASE ("The root path serves index.html", "[webui]")
{
    auto root = WebUIResourceProvider::get ("/");
    auto index = WebUIResourceProvider::get ("/index.html");

    REQUIRE (root.has_value());
    REQUIRE (index.has_value());
    CHECK (root->data == index->data);
}

TEST_CASE ("Query strings and fragments are stripped", "[webui]")
{
    auto plain = WebUIResourceProvider::get ("/assets/index.js");
    auto withQuery = WebUIResourceProvider::get ("/assets/index.js?v=2");
    auto withHash = WebUIResourceProvider::get ("/assets/index.js#top");

    REQUIRE (plain.has_value());
    REQUIRE (withQuery.has_value());
    REQUIRE (withHash.has_value());
    CHECK (withQuery->data.size() == plain->data.size());
    CHECK (withHash->data.size() == plain->data.size());
}

TEST_CASE ("MIME types are correct for the embedded files", "[webui]")
{
    // A wrong MIME type on the JS bundle makes the browser refuse to execute
    // it as a module, which again presents as a blank window.
    CHECK (mimeOf ("/index.html").find ("text/html") == 0);
    CHECK (mimeOf ("/assets/index.js").find ("text/javascript") == 0);
    CHECK (mimeOf ("/assets/index.css").find ("text/css") == 0);
}

TEST_CASE ("An extensionless unknown path falls back to index.html", "[webui]")
{
    // Client-side routing: a deep link must still boot the app.
    auto resource = WebUIResourceProvider::get ("/some/spa/route");
    auto index = WebUIResourceProvider::get ("/index.html");

    REQUIRE (resource.has_value());
    REQUIRE (index.has_value());
    CHECK (resource->data == index->data);
}

TEST_CASE ("A missing file with an extension is reported as missing", "[webui]")
{
    // Returning index.html here instead would serve HTML where the page asked
    // for a script, which is harder to debug than an honest 404.
    CHECK_FALSE (WebUIResourceProvider::get ("/assets/nope.js").has_value());
    CHECK_FALSE (WebUIResourceProvider::get ("/missing.png").has_value());
}

TEST_CASE ("The resource provider origin is a usable URL", "[webui]")
{
    const auto origin = WebUIResourceProvider::getOrigin();

    CHECK (origin.isNotEmpty());
    CHECK (juce::URL (origin).getOrigin().isNotEmpty());
}

TEST_CASE ("The artwork slot resolves as an image", "[webui]")
{
    /*  The background artwork is embedded like any other asset, and it is in
        the resource list unconditionally - the slot ships a 1x1 transparent
        placeholder so a build with no artwork commissioned yet still
        configures and still loads.

        Served with the wrong MIME type it simply never paints, and the UI
        falls back to its gradient without complaining anywhere. That is
        exactly the silent failure these tests exist for.
    */
    auto resource = WebUIResourceProvider::get ("/assets/backdrop.webp");

    REQUIRE (resource.has_value());
    CHECK (resource->data.size() > 0);
    CHECK (resource->mimeType.toStdString() == "image/webp");
}

TEST_CASE ("The image MIME types cover the formats artwork may ship as",
           "[webui]")
{
    // docs/artwork-brief.md tells whoever replaces the artwork that these
    // formats are already served, so the claim has to stay true.
    CHECK (WebUIResourceProvider::mimeTypeFor ("a.png").toStdString() == "image/png");
    CHECK (WebUIResourceProvider::mimeTypeFor ("a.jpg").toStdString() == "image/jpeg");
    CHECK (WebUIResourceProvider::mimeTypeFor ("a.jpeg").toStdString() == "image/jpeg");
    CHECK (WebUIResourceProvider::mimeTypeFor ("a.webp").toStdString() == "image/webp");
    CHECK (WebUIResourceProvider::mimeTypeFor ("a.avif").toStdString() == "image/avif");
}

/*  THE EMBEDDED BUNDLE IS THE REAL ONE, NOT THE DEV-MODE FALLBACK.

    JUCE's JavaScript frontend is not on npm; it lives inside the JUCE
    checkout and `ui/scripts/sync-juce-frontend.mjs` copies it out at build
    time. With no checkout present the script writes a FALLBACK instead -
    in-memory parameter state, no native backend - so that the UI still
    builds and runs in a plain browser while somebody works on components.

    A binary built against that fallback is the worst failure this project
    can produce, because it is not a crash and not a blank window: the plugin
    loads, draws its entire interface, responds to every click, and is
    completely disconnected from the audio engine. Presets do not load, the
    FX toggles do nothing, and every patch sounds identical - four symptoms
    that look like four bugs and are one.

    It shipped. Every release up to v0.1.3 embedded the fallback, because CI
    built the web bundle on a runner that had never configured CMake, and
    nothing anywhere asserted otherwise. CLAUDE.md said of the fallback "it
    is never what ships"; a comment is not a check.

    SO THE ASSERTION IS ON THE EMBEDDED BYTES, not on the build that produced
    them. Anything earlier in the chain - a CI step, a CMake condition - can
    be bypassed by building another way, and this has to hold for every
    binary however it was made.

    Both directions are checked. "Does not contain the fallback marker"
    passes trivially on an empty or truncated bundle, so the real backend's
    own symbols have to be present as well. */
TEST_CASE ("The embedded UI bundle has a native backend", "[webui]")
{
    auto bundle = WebUIResourceProvider::get ("/assets/index.js");

    REQUIRE (bundle.has_value());
    REQUIRE (bundle->data.size() > 0);

    /*  A string_view over the bytes, NOT a juce::String built from them. The
        bundle is ~680 KB of minified JavaScript that is not null-terminated
        and is not guaranteed to be valid UTF-8 at an arbitrary cut point;
        constructing a juce::String from it trips an assertion inside
        juce_String.cpp and copies the whole thing to do it. A view searches
        the bytes in place, which is what the question actually is. */
    const std::string_view source (reinterpret_cast<const char*> (bundle->data.data()),
                                   bundle->data.size());

    const auto holds = [&source] (std::string_view needle)
    {
        return source.find (needle) != std::string_view::npos;
    };

    /*  The fallback's own warning text, which it logs from every native call
        it cannot make. Matching on this rather than on its absence of
        features is deliberate: it is a string the real implementation has no
        reason to contain, so it cannot drift into a false pass. */
    CHECK_FALSE (holds ("called with no plugin backend"));

    /*  And the positive case. Vite mangles local names but not the property
        names it reads off the injected JUCE global, so these survive
        minification. */
    CHECK ((holds ("getNativeFunction") || holds ("__JUCE__")));
}
