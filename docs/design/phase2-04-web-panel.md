# The web panel inside the plugin

**State:** built and working in the JUCE 8 plugin on Linux (the Standalone
wrapper under Xvfb). Windows and macOS build in CI; nobody has opened the
panel in FL Studio or Ableton yet. The JUCE 6 plugin, the one that ships, is
unchanged and still shows Vital's editor.

## What it is

The JUCE 8 plugin opens on GNARL's own panel (`ui/`, the producer's layout)
instead of Vital's editor:

- **ADVANCED** (in the panel's header) shows Vital's full editor.
- **The G logo** in Vital's editor comes back to the panel.

Both are the same plugin state; only the view changes.

- `src/plugin/web_panel.{h,cpp}`: a `WebBrowserComponent` serving the panel
  and bridging it to the engine.
- `ui/src/bridge.ts`: the page's half.
- `CMakeLists.txt` builds `ui/` with npm into one HTML file and embeds it
  (`juce_add_binary_data`). The page is served from memory and makes no
  network request.

## How values move

Everything in `web_panel.cpp` runs on the **message thread**. The audio
thread sees no new code path:

- **Page to engine:** `SynthBase::valueChangedInternal`, the call Vital's
  own knobs make. It sets the control and tells the host, so a knob turned
  on the panel records automation.
- **Knob grabbed and let go:** `beginChangeGesture` / `endChangeGesture`.
- **Engine to page:** a 30 Hz timer compares every bound parameter with the
  last value sent and sends what changed. That covers automation, preset
  loads and Vital's editor.
- **The scope:** Vital's oscilloscope memory, read without a lock as Vital's
  editor reads it.
- **The wobble:** its phase, and its shape when the shape changes.

The page sends the **host's** 0..1 value, linear in the engine's value (as
`ValueBridge` defines it). The engine answers with its own readout text,
because only the engine knows its units and skews. The page shortens that
text to three significant figures (`tidyText`).

| Panel | Engine |
|---|---|
| knobs with a name in `params.ts` | that parameter |
| OSC mode FORMANT / SYNC / BEND | `osc_N_distortion_type` 2 / 1 / 4 |
| DIST HARD / SOFT | `distortion_type` 1 / 0 |
| OTT 3-BAND | `compressor_enabled_bands` 0 (Multiband) |
| section dots | `osc_1_on`, `osc_2_on`, `filter_1_on`, `distortion_on`, `compressor_on` |
| wobble DEPTH × destination toggles × section dot | `wobble_amount_wave_frame` / `_cutoff` / `_fm` = DEPTH when both are on, else 0 |
| wobble SHAPE | the wobble's LineGenerator: sine, rounded square, or the 16 drawn steps |
| SUB dot, LEVEL, DRIVE, -1 OCT | `mono_sub_on`, `_level`, `_drive`, `_octave` (docs/design/phase2-05-mono-sub.md); MONO is fixed on, since the sub is mono by construction |
| VOWEL FILTER knobs, A E I O U | follow filter 1's model; see docs/design/phase2-06-vowel-filter.md |
| wobble VOWEL destination | `wobble_amount_formant` |
| FOLD, CRUSH tiles | `distortion_fold_*`, `distortion_crush_*` (docs/design/phase2-07-drive-chain.md) |
| scope press, keys | MIDI note on/off through the plugin's keyboard state |
| PITCH, MOD wheels (left of the keys on a phone, beside the scope on a desktop) | `pitch_wheel`, `mod_wheel`, plus `pitchWheelGuiChanged` / `modWheelGuiChanged` as Vital's own wheels call: the voices read the engine's MIDI state, not the control. PITCH springs back to centre inside the gesture |

The controls that used to be dimmed (FM, TUBE, 2-BAND, CRUSH HARD/SOFT,
FOLD warp, wobble SMOOTH and PHASE, the mod matrix) are bound since
docs/design/phase2-08-panel-controls.md, which also gives the matrix's
`gnarlRoute` / `gnarlRoutes` protocol. A control with no engine path would
still be dimmed, struck through, with "Not in the engine yet" on hover; the
bridge test fails if any is. The preset arrows are disabled, since there is
no preset browser behind them yet; the name shown is the engine's.

## Found on the way

**1. The wobble macro went silent when Vital's editor was open or closed.**
This affects the JUCE 6 plugin too.

- Vital disables a modulation source whenever its editor button is inactive
  or destroyed and the source has no *matrix* connection
  (`ModulationButton` → `forceShowModulation` → `disableModSource`).
- The wobble's routes are fixed, not matrix connections, so Vital never
  found one and switched the wobble off.
- The renderer and the probe never create an editor, so no test could see
  it.
- Seen on the panel: the wobble's phase read exactly 0 with a note held.
  An LFO that is running reports its phase packed with a voice count, which
  is always at least 1.
- Fixed in `SynthVoiceHandler::disableModSource`, beside Vital's own
  exception for `env_1`. After the fix the phase advances.

**2. JUCE 8's Linux web view could not load a page larger than 64 KB.**
Two bugs in the pipe between the plugin and its WebKit child process:

- a character count used as a byte count;
- partial non-blocking reads that discarded data.

The child died with `std::bad_alloc`. Both are fixed in the JUCE patch (see
its README).

**3. The page was decoded as windows-1252** in the Linux web view, whatever
its meta tag and Content-Type said (`document.characterSet` reported it).
The build now emits pure ASCII: `vite.config.ts` sets `charset: 'ascii'`
and `inline.mjs` fails the build if anything non-ASCII survives.

**4. The fonts came from Google** and failed offline. The fallback font is
wider, and the grid overflowed the window by a third. The fonts are now
bundled (`ui/src/fonts.css`, woff2, Latin only, SIL OFL 1.1), and the page
makes no requests.

**6. The page sent every engine echo back, forever** (~12,000 values a
second idle) until 2026-09-30's fix; see phase2-06-vowel-filter.md.
`ui/tests/bridge.test.mjs` now guards it in CI.

**5. Vital's editor showed stale values after the panel changed them.**
`valueChangedInternal` deliberately does not redraw Vital's editor, because
it assumes the editor made the change. Switching to the editor now calls
`updateFullGui()` first.

## Verified (Linux, JUCE 8 Standalone wrapper, Xvfb)

- The page connects:
  - engine readouts under the knobs;
  - the init patch's state (OSC 2, filter, DIST and OTT off);
  - Vital's default wobble triangle.
- Panel to engine: OSC 2 on, OSC 2 warp, wobble rate 1/8T, SINE shape and
  the CUTOFF route at 70 %. Each is visible in Vital's editor after
  ADVANCED.
- Engine to panel: the WT route turned on in Vital's editor shows on the
  panel after the G logo.
- The scope, with a note held through a null ALSA device, shows the
  engine's real output.
- The wobble's playhead follows the engine while a note plays and
  disappears after release.
- Sound and parameters: `compare_renders.py` and `compare_plugins.py`, as
  in phase2-03.

**Not verified:**
- WebView2 on Windows or WKWebView on macOS;
- any DAW;
- a HiDPI display.

On Windows the panel needs the **WebView2 runtime**, which Windows 11 and
updated Windows 10 include.

## Opening fast, and the host's keys (2026-10-04)

The producer: the plugin "takes some time to load... Serum or Vital open
much faster", and "when I press F5 or F6 in FL Studio it doesn't open or
close". Measured, three costs, each removed:

| Cost | Where | Before | After |
|---|---|---|---|
| Vital's editor, built hidden behind the panel on every open | `SynthEditor` | 2.8 s (Linux, Xvfb, software GL) | built when ADVANCED is first pressed |
| MATCH's eighteen wavetables, built at page load | `match.ts` | ~1.6 s of the page's 2.4 s | built on MATCH's first use |
| 12 MB of patches and engine as JavaScript string literals | `inline.mjs` | parsed as code before the panel ran | inert `<script type="text/plain">` blocks, read on first use (`src/data.ts`); each wavetable stored once (45 patches, 230 distinct tables: 14.3 MB -> 7.6 MB) |

The page (headless Chromium, desktop size, three loads each): visible after
2.3-2.5 s before, 0.44-0.58 s after. The editor's construction to a made
panel: 189 ms on Linux. WebView2's own start-up on Windows is not measured
here. Pressing ADVANCED now builds Vital's editor (checked under Xvfb: it
appears, and the G logo returns to the panel).

**Keys.** A JUCE component that ignores a key posts it to the host's window
(`forwardMessageToParent`), which is how FL Studio's F5/F6/F7/F9 and Space
reach FL from Vital's editor. The web view keeps every key for itself - and
F5 there means reload the page. The panel now takes F1-F12 and Space
(outside text fields), stops the web view acting on them, and sends
`gnarlHostKey`; `src/plugin/host_keys.cpp` posts the key to the editor
window's parent exactly as JUCE does. Windows only for now: macOS and Linux
hosts are untested with the panel. `bridge.test.mjs` checks the keys sent;
that FL acts on them needs FL.

## Next

1. The producer opens the `GNARL-windows-vst3-juce8` artifact in FL Studio
   and Ableton:
   - the panel appears;
   - knobs record automation;
   - ADVANCED and the G logo switch views;
   - a saved project reopens with the same values.
2. ~~Engine features for the dimmed controls~~ - done,
   docs/design/phase2-08-panel-controls.md.
