# GNARL

A wavetable synthesizer for riddim and dubstep sound design. VST3 for
Windows and macOS.

GNARL is a fork of **[Vital](https://github.com/mtytel/vital)** by Matt
Tytel, used under the GNU General Public License v3. See
[LICENSING.md](LICENSING.md) for what that means for this repository, and
[LICENSE](LICENSE) for the licence itself.

GNARL is **not** Vital, is not endorsed by or affiliated with Matt Tytel or
Vital Audio, and does not use their names or branding. It will not connect
to any Vital service.

---

## Layout

```
src/synthesis/     the audio engine - oscillators, filters, effects,
                   modulators. Almost entirely JUCE-free.
src/interface/     the OpenGL UI, skinned through src/interface/look_and_feel
src/common/        preset load/save, tuning, the synth base class
src/plugin/        the VST3 / AU entry point
src/standalone/    the standalone application
src/headless/      an offline renderer: preset in, WAV out

plugin/ standalone/ headless/   Projucer projects and generated build files
third_party/       JUCE 6.0.5, the VST3 SDK, kissfft, json, concurrentqueue

site/              the marketing site (separate program, not GPL)
backend/           the Cloudflare Worker for licence activation (ditto)
tools/             site tooling and, from Phase 3, render + compare scripts
docs/
```

## Building

See [CLAUDE.md](CLAUDE.md) for the full build notes and the constraints
that apply when changing audio code.

```bash
# Linux (VST3)
make vst3 CONFIG=Release

# Linux (headless renderer - preset in, WAV out)
make headless_server CONFIG=Release
headless/builds/linux/build/gnarl-render --headless -o out.wav -l 4 -m C1 -b 140 patch.vital
```

Windows and macOS build from the generated projects in `plugin/builds/vs17`
and `plugin/builds/osx`, and in CI from the workflows in `.github/`.

## Attribution

Vital is copyright Matt Tytel and licensed GPLv3. The engine, the interface
framework and the preset format in this repository derive from that work.
Every modification GNARL makes is recorded in this repository's git history
from the merge commit onward, and summarised in [CHANGES.md](CHANGES.md).
