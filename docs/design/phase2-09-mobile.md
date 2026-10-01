# Phase 2 — the mobile version

**Status:** built and tested (2026-10-01). `ui/dist/gnarl-web.html` is the
phone panel with GNARL's real engine inside, compiled to WebAssembly and
running in an AudioWorklet. Nobody has played it on a phone yet.

## What it is

The panel's phone layout (phase2-02-ui.md) used to play nothing. It drew a
model of the patch, and it was the base "for the mobile version" (that
doc's last line). Now the same page carries the engine:

- **Tap to play.** A phone only lets a page make sound from a tap, so the
  page opens behind a start layer. The tap creates the AudioContext, loads
  the engine and connects the panel.
- **It opens on Vital's init patch**, as Vital and the plugin do: one plain
  saw on osc 1 (`WavetableGroup::loadDefaultGroup` draws the ramp), nothing
  else on - the "beep" a producer expects when a synth opens. The page sends
  nothing at start; the panel shows the engine. (The first version sent the
  panel's own riddim defaults instead; the producer asked for the standard
  init sound.) INIT in the preset sheet returns to it.
- **Everything on the panel works.** Keys, pitch and mod wheels, oscillators
  with FM and FOLD, sub, vowel filter, wobble (rate, depth, smooth, phase,
  shape, drawing), envelopes, the drive chain, OTT and the mod matrix.
  ADVANCED is hidden, because there is no classic editor in a browser.
- **Presets.** Tap the patch name for the preset sheet:
  - **SAVE** under a name;
  - the saved list (tap to load, x to delete);
  - **OPEN FILE** reads a `.vital`;
  - **EXPORT** hands this patch back as one;
  - **INIT** returns to the starting patch.

  The arrows step through the saved patches.
  - The format is the plugin's own (`gnarl_save` / `gnarl_load`), so a
    phone patch opens in the plugin and the other way round.
  - Saved patches stay in this browser (IndexedDB; a patch is about 180 kB,
    mostly wavetables), or for the visit where a browser keeps no storage.
  - A patch from an older version is refused, with the reason: the plugin
    opens it and, saved there once, it opens here.
- **Installable.** On the site, `/app` is an app a phone can put on its
  home screen. `site/public/app/` holds:
  - the manifest, which keeps the retired app's `id` so an old icon updates
    in place;
  - icons rendered from GNARL's mark;
  - a network-first service worker: online a visit always gets the current
    release, offline the last copy plays.

  `assemble_deploy.mjs` links them into the page at deploy. The first
  deploy had no manifest, and Chrome said the app "cannot be installed".
  Checked: the manifest parses with no errors and every icon is the size
  it declares. Headless Chromium's installability check answers "no errors"
  even for a page with no manifest, so it proves nothing and is not used.
- **No server.** One HTML file, about 1.5 MB, which fetches nothing. It can
  be hosted anywhere that serves HTML over https. It was published as a
  private Artifact for the producer to try.

## How it fits together

```
page (bridge.ts, unchanged protocol)
  -> window.__JUCE__, installed by web/host.ts
  -> MessagePort
  -> web/worklet.js (AudioWorkletProcessor; plays web_panel.cpp's part)
  -> web/engine-core.js
  -> wasm/build/gnarl.wasm (gnarl_web.cpp + the engine)
```

The page already talks to an engine through `window.__JUCE__` inside the
plugin. `host.ts` installs one whose other end is the worklet, speaking the
same messages as `src/plugin/web_panel.cpp`: `gnarlConnect`, `gnarlSet`,
`gnarlNote`, `gnarlWobbleShape`, `gnarlRoute` in, and `gnarlValues`,
`gnarlFrame`, `gnarlRoutes` out. So `bridge.ts` and every control run as
they do in the plugin. The differences are in two places:

- `isPlugin()` is false (`initialisationData.gnarlWeb`), which hides
  ADVANCED.
- The connect answer names the preset. If a frame announced it instead,
  the frame would arrive after the page had sent its defaults, and the
  bridge would treat it as a new preset and unlight the wobble shape. The
  bridge test that caught this is `web.test.mjs` "SHAPE still shows".

The build: `wasm/build.sh` makes the module (wasm/README.md). Then
`ui/scripts/inline.mjs` writes `gnarl-web.html` next to `gnarl-ui.html`,
with the module as base64 in a classic script ahead of the app. Without a
built module it skips the web page, so the plugin's build needs no
Emscripten.

## Measured

`tests/test_web.py` renders with `tools/web_render.mjs`, which mirrors
`SynthBase::renderAudioToFile` step for step, and compares the result with
`gnarl-render`. Random phase is off (wasm/README.md says why).

| What | Browser vs desktop |
|---|---|
| init patch | −138.5 dB |
| unison 7, FORMANT warp, mono sub, wobble 1/8T on cutoff | −131 to −140 dB |
| osc FOLD, TUBE, formant filter, reverb, FM knob, CRUSH | −99 to −129 dB |
| OTT | −82 dB |
| a desktop-saved patch with its own wavetable (spectral sine → square morph), a drawn LFO on its position and a drawn wobble, loaded by the browser | gain +0.012 dB; swept frames −41.5 dB once the gain is matched (the maths library, wasm/README.md) |
| the same with time-domain morphing | −131.4 dB |
| that patch saved by the browser, rendered by the desktop | bit-identical to the original |
| osc 2 an octave up | −59 dB over 2 s, growing from −71 to −56 dB through the note: a pitch drift from float rounding, well under a hundredth of a cent |
| pitch wheel at +1 | 65.406 → 73.416 Hz, 2.0000 semitones (bend range 2) |
| mod wheel → cutoff | at 0, bit-identical to no route; at 1, the 5th harmonic falls from −15 to −46 dBc |
| blocks 32 vs 128 (heavy patch, no chorus or CRUSH) | browser −44.2 dB, desktop −44.4 dB |
| speed, heavy patch (7+5 unison voices, every effect) | 4.4× real time (one core of the CI machine) |
| speed, the page's default patch | 5.0× real time for one note, 2.7× for three |

`ui/tests/web.test.mjs` opens the page in Chromium at phone size, taps to
start and checks:
- frames arrive;
- silence before a note, sound on a key;
- the wobble's phase runs;
- MASTER at zero silences;
- the engine's own text comes back;
- a matrix route goes in and out;
- the shape stays lit;
- an idle page sends nothing.

The negative control: with the engine's pitch-wheel line removed,
`test_web.py`'s pitch check reads 0.0000 semitones and fails.

## Found on the way

- **Vital's chorus depends on the block size**: −11.7 dB between blocks of 32
  and 128 on the heavy patch, identical in both builds, against −44 dB
  without it. The desktop plugin has it too, since hosts choose the block
  size. It is not fixed here: a fix changes the chorus's sound, which is a
  change of its own.
- **Chrome will not load a worklet from a `blob:` URL on a `file://` page**,
  whose origin is opaque. Served over http(s) it works. The test serves the
  page; a phone opens it from a URL.
- **An AudioWorklet has no `TextEncoder` or `TextDecoder`.** `engine-core.js`
  encodes UTF-8 itself.
- **Upstream's kissfft fallback is wrong, and the FFT's rounding is audible
  in wavetables.** The first browser build used it (the only path without
  JUCE); a custom wavetable came out 0.3 dB louder than the desktop's. The
  browser build now carries a copy of JUCE's fallback FFT, the one Windows
  and Linux run (wasm/README.md). A consequence for the desktop too: the
  Projucer Linux builds ask JUCE to load FFTW if a machine has `libfftw3f`
  (`JUCE_DSP_USE_SHARED_FFTW=1` in their Makefiles), and on such a machine
  their wavetables can differ the same way. The CMake / JUCE 8 builds do not
  ask, so they always run the fallback.

## Not done

- **Nobody has played it on a phone.** iPhone needs iOS 16.4+ (WebAssembly
  SIMD). The page sets `navigator.audioSession.type = 'playback'` so the
  silent switch does not mute it where Safari supports that (17+); on older
  iOS the switch mutes web audio.
- **CPU on a phone is unmeasured.** The numbers above are a server core. If
  it crackles, the first lever is oversampling (2× by default, as Vital's
  init).
- **Older patches** need the plugin's migration (`updateFromOldVersion`)
  and are refused. Porting it is the next step if producers' existing Vital
  patches should open on the phone directly.
- **Saving pauses audio for a moment** on a slow phone: the worklet builds
  the patch's JSON between blocks. Measured nowhere yet.
- **Hosting.** Before the page is public it needs its GPLv3 notice and a
  link to the source on the page (LICENSING.md). The site's `/app` tombstone
  is the natural home. Vercel builds the site without Emscripten, so that
  needs either a committed module or a CI step that deploys it.
