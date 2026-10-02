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
- **The same search**, smaller: `match-search.js`, a random round stratified
  by table then generations from the best two of the six best tables.
  Budget 160 random + 12 × 24 = 448 renders (`MATCH_BUDGET`), against 1,320
  for the desktop runs.
- **The input.** `decodeAudio` (44.1 kHz mono), `trimToWob` (from the first
  sound 26 dB under the peak, 1.25 s), `detectMidi` (YIN on the bass,
  30–200 Hz; MIDI 29 if it finds nothing). The status line names the note
  in FL Studio's naming.
- **The four.** Best first, distinct in table or filter, each with its
  volume set so its peak is at −4.5 dBFS (matched patches otherwise came
  out from −14 to −2 dBFS at one volume).

## Measurements

| Check | Result |
|---|---|
| JS log-mel vs Python on the same audio | within ~0.1 dB |
| In-app render vs desktop render of one patch | 0.20 dB on the measure (was 6.08 dB: the worker held the note; it now releases at 1 s, as match.py's renders do) |
| Known patch (Sig Wob 1's render), Node, 448 renders | closest 3.86 dB; match.py reached 3.38 in 1,320; a random patch scores 5.55 |
| Same, in Chromium through the page, 4 workers | 88 s; four matches 3.34–3.95 dB |

`ui/tests/web.test.mjs` runs MATCH end to end in Chromium on Sig Wob 1's
render and requires the closest under 5 dB. With the held-note bug put
back, it fails at 8.03 dB.

`tools/match_web.mjs` runs the in-app matcher from Node on one thread:

```bash
node tools/match_web.mjs target.wav --midi 29 --out p.vital
```

## Release

The desktop release jobs now `needs: web` and download the engine it built
(artifact `engine-wasm`) before CMake builds the plugin's page.

## What it is not

It is the same matcher at a third of the budget, so on a known patch it is
about 0.5 dB further than a desktop run. It matches the *spectrum over time*
of one wob; it does not hear a drop, and a wob with drums under it measures
the drums too (phase3-02's HPSS is not in the app). The numbers say a match
is close on this measure; the producer decides by ear.
