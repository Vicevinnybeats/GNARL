import { defaultRamp, type CurvePoint } from '../dsp/lfoCurve';
import { fetchModState } from './modState';

/**
 * The drawn curve and the slot's destination, cached for the audio path.
 *
 * WHY A CACHE. `fetchModState` is async — it crosses the bridge — and a
 * note-on runs inside a pointer event that must not wait on anything. So the
 * state is pulled once and refreshed whenever the editor changes it, and the
 * audio reads the last value synchronously. A note played in the gap before
 * the first fetch returns gets the default ramp, which is a shape rather
 * than silence.
 *
 * This mirrors the plugin's own arrangement (§10): the curves and the
 * destination strings are NOT parameters, so they do not come through the
 * relay with everything else and need their own path.
 */

let curve: CurvePoint[] = defaultRamp();
let destination = '';

export function previewCurve(): CurvePoint[] {
  return curve;
}

export function modDestination(): string {
  return destination;
}

/** Called on mount and whenever the editor writes a curve or a routing. */
export function refreshPreviewModState(): void {
  void fetchModState()
    .then((state) => {
      curve = state.curves[0] ?? defaultRamp();
      destination = state.destinations[0] ?? '';
    })
    .catch(() => {
      //  No bridge, or a shape this build does not understand. The defaults
      //  already in place are a working LFO, so a failure here costs the
      //  drawn shape rather than the sound.
    });
}

/** Used by the editor so a drawn change is heard on the very next note. */
export function setPreviewCurve(points: CurvePoint[]): void {
  curve = points;
}

export function setPreviewDestination(id: string): void {
  destination = id;
}
