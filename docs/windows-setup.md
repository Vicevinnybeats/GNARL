# Windows build setup

GNARL's interface is a React bundle running inside `juce::WebBrowserComponent`.
On macOS and Linux that is the system WebKit and needs nothing extra. **On
Windows it requires the Microsoft Edge WebView2 SDK, and this is not
optional.**

## Why it is mandatory

Without `JUCE_USE_WIN_WEBVIEW2`, JUCE:

1. compiles the entire native-integration API out of
   `WebBrowserComponent::Options` — no `withResourceProvider`, no
   `withNativeIntegrationEnabled`, no parameter relays, so `plugin/ui-bridge`
   does not even build; and
2. falls back to the **Internet Explorer** engine at runtime, which cannot run
   a modern ES2022 React bundle at all.

So this is not a nice-to-have for a nicer browser. Without it there is no
Windows UI.

GNARL links WebView2 **statically**
(`JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1`), so the shipped VST3 has no
`WebView2Loader.dll` beside it for a user to misplace. The static library ships
only in the NuGet package, which is why the package is required rather than
vcpkg.

Note that the WebView2 **runtime** is a separate thing from this SDK. It ships
with Windows 11 and current Windows 10, so end users generally already have it;
the installer should still check for it (Phase 9).

## Install the SDK

In PowerShell:

```powershell
Register-PackageSource -provider NuGet -name nugetRepository -location https://www.nuget.org/api/v2
Install-Package Microsoft.Web.WebView2 -Scope CurrentUser -Source nugetRepository
```

JUCE's `FindWebView2.cmake` then finds it automatically under
`%USERPROFILE%\AppData\Local\PackageManagement\NuGet\Packages`.

If it is installed somewhere else, point CMake at the directory that *contains*
the `Microsoft.Web.WebView2.*` folder:

```powershell
cmake -B build -DJUCE_WEBVIEW2_PACKAGE_LOCATION="C:/path/to/packages"
```

CI does exactly that: it runs `nuget install` into `nuget/` and passes that
path. The version is pinned in `.github/workflows/build.yml`
(`WEBVIEW2_VERSION`), because an unpinned SDK is an unreproducible build.

## Symptoms of a missing SDK

| Symptom | Cause |
|---|---|
| `error C2039: 'withResourceProvider': is not a member of 'juce::WebBrowserComponent::Options'` | `JUCE_USE_WIN_WEBVIEW2` is off — the SDK was not found at configure time |
| `WebView2 wasn't found in the local NuGet folder` at configure time | package not installed, or `JUCE_WEBVIEW2_PACKAGE_LOCATION` points at the wrong level |
| Plugin loads but the window is blank or shows a script error | the SDK was found, but the WebView2 **runtime** is missing on that machine |
| `error C1083: Cannot open include file: 'WebView2.h'` while building **GnarlTests** | the plugin found the SDK but the test target did not. `juce_add_plugin` links `juce::juce_webview2` privately, so anything that merely links the `GNARL` target inherits the defines without the include paths — `tests/CMakeLists.txt` links it explicitly for this reason |
