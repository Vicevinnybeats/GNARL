// A matcher worker: its own copy of the engine, rendering and judging
// candidates for the page (match.ts). The page prepends engine-core.js and
// match-core.js to this file and starts it from a blob: URL, as it starts the
// AudioWorklet (web/host.ts).

/* global createGnarlEngine, createMatcherCore */
let engine = null;
let core = null;
let job = null;

self.onmessage = (event) => {
  const message = event.data;
  try {
    if (message.type === 'init') {
      core = createMatcherCore();
      core.setTables(message.tables);
      engine = engine ?? createGnarlEngine(message.wasm, core.SR);
      job = message;
      self.postMessage({ type: 'ready' });
    } else if (message.type === 'peaks') {
      // The AI's levelling (generate.ts levelVolume): each patch's loudest
      // sample over the given notes, held.
      const results = message.patches.map((text) =>
        Math.max(...message.midis.map((m) => core.peakOf(engine, text, m, message.seconds) ?? 0)));
      self.postMessage({ type: 'peaks', id: message.id, results });
    } else if (message.type === 'render') {
      // RIDDIMIZE's USE MY SOUND: the loaded patch's held note, as audio.
      const audio = core.renderHeld(engine, message.patch, message.midi, message.seconds);
      self.postMessage({ type: 'rendered', id: message.id, audio }, audio ? [audio.buffer] : []);
    } else if (message.type === 'scoreText') {
      // MATCH inside the riddim recipe (recipe-search.ts): whole patches,
      // rendered and judged as the genes are below.
      const results = message.patches.map((text) => {
        const audio = core.renderMono(engine, text, job.midi, job.length / core.SR);
        if (!audio) return { d: Infinity, peak: 0 };
        let peak = 0;
        for (const v of audio) peak = Math.max(peak, Math.abs(v));
        if (!(peak > 1e-4) || !Number.isFinite(peak)) return { d: Infinity, peak: 0 };
        return { d: core.distance(core.features(audio, job.length), job.target), peak };
      });
      self.postMessage({ type: 'scored', id: message.id, results });
    }
  } catch (error) {
    self.postMessage({ type: 'error', id: message.id, message: String(error) });
  }
};
