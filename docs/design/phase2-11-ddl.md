# Phase 2 #11: the delay line

The producer asked for a digital delay line after a hardware-style step
delay (Reason's DDL-1), in GNARL's own look: above all its **LED counter** -
the four-digit, seven-segment display with up and down buttons that sets
the delay's length.

## What it is

The SPACE page's DELAY LINE slot (two columns wide on the desktop):

| Control | What it sets |
|---|---|
| LED + up / down | the delay's length: **STEPS** 1..16, or **MS** 2..4000. Buttons repeat while held (in MS, by 10 after a moment); drag the LED up or down |
| UNIT (STEPS / MS) | tempo-synced steps, or a free time |
| STEP LENGTH (1/16, 1/8T, 1/8) | what one step is. 1/16 and 1/8T are the hardware's two; 1/8 is Vital's init delay, so a new patch reads as it is |
| FEEDBK, MIX | Vital's delay feedback (bipolar) and mix |
| MONO / STEREO / PING | Vital's delay style, in the header |

The LED is drawn in the brand cyan, `#64e6ff`, with the unlit segments a
faint ghost of it, as a real LED shows its 8s.

## The engine

One new parameter, appended: **`delay_steps`** (1..16, default 1,
`version_added 0x01000C`, so it sorts last in a host's list -
`tests/host_parameters.txt`). In `DelayModule`, a `DelayStepsScale` divides
the tempo-synced delay frequency by the step count - the delay time is
`steps x note value` - and passes a free (seconds) frequency through
untouched. Both taps take it, since STEREO and PING read the second tap's
time for the right channel. Block-rate, as the frequency it scales. No
allocation; one division per block.

Everything else is Vital's own parameters, which the panel drives in pairs
(`ui/src/bridge.ts`, both taps alike):

| Panel | Engine |
|---|---|
| STEPS, 1/16 | `delay_sync` 1 (tempo), `delay_tempo` 10 (1/16), `delay_steps` |
| STEPS, 1/8T | `delay_sync` 3 (triplet), `delay_tempo` 9 (1/8) |
| STEPS, 1/8 | `delay_sync` 1, `delay_tempo` 9 |
| MS | `delay_sync` 0, `delay_frequency` = log2(1000 / ms) |

A preset whose synced time is none of the three (1/4, say) shows STEPS with
the step length dimmed; MS remembers the step length for the way back.

**Every existing preset keeps its delay.** A patch without `delay_steps`
loads it at 1, and 1 step divides by 1.

## Measured

`tests/test_ddl.py`: a click through the delay at 100% mix and no feedback,
so the first sound in the render is the echo, at 140 bpm:

| Setting | Echo at | Expected |
|---|---|---|
| 1 x 1/16 | 107.2 ms | 107.1 ms |
| 3 x 1/16 | 321.5 ms | 321.4 ms |
| 16 x 1/16 (one bar) | 1714.3 ms | 1714.3 ms |
| 3 x 1/8T | 428.6 ms | 428.6 ms |
| MS 250, STEPS 5 (ignored) | 250.0 ms | 250.0 ms |

- A patch without the key renders bit-identically to `delay_steps` 1.
- 5 x 1/16 lands at 535.8 ms in blocks of 32 and of 512.
- Negative control: with the scale bypassed, the three multi-step cases fail
  (each echo at one step).
- Browser vs desktop, 5 x 1/16 with feedback: -71.0 dB; Vital's delay alone
  (1 step) is -73.1 dB, so the steps add nothing to the difference.
- `ui/tests/web.test.mjs`, against the real engine in Chromium: the init
  patch reads 1 step of 1/8; two taps up make 3 in the LED and the engine;
  STEP LENGTH steps 1/8 -> 1/16 -> 1/8T on both taps; UNIT to MS shows
  250 and sets free time on both taps; up makes 251 ms in the engine; back
  to STEPS restores 3 of 1/8T.

## Not done

- **PAN.** The hardware pans the echo; Vital's delay has no pan, and
  adding one is an engine change of its own. STEREO and PING are the stereo
  this delay has.
- The longest delay is Vital's 4 s buffer: 16 steps of 1/8T reach it below
  80 bpm, and longer times are clamped there.
