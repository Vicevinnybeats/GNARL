#!/usr/bin/env python3
"""tools/measure.py, measured: signals whose answers are known.

    python3 tests/test_measure.py [path/to/gnarl-render]

CLAUDE.md §7: measure the instrument before trusting it. Each past mistake
of this project's analysis is run here as a NEGATIVE CONTROL - the wrong
method on the same signal must give the wrong answer, or the check could
not tell the difference. Exit status is the number of failures.

1. Loudness: a full-scale 997 Hz sine in one channel is -3.01 LUFS, in both
   0.00 LUFS (BS.1770-4's own calibration), at 48 and 44.1 kHz.
2. Band POWER: a 50 Hz sine plus white noise; the sub share must match the
   analytic power ratio within 2%. Control: summing magnitudes, as once
   happened, misses by far more.
3. Stereo: L = R has no side; independent noise in L and R is side = mid.
4. The kick: a 140 BPM four-on-the-floor kick plus a growl band wobbling at
   1/8T (7.00 Hz). The growl band must find 7.00 Hz (3 per beat). Control:
   the full-band envelope finds the kick's 2.33 Hz instead.
5. Aliasing: a 1/16 wobble (9.33 Hz) is found at 9.33 Hz. Control: an
   8192-sample hop, as once used, does not find it.
6. Depth: an 80% amplitude wobble measures 0.8 within 15%; plain noise
   measures under 0.1.
7. A filter wobble at constant level - equal-power crossfade between dark
   and bright noise at 1/8 (4.67 Hz) - shows in the centroid's movement.
8. GNARL itself (with a renderer): the wobble macro at 1/8T on cutoff is
   found at 3 per beat, by level and by centroid. Control: its loudest level
   line is the 2nd harmonic (6 per beat), which is why measure.py reports
   the harmonic series' fundamental as the RATE. At C1, where the note
   beats against the wobble's harmonics, the rate is still 3 per beat; a
   held note with no wobble has no rate. The mono sub adds sub power with
   no side.
"""
import json, os, subprocess, sys, tempfile
import numpy as np
from scipy import signal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'tools'))
import measure  # noqa: E402
import wavio  # noqa: E402

RENDER = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'headless/builds/linux/build/gnarl-render')
FAILS = []
RNG = np.random.default_rng(1)


def check(ok, msg):
    print(('PASS ' if ok else 'FAIL ') + msg)
    if not ok:
        FAILS.append(msg)


def top(peaks):
    return peaks[0]['hz'] if peaks else 0.0


def stereo_of(x):
    return np.stack([x, x], axis=1)


def kick_train(sr, seconds, bpm):
    """A kick per beat: a 150 -> 50 Hz sweep, 300 ms decay, plus a 3 ms click."""
    out = np.zeros(int(sr * seconds))
    n = int(0.3 * sr)
    t = np.arange(n) / sr
    freq = 50 + 100 * np.exp(-t * 30)
    body = np.sin(2 * np.pi * np.cumsum(freq) / sr) * np.exp(-t * 12)
    click = np.zeros(n)
    c = int(0.003 * sr)
    click[:c] = RNG.standard_normal(c) * 0.5
    hit = body + click
    for start in np.arange(0, seconds, 60 / bpm):
        i = int(start * sr)
        out[i:i + n] += hit[:len(out) - i]
    return out


def growl_noise(sr, seconds, lo=300, hi=3000):
    sos = signal.butter(4, (lo, hi), btype='bandpass', fs=sr, output='sos')
    return signal.sosfilt(sos, RNG.standard_normal(int(sr * seconds)))


