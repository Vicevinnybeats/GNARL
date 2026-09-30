# Phase 2, feature 4 — the drive chain: DIST → FOLD → CRUSH

**Status:** built and tested (2026-09-30). `tests/test_drive_chain.py`
checks it. The panel's DIST, FOLD and CRUSH tiles are bound. Nobody has
heard it yet.

## What the producer gets

Three stages in series, each with its own on/off:

| Tile | Engine | Controls |
|---|---|---|
| 01 DIST | Vital's distortion stage (`distortion_on`) | DRIVE, MIX, SOFT/HARD (TUBE is not in the engine) |
| 02 FOLD | `distortion_fold_*` | AMOUNT (drive, dB), MIX, SINE/LINEAR |
| 03 CRUSH | `distortion_crush_*` | BITS (1–16), RATE (sample hold, 1× to 64×). HARD/SOFT is not in the engine |

Vital has one distortion stage with six modes: soft clip, hard clip,
linear fold, sine fold, bit crush and down-sample. A riddim chain wants
several at once: clip into fold into crush.

## Where it lives

**Inside Vital's distortion slot**, as two more stages after the existing
one. The slot runs while any stage is on, and `distortion_on` now switches
only the drive stage.

Adding effects to the reorderable effect chain instead was rejected:
- its order is one float that encodes a permutation of nine effects;
- Vital's editor draws one panel per effect;
- presets store both.

A new effect there changes all three. A stage inside the slot changes
nothing a preset or the editor relies on, and the chain moves as one
block when the slot is reordered.

- **FOLD** is a second instance of Vital's own `Distortion` processor, set
  to its sine or linear fold. Its drive has the same range and audio-rate
  smoothing, and its dry/wet mix is done exactly as the drive stage's.
  This is why FOLD alone can be *bit-identical* to Vital's single stage
  (check 2).
- **CRUSH** is new code in natural units. Vital's bit crush and
  down-sample are driven in dB, which would put a dB readout under a BITS
  knob.
  - It quantises to 2^bits levels over −1…1.
  - It holds each quantised sample for `64^rate` engine samples. Rate 0 %
    holds for 1 sample, which is no effect.
  - Bits and hold ramp across the block. The hold counter carries over, so
    CRUSH is exactly block-size independent (check 7a).
  - The hold is in *engine* samples, so the perceived rate scales with
    oversampling (2× by default, where 64 samples is 1.4 kHz).

## Parameters

Appended at `version_added` **0x01000A**. Every automation lane before them
keeps its index (`tests/host_parameters.txt`).

| Name | Range | Default |
|---|---|---|
| `distortion_fold_on` | off / on | off |
| `distortion_fold_type` | Sine / Linear | Sine |
| `distortion_fold_drive` | −30…30 dB | 0 dB |
| `distortion_fold_mix` | 0–100 % | 100 % |
| `distortion_crush_on` | off / on | off |
| `distortion_crush_bits` | 1–16 | 8 |
| `distortion_crush_rate` | 0–100 % | 0 % |

FOLD and CRUSH are off by default, so an existing patch renders
bit-identically (check 1).

## Tests (`tests/test_drive_chain.py`)

1. **Off changes nothing:** bit-identical to a patch without the keys,
   with the drive stage on and off.
2. **FOLD is Vital's fold:** FOLD alone is bit-identical to Vital's single
   stage at the same fold type and drive, sine and linear.
3. **FOLD MIX 0** is bit-identical to FOLD off.
4. **Both stages act:** drive + fold differs from drive alone by 5.2 dB and
   from fold alone by 4.9 dB.
5. **CRUSH bits:** at 1× oversampling, 3 bits leaves 5 distinct output
   values in the sustained note. The clean render has 43,390.
6. **CRUSH rate:** at 50 % the median run of held samples is 8 (= 64^0.5).
7. **Block size:**
   - (a) CRUSH alone is exact, bit-identical at blocks 32 and 512;
   - (b) FOLD alone matches Vital's own fold stage: −113.5 dB (linear)
     and −113.8 dB (sine), from the drive smoothing.

**The bugs put back**, each caught:

| Bug | Result |
|---|---|
| sine and linear swapped | check 2 fails for both |
| no quantisation | 43,390 levels in check 5 |
| hold counter reset every block | −0.8 dB in check 7a |
| the first-use hold ramp that the tests' own design exposed | −2.4 dB in check 7a |

**A check that was replaced.** The first block-size check rendered
drive + fold + crush together. With the fold types swapped, it failed at
−27 dB, which turned out not to be the chain: 5-bit quantisation turns
Vital's own −113 dB block differences into whole-step flips wherever a
sample sits on a step boundary. It passed or failed by where the steps
fell, so it was split into 7a and 7b.
