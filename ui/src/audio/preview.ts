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

let startedAt = 0;
let duration = 0;

/**
 * Plays a rendered sound, stopping the last. `keepPlace`: a loop being
 * edited carries on from where it was rather than starting again - the
 * producer heard every DRUMS edit restart the loop.
 */
export function playPreview(channels: readonly Float32Array[], sampleRate: number, loop = false, onEnd?: () => void, keepPlace = false): void {
  const at = keepPlace ? previewPosition() : 0;
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
  duration = length / sampleRate;
  const offset = at !== null && at < duration ? at : 0;
  source.start(0, offset);
  startedAt = context.currentTime - offset;
  playing = source;
}

/** Seconds into the playing sound (looped sounds wrap), or null when none plays. */
export function previewPosition(): number | null {
  if (!playing || !context || duration <= 0) return null;
  const t = context.currentTime - startedAt;
  return playing.loop ? t % duration : Math.min(t, duration);
}

export function isPreviewing(): boolean {
  return playing !== null;
}
