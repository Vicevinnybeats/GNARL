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
    const char* paths[] = { "/index.html", "/assets/index.js", "/assets/index.css" };

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
