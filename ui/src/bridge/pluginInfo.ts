/**
 * Values handed over by WebUIEditor::makeWebOptions at page load.
 *
 * These arrive on `window.__JUCE__.initialisationData`, not as a module
 * export — JUCE's JS library exposes only the control-state factories.
 */
export interface PluginInfo {
  pluginVersion: string;
  stateVersion: number;
  /** Platform string the C++ side reported, e.g. "mac" or "windows". */
  platform: string;
  /** True when the page is running in a browser with no plugin behind it. */
  isMock: boolean;
}

export function getPluginInfo(): PluginInfo {
  const data = window.__JUCE__?.initialisationData;

  // An empty __juce__platform is how JUCE's browser stand-in identifies
  // itself: the real WebBrowserComponent always reports a platform.
  const platform = data?.__juce__platform?.[0] ?? '';

  return {
    pluginVersion: String(data?.pluginVersion ?? 'unknown'),
    stateVersion: Number(data?.stateVersion ?? 0),
    platform,
    isMock: platform.length === 0,
  };
}
