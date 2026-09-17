/**
 * Mirror of plugin/source/params/ParameterIDs.h.
 *
 * These strings are the contract between the C++ parameter tree and the UI.
 * A mismatch fails silently (the relay simply never connects), so when you add
 * a parameter, add it in BOTH files in the same commit.
 */
export const PID = {
  masterGain: 'master_gain',
  bypass: 'bypass',
} as const;

export type ParameterId = (typeof PID)[keyof typeof PID];
