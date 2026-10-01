/*
 * OSC 1 and 2's tables: GNARL's own, computed here from formulas, so there is
 * no file to license and nothing to download. Each is a morph from frame 0 to
 * frame 255 (WT POS), sent to the engine as Vital's wavetable JSON: keyframes
 * of 2048 samples that Vital interpolates spectrally between (WaveSource's
 * kFrequency), band-limits per note, and saves inside the patch.
 */

/** Vital's frame size (WaveFrame::kWaveformSize) and last frame. */
const SIZE = 2048;
const LAST_FRAME = 255;
/** Keyframes per table: enough that the spectral blend between neighbours
 * stays a blend of like shapes (a pulse at 30% and 35%, not 5% and 50%). */
const KEYFRAMES = 16;
const TAU = Math.PI * 2;

/** One sample: phase 0..1 through the cycle, m 0..1 through the table. */
type Shape = (phase: number, m: number) => number;

/** A sum of sine harmonics with amplitudes amp(k, m), k from 1. */
function additive(amp: (k: number, m: number) => number, harmonics: number): Shape {
  return (phase, m) => {
    let v = 0;
    for (let k = 1; k <= harmonics; k += 1) {
      const a = amp(k, m);
      if (a !== 0) v += a * Math.sin(TAU * k * phase);
    }
    return v;
  };
}

/*
 * Vowel formants (Hz), A E I O U: the textbook adult averages (Peterson and
 * Barney's figures, rounded). A wavetable has no pitch of its own, so they
 * sit on the harmonics of a nominal 65 Hz: about C2, the middle of where a
 * riddim bass plays. Lower notes read darker, higher ones brighter - as a
 * real formant table does.
 */
const VOWELS: readonly (readonly [number, number, number])[] = [
  [730, 1090, 2440],
  [530, 1840, 2480],
  [270, 2290, 3010],
  [570, 840, 2410],
  [300, 870, 2240],
];
const VOWEL_F0 = 65;

function vowelAt(m: number): readonly [number, number, number] {
  const x = m * (VOWELS.length - 1);
  const i = Math.min(VOWELS.length - 2, Math.floor(x));
  const t = x - i;
  const a = VOWELS[i] ?? VOWELS[0]!;
  const b = VOWELS[i + 1] ?? a;
  return [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t];
}

const saw = (phase: number): number => 1 - 2 * (phase - Math.floor(phase));

