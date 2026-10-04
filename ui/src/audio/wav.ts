/*
 * WAV files for DRUMS and RIDDIMIZE (docs/design/phase4-06-drums-riddimize.md):
 * 24-bit PCM, what FL Studio's playlist and every DAW take. Pure, no DOM, so
 * tests run it in Node.
 */

/** A 24-bit PCM WAV of one or two channels of the same length. */
export function encodeWav(channels: readonly Float32Array[], sampleRate: number): Uint8Array {
  const count = channels.length;
  const length = channels[0]?.length ?? 0;
  const bytesPerSample = 3;
  const dataBytes = length * count * bytesPerSample;
  const out = new Uint8Array(44 + dataBytes);
  const view = new DataView(out.buffer);
  const text = (at: number, s: string): void => {
    for (let i = 0; i < s.length; i += 1) out[at + i] = s.charCodeAt(i);
  };
  text(0, 'RIFF');
  view.setUint32(4, 36 + dataBytes, true);
  text(8, 'WAVE');
  text(12, 'fmt ');
  view.setUint32(16, 16, true);
  view.setUint16(20, 1, true); // PCM
  view.setUint16(22, count, true);
  view.setUint32(24, sampleRate, true);
  view.setUint32(28, sampleRate * count * bytesPerSample, true);
  view.setUint16(32, count * bytesPerSample, true);
  view.setUint16(34, 8 * bytesPerSample, true);
  text(36, 'data');
  view.setUint32(40, dataBytes, true);
  let at = 44;
  for (let i = 0; i < length; i += 1) {
    for (let c = 0; c < count; c += 1) {
      const v = Math.max(-1, Math.min(1, channels[c]?.[i] ?? 0));
      const n = Math.round(v * 8388607);
      out[at] = n & 0xff;
      out[at + 1] = (n >> 8) & 0xff;
      out[at + 2] = (n >> 16) & 0xff;
      at += 3;
    }
  }
  return out;
}

/** Scale so the loudest sample sits at `peakDb` dBFS (in place); returns the gain. */
export function normalize(channels: readonly Float32Array[], peakDb: number): number {
  let peak = 0;
  for (const ch of channels) for (const v of ch) peak = Math.max(peak, Math.abs(v));
  if (peak <= 0) return 1;
  const gain = Math.pow(10, peakDb / 20) / peak;
  for (const ch of channels) for (let i = 0; i < ch.length; i += 1) ch[i] = (ch[i] ?? 0) * gain;
  return gain;
}
