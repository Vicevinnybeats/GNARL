/*
 * Hearing a rendered WAV before exporting it: one AudioContext, one sound at a
 * time. In the plugin this plays through the computer's own audio output, not
 * through the DAW (a web view has no route into the host's mixer); the
 * exported file is what goes into the track.
 */

let context: AudioContext | null = null;
let playing: AudioBufferSourceNode | null = null;

export function stopPreview(): void {
  try {
    playing?.stop();
  } catch {
    // Already stopped.
  }
  playing = null;
}

export function playPreview(channels: readonly Float32Array[], sampleRate: number, loop = false, onEnd?: () => void): void {
  stopPreview();
  context ??= new AudioContext();
  void context.resume();
  const length = channels[0]?.length ?? 0;
  if (length === 0) return;
  const buffer = context.createBuffer(channels.length, length, sampleRate);
  channels.forEach((ch, i) => buffer.copyToChannel(ch as Float32Array<ArrayBuffer>, i));
  const source = context.createBufferSource();
  source.buffer = buffer;
  source.loop = loop;
  source.connect(context.destination);
  source.onended = () => {
    if (playing === source) playing = null;
    onEnd?.();
  };
  source.start();
  playing = source;
}

export function isPreviewing(): boolean {
  return playing !== null;
}
