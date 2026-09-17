/**
 * The `window.__JUCE__` object injected by juce::WebBrowserComponent when
 * native integration is enabled.
 *
 * This is NOT part of JUCE's JS module surface — `initialisationData` and the
 * backend event bus are only reachable through this global. JUCE's
 * check_native_interop.js installs a no-op stand-in when the page runs in a
 * plain browser, so reads are safe either way.
 */

/** Opaque handle returned by Backend.addEventListener. */
export type JuceEventToken = [string | null, number];

export interface JuceBackend {
  addEventListener(
    eventId: string,
    fn: (payload: unknown) => void,
  ): JuceEventToken;
  removeEventListener(token: JuceEventToken): void;
  emitEvent(eventId: string, payload?: unknown): void;
}

export interface JuceInitialisationData {
  /** Control names the C++ side registered a relay for. */
  __juce__sliders: string[];
  __juce__toggles: string[];
  __juce__comboBoxes: string[];
  /** Names registered with Options::withNativeFunction(). */
  __juce__functions: string[];
  __juce__registeredGlobalEventIds: string[];
  /** Single-element array, e.g. ["mac"], or empty in a browser. */
  __juce__platform: string[];

  /** Anything passed to Options::withInitialisationData() in WebUIEditor. */
  [key: string]: unknown;
}

export interface JuceGlobal {
  initialisationData: JuceInitialisationData;
  backend: JuceBackend;
  postMessage(message: unknown): void;
  /** Android only; absent on desktop. */
  getAndroidUserScripts?: () => string;
}

declare global {
  interface Window {
    __JUCE__: JuceGlobal;
  }
}