def main():
    # 1. Loudness calibration.
    for sr in (48000, 44100):
        t = np.arange(sr * 10) / sr
        sine = np.sin(2 * np.pi * 997 * t)
        one = measure.integrated_loudness(np.stack([sine, np.zeros_like(sine)], axis=1), sr)
        both = measure.integrated_loudness(stereo_of(sine), sr)
        check(abs(one + 3.01) < 0.05 and abs(both) < 0.05,
              f'{sr} Hz: full-scale 997 Hz sine, one channel {one:.2f} LUFS (-3.01), both {both:.2f} LUFS (0.00)')

    # 2. Band power, against the analytic ratio.
    sr, seconds, a, sigma = 48000, 20, 0.5, 0.1
    t = np.arange(sr * seconds) / sr
    x = a * np.sin(2 * np.pi * 50 * t) + sigma * RNG.standard_normal(len(t))
    density = sigma ** 2 / (sr / 2)  # white noise power per Hz, one-sided
    expect = (a * a / 2 + density * 40) / (a * a / 2 + density * (20000 - 20))
    got = measure.bands(x, sr)['sub']
    check(abs(got / expect - 1) < 0.02, f'sub power share {100 * got:.2f}%, analytic {100 * expect:.2f}%')
    f, p = measure.psd(x, sr)
    mag = np.sqrt(p)
    wrong = mag[(f >= 20) & (f < 60)].sum() / mag[(f >= 20) & (f < 20000)].sum()
    check(abs(wrong / expect - 1) > 0.5, f'control: summing magnitudes reads {100 * wrong:.2f}% - wrong, as it must be')

    # 3. Stereo.
    # 20 s: the 20-60 Hz band has few bins, and 5 s of noise scattered it by 0.55 dB.
    noise = RNG.standard_normal(sr * 20)
    same = measure.stereo(noise, noise, sr)['side_to_mid_db']
    apart = measure.stereo(noise, RNG.standard_normal(sr * 20), sr)
    check(max(same.values()) < -100, f'L = R: side/mid at most {max(same.values()):.0f} dB')
    check(all(abs(v) < 0.5 for v in apart['side_to_mid_db'].values()) and abs(apart['correlation']) < 0.05,
          f'independent L, R: side/mid {min(apart["side_to_mid_db"].values()):.2f}..'
          f'{max(apart["side_to_mid_db"].values()):.2f} dB, correlation {apart["correlation"]:.3f}')

    # 4. The kick does not win.
    sr, seconds, bpm = 44100, 16, 140
    t = np.arange(sr * seconds) / sr
    wobble = 1 + 0.8 * np.sin(2 * np.pi * (bpm / 60 * 3) * t)
    mix = kick_train(sr, seconds, bpm) + 0.15 * growl_noise(sr, seconds) * wobble
    m = measure.modulation(mix, sr, bpm)
    growl, full = m['growl_band']['peaks'][0], m['full_band']['peaks'][0]
    check(abs(growl['hz'] - 7.0) < 0.07, f'kick + 1/8T growl: growl band finds {growl["hz"]} Hz '
          f'({growl["per_beat"]}/beat, expected 3)')
    check(abs(full['hz'] - bpm / 60) < 0.05, f'control: the full-band envelope finds the kick, {full["hz"]} Hz')

    # 5. Aliasing.
    rate = bpm / 60 * 4
    x = growl_noise(sr, seconds) * (1 + 0.8 * np.sin(2 * np.pi * rate * t))
    good = top(measure.modulation(x, sr, bpm)['growl_band']['peaks'])
    bad = top(measure.modulation(x, sr, bpm, hop=8192)['growl_band']['peaks'])
    check(abs(good - rate) < 0.05, f'1/16 wobble ({rate:.3f} Hz) found at {good} Hz')
    check(abs(bad - rate) > 0.5, f'control: an 8192-sample hop reports {bad} Hz - aliased, as it must be')

    # 6. Depth.
    x = growl_noise(sr, seconds) * (1 + 0.8 * np.sin(2 * np.pi * 7 * t))
    depth = measure.modulation(x, sr)['growl_band']['depth']
    flat = measure.modulation(growl_noise(sr, seconds), sr)['growl_band']['depth']
    check(abs(depth / 0.8 - 1) < 0.15 and flat < 0.1, f'80% wobble depth {depth:.3f}; plain noise {flat:.3f}')

    # 7. A filter wobble at constant level.
    rate = bpm / 60 * 2
    dark = growl_noise(sr, seconds, 100, 600)
    bright = growl_noise(sr, seconds, 2000, 8000)
    dark, bright = dark / np.std(dark), bright / np.std(bright)
    phase = 0.5 + 0.5 * np.sin(2 * np.pi * rate * t)
    x = 0.2 * (np.sqrt(1 - phase) * dark + np.sqrt(phase) * bright)
    b = measure.brightness(x, sr, bpm)
    moved = b['movement']['peaks'][0]
    check(abs(moved['hz'] - rate) < 0.05, f'filter wobble at {rate:.3f} Hz: centroid moves at {moved["hz"]} Hz '
          f'({moved["per_beat"]}/beat), centroid {b["centroid_mean_hz"]:.0f} +- {b["centroid_std_hz"]:.0f} Hz')

    # 8. GNARL.
    if os.path.exists(RENDER):
        tmp = tempfile.mkdtemp(prefix='gnarl-measure-')
        init = os.path.join(tmp, 'init.vital')
        subprocess.run([RENDER, '--headless', '--save', init, '-o', os.path.join(tmp, 'x.wav'), '-l', '0.1'],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        base = json.load(open(init))

        def render(name, seconds, **overrides):
            d = json.loads(json.dumps(base))
            d['settings'].update({k: float(v) for k, v in overrides.items()})
            path = os.path.join(tmp, name + '.vital')
            json.dump(d, open(path, 'w'))
            subprocess.run([RENDER, '--headless', '-o', path + '.wav', '-l', str(seconds), '-m', 'C2', '-b', '140',
                            '--bits', '32', path], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            x, sr = wavio.read(path + '.wav')
            return x[int(1 * sr):int((seconds - 1) * sr)], sr

        wob = dict(env_1_sustain=1, filter_1_on=1, filter_1_cutoff=40, osc_1_random_phase=0)
        x, sr = render('wobble', 10, wobble_amount_cutoff=0.6, wobble_rate=2, **wob)
        m = measure.measure(x, sr, 140)
        level = m['modulation']['growl_band']
        found = [level['rate']['per_beat'], m['brightness']['movement']['rate']['per_beat']]
        check(all(abs(v - 3) < 0.03 for v in found), f'GNARL wobble 1/8T: level and centroid rates {found} per beat')
        # The reason the RATE exists: this wobble's loudest level line is its
        # 2nd harmonic. Taking the argmax would call it 1/16-triplet.
        check(abs(level['peaks'][0]['per_beat'] - 6) < 0.05,
              f'control: the loudest level peak alone is {level["peaks"][0]["per_beat"]}/beat, not the rate')
        # At C1 the note itself (32.7 Hz) beats against the wobble's harmonics
        # and puts a second series at 4.7, 11.7, 18.7, 25.7 Hz; the first
        # rate picker reported 4.7 Hz. A held note with no wobble has no rate.
        def render_c1(name, **o):
            d = json.loads(json.dumps(base))
            d['settings'].update({k: float(v) for k, v in o.items()})
            path = os.path.join(tmp, name + '.vital')
            json.dump(d, open(path, 'w'))
            subprocess.run([RENDER, '--headless', '-o', path + '.wav', '-l', '10', '-m', 'C1', '-b', '140',
                            '--bits', '32', path], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            x, sr = wavio.read(path + '.wav')
            return measure.measure(x[sr:9 * sr], sr, 140)
        low = render_c1('c1', wobble_amount_cutoff=0.6, wobble_rate=2, **wob)
        r = low['modulation']['growl_band']['rate']
        check(r and abs(r['per_beat'] - 3) < 0.03, f'GNARL wobble 1/8T at C1: level rate {r and r["per_beat"]}/beat')
        still = render_c1('c1still', **wob)
        check(still['modulation']['growl_band']['rate'] is None and still['brightness']['movement']['rate'] is None,
              f'held C1, no wobble: no rate (depth {still["modulation"]["growl_band"]["depth"]})')

        plain, _ = render('nosub', 4, osc_1_random_phase=0)
        sub, _ = render('sub', 4, osc_1_random_phase=0, mono_sub_on=1)
        p, s = measure.measure(plain, sr), measure.measure(sub, sr)
        check(s['bands']['sub'] > 2 * p['bands']['sub'] and s['stereo']['side_to_mid_db']['sub'] < -100,
              f'GNARL mono sub: sub share {100 * p["bands"]["sub"]:.1f}% -> {100 * s["bands"]["sub"]:.1f}%, '
              f'sub side/mid {s["stereo"]["side_to_mid_db"]["sub"]:.0f} dB')
    else:
        print(f'skip GNARL checks: no renderer at {RENDER}')

    # --drops: a track shaped like a riddim tune - 16 quiet bars, a 32-bar drop
    # with a one-bar fill dip every 16 bars (which must NOT count as a new
    # drop), 8 quiet bars, a 16-bar drop - starting 0.3 s off the bar grid.
    rng = np.random.default_rng(3)
    bpm, sr = 140, 44100
    bar = int(4 * 60 / bpm * sr)
    levels = [0.15] * 16 + ([1.0] * 15 + [0.2]) * 2 + [0.15] * 8 + [1.0] * 16
    lead = int(0.3 * sr)
    track = np.concatenate([np.zeros(lead)] + [lv * rng.standard_normal(bar) for lv in levels])
    found = measure.find_drops(track, sr, bpm)
    expected = [(lead + 16 * bar) / sr, (lead + 56 * bar) / sr]
    check(len(found) == 2 and all(abs(f - e) < 0.06 for f, e in zip(found, expected)),
          f'--drops finds two drops at {[round(f, 2) for f in found]} s, expected {[round(e, 2) for e in expected]} '
          '(the fills are not drops)')
    flat = rng.standard_normal(40 * bar)
    check(measure.find_drops(flat, sr, bpm) == [], 'a track at one level has no drop')

    print(f'\n{len(FAILS)} failure(s)')
    return len(FAILS)


if __name__ == '__main__':
    sys.exit(main())
