import { getSliderState, getComboBoxState } from '../juce/index.js';
import defaults from './parameterDefaults.json';
import { denormalise } from './formatters';
import { getPluginInfo } from './pluginInfo';

/**
 * A Web Audio PREVIEW of the engine, for the browser and the installed app.
 *
 * WHAT THIS IS NOT. It is not GNARL. The instrument is 15,000 lines of C++
 * with zero-delay filters, oversampled drive, wavetable mip levels and a
 * formant bank, and none of that is here. This is a small subtractive voice
 * driven by the same parameters, so that the interface in a browser responds
 * to a key with a sound in the right family instead of with silence.
 *
 * WHY HAVE IT AT ALL, given CLAUDE.md's rule that duplicated logic is a
 * liability. Because the alternative shipped once already: an interface that
 * draws perfectly, answers every gesture and makes no sound is the exact
 * failure this project spent a day digging out of, and an installable app
 * that does it is worse — it looks finished. `previewEngine.ts` already
 * simulates modulation frames for the same reason.
 *
 * SO IT SAYS WHAT IT IS. `isPreview` is exported and the status bar shows it.
 * A preview that lets somebody believe they have heard the product is worse
 * than no preview, because they will judge the product by it.
 *
 * IT ONLY RUNS IN A BROWSER. Inside the plugin there is a real engine behind
 * the relays and a second one making sound would be a bug you could hear.
 */

const HINTS = defaults as Record<string, { min?: number; max?: number; skew?: number }>;

/** Reads a parameter's real-world value through the same relay the UI uses. */
function read(id: string, fallback: number): number {
  try {
    const state = getSliderState(id);
    const normalised = state?.getNormalisedValue?.();

    if (typeof normalised !== 'number' || Number.isNaN(normalised)) return fallback;

    const hint = HINTS[id];

    if (!hint || hint.min === undefined || hint.max === undefined) return normalised;

    return denormalise(normalised, hint.min, hint.max, hint.skew ?? 1);
  } catch {
    return fallback;
  }
}

function readChoice(id: string, fallback: number): number {
  try {
    const state = getComboBoxState(id);
    const v = state?.getChoiceIndex?.();
    return typeof v === 'number' && !Number.isNaN(v) ? v : fallback;
  } catch {
    return fallback;
  }
}

/*  A band-limited saw as a PeriodicWave.
 *
 *  Not an OscillatorNode 'sawtooth', which is band-limited by the browser but
 *  cannot be shaped. Building the harmonic series explicitly means the table
 *  position control can thin it out, which is the one thing that makes this
 *  read as a wavetable rather than as a plain saw.
 *
 *  Harmonics are limited so the highest stays under Nyquist at the top of the
 *  keyboard — the aliasing lesson from the real oscillator, in miniature. */
function buildWave(context: AudioContext, brightness: number): PeriodicWave {
  const count = 64;
  const real = new Float32Array(count);
  const imag = new Float32Array(count);

  for (let n = 1; n < count; n++) {
    //  1/n is a saw; the exponent tilts the spectrum from hollow to bright.
    const tilt = Math.pow(n, -1 - (1 - brightness) * 1.6);
    imag[n] = tilt;
  }

  return context.createPeriodicWave(real, imag, { disableNormalization: false });
}

/** A soft-clip curve for the drive stage. */
function driveCurve(amount: number): Float32Array<ArrayBuffer> {
  const n = 1024;
  /*  Explicitly over an ArrayBuffer, not ArrayBufferLike: lib.dom types
      WaveShaper.curve as Float32Array<ArrayBuffer>, and a plain
      `new Float32Array(n)` widens to ArrayBufferLike, which includes
      SharedArrayBuffer and does not assign. */
  const curve = new Float32Array(new ArrayBuffer(n * 4));
  const k = 1 + amount * 24;

  for (let i = 0; i < n; i++) {
    const x = (i / (n - 1)) * 2 - 1;
    curve[i] = Math.tanh(k * x) / Math.tanh(k);
  }

  return curve;
}

interface Voice {
  stop: (at: number) => void;
  note: number;
}

export class AudioPreview {
  private context: AudioContext | null = null;
  private master: GainNode | null = null;
  private shaper: WaveShaperNode | null = null;
  private voices = new Map<number, Voice>();

  /** True when this is the preview rather than the real engine. */
  readonly isPreview = true;

  /** Browsers require a user gesture before audio starts. */
  async resume(): Promise<void> {
    if (!this.context) this.build();
    if (this.context?.state === 'suspended') await this.context.resume();
  }

