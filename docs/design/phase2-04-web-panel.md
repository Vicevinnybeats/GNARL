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
| scope press, keys | MIDI note on/off through the plugin's keyboard state |

Anything with no engine parameter behind it yet is **dimmed**, and struck
through on a button, with "Not in the engine yet" on hover:

- FM amount
- the vowel buttons
- FOLD and CRUSH
- TUBE, 2-BAND and the FOLD warp mode
- the wobble's VOWEL route, SMOOTH and PHASE
- the mod matrix

These are the Phase 2 list. The preset arrows are disabled, since there is
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

## Next

1. The producer opens the `GNARL-windows-vst3-juce8` artifact in FL Studio
   and Ableton:
   - the panel appears;
   - knobs record automation;
   - ADVANCED and the G logo switch views;
   - a saved project reopens with the same values.
2. Engine features for the dimmed controls, one at a time (the Phase 2
   list in CLAUDE.md §9).
