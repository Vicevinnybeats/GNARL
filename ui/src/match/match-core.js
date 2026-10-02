// The sound matcher's core (tools/match.py, docs/design/phase4-04-match-in-app.md),
// in plain JavaScript with no imports, like engine-core.js: the page, its Web
// Workers and the Node tests all load this file as it is (new Function).
//
// A candidate is a set of genes: osc 1's table and where LFO 1 takes it, the
// LFO's shape, rate and one-shot or loop, a warp, filter 1 and its movement,
// drive, fold, OTT, a shelf, unison and the amp envelope. It becomes a patch
// (build), is rendered by the engine (renderMono) and compared with the target
// on a log-mel picture of the growl band (features, distance) - all as
// tools/match.py does, so a result here means what one there means.

/* exported createMatcherCore */
function createMatcherCore() {
  const SR = 44100;
  // The growl band, 100 Hz to 8 kHz: the sub below is a separate voice.
  const FMIN = 100;
  const FMAX = 8000;
  const MELS = 24;
  const N_FFT = 2048;
  const HOP = 441; // 10 ms
  const SMOOTH = 8; // frames: power averaged over 80 ms (unison beating)
  const FLOOR_DB = -50;
  const BPM = 140;

  const CHOICES = {
    table: [], // filled by setTables
    rate: [[1, 7], [1, 8], [1, 9], [3, 9], [1, 6]], // 1/2, 1/4, 1/8, 1/8T, 1/1
    loop: [0, 1],
    double: [0, 1],
    warp: [0, 1, 2, 4, 5], // none, sync, formant, bend, squeeze
    filter: ['off', 'analog', 'dirty', 'formant', 'comb'],
  };
  const CONTINUOUS = ['wt_start', 'wt_depth', 'peak', 'peak2', 'dip', 'lfo_start', 'curve', 'warp_amount',
    'warp_depth', 'cutoff', 'resonance', 'formant_x', 'formant_y', 'filter_depth', 'drive', 'fold', 'ott',
    'shelf', 'unison', 'detune', 'decay', 'sustain'];

  // mulberry32, as generate.ts: the same search in every browser and in Node.
  function random(seed) {
    let a = seed >>> 0;
    return () => {
      a = (a + 0x6d2b79f5) >>> 0;
      let t = a;
      t = Math.imul(t ^ (t >>> 15), t | 1);
      t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }
  function gauss(r) {
    const u = Math.max(1e-12, r());
    return Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * r());
  }
  const pick = (r, n) => Math.min(n - 1, Math.floor(r() * n));

  function randomGenes(r) {
    const g = {};
    for (const k of CONTINUOUS) g[k] = r();
    for (const [k, options] of Object.entries(CHOICES)) g[k] = pick(r, options.length);
    return g;
  }
  function mutate(g, r, sigma) {
    const child = { ...g };
    for (const k of CONTINUOUS) {
      if (r() < 0.35) child[k] = Math.min(1, Math.max(0, g[k] + gauss(r) * sigma));
    }
    for (const [k, options] of Object.entries(CHOICES)) {
      if (r() < (k === 'table' ? 0.03 : 0.12)) child[k] = pick(r, options.length);
    }
    return child;
  }

  const lerp = (a, b, t) => a + (b - a) * t;
  function route(s, slot, source, destination, amount) {
    s.modulations[slot - 1] = { source, destination };
    s[`modulation_${slot}_amount`] = amount;
    s[`modulation_${slot}_bipolar`] = 0;
    s[`modulation_${slot}_bypass`] = 0;
    s[`modulation_${slot}_power`] = 0;
    s[`modulation_${slot}_stereo`] = 0;
  }

  let tables = {};
  function setTables(byName) {
    tables = byName;
    CHOICES.table = Object.keys(byName).sort();
  }

  /** Genes -> .vital patch (an object), on a base patch's text. */
  function build(g, baseText, name) {
    const patch = JSON.parse(baseText);
    const s = patch.settings;
    for (let i = 0; i < s.modulations.length; i += 1) {
      s.modulations[i] = { source: '', destination: '' };
      s[`modulation_${i + 1}_amount`] = 0;
    }
    s.wavetables[0] = JSON.parse(tables[CHOICES.table[g.table]]);

    const [sync, tempo] = CHOICES.rate[g.rate];
    const loop = CHOICES.loop[g.loop];
    Object.assign(s, { lfo_1_sync: sync, lfo_1_tempo: tempo, lfo_1_sync_type: loop ? 0 : 2 });
    // LFO 1 over one wob, Vital's points: x, then y with 0 at the TOP.
    const startY = g.lfo_start;
    let points;
    if (CHOICES.double[g.double]) {
      const p1 = lerp(0.05, 0.45, g.peak);
      const p2 = lerp(0.55, 0.95, g.peak2);
      points = [[0, startY], [p1, 0], [(p1 + p2) / 2, lerp(0.3, 1, g.dip)], [p2, 0], [1, 1]];
    } else {
      points = [[0, startY], [lerp(0.05, 0.95, g.peak), 0], [1, 1]];
    }
    const curve = lerp(-4, 4, g.curve);
    s.lfos[0] = { name: 'Matched', num_points: points.length, points: points.flat(),
      powers: points.map(() => curve), smooth: false };

    Object.assign(s, {
      osc_1_wave_frame: lerp(0, 256, g.wt_start),
      osc_1_unison_voices: Math.round(lerp(1, 6, g.unison)), osc_1_unison_detune: lerp(0, 2.5, g.detune),
      osc_1_stereo_spread: 0.5, osc_1_random_phase: 0,
      osc_1_distortion_type: CHOICES.warp[g.warp], osc_1_distortion_amount: g.warp_amount,
      env_1_attack: 0, env_1_hold: 0, env_1_decay: lerp(0.3, 1.6, g.decay), env_1_sustain: g.sustain,
      env_1_release: 0.2,
      distortion_on: 1, distortion_type: 0, distortion_drive: lerp(0, 30, g.drive), distortion_mix: 1,
      distortion_fold_on: g.fold > 0.15 ? 1 : 0, distortion_fold_drive: lerp(0, 12, g.fold),
      distortion_crush_on: 0,
      compressor_on: 1, compressor_mix: g.ott,
      eq_on: 1, eq_low_gain: 0, eq_band_gain: 0, eq_high_gain: lerp(-6, 10, g.shelf), eq_high_cutoff: 88,
      flanger_on: 0, phaser_on: 0, chorus_on: 0, reverb_on: 0, delay_on: 0, filter_2_on: 0,
      volume: 4300,
      beats_per_minute: BPM / 60,
    });
    route(s, 1, 'lfo_1', 'osc_1_wave_frame', lerp(-1, 1, g.wt_depth));
    if (s.osc_1_distortion_type) route(s, 2, 'lfo_1', 'osc_1_distortion_amount', lerp(-1, 1, g.warp_depth));

    const kind = CHOICES.filter[g.filter];
    s.filter_1_on = kind === 'off' ? 0 : 1;
    if (kind === 'analog' || kind === 'dirty' || kind === 'comb') {
      Object.assign(s, { filter_1_model: { analog: 0, dirty: 1, comb: 6 }[kind], filter_1_style: 0,
        filter_1_cutoff: lerp(30, 130, g.cutoff), filter_1_resonance: lerp(0, 0.9, g.resonance),
        filter_1_drive: 0, filter_1_mix: 1 });
      route(s, 3, 'lfo_1', 'filter_1_cutoff', lerp(-1, 1, g.filter_depth));
    } else if (kind === 'formant') {
      Object.assign(s, { filter_1_model: 5, filter_1_style: 0, filter_1_formant_x: g.formant_x,
        filter_1_formant_y: g.formant_y, filter_1_formant_resonance: lerp(0.3, 1, g.resonance), filter_1_mix: 1 });
      route(s, 3, 'lfo_1', 'filter_1_formant_x', lerp(-1, 1, g.filter_depth));
    }
    patch.preset_name = name;
    patch.author = 'GNARL matcher';
    return patch;
  }

  /**
   * One note of a patch, mono, `seconds` long, at `midi`: as the renderer
   * plays it for tools/match.py (gnarl-render -l 1: transport running,
   * note-on at 0, note-off at 1 s, the release after), after a short
   * pre-roll. Holding the note through instead put the same patch 6.08 dB
   * from the desktop's render of it; with the note-off, as web_render.mjs
   * does, they agree. A patch the engine refuses gives null.
   */
  function renderMono(engine, patchText, midi, seconds) {
    if (engine.load(patchText) !== 0) return null;
    engine.allNotesOff();
    engine.bpm(BPM);
    const block = 128;
    const dt = 1 / SR;
    let time = -0.1;
    for (let done = 0; done < 0.1 * SR; done += block) {
      engine.time(time);
      engine.process(block);
      time += block * dt;
    }
    engine.note(midi, true);
    const total = Math.round(seconds * SR);
    const noteOff = SR; // 1 s
    const out = new Float32Array(total);
    for (let done = 0; done < total;) {
      let n = Math.min(block, total - done);
      if (done < noteOff) n = Math.min(n, noteOff - done);
      engine.time(time);
      const audio = engine.process(n);
      for (let i = 0; i < n; i += 1) out[done + i] = 0.5 * (audio[2 * i] + audio[2 * i + 1]);
      time += n * dt;
      done += n;
      if (done === noteOff) engine.note(midi, false);
    }
    engine.note(midi, false);
    engine.allNotesOff();
    return out;
  }

  // ---- the log-mel picture (librosa's defaults: centred frames, a periodic
  // Hann window, Slaney mel filters) ---------------------------------------
  const window = new Float64Array(N_FFT);
  for (let i = 0; i < N_FFT; i += 1) window[i] = 0.5 - 0.5 * Math.cos((2 * Math.PI * i) / N_FFT);
  const hzToMel = (f) => {
    const fsp = 200 / 3;
    const minLogHz = 1000;
    const minLogMel = minLogHz / fsp;
    const logstep = Math.log(6.4) / 27;
    return f >= minLogHz ? minLogMel + Math.log(f / minLogHz) / logstep : f / fsp;
  };
  const melToHz = (m) => {
    const fsp = 200 / 3;
    const minLogHz = 1000;
    const minLogMel = minLogHz / fsp;
    const logstep = Math.log(6.4) / 27;
    return m >= minLogMel ? minLogHz * Math.exp(logstep * (m - minLogMel)) : fsp * m;
  };
  const BINS = N_FFT / 2 + 1;
  const melFilters = (() => {
    const lo = hzToMel(FMIN);
    const hi = hzToMel(FMAX);
    const pts = [];
    for (let i = 0; i < MELS + 2; i += 1) pts.push(melToHz(lo + ((hi - lo) * i) / (MELS + 1)));
    const filters = [];
    for (let m = 0; m < MELS; m += 1) {
      const f = new Float64Array(BINS);
      const [a, b, c] = [pts[m], pts[m + 1], pts[m + 2]];
      const norm = 2 / (c - a); // Slaney: equal area
      for (let k = 0; k < BINS; k += 1) {
        const hz = (k * SR) / N_FFT;
        const w = Math.max(0, Math.min((hz - a) / (b - a), (c - hz) / (c - b)));
        f[k] = w * norm;
      }
      filters.push(f);
    }
    return filters;
  })();

  // In-place radix-2 FFT.
  function fft(re, im) {
    const n = re.length;
    for (let i = 1, j = 0; i < n; i += 1) {
      let bit = n >> 1;
      for (; j & bit; bit >>= 1) j ^= bit;
      j ^= bit;
      if (i < j) {
        [re[i], re[j]] = [re[j], re[i]];
        [im[i], im[j]] = [im[j], im[i]];
      }
    }
    for (let len = 2; len <= n; len <<= 1) {
      const ang = (-2 * Math.PI) / len;
      const wr = Math.cos(ang);
      const wi = Math.sin(ang);
      for (let i = 0; i < n; i += len) {
        let cr = 1;
        let ci = 0;
        for (let j = 0; j < len / 2; j += 1) {
          const ar = re[i + j + len / 2] * cr - im[i + j + len / 2] * ci;
          const ai = re[i + j + len / 2] * ci + im[i + j + len / 2] * cr;
          re[i + j + len / 2] = re[i + j] - ar;
          im[i + j + len / 2] = im[i + j] - ai;
          re[i + j] += ar;
          im[i + j] += ai;
          const t = cr * wr - ci * wi;
          ci = cr * wi + ci * wr;
          cr = t;
        }
      }
    }
  }

  /** The log-mel picture of the first `length` samples: MELS x frames, dB. */
  function features(x, length) {
    const y = new Float64Array(length);
    for (let i = 0; i < Math.min(length, x.length); i += 1) y[i] = x[i];
    const frames = 1 + Math.floor(length / HOP);
    const pad = N_FFT / 2;
    const mel = Array.from({ length: MELS }, () => new Float64Array(frames));
    const re = new Float64Array(N_FFT);
    const im = new Float64Array(N_FFT);
    for (let t = 0; t < frames; t += 1) {
      for (let i = 0; i < N_FFT; i += 1) {
        const k = t * HOP + i - pad;
        re[i] = k >= 0 && k < length ? y[k] * window[i] : 0;
        im[i] = 0;
      }
      fft(re, im);
      for (let m = 0; m < MELS; m += 1) {
        const f = melFilters[m];
        let sum = 0;
        for (let k = 0; k < BINS; k += 1) if (f[k] !== 0) sum += f[k] * (re[k] * re[k] + im[k] * im[k]);
        mel[m][t] = sum;
      }
    }
    // Power averaged over SMOOTH frames (centred, edges held), then dB.
    let max = -Infinity;
    const db = mel.map((row) => {
      const out = new Float64Array(frames);
      const half = Math.floor(SMOOTH / 2);
      for (let t = 0; t < frames; t += 1) {
        let sum = 0;
        for (let k = t - half; k < t - half + SMOOTH; k += 1) sum += row[Math.min(frames - 1, Math.max(0, k))];
        out[t] = 10 * Math.log10(sum / SMOOTH + 1e-20);
        if (out[t] > max) max = out[t];
      }
      return out;
    });
    for (const row of db) for (let t = 0; t < row.length; t += 1) row[t] = Math.max(row[t] - max, FLOOR_DB);
    return db;
  }

  /** Mean absolute difference, dB per cell. */
  function distance(a, b) {
    let sum = 0;
    let n = 0;
    for (let m = 0; m < a.length; m += 1) {
      for (let t = 0; t < a[m].length; t += 1) {
        sum += Math.abs(a[m][t] - b[m][t]);
        n += 1;
      }
    }
    return sum / n;
  }

  return { SR, CHOICES, CONTINUOUS, random, randomGenes, mutate, setTables, build, renderMono, features, distance };
}