/** The tables, in the picker's order. */
export const TABLES: readonly { name: string; about: string; shape: Shape }[] = [
  {
    name: 'Basic',
    about: 'saw into square',
    // Even harmonics fade out: a saw is every harmonic at 1/k, a square the odd ones.
    shape: additive((k, m) => (k % 2 ? 1 : 1 - m) / k, 96),
  },
  {
    name: 'Growl',
    about: 'a saw with a resonant peak sweeping up its harmonics',
    shape: additive((k, m) => {
      const centre = 2 + 26 * m ** 1.4;
      const width = 1.2 + centre * 0.14;
      return (0.35 + 3 * Math.exp(-(((k - centre) / width) ** 2))) / k;
    }, 96),
  },
  {
    name: 'Vowel',
    about: 'A, E, I, O, U formants across the table',
    shape: (() => {
      const cache = new Map<number, Float64Array>();
      const amps = (m: number): Float64Array => {
        const key = Math.round(m * 1024);
        let a = cache.get(key);
        if (!a) {
          const f = vowelAt(key / 1024);
          a = new Float64Array(64);
          for (let k = 1; k < 64; k += 1) {
            const hz = k * VOWEL_F0;
            const peak = (fc: number, bw: number, g: number): number => g / (1 + ((hz - fc) / bw) ** 2);
            a[k] = (peak(f[0], 90, 1) + peak(f[1], 110, 0.55) + peak(f[2], 150, 0.3)) / Math.sqrt(k);
          }
          cache.set(key, a);
        }
        return a;
      };
      return (phase: number, m: number): number => {
        const a = amps(m);
        let v = 0;
        for (let k = 1; k < 64; k += 1) v += (a[k] ?? 0) * Math.sin(TAU * k * phase);
        return v;
      };
    })(),
  },
  {
    name: 'Croak',
    about: 'a decaying ring each cycle, the ring rising: froggy',
    // A glottal-style pulse: each cycle restarts a damped sine whose pitch
    // climbs from the 2nd harmonic to the 11th - a formant that talks.
    shape: (phase, m) => {
      const ring = 2 + 9 * m;
      return Math.exp(-5 * phase) * Math.sin(TAU * ring * phase) - 0.05;
    },
  },
  {
    name: 'Fold',
    about: 'a sine folded harder and harder',
    shape: (phase, m) => Math.sin((Math.PI / 2) * (1 + 7 * m) * Math.sin(TAU * phase)),
  },
  {
    name: 'Sync',
    about: 'a hard-synced saw, its ratio 1 to 8',
    shape: (phase, m) => saw(phase * (1 + 7 * m)),
  },
  {
    name: 'FM',
    about: 'a sine frequency-modulated at twice its rate, index 0 to 6',
    shape: (phase, m) => Math.sin(TAU * phase + 6 * m * Math.sin(TAU * 2 * phase)),
  },
  {
    name: 'Pulse',
    about: 'a pulse narrowing from 50% to 4%',
    shape: (phase, m) => (phase < 0.5 - 0.46 * m ? 1 : -1),
  },
  {
    name: 'Steps',
    about: 'a saw in fewer and fewer steps, 32 to 3',
    shape: (phase, m) => {
      const steps = Math.round(32 * (3 / 32) ** m);
      return saw(Math.floor(phase * steps) / steps);
    },
  },
  {
    name: 'Wub',
    about: 'a saw through a resonant low pass opening from the 2nd harmonic to the 40th: the classic wub',
    // A two-pole low pass's magnitude at harmonic k, cutoff c, Q 2.5: the
    // resonant bump that makes a filter sweep talk.
    shape: additive((k, m) => {
      const c = 1.5 * (40 / 1.5) ** m;
      const x = k / c;
      return 1 / k / Math.sqrt((1 - x * x) ** 2 + (x / 2.5) ** 2);
    }, 96),
  },
  {
    name: 'Yoi',
    about: 'an I sliding into an O, the second formant falling: the yoi',
    shape: (() => {
      const amps = new Map<number, Float64Array>();
      return (phase: number, m: number): number => {
        const key = Math.round(m * 1024);
        let a = amps.get(key);
        if (!a) {
          const t = key / 1024;
          // I (270, 2290) to O (570, 840), Hz on a 65 Hz fundamental, as Vowel.
          const f1 = 270 + 300 * t;
          const f2 = 2290 - 1450 * t;
          a = new Float64Array(64);
          for (let k = 1; k < 64; k += 1) {
            const hz = k * VOWEL_F0;
            a[k] = (1 / (1 + ((hz - f1) / 90) ** 2) + 0.7 / (1 + ((hz - f2) / 120) ** 2)) / Math.sqrt(k);
          }
          amps.set(key, a);
        }
        let v = 0;
        for (let k = 1; k < 64; k += 1) v += (a[k] ?? 0) * Math.sin(TAU * k * phase);
        return v;
      };
    })(),
  },
  {
    name: 'Screech',
    about: 'a saw with a narrow, loud peak climbing from the 12th harmonic to the 60th',
    shape: additive((k, m) => {
      const centre = 12 + 48 * m;
      return (0.25 + 6 * Math.exp(-(((k - centre) / 1.6) ** 2))) / k;
    }, 128),
  },
  {
    name: 'Hollow',
    about: 'a square with a notch sweeping up through its harmonics',
    shape: additive((k, m) => {
      if (k % 2 === 0) return 0;
      const centre = 3 + 37 * m;
      return (1 - 0.95 * Math.exp(-(((k - centre) / (1 + centre * 0.2)) ** 2))) / k;
    }, 96),
  },
  {
    name: 'PD',
    about: 'a cosine phase-distorted into a resonant saw, the knee from 50% to 2%',
    // Casio's phase distortion: the cycle's first half squeezed into d of it.
    shape: (phase, m) => {
      const d = 0.5 - 0.48 * m;
      const warped = phase < d ? (phase * 0.5) / d : 0.5 + ((phase - d) * 0.5) / (1 - d);
      return -Math.cos(TAU * warped);
    },
  },
  {
    name: 'Comb',
    about: 'a saw through a comb whose notches move closer together',
    shape: additive((k, m) => Math.abs(Math.cos((Math.PI * k) / (2 + 14 * m))) / k, 96),
  },
  {
    name: 'Metal',
    about: 'clusters of high harmonics growing out of a soft tone: bell and grind',
    shape: additive((k, m) => {
      let a = 1 / k ** 1.5;
      for (const c of [5, 9, 14, 21]) a += 2 * m * Math.exp(-(((k - c * (1 + 0.5 * m)) / 0.6) ** 2)) / Math.sqrt(k);
      return a;
    }, 96),
  },
  {
    name: 'Tear',
    about: 'a sine-plus-saw folded unevenly, harder across the table: tearout grit',
    // The offset makes the fold asymmetric, so even harmonics appear too.
    shape: (phase, m) => Math.sin((1 + 6 * m) * (Math.sin(TAU * phase) + 0.5 * saw(phase)) + 0.6 * m),
  },
  {
    name: 'Harmonic',
    about: 'harmonics added one at a time, 1 to 64',
    shape: additive((k, m) => {
      const top = 1 + 63 * m;
      return k <= top ? 1 / Math.sqrt(k) : k < top + 1 ? (top + 1 - k) / Math.sqrt(k) : 0;
    }, 64),
  },
];