  private build(): void {
    const Ctor =
      window.AudioContext ??
      (window as unknown as { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;

    if (!Ctor) return;

    const context = new Ctor({ latencyHint: 'interactive' });

    const master = context.createGain();
    master.gain.value = 0.22;   // headroom: several voices sum

    /*  A limiter in spirit: a compressor with a fast attack and a hard ratio.
        Without it a chord through the drive stage clips the output node, and
        a phone's speaker turns that into a rattle rather than a warning. */
    const limiter = context.createDynamicsCompressor();
    limiter.threshold.value = -6;
    limiter.knee.value = 0;
    limiter.ratio.value = 20;
    limiter.attack.value = 0.002;
    limiter.release.value = 0.12;

    const shaper = context.createWaveShaper();
    shaper.curve = driveCurve(0.2);
    shaper.oversample = '4x';

    master.connect(shaper);
    shaper.connect(limiter);
    limiter.connect(context.destination);

    this.context = context;
    this.master = master;
    this.shaper = shaper;
  }

  noteOn(note: number, velocity = 1): void {
    void this.resume().then(() => {
      const context = this.context;
      const master = this.master;

      if (!context || !master) return;

      this.noteOff(note);

      const now = context.currentTime;
      const freq = 440 * Math.pow(2, (note - 69) / 12);

      //  --- read the patch ------------------------------------------------
      const tablePos = read('osc1_table_pos', 0.3);
      const oscLevel = read('osc1_level', 0.8);
      const subLevel = read('sub_level', 0.7);
      const subOctave = readChoice('sub_octave', 1) - 1;   // index -> octaves
      const cutoff = read('filter1_cutoff', 1200);
      const resonance = read('filter1_resonance', 0.2);
      const drive = read('filter1_drive', 0.2);
      const attack = read('env1_attack', 0.005);
      const decay = read('env1_decay', 0.4);
      const sustain = read('env1_sustain', 0.7);
      const release = read('env1_release', 0.25);
      const lfoRate = read('lfo1_rate', 4.0);

      if (this.shaper) this.shaper.curve = driveCurve(Math.min(1, Math.max(0, drive)));

      //  --- the voice -------------------------------------------------------
      const amp = context.createGain();
      amp.gain.value = 0;

      const filter = context.createBiquadFilter();
      filter.type = 'lowpass';
      filter.frequency.value = Math.min(18000, Math.max(40, cutoff || 1200));
      filter.Q.value = 0.7 + Math.min(0.95, Math.max(0, resonance)) * 12;

      filter.connect(amp);
      amp.connect(master);

      const stopped: Array<{ stop: (t: number) => void }> = [];

      //  Two detuned oscillators, as the real voice has.
      for (const detune of [-6, 6]) {
        const osc = context.createOscillator();
        osc.setPeriodicWave(buildWave(context, Math.min(1, Math.max(0, tablePos))));
        osc.frequency.value = freq;
        osc.detune.value = detune;

        const gain = context.createGain();
        gain.gain.value = Math.min(1, Math.max(0, oscLevel)) * 0.5;

        osc.connect(gain);
        gain.connect(filter);
        osc.start(now);
        stopped.push(osc);
      }

      //  The sub, around the filter — as the real one is routed direct.
      const sub = context.createOscillator();
      sub.type = 'sine';
      sub.frequency.value = freq * Math.pow(2, -Math.abs(subOctave || 1));

      const subGain = context.createGain();
      subGain.gain.value = 0;

      sub.connect(subGain);
      subGain.connect(master);
      sub.start(now);
      stopped.push(sub);

      /*  THE LFO ON THE CUTOFF, which is the sound of this genre. Modulating
          the filter rather than the amplitude: a growl is a formant moving,
          and this is the nearest a biquad gets to it. */
      const lfo = context.createOscillator();
      lfo.frequency.value = Math.min(30, Math.max(0.1, lfoRate || 4));

      const lfoDepth = context.createGain();
      lfoDepth.gain.value = Math.min(4000, filter.frequency.value * 0.9);

      lfo.connect(lfoDepth);
      lfoDepth.connect(filter.frequency);
      lfo.start(now);
      stopped.push(lfo);

      //  --- the envelope ------------------------------------------------
      const peak = 0.9 * Math.min(1, Math.max(0.05, velocity));
      const a = Math.max(0.002, attack);
      const d = Math.max(0.01, decay);
      const s = Math.min(1, Math.max(0, sustain));

      amp.gain.cancelScheduledValues(now);
      amp.gain.setValueAtTime(0, now);
      amp.gain.linearRampToValueAtTime(peak, now + a);
      amp.gain.setTargetAtTime(peak * s, now + a, d / 3);

      subGain.gain.setValueAtTime(0, now);
      subGain.gain.linearRampToValueAtTime(Math.min(1, Math.max(0, subLevel)) * 0.55, now + a);

      const rel = Math.max(0.03, release);

      this.voices.set(note, {
        note,
        stop: (at: number) => {
          amp.gain.cancelScheduledValues(at);
          amp.gain.setValueAtTime(amp.gain.value, at);
          amp.gain.linearRampToValueAtTime(0, at + rel);

          subGain.gain.cancelScheduledValues(at);
          subGain.gain.setValueAtTime(subGain.gain.value, at);
          subGain.gain.linearRampToValueAtTime(0, at + rel);

          //  Stopped AFTER the release, not at note-off — stopping a source
          //  mid-envelope is a click, which is the one artefact a listener
          //  always notices.
          for (const node of stopped) {
            try {
              node.stop(at + rel + 0.05);
            } catch {
              /* already stopped */
            }
          }
        },
      });
    });
  }

  noteOff(note: number): void {
    const voice = this.voices.get(note);

    if (!voice || !this.context) return;

    voice.stop(this.context.currentTime);
    this.voices.delete(note);
  }

  allNotesOff(): void {
    for (const note of [...this.voices.keys()]) this.noteOff(note);
  }
}

let instance: AudioPreview | null = null;

/** The preview engine, or null inside the plugin where a real one exists. */
export function getAudioPreview(): AudioPreview | null {
  if (!getPluginInfo().isMock) return null;
  if (!instance) instance = new AudioPreview();
  return instance;
}
