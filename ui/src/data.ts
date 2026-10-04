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

/** Whether this page carries the engine (gnarl-web.html, and the plugin's page for MATCH). */
export function hasEngineData(): boolean {
  return block('gnarl-wasm') !== null;
}

let wasm: string | undefined;
/** The engine as base64, or undefined in a page without it. */
export function engineWasm(): string | undefined {
  wasm ??= block('gnarl-wasm')?.textContent ?? undefined;
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
  if (!text) return (patches = {});
  const packed = JSON.parse(text) as Packed;
  patches = Object.fromEntries(Object.entries(packed.patches).map(([name, body]) =>
    [name, body.replace(/"@gnarl:(\d+)"/g, (_, i: string) => `"${packed.strings[Number(i)] ?? ''}"`)]));
  return patches;
}