export const TABLE_NAMES: readonly string[] = TABLES.map((t) => t.name);

/** One frame, peak-normalised, as Vital's normalise would leave it. */
function frame(shape: Shape, m: number, size: number): Float32Array {
  const out = new Float32Array(size);
  let mean = 0;
  for (let i = 0; i < size; i += 1) {
    const v = shape(i / size, m);
    out[i] = v;
    mean += v;
  }
  mean /= size;
  let peak = 0;
  for (let i = 0; i < size; i += 1) {
    const v = (out[i] ?? 0) - mean;
    out[i] = v;
    peak = Math.max(peak, Math.abs(v));
  }
  if (peak > 0) for (let i = 0; i < size; i += 1) out[i] = (out[i] ?? 0) / peak;
  return out;
}

function base64(samples: Float32Array): string {
  // Little-endian floats, as WaveSourceKeyframe::stateToJson writes them
  // (every platform GNARL runs on is little-endian, as is this typed array).
  const bytes = new Uint8Array(samples.buffer, samples.byteOffset, samples.byteLength);
  let text = '';
  for (let i = 0; i < bytes.length; i += 0x8000) text += String.fromCharCode(...bytes.subarray(i, i + 0x8000));
  return btoa(text);
}

/** The table as Vital's wavetable JSON text (WavetableCreator::jsonToState). */
export function tableJson(name: string): string | null {
  const table = TABLES.find((t) => t.name === name);
  if (!table) return null;
  const keyframes = [];
  for (let i = 0; i < KEYFRAMES; i += 1) {
    const m = i / (KEYFRAMES - 1);
    keyframes.push({ position: Math.round(m * LAST_FRAME), wave_data: base64(frame(table.shape, m, SIZE)) });
  }
  return JSON.stringify({
    name: table.name,
    author: 'GNARL',
    // The wavetable format's version, not the plugin's: past every migration
    // in WavetableCreator::updateJson (the last is 0.3.7).
    version: '1.0.0',
    remove_all_dc: true,
    full_normalize: true,
    groups: [{ components: [{ type: 'Wave Source', interpolation: 1, interpolation_style: 1, keyframes }] }],
  });
}

/* The display's copy: 33 frames of 256 samples per table, made on first use. */
const PREVIEW_FRAMES = 33;
const PREVIEW_SIZE = 256;
const previews = new Map<string, Float32Array[]>();

/** A sample of the named table at phase 0..1 and position 0..1, or null. */
export function tableSample(name: string, phase: number, position: number): number | null {
  let frames = previews.get(name);
  if (!frames) {
    const table = TABLES.find((t) => t.name === name);
    if (!table) return null;
    frames = [];
    for (let i = 0; i < PREVIEW_FRAMES; i += 1) frames.push(frame(table.shape, i / (PREVIEW_FRAMES - 1), PREVIEW_SIZE));
    previews.set(name, frames);
  }
  const f = frames[Math.round(Math.min(1, Math.max(0, position)) * (PREVIEW_FRAMES - 1))];
  const p = (((phase % 1) + 1) % 1) * PREVIEW_SIZE;
  return f?.[Math.min(PREVIEW_SIZE - 1, Math.floor(p))] ?? null;
}
