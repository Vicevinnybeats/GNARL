// The GNARL engine (wasm/gnarl_web.cpp) behind a small JavaScript surface.
// Plain JavaScript, no imports: the AudioWorklet (worklet.js) and the Node
// tests (tests/test_web.mjs) both load this file as it is.
//
// The module is standalone WebAssembly: its only imports are a few WASI calls
// libc keeps for stdio, the clock and the environment, none of which the
// engine uses while it plays, and a memory-growth notice. They are stubbed here. The clock reads
// zero, so nothing seeded from it can make two runs differ.

/* exported createGnarlEngine */
function createGnarlEngine(wasmBytes, sampleRate) {
  const ENOSYS = 52;
  let memory = null;
  const imports = {
    env: { emscripten_notify_memory_growth: () => {} },
    wasi_snapshot_preview1: {
      clock_time_get: (_id, _precision, out) => {
        new BigUint64Array(memory.buffer, out, 1)[0] = 0n;
        return 0;
      },
      // An empty environment (the exception runtime asks for one).
      environ_sizes_get: (count, size) => {
        const view = new DataView(memory.buffer);
        view.setUint32(count, 0, true);
        view.setUint32(size, 0, true);
        return 0;
      },
      environ_get: () => 0,
      fd_close: () => ENOSYS,
      fd_seek: () => ENOSYS,
      // Count the bytes as written, so a stray printf cannot loop forever.
      fd_write: (_fd, iovs, iovsLength, written) => {
        const view = new DataView(memory.buffer);
        let total = 0;
        for (let i = 0; i < iovsLength; i += 1) total += view.getUint32(iovs + 8 * i + 4, true);
        view.setUint32(written, total, true);
        return 0;
      },
    },
  };
  const compiled = wasmBytes instanceof WebAssembly.Module ? wasmBytes : new WebAssembly.Module(wasmBytes);
  const instance = new WebAssembly.Instance(compiled, imports);
  const x = instance.exports;
  memory = x.memory;
  x._initialize();

  // UTF-8 by hand: an AudioWorklet's global scope has no TextEncoder or
  // TextDecoder. Names are ASCII; readout units may not be (a degree sign).
  const encode = (text) => {
    const out = [];
    for (const ch of text) {
      const c = ch.codePointAt(0);
      if (c < 0x80) out.push(c);
      else if (c < 0x800) out.push(0xc0 | (c >> 6), 0x80 | (c & 63));
      else if (c < 0x10000) out.push(0xe0 | (c >> 12), 0x80 | ((c >> 6) & 63), 0x80 | (c & 63));
      else out.push(0xf0 | (c >> 18), 0x80 | ((c >> 12) & 63), 0x80 | ((c >> 6) & 63), 0x80 | (c & 63));
    }
    return Uint8Array.from(out);
  };
  const decode = (bytes) => {
    let text = '';
    for (let i = 0; i < bytes.length;) {
      const b = bytes[i];
      let c;
      let n;
      if (b < 0x80) [c, n] = [b, 1];
      else if (b < 0xe0) [c, n] = [b & 31, 2];
      else if (b < 0xf0) [c, n] = [b & 15, 3];
      else [c, n] = [b & 7, 4];
      for (let k = 1; k < n; k += 1) c = (c << 6) | (bytes[i + k] & 63);
      text += String.fromCodePoint(c);
      i += n;
    }
    return text;
  };
  const bytes = () => new Uint8Array(memory.buffer);
  const floats = (pointer, length) => new Float32Array(memory.buffer, pointer, length);
  const nameBuffer = x.gnarl_name_buffer();

  // NUL-terminated strings, one after another, into the name buffer.
  function writeNames(...names) {
    let at = nameBuffer;
    const view = bytes();
    for (const name of names) {
      const encoded = encode(name);
      if (at + encoded.length + 1 - nameBuffer > 256) throw new Error('names too long');
      view.set(encoded, at);
      view[at + encoded.length] = 0;
      at += encoded.length + 1;
    }
  }
  function readString(pointer) {
    const view = bytes();
    let end = pointer;
    while (view[end] !== 0) end += 1;
    return decode(view.subarray(pointer, end));
  }

  x.gnarl_init(sampleRate);
  const indexCache = new Map();

  return {
    /** The control's index, or -1 when the engine has no such name. */
    index(name) {
      if (!indexCache.has(name)) {
        writeNames(name);
        indexCache.set(name, x.gnarl_control_index());
      }
      return indexCache.get(name);
    },
    get: (index) => x.gnarl_get(index),
    set: (index, host) => x.gnarl_set(index, host),
    /** In the engine's own units, as a preset stores it. */
    setValue: (index, value) => x.gnarl_set_value(index, value),
    steps: (index) => x.gnarl_steps(index),
    text: (index) => readString(x.gnarl_text(index)),
    note: (note, on) => x.gnarl_note(note, on ? 1 : 0),
    allNotesOff: () => x.gnarl_all_notes_off(),
    bpm: (bpm) => x.gnarl_bpm(bpm),
    time: (seconds) => x.gnarl_time(seconds),
    /** One block of at most 128 samples, interleaved L/R (a view: copy it). */
    process: (n) => floats(x.gnarl_process(n), 2 * n),
    scope: () => floats(x.gnarl_scope(), 256),
    wobblePhase: () => x.gnarl_wobble_phase(),
    /** kind: 0 sine, 1 square, 2 drawn (points 0..1, one per step). */
    wobbleShape(kind, points = []) {
      floats(x.gnarl_scratch(), points.length).set(points);
      x.gnarl_wobble_shape(kind, points.length);
    },
    wobbleCurve: () => Array.from(floats(x.gnarl_wobble_curve(), 64)),
    /** Connect or retune (amount -1..1), or remove; the slot, or -1. */
    route(source, destination, amount, remove) {
      writeNames(source, destination);
      return x.gnarl_route(amount, remove ? 1 : 0);
    },
    /** The patch as .vital JSON text, saved under `name`. */
    save(name) {
      writeNames(name.slice(0, 120));
      return readString(x.gnarl_save());
    },
    /** Load .vital JSON text: 0 loaded, 1 not a patch, 2 newer GNARL, 3 needs
     * the plugin's migration (an older version). */
    load(text) {
      const encoded = encode(text);
      const pointer = x.gnarl_load_buffer(encoded.length);
      bytes().set(encoded, pointer);
      return x.gnarl_load();
    },
    /** The init patch. */
    reset: () => x.gnarl_reset(),
    presetName: () => readString(x.gnarl_preset_name()),
    /** OSC `osc` (0, 1) from wavetable JSON text: 0 loaded, 1 refused. */
    loadWavetable(osc, text) {
      const encoded = encode(text);
      const pointer = x.gnarl_load_buffer(encoded.length);
      bytes().set(encoded, pointer);
      return x.gnarl_load_wavetable(osc);
    },
    wavetableName: (osc) => readString(x.gnarl_wavetable_name(osc)),
    routes() {
      const list = [];
      const count = x.gnarl_route_count();
      for (let i = 0; i < count; i += 1) {
        const amount = x.gnarl_route_at(i);
        const source = readString(nameBuffer);
        const destination = readString(nameBuffer + encode(source).length + 1);
        list.push({ source, destination, amount });
      }
      return list;
    },
  };
}

