/*
 * The page's large data: the engine (WebAssembly, base64) and the built-in
 * patches. Not JavaScript: scripts/inline.mjs puts each in an inert
 * <script type="text/plain"> block at the end of the page, read here on first
 * use. As string literals inside the app they were parsed as code before the
 * panel could show - with the wavetables built at load, the plugin's window
 * took over two seconds to open (the producer: "Serum or Vital open much
 * faster"). The app is a module script, so it runs after the whole page is
 * parsed: the blocks are always there by then.
 */

const block = (id: string): HTMLElement | null => document.getElementById(id);

/*
 * In the PLUGIN's page the blocks are empty, with a data-src: the data is a
 * file the plugin serves itself (web_panel.cpp's resource provider, so no
 * network), fetched once the panel is up. 7.8 MB of inert text inside the
 * page cost the window half its opening time in Chromium - 340-610 ms to a
 * built panel against 185-230 ms without (the producer: "it lags when I open
 * it"). The phone page still carries the data inline: it is one file.
 */
/** One native call (JUCE's __juce__invoke), its ids far from bridge.ts's. */
let nextAsk = 1_000_000_000;
function askPlugin(file: string): Promise<string> {
  const backend = window.__JUCE__?.backend;
  if (!backend) return Promise.resolve('');
  const resultId = nextAsk++;
  return new Promise((resolve) => {
    const timer = setTimeout(() => resolve(''), 10_000);
    backend.addEventListener('__juce__complete', (payload) => {
      const { promiseId, result } = payload as { promiseId: number; result: unknown };
      if (promiseId !== resultId) return;
      clearTimeout(timer);
      resolve(typeof result === 'string' ? result : '');
    });
    backend.emitEvent('__juce__invoke', { name: 'gnarlData', params: [file], resultId });
  });
}

let loading: Promise<void> | null = null;
/** Every block's data in place; resolves at once when the page carries it inline. */
export function loadData(): Promise<void> {
  loading ??= Promise.all(
    [...document.querySelectorAll<HTMLElement>('script[type="text/plain"][data-src]')]
      .filter((node) => !node.textContent)
      .map(async (node) => {
        try {
          const response = await fetch(node.dataset.src ?? '');
          if (response.ok) node.textContent = await response.text();
        } catch {
          // Asked of the plugin below.
        }
        // A web view that will not fetch from its own scheme: the same file
        // through the channel every knob already uses (web_panel.cpp
        // gnarlData). Left empty if that fails too: what needs it says so.
        if (!node.textContent) node.textContent = await askPlugin(node.dataset.src ?? '');
      }),
  ).then(() => undefined);
  return loading;
}

/** Whether this page carries the engine (gnarl-web.html, and the plugin's page for MATCH). */
export function hasEngineData(): boolean {
  return block('gnarl-wasm') !== null;
}

let wasm: string | undefined;
/** The engine as base64, or undefined in a page without it. */
export function engineWasm(): string | undefined {
  // Not cached while still empty: in the plugin it arrives by loadData().
  wasm ??= block('gnarl-wasm')?.textContent || undefined;
  return wasm;
}

interface Packed {
  /** Long base64 strings (wavetables, mostly), each stored once. */
  strings: string[];
  /** Each patch's text, every long string replaced by "@gnarl:N". */
  patches: Record<string, string>;
}

let patches: Record<string, string> | null = null;
/** Every built-in patch (presets/*.vital), its .vital text by name. */
export function builtInPatches(): Readonly<Record<string, string>> {
  if (patches) return patches;
  const text = block('gnarl-presets')?.textContent;
  // Still empty in the plugin until loadData() has run: not cached, so the
  // next call after it finds the patches.
  if (!text) return block('gnarl-presets')?.dataset.src ? {} : (patches = {});
  const packed = JSON.parse(text) as Packed;
  patches = Object.fromEntries(Object.entries(packed.patches).map(([name, body]) =>
    [name, body.replace(/"@gnarl:(\d+)"/g, (_, i: string) => `"${packed.strings[Number(i)] ?? ''}"`)]));
  return patches;
}
