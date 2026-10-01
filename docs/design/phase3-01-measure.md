# Phase 3 — measure and compare

**State:** tooling built and tested (2026-10-01). No reference track has
been measured yet: that is the producer's step, on their own machine.

## What it is

- **`tools/measure.py WAV`**: every number below, printed and saved as JSON.
- **`tools/compare.py REFERENCE TARGET`**: two of them side by side.
  - Each side is a JSON, a WAV or a `.vital` patch; a patch is rendered with
    `gnarl-render` first.
  - It prints the differences.
  - It never says a patch "sounds close". The producer decides that
    (CLAUDE.md §7).
- **`references/`**: measurements only, and a `.gitignore` that refuses
  audio.
- **`tools/check_fork.py` check 8**: CI fails if any audio file is committed.

| Measurement | How | The mistake it avoids |
|---|---|---|
| Loudness | BS.1770-4 LUFS: K-weighting designed for any rate, 400 ms blocks, both gates | — |
| Band shares | **power** from a Welch PSD | summing magnitudes read 12–21 % for a 79 % sub |
| Stereo | side/mid power per band, L/R correlation | — |
| Level wobble | RMS envelope in **128-sample** hops of the **200–5000 Hz** band; its spectrum 0.25–30 Hz | an 8192-sample hop aliased; the full band found the kick |
| Brightness wobble | spectral centroid over time, and the spectrum of its movement | a filter wobble barely moves the level |
| Rate | the fundamental of the peaks' harmonic series, by power, harmonics within 0.03 Hz / 0.3 % | the argmax (see below) |

Every spectrum is in the JSON whole, on a 0.05 Hz grid, not only its peaks.

## The instrument, measured (`tests/test_measure.py`)

Signals whose answers are known. Each past mistake is run as a negative
control and must give the wrong answer.

| Check | Result |
|---|---|
| Full-scale 997 Hz sine, at 48 and 44.1 kHz | one channel −3.01 LUFS, both 0.00 (BS.1770's calibration) |
| 50 Hz sine + white noise | sub share 93.74 %, analytic 93.77 % |
| ↳ control: summing magnitudes | 10.14 % |
| Stereo, L = R | side −265 dB |
| Stereo, independent noise | side = mid within 0.5 dB (20 s: the sub band has few bins) |
| 140 BPM kick + growl at 1/8T | growl band 7.00 Hz = 3/beat |
| ↳ control: full band | the kick, 2.33 Hz |
| 1/16 wobble, 9.33 Hz | 9.333 Hz |
| ↳ control: 8192-sample hop | 1.43 Hz |
| 80 % wobble depth | 0.80; plain noise 0.016 |
| Filter wobble at constant level | centroid moves at 4.667 Hz |
| GNARL wobble 1/8T on cutoff | level and centroid rates 3/beat |
| ↳ control: loudest level peak | 6/beat |
| Same at C1 | 3/beat |
| ↳ control: the first rate picker | 2.014/beat |
| Held C1, no wobble | no rate (depth 0.04) |
| GNARL mono sub | sub share 6.2 % → 62.4 %, side −278 dB |

## Two things the GNARL renders taught the instrument

**1. A shaped wobble's loudest line can be its 2nd harmonic.** The triangle
wobble on cutoff measured 14 Hz at 0 dB and its 7 Hz rate at −1.4 dB. The
argmax would call 1/8T "1/16-triplet". So the rate is the fundamental of
the harmonic series, chosen by power.

**2. A low note beats against the wobble's harmonics.**
- At C1 (32.7 Hz) a second series appears at 32.7 − 7k Hz: 25.7, 18.7,
  11.7 and 4.7 Hz.
- The first rate picker allowed 1 % per harmonic. It let 4.7 Hz "explain"
  14, 18.7 and 28 Hz, so it reported 2.01 per beat.
- Harmonics must now sit within 0.03 Hz or 0.3 % (peaks are located to
  about 0.01 Hz).
- With no wobble the depth is 0.04, so a rate is named only above 0.1.

## Next

1. **The producer measures 3–5 reference drops** locally, each 8 seconds,
   and commits the JSON.
2. **Compare them with GNARL patches built by hand** in the plugin. The
   numbers say what differs: rate, depth, sub share, brightness, width.
   The producer says what matters.
3. **Only then a preset pack**, by hand. Phase 4 (AI presets) stays
   unstarted.
