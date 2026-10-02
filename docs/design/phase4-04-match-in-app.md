# Phase 4-04: MATCH A SOUND — the matcher inside the app

The producer asked for "the AI engine inside the plugin". The AI engine that
has produced GNARL's closest sounds is the sound matcher (`tools/match.py`,
phase4-02): it searches GNARL settings for the patch whose render is closest
to a target wob. Until now it ran only here, in Python, against the desktop
renderer. This puts it in the app: the plugin's panel and the phone page.

## What the producer does

AI → **MATCH A SOUND** → choose an audio file of one wob (WAV, MP3, anything
the browser decodes; 1–2 s of the bass alone is best) → a progress bar with
STOP → after about a minute and a half, the four closest patches in the AI
sheet, named Match 1–4. Tap to hear, PICK to carry on by ear (phase4-03).

The file never leaves the device. Nothing is fetched; there is no server.

## How

- **The same engine.** `wasm/build/gnarl.wasm`, the engine compiled to
  WebAssembly (phase2-09), runs in Web Workers — one copy each, up to four,
  one fewer than the device's cores. The plugin's page now carries the
  engine too (`ui/scripts/inline.mjs`): 1.26 MB of WebAssembly, 1.7 MB as
  base64, so `gnarl-ui.html` grows from 8.5 MB to 10.2 MB. The plugin still *plays* through its own native engine:
  `hasWebEngine()` is false wherever `window.__JUCE__` exists. Without the
  engine (a build with no `wasm/build`), MATCH is hidden.
- **The same measure.** `ui/src/match/match-core.js` ports match.py's genes,
  patch build and log-mel (growl band 100 Hz–8 kHz, 24 Slaney mels, n_fft
  2048, hop 441, power averaged over 8 frames, normalised, floored at
  −50 dB, mean absolute dB). On the same audio it agrees with Python to
  about 0.1 dB.
- **The search, inside the riddim recipe** (since 2026-10-02,
  `recipe-search.ts`): every candidate is a sound the AI makes from Vinny
  Bass 2 (generate.ts), so every match wobs. A random round stratified by
  rhythm (1/8, 1/8T, 1/4, 1/16, 1/4T), then generations of children of the
  best two of the best four rhythms, steps shrinking. Budget 160 + 12 × 24
  = 448 renders (`MATCH_BUDGET`). It replaced `match-search.js`, which
  searched match.py's one-wob genes - see "Inside the recipe" below.
- **The input.** `decodeAudio` (44.1 kHz mono), `trimToWob` (from the first
  sound 26 dB under the peak, 1.25 s), `detectMidi` (YIN on the bass,
  30–200 Hz; MIDI 29 if it finds nothing). The status line names the note
  in FL Studio's naming.
- **The four.** Best first, distinct in rhythm or table, each levelled
  as the AI's sounds are (rendered, peak at −5 dBFS; generate.ts
  levelVolume).

## Measurements

| Check | Result |
|---|---|
| JS log-mel vs Python on the same audio | within ~0.1 dB |
| In-app render vs desktop render of one patch | 0.20 dB on the measure (was 6.08 dB: the worker held the note; it now releases at 1 s, as match.py's renders do) |
| Known patch (Sig Wob 1's render), Node, 448 renders | closest 3.86 dB; match.py reached 3.38 in 1,320; a random patch scores 5.55 |
| Same, eight seeds (1–8), Node | closest 3.15, 4.22, 4.76, 4.12, 3.12, 3.76, 3.24, 3.77: median 3.77, worst 4.76 |
| Same, in Chromium through the page, seed 1 | 92 s; closest 3.15 dB, the Node figure for seed 1 exactly |
| One unpinned run in Chromium | 5.19 dB: a bad seed, near a random patch's 5.55 |

**The result depends on the seed.** Each MATCH starts a fresh random
search, and over eight seeds the closest found ran from 3.12 to 4.76 dB; one
browser run reached only 5.19. Running MATCH again is a real second
chance, not a repeat. A bigger budget or several searches from different
seeds would narrow it, at the cost of time on a phone.

`ui/tests/web.test.mjs` runs MATCH end to end in Chromium on Wob Triplet
Dry's render at D#3 with the seed pinned to 1 (`window.__GNARL_MATCH_SEED__`)
and requires the closest under 7.0 (the best of thirty random sounds of the
recipe: 7.40). Until 2026-10-02 it used Sig Wob 1's render and 5 dB; with
the held-note bug put back, that version failed at 8.03.

`tools/match_web.mjs` runs the in-app matcher from Node on one thread:

```bash
node tools/match_web.mjs target.wav --midi 39 --out p.vital
```

## Inside the recipe

The first version's matches were what the producer heard as "screech
instead of a wob": match.py's genes build one wob - an LFO that runs once -
so a held note is one sweep and then a steady tone (phase4-05 measured no
beat-locked movement in them). Since 2026-10-02 MATCH searches only sounds
of the AI's recipe, which keep Vinny Bass 2's four LFO routes.

Against Wob Triplet Dry rendered at D#3 (FL) for 1.25 s:

| | Distance |
|---|---|
| The patch itself | 3.78 (its note is held longer than the matcher's render) |
| Thirty random sounds of the recipe | 7.40–14.73, median 9.75 |
| MATCH, Node, seeds 1 and 2 | 5.77, 5.23 - both at 1/8T, the target's rhythm |
| MATCH, Chromium, seed 1 | 6.24, in 105 s |

The distances are larger than the first version's (3–5) because the
candidates are whole Vinny Bass 2 patches with far more that can differ;
they are not comparable across the two. Against a static target (Sig Wob 1)
the closest is 7.27: there is no static sound to find any more.

## Release

The desktop release jobs now `needs: web` and download the engine it built
(artifact `engine-wasm`) before CMake builds the plugin's page.

## What it is not

It is the same matcher at a third of the budget, so on a known patch it is
about 0.5 dB further than a desktop run. It matches the *spectrum over time*
of one wob; it does not hear a drop, and a wob with drums under it measures
the drums too (phase3-02's HPSS is not in the app). The numbers say a match
is close on this measure; the producer decides by ear.
