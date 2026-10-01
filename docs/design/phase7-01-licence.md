# Phase 7 — the licence check, in the fork

**Status:** ported and tested (2026-10-01). `tests/licence_tests.cpp` checks
the policy and the client. Enforcement is OFF in every build until an
endpoint is configured: CI's builds and the artifacts the producer installs
say "Development build" and save presets normally.

The policy was written and tested on the retired engine (branch
`custom-engine-archive`, `plugin/source/license/`). The backend that answers
it is `backend/` (Cloudflare Worker + D1, docs/backend.md). This brings the
client into the Vital fork.

## The rules (CLAUDE.md §8, unchanged)

- **It never silences the plugin.** No licence state reaches the audio
  thread. `State::audioAllowed()` is a compile-time `true`, so anyone looking
  for the code path that stops the sound finds that constant instead.
- **Thirty days of offline grace.**
- **Only an explicit `valid` / `rejected` is a decision.** Every error,
  timeout, non-200 (the Worker's own 503 included), and every page that is
  not our JSON (a captive portal) opens the grace period.
- **The key lives in machine settings, never in a preset or a project.**
  `GNARL.settings` in `~/.config/GNARL` on Linux, `GNARL` under
  Application Support on macOS, and `GNARL` in AppData on Windows.
- **The machine id sent is a SHA-256 of the device id**, salted with the
  product name and truncated to 32 hex characters. The raw id never leaves
  the machine.
- A customer can remove the check (GPLv3). It makes paying the easy path.

## What is gated

**Saving preset files.** Nothing else: GNARL's AI features are Phase 4,
which is not built.

- The gate is `SynthBase::presetSavingAllowed()`, asked at the top of
  `SynthBase::saveToFile`.
- Every build answers yes except the licensed plugin. Vital's editor saves
  through that function. (The standalone app is a Projucer build, so its
  gate always answers yes.)
- When the gate refuses, the plugin says why in an alert, because Vital's
  save dialog has no other way to find out.
- A DAW's project **never** goes through it: `getStateInformation` writes the
  state directly. A customer whose licence lapses loses no session. A
  refused preset save is the whole penalty, plus the banner.

## Where it lives

`src/plugin/licence/`:

| File | What |
|---|---|
| `licence_state.{h,cpp}` | The seven statuses, the gate, the banner's sentences |
| `licence_manager.{h,cpp}` | The policy: grace, timeout, restore. Verifier and clock injected |
| `licence_client.{h,cpp}` | The request, the three-answer reading, the fingerprint, settings storage |

These files are in the CMake / JUCE 8 build only (`GNARL_LICENSING`), whose
panel shows the banner. The Projucer builds compile none of them; their
`presetSavingAllowed` is SynthBase's `true`.

`SynthPlugin` starts it (`startLicensing`):

| Build | Status | Saving | Banner |
|---|---|---|---|
| no `GNARL_LICENCE_ENDPOINT` (every build today) | unenforced | on | "Development build - licence checking is not configured." |
| `-DGNARL_PERSONAL_BUILD=ON` | personal | on | none |
| an endpoint, a valid key | licensed | on | none |
| an endpoint, offline after a licence | offline | on | days left |
| ... more than 30 days | expired | off | "Audio still works. ..." |
| an endpoint, rejected | invalid | off | says so |
| an endpoint, no key yet | unlicensed | off | asks for a key |

Turning enforcement on is one setting, given when the Worker is deployed:

```bash
cmake -B build -DGNARL_LICENCE_ENDPOINT=https://gnarl-licence.<sub>.workers.dev/activate
```

The same setting turns on libcurl for the Linux build (`JUCE_USE_CURL`,
`NEEDS_CURL`). Without an endpoint there is nothing to reach, and the
default build stays exactly as it was. `tools/compare_plugins.py` still
finds it bit-identical to the JUCE 6 plugin.

## Changed from the retired engine's version

- **The timeout is enforced on the message thread.** The old manager called
  the verifier inside its worker thread and measured how long it took
  afterwards. A verifier that never returned was therefore never reported:
  the comment said "abandoned", but the code waited. Now a 50 ms timer
  decides the check at `kTimeoutMs` (8 s) as unreachable. A late answer is
  dropped: `run()` stores its reply only into an empty slot, and the timeout
  path has already filled it.
- **The last successful check is saved** beside the key (`saveVerification`)
  and restored at start-up. A machine that goes offline then starts its
  grace period from the right day, rather than looking never-licensed.
- Restyled to Vital's conventions (CLAUDE.md §11) and renamed "licence".
  The settings property name `licenceKey` is kept, and frozen.

## The panel

- `web_panel.cpp` adds `licence` to the connect answer and sends
  `gnarlLicence {status, message, saving, hasKey}` whenever it changes.
- The page sends `gnarlLicenceKey {key}`. The plugin stores it, swaps in a
  verifier with the new key and checks.
- `main.ts licenceChip`: a chip in the header (desktop and phone) that
  appears only when there is a sentence to show.
  - It reads NOT SAVING while saving is off, and is amber for a state that
    needs a hand, dim for news (offline, development build).
  - Never red: nothing has stopped.
  - Tapping it shows the sentence and a key field. A development build has
    no field, because nothing would check the key.
  - Typing in the field plays no notes.
- The mobile version (phase2-09) has no licence: its host never sends the
  event, and the chip stays hidden.

## Tested

`gnarl_licence_tests` (43 checks, in CI):

- **Policy:**
  - audio allowed in all seven states;
  - valid licenses;
  - unreachable after a licence goes offline with the days counting down
    (27 left after 3, "1 day" singular on day 29);
  - expired on day 30, and reconnecting restores it;
  - a rejection gets no grace;
  - never-licensed and unreachable is unlicensed, not in grace;
  - a hung verifier is decided at the timeout (303 ms with a 300 ms
    timeout) and its late rejection is dropped;
  - a restore 10 days on has 20 left, and `refreshGrace` notices expiry;
  - three overlapping checks run the verifier once;
  - unenforced and personal never drift.
- **Client:**
  - a 503 whose body says `rejected` is unreachable;
  - only 200 + valid/rejected decides;
  - eight non-decisions (portal HTML, unknown status, `{}`, empty, 401,
    302, 0, 500);
  - the body's fields, an omitted empty label, punctuation in a key;
  - the fingerprint is 32 hex characters, stable, and not the raw id;
  - no endpoint or no key is unreachable;
  - the stored key round-trips trimmed and clears.

Negative controls:
- trusting a non-200 body fails three checks (the 503 and the 401);
- removing the timeout fails the hang check (the check took 15 s, the
  test's limit, instead of 0.3 s).

`ui/tests/bridge.test.mjs`:
- an expired licence shows NOT SAVING with the message;
- the typed key reaches the plugin, and typing plays no notes;
- a licensed answer hides the chip.

## Checked end to end

Setup:
- the JUCE 8 Standalone built with
  `-DGNARL_LICENCE_ENDPOINT=http://127.0.0.1:8789/activate`;
- a local fake Worker that answers whatever a file says;
- Xvfb, with the settings folder emptied first.

| Step | What happened |
|---|---|
| fresh machine, no key | no request sent; chip NOT SAVING; "Unlicensed - audio works; enter a licence key to save presets." |
| key typed, server says valid | one POST: `{key, machineId: 4aebd24a…(32 hex), machineLabel: "vm"}`, no raw device id; chip gone; `~/.config/GNARL/GNARL.settings` holds the key, `licenceEverVerified` and the time |
| restart, server returns 503 | offline, "30 days left", chip LICENCE (dim), saving on |
| restart, server says rejected | invalid, chip NOT SAVING, "Audio still works; preset saving is disabled." |

Found on the way:
- **The popover was open before it was tapped**: its `display: flex`
  overrode the `hidden` attribute. Fixed, and the bridge test checks it. The
  check fails with the fix removed.
- **The chip wrapped the header** ("NOT / SAVING", "AI / PRESET"). Fixed.
- **Characters were dropped when xdotool typed fast** into the key field of
  the software-rendered web view under Xvfb. At 250 ms per key only the last
  one was lost; the same field in Chromium loses nothing. Typing and pasting
  a key in a real DAW is on the producer's list.

Not exercised end to end: the refused save itself (the classic editor's
save dialog with the alert). The gate is one line in `saveToFile`, and
`presetSavingAllowed` follows `featuresAllowed`, which the tests cover.

## Not done

- **The Worker is not deployed** (docs/backend.md: `wrangler deploy` is run
  by a person) and Stripe is not connected. Until both are, every build
  stays unenforced. That is correct, and the banner says so.
- **The JUCE 6 / Projucer builds have no check.** They show Vital's editor
  and are on their way out. A shipped build is the JUCE 8 one.
