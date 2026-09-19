import { useEffect, useState } from 'react';

import { getNativeFunction } from '../juce/index.js';

import { getPluginInfo } from './pluginInfo';

/**
 * The licence, for the banner and the feature gates.
 *
 * READ ON DEMAND AND PUSHED ON CHANGE, never polled. The licence changes
 * about once an hour at most — a poll would be bridge traffic for nothing,
 * which is the same reasoning that makes the engine drop a modulation frame
 * identical to the last one.
 *
 * The policy itself lives in C++ (`plugin/source/license/`) and is not
 * duplicated here. This module reports what the engine decided; it does not
 * decide anything. In particular it does not compute the grace period, and
 * **it never gates audio** — there is no path from here to the audio thread,
 * and `audioAllowed` is reported so the UI can state it rather than assume
 * it.
 */

/** Mirrors `gnarl::license::Status`, by NAME.
 *
 * By name and not by index, deliberately. An index would be a second frozen
 * ordering to maintain — the same argument that makes mod destinations
 * parameter-ID strings — and appending `unenforced` to the C++ enum would
 * have silently renamed whatever sat at that number here.
 */
export type LicenseStatus =
  | 'unlicensed'
  | 'licensed'
  | 'offline'
  | 'expired'
  | 'invalid'
  /** Not a licence state: this build has no activation endpoint configured
      and therefore does not check. Features stay on and the banner says so. */
  | 'unenforced';

export interface LicenseState {
  status: LicenseStatus;
  message: string;
  graceDaysRemaining: number;
  featuresAllowed: boolean;
  shouldWarn: boolean;
  audioAllowed: boolean;
}

/** What the browser preview reports. Honest about being a preview rather than
    pretending to hold a licence. */
const PREVIEW_STATE: LicenseState = {
  status: 'unenforced',
  message: 'Browser preview - no plugin, no licence check.',
  graceDaysRemaining: 0,
  featuresAllowed: true,
  shouldWarn: true,
  audioAllowed: true,
};

const JUCE_EVENT_ID = 'gnarlLicense';

function parse(payload: unknown): LicenseState | null {
  if (typeof payload !== 'object' || payload === null) return null;

  const record = payload as Record<string, unknown>;
  const status = String(record.status ?? '') as LicenseStatus;

  if (status.length === 0) return null;

  return {
    status,
    message: String(record.message ?? ''),
    graceDaysRemaining: Number(record.graceDaysRemaining) || 0,
    featuresAllowed: record.featuresAllowed === true,
    shouldWarn: record.shouldWarn === true,
    // Defaulted TRUE. Audio is always allowed; a frame that lost the field
    // must not be read as the plugin having gone silent.
    audioAllowed: record.audioAllowed !== false,
  };
}

export async function fetchLicenseState(): Promise<LicenseState> {
  if (getPluginInfo().isMock) return PREVIEW_STATE;

  try {
    const reply = await getNativeFunction('gnarlLicenseStatus')();
    return parse(reply) ?? PREVIEW_STATE;
  } catch {
    // A bridge that did not answer is not a licence failure, and must not be
    // shown as one. Leave the features on and say nothing.
    return PREVIEW_STATE;
  }
}

export function subscribeToLicense(listener: (state: LicenseState) => void): () => void {
  if (getPluginInfo().isMock) return () => {};

  const token = window.__JUCE__.backend.addEventListener(JUCE_EVENT_ID, (payload) => {
    const state = parse(payload);
    if (state) listener(state);
  });

  return () => window.__JUCE__.backend.removeEventListener(token);
}

/**
 * The licence as React state.
 *
 * Safe to keep in state, unlike the modulation frame: this changes about
 * once an hour, not sixty times a second.
 */
export function useLicense(): LicenseState | null {
  const [state, setState] = useState<LicenseState | null>(null);

  useEffect(() => {
    let live = true;

    void fetchLicenseState().then((next) => {
      if (live) setState(next);
    });

    const unsubscribe = subscribeToLicense(setState);

    return () => {
      live = false;
      unsubscribe();
    };
  }, []);

  return state;
}
