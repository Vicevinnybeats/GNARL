# Phase 4 — the AI button: a generator

The producer asked for the AI button to make sounds "like the references in
different styles". Two ways were put to them: a generator inside GNARL (no
network, nothing to pay per press), or a language model behind a server (a
key, a cost per press, a connection). With no answer either way, and asked
to continue, the generator was built first. A model can come later, behind
the same button.

## What it does

`ui/src/generate.ts` makes a patch from a seed (mulberry32, so a seed
always makes the same patch, in every browser and in Node). It starts from
Yoi Talk, the styles' shared voice, and sets every setting the styles vary:

- osc 1's table: one of GNARL's seventeen, all but Basic;
- one wob per key (70%: two beats or one) or a wobble looping while held
  (30%: 1/4, 1/8T or 1/8);
- LFO 1's shape: one swell (peak anywhere from 30% to 85%), an early or a
  late peak, or two bumps. The references' wobs mostly turn once;
- WT depth, start frame, unison, drive, fold, OTT and the high shelf, in
  ranges either side of where the styles measured;
- now and then: a crusher, Vital's formant filter moved by the same LFO,
  a pitch bend, flanger, phaser, chorus or reverb.

A table that runs dark or bright through that voice is corrected: a
different shelf and starting frame for PD, Wub, Harmonic, Pulse, Hollow,
Sync and Steps, measured over 40 seeds, with the volume giving back half
the shelf. The name is an adjective, a noun from the table, and the seed.

The button loads the patch the way a starting sound loads: whole, through
`gnarlPresetFactory`. In the plugin that is `setStateInformation`; in the
browser, the worklet's `load`. SAVE keeps it.

## Measured

`tests/test_generate.py` generates seeds 1–24 through
`tools/generate_patches.mjs` and renders each with the desktop renderer at
D1, F1 and F2, 140 BPM. It measures F1 as the references' wobs were (growl
band 150 Hz–6 kHz, the span within 20 dB of the loudest):

| | Generated (24 seeds) | References |
|---|---|---|
| Median centroid | 2204 Hz | 1874–2233 Hz (per track) |
| Share centred at 1.6–2.6 kHz | 92% | — |
| Loudest peak, D1–F2 | −2.9 dBFS | — |

The test also checks:

- that a seed is deterministic;
- that names are unique;
- that every patch loads and renders finite.

With the volume raised 6 dB, the level check fails at +3.1 dBFS.

`ui/tests/bridge.test.mjs` checks that the plugin's panel sends a whole
generated patch. `ui/tests/web.test.mjs` checks that the phone loads one,
names it, shows its table, and plays it.

What the numbers do not say is whether a generated sound is good. The
producer judges that.

## Second version: variations of matched patches

The producer, on the first version's sounds: "very bad … not even close".
Two numbers per wob (brightness, length) were the whole target, and sounds
that share them can sound nothing alike. The generator now starts from
patches that were matched to the references' wobs on the whole spectrogram
over time (`tools/match.py`, phase4-02-matcher.md): Ref Wob 1–6, plus the
producer's own Vinny Bass 2.

A variation does the following:

- moves where the wob starts in the table, and LFO 1's depth (×0.75–1.25);
- shifts LFO 1's turning points by ±8% of the cycle;
- changes the drive (±4 dB), fold (±1.5 dB), shelf (±2 dB) and filter;
- sometimes swaps a one-shot between one beat and two (20%);
- sometimes swaps the table (15%);
- sometimes adds flanger (20%), phaser or reverb (15% each).

It stays a neighbour of a sound that measured close, instead of a point
anywhere in the space.
