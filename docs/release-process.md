# Release process

What has to happen, in order, to put a build in front of a customer. Written
down because a release is the one thing that is done rarely, under time
pressure, and cannot be un-done once a binary is out.

**Phase 9 is not started.** The steps below that need something we do not have
yet — code-signing certificates, a notarisation account, an installer
pipeline — are marked **BLOCKED** and say exactly what is needed and who has
to get it. Everything else is live now.

---

## 0. What a release is

| | |
|---|---|
| Version | `project(GNARL VERSION …)` in the root `CMakeLists.txt` — the single source |
| Artefacts | VST3 (macOS, Windows), AU (macOS), Standalone (macOS, Windows) |
| Not shipped | Linux builds. They work and CI uses them; they are not a release target. |

## 1. Version and compatibility gate

Before anything is built, answer these. A "yes" to any of them is a
compatibility event and changes what the release is.

- [ ] Did any **parameter ID** change, or get removed? → **Stop.** IDs are
      frozen once shipped. Renaming one breaks every saved preset and every
      host automation lane pointing at it. Retire a parameter by leaving the
      ID declared and no longer reading it.
- [ ] Was an entry **inserted into the middle** of a choice list? → **Stop.**
      A preset stores the index, not the name. Append only.
- [ ] Did **parameter creation order** change in `ParameterLayout.cpp`? →
      **Stop.** It sets the index a host shows in its automation list.
- [ ] Did the **meaning** of an existing parameter change? → Bump
      `pid::kStateVersion` and write the migration in `setStateInformation`.
- [ ] Did the **preset file's shape** change? → Bump
      `preset::kFormatVersion`. This is a different number from the one
      above; see [`preset-format.md`](preset-format.md).
- [ ] Did anyone touch `pid::kParameterVersionHint`? → **Stop.** It must
      never change. JUCE folds it into the AU parameter ID, so bumping it
      invalidates every AU automation lane a customer has drawn.

Then set the version in the root `CMakeLists.txt`. Nowhere else.

## 2. Regenerate everything generated

These go stale **silently** — the build succeeds and the product is wrong.
Each has been wrong in this repository at least once.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DGNARL_BUILD_DEMO_RENDERER=ON

# Parameter defaults for the browser preview
cmake --build build --target GnarlDumpDefaults
./build/tests/GnarlDumpDefaults ui/src/bridge/parameterDefaults.json

# Reference vectors for the TypeScript mirrors of C++ logic
cmake --build build --target GnarlDumpReference
./build/tests/GnarlDumpReference ui/tests/referenceVectors.json

# Wavetable spectra for the animated display
cmake --build build --target GnarlDumpSpectra
./build/tests/GnarlDumpSpectra ui/src/bridge/wavetableSpectra.ts
```

`ParameterIDs.h` and its TypeScript mirror `ui/src/bridge/parameterIds.ts` are
also generated — regenerate rather than hand-editing, and commit both in the
same commit.

Then confirm nothing drifted:

```bash
npm --prefix ui run check-reference
```

## 3. Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

- [ ] The whole suite is green. Not "green except".
- [ ] CI is green on macOS **and** Windows.
- [ ] `pluginval --strictness-level 10` passes on the VST3 (both platforms)
      and the AU. **Strictness 10 is the bar; do not lower it to get green.**

Benchmarks in the CLAUDE.md performance table are **Release** figures. Debug
is ~15× slower here and a Debug measurement of DSP code is a different shape
of the truth, not a slow version of it.

## 4. Test in real hosts, by hand

CI and pluginval do not catch everything a host does. At minimum, on each
shipping platform:

- [ ] Load, play, close, reload — in **Ableton Live**, **FL Studio** and
      **Logic** (AU, macOS).
- [ ] Save a project, close the host completely, reopen: the patch recalls
      identically.
- [ ] Automate a parameter, then reopen the project: the lane still points at
      the same parameter.
- [ ] Change the sample rate and the buffer size **while a note is held**.
- [ ] Open the interface, switch themes, resize the window.
- [ ] Load every factory preset and listen to it. A preset built from DSP
      knowledge and never heard is not a shipped preset.

## 5. Sign and notarise — **BLOCKED**

Neither platform will let a customer run an unsigned plugin without a warning
most people will not click through.

**macOS** — needs an Apple Developer Program membership ($99/yr), a
*Developer ID Application* certificate and an app-specific password for
notarisation. Then `codesign` every bundle, `notarytool submit --wait`, and
`stapler staple`.

**Windows** — needs an OV or EV code-signing certificate from a CA. EV is
required to avoid a SmartScreen warning on a new publisher. Then `signtool`
over the VST3 and the installer.

> **Needed from the project owner:** the Apple Developer membership and the
> Windows certificate. Nothing else in this section can start without them,
> and the Windows EV certificate in particular can take days to issue.

## 6. Installers — **BLOCKED on step 5**

- macOS: a signed and notarised `.pkg` that installs the VST3, the AU and the
  Standalone.
- Windows: an installer (Inno Setup or WiX) that installs the VST3 and the
  Standalone, and offers the install location.

An installer must be signed too — signing the plugin and shipping it in an
unsigned installer warns the customer anyway.

## 7. Legal check

- [ ] Every shipped wavetable is one we generated or hold a commercial
      licence for. **Nothing** imported from Serum, Vital, Massive, Malström
      or any other commercial product.
- [ ] No artwork, colour scheme, knob art, panel texture or typeface taken
      from another product. Matching Serum's *information architecture* is a
      category convention and is fine; matching its look is not.
- [ ] The background artwork's licensing position is settled — see
      [`artwork-brief.md`](artwork-brief.md).
- [ ] No private key, certificate or `.env` is in the repository.
      `.gitignore` covers `*.pem`, `*.p12`, `*.key` and `.env`; confirm rather
      than assume.

## 8. Licence server readiness — **BLOCKED on Phase 7**

- [ ] **`GNARL_LICENCE_ENDPOINT` is set at configure time.** Empty is the
      default and it turns enforcement OFF — the plugin reports
      `Status::unenforced` and its banner reads "Development build". A
      release build with this unset is a release build that never checks.
      `LicenseTests` asserts that a build *with* an endpoint never reports
      `unenforced`, so the suite catches the inverse, but nothing can catch
      a release configured without one except this line.
- [ ] Activation endpoint live, and load-tested for a launch-day spike.
- [ ] **Verify by pulling the network cable, not by reading the code**: audio
      keeps playing, preset saving disables, the banner appears, and the
      30-day grace period counts down correctly.
- [ ] Confirm what happens when the server answers *slowly* rather than not
      at all — the client has an 8 s timeout and must not block the interface
      while it waits.

## 9. Ship

- [ ] Tag the commit `v<version>`.
- [ ] Upload the signed installers.
- [ ] Publish release notes, including **any compatibility event from step
      1**, in plain language: "presets from 0.9 load correctly" or "the decay
      curve parameter now means X; older presets are migrated on load".
- [ ] Update the manual ([`manual.md`](manual.md)) for anything that changed.

## 10. After

- [ ] Keep the exact source of every shipped build tagged. A customer bug
      report against 1.0.2 is not reproducible against `main`.
- [ ] Keep the signed artefacts. Re-signing is not reproducible either.

---

## The short version

Generated files regenerated, suite green, pluginval 10 green on both
platforms, every factory preset actually listened to, signed and notarised,
licence failure tested by unplugging the network, tagged.
