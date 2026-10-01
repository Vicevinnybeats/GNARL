// The GNARL engine's AudioWorklet (docs/design/phase2-09-mobile.md). The page
// prepends engine-core.js to this file, so createGnarlEngine is in scope.
//
// It plays the plugin's half of the panel protocol (src/plugin/web_panel.cpp)
// over the worklet's message port: host.ts relays the page's events here and
// this posts the plugin's events back. Everything happens on the audio
// thread, between blocks, which is where Vital applies UI changes anyway
// (SynthBase::processModulationChanges) - so no lock and no second queue.

/* global createGnarlEngine, sampleRate, registerProcessor, AudioWorkletProcessor */

// As WebPanel: about 30 frames a second, and a value counts as changed by
// more than this.
const FRAME_SECONDS = 1 / 30;
const VALUE_EPSILON = 1e-6;

class GnarlProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    this.engine = null;
    this.bound = new Map(); // name -> { index, last }
    this.samplesToFrame = 0;
    this.curveChanged = true;
    this.lastRoutes = '';
    this.presetName = 'Init';
    this.presetChanged = false;
    this.port.onmessage = (event) => this.receive(event.data);
    try {
      this.engine = createGnarlEngine(options.processorOptions.wasm, sampleRate);
      this.port.postMessage({ type: 'ready' });
    } catch (error) {
      this.port.postMessage({ type: 'error', message: String(error) });
    }
  }

  entry(index) {
    return [this.engine.get(index), this.engine.text(index)];
  }

  receive(message) {
    const engine = this.engine;
    if (!engine) return;
    switch (message.type) {
      // WebPanel::connect: every name the engine has, with its value, text
      // and step count, and the matrix.
      case 'connect': {
        this.bound.clear();
        const values = {};
        const steps = {};
        for (const name of message.names) {
          const index = engine.index(name);
          if (index < 0) continue;
          const entry = this.entry(index);
          this.bound.set(name, { index, last: entry[0] });
          values[name] = entry;
          const count = engine.steps(index);
          if (count > 0) steps[name] = count;
        }
        const routes = engine.routes();
        this.lastRoutes = JSON.stringify(routes);
        // The patch's name and shape go out with the first frame, as the
        // plugin's do: the page then shows the engine's own wobble shape.
        this.announcePreset(this.presetName);
        const result = { version: message.version, values, steps, routes };
        this.port.postMessage({ type: 'connected', id: message.id, result });
        break;
      }
      // WebPanel::setValue: not echoed back, but the text is.
      case 'set': {
        const bound = this.bound.get(message.name);
        if (!bound) return;
        engine.set(bound.index, message.value);
        bound.last = engine.get(bound.index);
        this.port.postMessage({ type: 'event', id: 'gnarlValues', payload: { [message.name]: this.entry(bound.index) } });
        break;
      }
      case 'note':
        engine.note(message.note, message.on);
        break;
      case 'allNotesOff':
        engine.allNotesOff();
        break;
      case 'bpm':
        engine.bpm(message.bpm);
        break;
      case 'shape': {
        const kinds = { sine: 0, square: 1, draw: 2 };
        if (!(message.kind in kinds)) return;
        engine.wobbleShape(kinds[message.kind], message.points ?? []);
        this.curveChanged = true;
        break;
      }
      case 'route':
        engine.route(message.source, message.destination, message.amount ?? 0, message.remove);
        break;
      // Presets (web/presets.ts). Saving builds a few hundred kB of JSON and
      // loading renders wavetables: a pause between blocks, at a tap, which
      // is where Vital's own editor does the same work.
      case 'save':
        // Saved under a name, the patch is that patch now (as in Vital). Its
        // shape has not changed, so no curve goes with the name.
        this.presetName = message.name;
        this.presetChanged = true;
        this.port.postMessage({ type: 'event', id: 'gnarlPresetSaved',
          payload: { name: message.name, json: engine.save(message.name) } });
        break;
      case 'load': {
        const result = engine.load(message.json);
        if (result === 0) this.announcePreset(engine.presetName());
        this.port.postMessage({ type: 'event', id: 'gnarlPresetLoaded', payload: { result, name: engine.presetName() } });
        break;
      }
      case 'init':
        engine.reset();
        this.announcePreset(message.name ?? 'Init');
        break;
      // A starting sound (factory.json): the init, then its settings by name
      // and its wobble shape - as tools/factory_sounds.mjs, which measures them.
      case 'factory': {
        engine.reset();
        for (const [name, value] of Object.entries(message.settings ?? {})) {
          const index = engine.index(name);
          if (index >= 0) engine.setValue(index, value);
        }
        const shapes = { sine: 0, square: 1 };
        if (message.shape in shapes) engine.wobbleShape(shapes[message.shape]);
        this.announcePreset(message.name);
        break;
      }
      default:
        break;
    }
  }

  // A new patch: its name, and its wobble shape, go out with the next frame.
  // (The bridge reads a new preset name as a new wobble shape.)
  announcePreset(name) {
    this.presetName = name;
    this.presetChanged = true;
    this.curveChanged = true;
  }

  // WebPanel::timerCallback: changed values, the scope and wobble, the
  // matrix when it changed.
  frame() {
    const engine = this.engine;
    let changed = null;
    for (const [name, bound] of this.bound) {
      const value = engine.get(bound.index);
      if (Math.abs(value - bound.last) > VALUE_EPSILON) {
        bound.last = value;
        changed = changed ?? {};
        changed[name] = this.entry(bound.index);
      }
    }
    if (changed) this.port.postMessage({ type: 'event', id: 'gnarlValues', payload: changed });

    const frame = { scope: Array.from(engine.scope()), wobblePhase: engine.wobblePhase() };
    if (this.curveChanged) {
      frame.curve = engine.wobbleCurve();
      this.curveChanged = false;
    }
    if (this.presetChanged) {
      frame.preset = this.presetName;
      this.presetChanged = false;
    }
    this.port.postMessage({ type: 'event', id: 'gnarlFrame', payload: frame });

    const routes = engine.routes();
    const text = JSON.stringify(routes);
    if (text !== this.lastRoutes) {
      this.lastRoutes = text;
      this.port.postMessage({ type: 'event', id: 'gnarlRoutes', payload: routes });
    }
  }

  process(_inputs, outputs) {
    const out = outputs[0];
    if (!this.engine || !out || out.length === 0) return true;
    const left = out[0];
    const right = out[1] ?? out[0];
    // The render quantum is 128 frames, the engine's largest block, but a
    // browser may one day send more: take it in engine-sized pieces.
    for (let done = 0; done < left.length;) {
      const n = Math.min(128, left.length - done);
      const audio = this.engine.process(n);
      for (let i = 0; i < n; i += 1) {
        left[done + i] = audio[2 * i];
        right[done + i] = audio[2 * i + 1];
      }
      done += n;
    }

    this.samplesToFrame -= left.length;
    if (this.samplesToFrame <= 0 && this.bound.size > 0) {
      this.samplesToFrame += Math.round(FRAME_SECONDS * sampleRate);
      this.frame();
    }
    return true;
  }
}

registerProcessor('gnarl', GnarlProcessor);
