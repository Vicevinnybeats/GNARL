#!/usr/bin/env python3
"""Renders the same patches with two gnarl-render binaries and compares them
sample for sample.

    python3 tools/compare_renders.py OLD_RENDERER NEW_RENDERER

This is the gate for moving the build (Projucer + JUCE 6 -> CMake + JUCE 8):
the engine's code does not change, so its output must not either. Every
render is 32-bit float, so a difference far below 16-bit's floor still
shows. Exits non-zero unless every pair is bit-identical.

The patches are built from the OLD binary's own `--save` of its init patch
(tools/make_test_patch.py), so each one is complete, and they switch on as
much of the engine as possible: every effect, the filters, unison, warp
modes, the noise/sample oscillator and the wobble routes.
"""
import os, subprocess, sys, tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import wavio  # noqa: E402

PATCH_DIR = os.path.join(HERE, '..', 'tests', 'patches')

VARIANTS = {
    'init': {},
    'all_fx': dict(chorus_on=1, compressor_on=1, delay_on=1, distortion_on=1, eq_on=1,
                   filter_fx_on=1, flanger_on=1, phaser_on=1, reverb_on=1),
    'growl': dict(osc_1_distortion_type=2, osc_1_distortion_amount=0.6, osc_1_unison_voices=7,
                  osc_2_on=1, osc_2_distortion_type=1, osc_2_distortion_amount=0.4,
                  sample_on=1, filter_1_on=1, filter_1_resonance=0.6, filter_2_on=1,
                  wobble_rate=2, wobble_amount_cutoff=0.7, wobble_amount_wave_frame=0.5,
                  wobble_amount_fm=0.4, distortion_on=1, compressor_on=1),
}
FILES = ['fast_tail.vital', 'long_tail.vital']
BLOCKS = [64, 512]


def run(binary, *args):
    subprocess.run([binary, '--headless', *args], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    old, new = (os.path.abspath(p) for p in sys.argv[1:])
    tmp = tempfile.mkdtemp(prefix='gnarl-compare-')

    init = os.path.join(tmp, 'init.vital')
    run(old, '--save', init)
    patches = {}
    for name, overrides in VARIANTS.items():
        path = os.path.join(tmp, f'{name}.vital')
        pairs = [f'{k}={v}' for k, v in overrides.items()]
        subprocess.run([sys.executable, os.path.join(HERE, 'make_test_patch.py'), init, path, *pairs],
                       check=True)
        patches[name] = path
    for f in FILES:
        patches[f.rsplit('.', 1)[0]] = os.path.join(PATCH_DIR, f)

    failures = 0
    print(f'{"patch":<12} {"block":>5}  {"frames":>7}  result')
    for name, patch in patches.items():
        for block in BLOCKS:
            outs = []
            for tag, binary in (('old', old), ('new', new)):
                wav = os.path.join(tmp, f'{name}-{block}-{tag}.wav')
                run(binary, '-o', wav, '-l', '4', '-m', 'C1', '-b', '140',
                    '--bits', '32', '--block', str(block), patch)
                outs.append(wav)
            a, sr_a = wavio.read(outs[0])
            b, sr_b = wavio.read(outs[1])
            if a.shape != b.shape or sr_a != sr_b:
                result = f'DIFFERENT SHAPE {a.shape}@{sr_a} vs {b.shape}@{sr_b}'
                failures += 1
            elif np.array_equal(a, b):
                peak = np.max(np.abs(a))
                result = 'bit-identical' + ('' if peak > 0 else '  (WARNING: silent render)')
            else:
                diff = np.max(np.abs(a - b))
                peak = max(np.max(np.abs(a)), 1e-30)
                result = f'DIFFERS: max |diff| {20 * np.log10(diff / peak):.1f} dB re peak, ' \
                         f'{np.count_nonzero(a != b)} samples'
                failures += 1
            print(f'{name:<12} {block:>5}  {a.shape[0]:>7}  {result}')

    print(f'\n{failures} of {len(patches) * len(BLOCKS)} renders differ' if failures
          else f'\nall {len(patches) * len(BLOCKS)} renders bit-identical')
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()
