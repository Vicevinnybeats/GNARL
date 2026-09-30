#!/usr/bin/env python3
"""The vowel buttons produce the vowels they are labelled with.

    python3 tests/test_vowel.py [path/to/gnarl-render]

docs/design/phase2-06-vowel-filter.md. Exit status is the number of failures.

ui/src/vowels.json says where each of the panel's A E I O U buttons puts
filter 1's formant filter (style, X, Y). For each entry this renders white
noise (the sample oscillator) through that setting and through no filter,
and divides: what is left is the filter's own magnitude response, with the
noise's randomness cancelled. The vowel is the one whose first two formant
frequencies - Vital's own tables, formant_filter.cpp - carry the most
response. Each must win by at least 6 dB.

A NEGATIVE CONTROL runs the same classifier on the map with two vowels
swapped, which must fail: otherwise the test could pass on any map.
"""
import json, os, subprocess, sys, tempfile
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'tools'))
import wavio  # noqa: E402

RENDER = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'headless/builds/linux/build/gnarl-render')
VOWELS = {k: v for k, v in json.load(open(os.path.join(HERE, '..', 'ui/src/vowels.json'))).items() if len(k) == 1}
TMP = tempfile.mkdtemp(prefix='gnarl-vowel-')
FAILS = []

# F1 and F2 of each vowel as MIDI notes, from formant_filter.cpp.
FORMANTS = {'A': (75.7552, 84.5455), 'E': (67.3500, 92.3995), 'I': (61.7826, 94.0496),
            'O': (67.3500, 79.3500), 'U': (65.0382, 74.3695)}
# White noise from the sample oscillator into filter 1, oscillator 1 off.
NOISE = dict(osc_1_on=0, sample_on=1, sample_destination=0, env_1_sustain=1)
FORMANT_MODEL = 5


def check(ok, msg):
    print(('PASS ' if ok else 'FAIL ') + msg)
    if not ok:
        FAILS.append(msg)


def hz(midi):
    return 440 * 2 ** ((midi - 69) / 12)


def render(base, name, **overrides):
    d = json.loads(json.dumps(base))
    for k, v in overrides.items():
        if k not in d['settings']:
            raise KeyError(k)
        d['settings'][k] = float(v)
    path = os.path.join(TMP, name + '.vital')
    json.dump(d, open(path, 'w'))
    subprocess.run([RENDER, '--headless', '-o', path + '.wav', '-l', '2', '-m', 'C2', '--bits', '32', path],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    x, sr = wavio.read(path + '.wav')
    return x[int(0.3 * sr):int(1.6 * sr), 0], sr


def power(x, n=4096):
    """Welch average: half-overlapped Hann segments."""
    w = np.hanning(n)
    return np.mean([np.abs(np.fft.rfft(x[i:i + n] * w)) ** 2 for i in range(0, len(x) - n, n // 2)], axis=0)


def classify(response_db, freqs):
    """Score each vowel by the response at its two formants; best and margin."""
    def at(f):
        return response_db[np.argmin(np.abs(freqs - f))]
    scores = {v: at(hz(f1)) + at(hz(f2)) for v, (f1, f2) in FORMANTS.items()}
    ranked = sorted(scores, key=scores.get, reverse=True)
    return ranked[0], scores[ranked[0]] - scores[ranked[1]]


def main():
    subprocess.run([RENDER, '--headless', '--save', os.path.join(TMP, 'init.vital'), '-o', os.path.join(TMP, 'x.wav'),
                    '-l', '0.1'], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    base = json.load(open(os.path.join(TMP, 'init.vital')))

    dry, sr = render(base, 'dry', filter_1_on=0, **NOISE)
    dry_power = power(dry)
    freqs = np.fft.rfftfreq(4096, 1 / sr)

    heard = {}
    for vowel, pos in VOWELS.items():
        wet, _ = render(base, 'v' + vowel, filter_1_on=1, filter_1_model=FORMANT_MODEL, filter_1_style=pos['style'],
                        filter_1_formant_x=pos['x'], filter_1_formant_y=pos['y'], **NOISE)
        response = np.convolve(np.sqrt(power(wet) / dry_power), np.ones(3) / 3, 'same')
        heard[vowel] = classify(20 * np.log10(response + 1e-12), freqs)
        got, margin = heard[vowel]
        check(got == vowel and margin >= 6,
              f'button {vowel} (style {pos["style"]}, x {pos["x"]}, y {pos["y"]}) sounds {got}, by {margin:.1f} dB')

    # Negative control: with A and E swapped in the map, button A would play
    # E's position. The same judgement must call both buttons wrong.
    swap = {'A': 'E', 'E': 'A'}
    wrong = [button for button, position in swap.items() if heard[position][0] != button]
    check(len(wrong) == 2, f'negative control: with A and E swapped in the map, {len(wrong)} of 2 judged wrong')

    print(f'\n{len(FAILS)} failure(s)')
    return len(FAILS)


if __name__ == '__main__':
    sys.exit(main())
