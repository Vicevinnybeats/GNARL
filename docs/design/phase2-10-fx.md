# Phase 2 #10: the effects rack, and a wobble on a level

## What it is

The panel's effects are now ten slots on three pages:

| Page | Slots | Engine |
|---|---|---|
| DRIVE | 01 DIST, 02 FOLD, 03 CRUSH, 04 OTT | GNARL's drive chain (phase2-07) and Vital's compressor |
| MOD | 05 CHORUS, 06 FLANGER, 07 PHASER, 08 EQ | Vital's own effects |
| SPACE | 09 DELAY, 10 REVERB | Vital's own effects |

Nothing in the engine changed. Every one of these effects was already in
Vital, with its parameters, in every preset; the panel binds them
(`ui/src/params.ts`), so a patch made in the plugin opens with the same
effect settings on the phone and the other way round. On the desktop the
pages keep the panel at 1280 x 720; on the phone they are three buttons at
the top of the FX tab.

Each slot shows the controls a producer reaches for first. Everything else
(delay filter, reverb shelves, the chorus delay times, EQ band modes) stays
at the patch's value and is in Vital's editor (ADVANCED, plugin only).

| Slot | Knobs | Buttons |
|---|---|---|
| CHORUS | DEPTH, FEEDBK (bipolar), MIX | |
| FLANGER | DEPTH, FEEDBK (bipolar), MIX | rate 4/1, 1/1, 1/4, 1/8 |
| PHASER | FEEDBK, CENTER, MIX | rate 8/1, 1/1, 1/4, 1/8 |
| EQ | LOW, MID (gain), FREQ (mid), HIGH | |
| DELAY LINE | LED (steps or ms), FEEDBK (bipolar), MIX | UNIT, STEP LENGTH; MONO, STEREO, PING (phase2-11-ddl.md) |
| REVERB | SIZE, DECAY, MIX | |

The numbers name a slot, not the signal order. The engine runs chorus, OTT,
delay, the drive chain, EQ, flanger, phaser, reverb (`constants::Effect`).

The rates' first option is the init patch's (Vital's default flanger rate is
4/1, the phaser's 8/1), so a fresh patch shows its real rate. A value with
no button - a preset's 2/1 - shows the first option dimmed.

## A wobble on a level

The matrix has two more destinations, OSC1 LEVEL and OSC2 LEVEL. The
matrix's amount bar is now **bipolar**, as Vital's: the centre is zero and
left of it is negative. That matters here. Measured on the init saw, C3, 140
bpm, the wobble at 1/8 (sine):

| Route | Held RMS | Peak | Level movement |
|---|---|---|---|
| none | -16.8 dBFS | -8.9 dBFS | - |
| WOBBLE -> OSC1 LEVEL, +50% | -10.7 dBFS | **+0.4 dBFS** | 2.0 per beat, depth 0.51 |
| WOBBLE -> OSC1 LEVEL, -50% | -21.5 dBFS | -8.9 dBFS | 2.0 per beat, depth 0.95 |
| LFO 1 -> OSC1 LEVEL, -50% | -15.4 dBFS | -3.7 dBFS | 0.49 per beat (LFO 1's own rate) |

A positive amount adds to the level the patch already has: louder, and over
full scale on a phone. **A negative amount is the tremolo**: it cuts from
the patch's level, so the peak stays where it was and the chop is nearly
twice as deep. LFO 1's rate is not on the panel; the WOBBLE is the
tempo-synced one. (Movement: `tools/measure.py`, full band.)

## Measured

Each effect switched on at the engine's defaults, init saw, C3, 2 s at 140
bpm, browser build (`tools/web_render.mjs`):

| | Held RMS | Side (stereo) | Peak | 0.1 s after release |
|---|---|---|---|---|
| dry | -16.8 | none | -8.9 | silent |
| CHORUS | -14.5 | -20.4 | -2.7 | -57.8 |
| FLANGER | -18.8 | -24.9 | -7.9 | -120.4 |
| PHASER | -19.3 | -25.4 | -9.0 | -91.4 |
| EQ | -16.8 | none | -8.9 | silent: **bit-identical to dry** |
| EQ, LOW at +15 dB | -11.0 | none | -3.2 | |
| DELAY | -12.7 | none (MONO) | -7.4 | -22.1, falling about 2.5 dB per 0.1 s |
| REVERB | -17.4 | -34.1 | -10.1 | -41.6, falling to -53 by 0.5 s |

Two things a producer will notice, neither a bug: the chorus at Vital's
defaults lifts the peak 6 dB (four voices summing), and EQ does nothing
until a gain moves, because every gain defaults to 0 dB.

`ui/tests/web.test.mjs` checks, against the real engine in Chromium:
- each new switch sets its `*_on`;
- DELAY's style button steps MONO -> STEREO (the engine's text);
- DIST's mode button steps;
- a matrix route to OSC1 LEVEL, pulled left of centre, arrives at -0.5.

## Found on the way

**The FX mode buttons never changed.** The cycle button shows only the lit
option, and its click set that same option again: DIST's TUBE, FOLD's SINE,
CRUSH's HARD and OTT's 3-BAND could not be changed from the panel since it
was first built. A tap now steps to the next option. With the old click put
back, the three new button checks fail.

## Not done

- Effect order is not on the panel (Vital's editor has it).
- LFO 1's rate and shape are not on the panel.
