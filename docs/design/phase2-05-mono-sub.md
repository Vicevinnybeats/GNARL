# Phase 2, feature 2 — the clean mono sub

**Status:** built and tested (2026-09-30). `tests/test_sub.py` checks it
against the real engine. It is bound to the panel's SUB section. Nobody has
heard it yet: the producer decides whether it sounds right.

## What the producer gets

The SUB panel:

- **on/off** (the panel's dot);
- **LEVEL**;
- **DRIVE**;
- **-1 OCT**.

Under the growl sits a sine at the played note:

- **Clean.** It joins the output *after* every effect and both filters, so
  distortion, OTT or a formant filter on the growl never touch it.
- **Mono.** The same samples in both channels, with no unison and no
  detune. A width or stereo effect cannot smear it.
- **Tight to the note.** It follows the played pitch, including glide and
  pitch bend. Its phase restarts at 0 on every note-on, so every hit starts
  the same way. It rides the amp envelope (env 1), as the oscillators do,
  so a plucked patch gets a plucked sub.

## Why not "put a sine on osc 3 and route it to Direct Out"

That works in Vital and is what producers do there. It costs four settings,
and a wavetable oscillator is not a pure sine. A dedicated path gets:

- one switch;
- a sine that is measurably pure (below);
- a phase reset that does not depend on the oscillator's random-phase
  setting.

## Parameters

Appended at `version_added` **0x010008**, after the wobble, so every
existing host automation lane keeps its index (`tests/host_parameters.txt`).

| Name | Range | Default | Rate |
|---|---|---|---|
| `mono_sub_on` | off / on | off | block |
| `mono_sub_level` | 0–1, quadratic like every Vital level | 0.707 (amplitude 0.5) | block, ramped across the block |
| `mono_sub_octave` | 0, −1, −2 octaves | −1 | block (read once per block) |
| `mono_sub_drive` | 0–100 % | 0 | block, ramped |

**Not `sub_*`.** Those names belong to Vital's retired sub oscillator.
They are still host parameters, dead in the engine, and
`LoadSave::updateFromOldVersion` migrates them into oscillator 3 for old
presets. Reusing them would give an old preset's `sub_on` two meanings.

**Off by default**, so a patch that predates the sub renders bit-identically.
The processor copies its input unchanged when off, rather than adding zero:
`-0.0 + 0.0` is `+0.0`, and the render comparison is byte-exact.

## Engine

`SubOscillator` (`producers_module.{h,cpp}`) is a per-voice processor
between the producers' direct output and `direct_output_`, which the voice
handler multiplies by the amp envelope. `SoundEngine` sums it with the
effect chain's output.

- **Pitch:** `bent_midi_` (glide, bend, voice tune and transpose) minus
  12 × octave. It is control-rate, so the phase increment is interpolated
  linearly across each block, as Vital's oscillators interpolate theirs.
- **Sine:** `sinf`, not Vital's polynomial `futils::sin1`.
  - Measured on the rendered sub: the polynomial's 5th harmonic is at
    **−64 dBc**, which is 164 Hz for a C1 sub, inside the growl's body.
  - `sinf`: every harmonic at or below −93 dBc.
  - Cost: four `sinf` per voice pair per sample, only while the sub is on.
- **Drive:** `tanh(k·x) / tanh(k)`, with `k = 7·drive`.
  - At k = 7 the waveform is close to a rounded square: the 3rd harmonic
    (−10 dBc) that makes a sub audible on a phone speaker, without the buzz
    of a hard clip.
  - At drive 0 the shaper is skipped, not merely made gentle:
    `tanh(x)/tanh(1)` is not the identity.
  - The shaped wave is scaled back to the sine's RMS (CLAUDE.md §3: a drive
    control must not double as a volume control). The make-up gain comes
    from a 65-entry table computed in the constructor and interpolated per
    block. Measured: +0.01 dB at full drive; +2.61 dB without the make-up.
- **Phase reset** on the sample the note starts, not the block's first
  sample. A host places notes between block boundaries, and the renderer
  never does, so check 9 goes through the VST3.
- **Monophonic playing is the patch's job.** A riddim bass plays with
  polyphony 1 or legato, and then there is one sub. Held chords get one sub
  per voice. Selecting only the newest voice from inside per-voice
  processing needs a voice mask Vital does not expose per pair. Left until
  a producer asks for it.

## What the tests measure (`tests/test_sub.py`)

1. **Off changes nothing:** `mono_sub_on = 0` renders bit-identically to
   the same patch with every `mono_sub_*` key removed.
2. **Pitch:** the sub is the difference between renders with it on and
   off. C2 with −1 octave peaks at 32.70 Hz, and −2 octaves at 16.35 Hz,
   within 0.5 %.
3. **Purity:** at drive 0, the 2nd–5th harmonics are each below −80 dBc
   (Blackman-Harris window). Measured: −92.9, −109.5, −110.0 and
   −123.7 dBc.
4. **Clean under the FX:** the same, with distortion at full drive and OTT
   on, and volume −12 dB. The harmonics are identical to the clean render,
   and the RMS is −12.0000 dB.
   - Why −12 dB: at 0 dB the growl plus the sub reached SoundEngine's
     ±2.1 output clamp (122 samples). The clipping put the residual's
     harmonics at −78 dBc.
   - That clamp is the host's safety net, not the sub's path, so the test
     stays below it and asserts that it did.
5. **Mono:** left minus right of the residual is exactly zero.
6. **Level:** at sustain 1, the residual's peak is `level²` within 0.1 dB.
7. **Drive:** at 100 % the 3rd harmonic rises above −20 dBc, and the RMS
   stays within 0.5 dB of drive 0.
8. **Block size:** the residual at blocks 32 and 512 differs by less than
   −100 dB. Measured: −114.4 dB.
9. **Through the VST3** (`--plugin`, `vst3_probe offset=N`): a note starting
   100 samples into a block gives the same sub as one starting at 0,
   shifted by 100 samples. Measured: identical.

**The bugs put back**, each caught:

| Bug | Result |
|---|---|
| sub routed into the FX path | harmonics −0.3 dBc, level −2 dB: 2 failures |
| Vital's polynomial sine | 5th harmonic −64 dBc: 2 failures |
| phase reset at the block start | −7.5 dB in check 9 |
| no RMS make-up | +2.61 dB |

The first threshold for check 3 was −60 dBc. The polynomial sine passed
it, so it was tightened to −80.
