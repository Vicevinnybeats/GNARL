#!/usr/bin/env python3
"""Compares two builds of the GNARL VST3 the way a host sees them.

    python3 tools/compare_plugins.py PROBE OLD/GNARL.so NEW/GNARL.so

PROBE is tools/vst3_probe.cpp, compiled. The gate for moving the plugin's
build (Projucer + JUCE 6 -> CMake + JUCE 8, docs/design/phase2-03-juce8.md):

  1. identity   vendor, classes and class IDs: what a DAW stores in a project
  2. parameters every host parameter, in host order, with its ID
  3. audio      a note rendered through the VST3 wrapper - once on the init
                patch, once after the host changes twelve parameters (every
                effect on, a filter, the wobble routes) - sample for sample

Exits non-zero unless all of them are identical.
"""
import filecmp, os, subprocess, sys, tempfile

# Host parameter IDs (from `probe --params`) and normalised values: every
# effect's switch, filter 1, and the wobble depths and rate (1/8T).
SETTINGS = ','.join([
    '56=1', '1606=1', '1634=1', '1665=1', '48660=1', '48780=1', '51577=1', '51637=1',
    '54610=0.85', '54611=0.7', '54612=0.8', '54614=0.6667',
])


def probe(binary, plugin, *args):
    result = subprocess.run([binary, plugin, *args], capture_output=True, text=True, timeout=300)
    if result.returncode != 0:
        sys.exit(f'probe failed on {plugin} {" ".join(args)}:\n{result.stdout}{result.stderr}')
    return result.stdout


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    binary, old, new = (os.path.abspath(p) for p in sys.argv[1:])
    tmp = tempfile.mkdtemp(prefix='gnarl-plugins-')
    failures = 0

    for name, args in (('identity', []), ('parameters', ['--params'])):
        a, b = probe(binary, old, *args), probe(binary, new, *args)
        same = a == b
        failures += not same
        print(f'{name:<12} {"identical" if same else "DIFFERENT"} ({len(a.splitlines())} lines)')
        if not same:
            for line_a, line_b in zip(a.splitlines(), b.splitlines()):
                if line_a != line_b:
                    print(f'    old: {line_a}\n    new: {line_b}')
                    break

    for name, extra in (('audio init', []), ('audio +12', [SETTINGS])):
        outs = []
        for tag, plugin in (('old', old), ('new', new)):
            path = os.path.join(tmp, f'{name.replace(" ", "_")}-{tag}.f32')
            probe(binary, plugin, '--render', path, *extra)
            outs.append(path)
        same = filecmp.cmp(outs[0], outs[1], shallow=False)
        silent = os.path.getsize(outs[0]) == 0 or not any(open(outs[0], 'rb').read())
        failures += (not same) or silent
        print(f'{name:<12} {"bit-identical" if same else "DIFFERENT"}'
              f'{"  (FAIL: silent render)" if silent else ""}')

    print(f'\n{failures} check(s) failed' if failures else '\nall checks identical')
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()
