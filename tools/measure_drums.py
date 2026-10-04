#!/usr/bin/env python3
"""Measure the drums of riddim tracks: where kick, snare and hats fall in the
bar, and what each sounds like - for GNARL's DRUMS tab
(docs/design/phase4-06-drums-riddimize.md).

    python3 tools/measure_drums.py track1.mp3 track2.mp3 ... --json references/drums.json

Only numbers come out; the audio is decoded to a temporary file and deleted
(CLAUDE.md section 7). For each track:

1. the tempo (librosa's beat tracker, as tools/measure.py) and its drops
   (measure.find_drops), the first DROP_BARS bars of each, at most MAX_DROPS;
2. the percussive part (HPSS), so the bass's sustained notes mostly leave;
3. three onset functions on bands of it - kick 35-110 Hz, snare 150-300 Hz
   with 1.5-5 kHz, hat 7-16 kHz - each a rise in log energy per 128-sample hop;
4. a 48-tick bar (16ths are 3 ticks, triplet 8ths 4), its phase fixed by the
   kick: the offset within a beat either side of the drop's start that puts
   the most kick on tick 0;
5. per band and tick, the share of bars with a hit there (a peak of the
   onset function within 12 ms of the tick, above half the band's typical
   hit), so a pattern is a probability per tick, straight and triplet alike;
6. each drum's sound, from its strongest hits: the time to fall 20 dB, the
   kick's pitch at the hit and 80 ms later, the snare's and hat's spectral
   centroid.

A kick under a riddim bass is hard to separate: the sub sits in the same
band. The bar-phase search and the per-bar probabilities are what keep a
stray bass note from reading as a pattern; tests/test_measure_drums.py runs
the tool on a synthesized loop whose hits are known.
"""

import argparse
import json
import math
import os
import sys

import numpy as np
from scipy import signal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import measure  # noqa: E402
import isolate  # noqa: E402

HOP = 128
TICKS = 48  # per bar: 16ths (3 ticks) and triplet 8ths (4 ticks) both land on it
DROP_BARS = 8
MAX_DROPS = 2
# A hit is an onset peak within this of a tick: a 16th at 150 bpm is 100 ms,
# a tick 31 ms, so 12 ms keeps neighbours apart and forgives a human nudge.
SNAP_SECONDS = 0.015
BANDS = {
    'kick': [(30, 120)],
    'snare': [(150, 300), (1500, 5000)],
    'hat': [(7000, 16000)],
}


# Each band's energy is averaged over a window long enough to hold a cycle of
# its lowest note: per 128-sample hop, a 55 Hz bass (18 ms a cycle) rises and
# falls every few hops and read as a stream of kicks.
SMOOTH_SECONDS = {'kick': 0.008, 'snare': 0.006, 'hat': 0.003}
# A kick's pitch starts high (150-300 Hz) and falls into the sub in tens of
# ms, so its onset is timed on 40-250 Hz and only CALLED a kick if the sub
# (30-120 Hz) then rises: on a synthesized 180-to-50 Hz kick, timing on the
# sub alone put every kick one or two ticks late.
KICK_TIMING = [(40, 250)]
KICK_BODY = [(30, 120)]
# Measured on GNARL's loops with a held 55 Hz bass under them, as in a track:
# kicks raise the sub 4.9-10.9 dB, snares at most 2.6 (tests/
# test_drums_riddimize.py). 4 dB sits between.
BODY_RISE_DB = 4.0
# A snare is noise over 1.5-5 kHz WITH a body around 200 Hz; a hat's noise
# reaches 1.5-5 kHz too, but nothing under it rises.
SNARE_TIMING = [(1500, 5000)]
SNARE_BODY = [(150, 300)]


# A hat is brightness: over the 20 ms after its onset, 7-16 kHz at least
# HAT_OVER_DB louder than 1.5-5 kHz. A snare's crack and its noisy tail reach
# 7-16 kHz too, and without this read as hats around every snare. Measured on
# GNARL's own loops (tests/test_drums_riddimize.py): closed hats +11 to +16
# dB. (A rise in each band does not separate them: from near silence both
# rise by about the same.)
HAT_OVER_DB = 6.0


def hat_onsets(x, sr):
    """A hat: brighter than the crack band by HAT_OVER_DB over the next 20 ms,
    OR rising by HAT_OVER_DB more than it. The first holds over silence, where
    every band rises alike; the second under a growl, which keeps 1.5-5 kHz
    full all the time while a hat lifts only the top."""
    timing = onsets(band_energy(x, sr, BANDS['hat'], SMOOTH_SECONDS['hat']))
    hat = band_energy(x, sr, BANDS['hat'], 0.004)
    crack = band_energy(x, sr, SNARE_TIMING, 0.004)
    out = np.zeros_like(timing)
    at, heights = peaks(timing, sr)
    before, after = int(0.03 * sr / HOP), int(0.02 * sr / HOP)
    for a, h in zip(at, heights):
        if a - before < 0 or a + after > len(hat):
            continue
        balance = hat[a:a + after].max() - crack[a:a + after].max()
        lift = (hat[a:a + after].max() - hat[a - before:a - 1].mean()) - \
               (crack[a:a + after].max() - crack[a - before:a - 1].mean())
        if balance >= HAT_OVER_DB or lift >= HAT_OVER_DB:
            out[a] = h
    return out


def kick_onsets(x, sr):
    return body_onsets(x, sr, KICK_TIMING, KICK_BODY, 0.012)


def snare_onsets(x, sr):
    return body_onsets(x, sr, SNARE_TIMING, SNARE_BODY, 0.004, loud_check=False)


def band_energy(x, sr, ranges, smooth_s=0.003):
    """Log energy per hop of x filtered to the given bands (summed)."""
    count = len(x) // HOP
    power = np.zeros(len(x))
    for lo, hi in ranges:
        sos = signal.butter(4, [lo, min(hi, sr / 2 * 0.95)], btype='bandpass', fs=sr, output='sos')
        power += signal.sosfiltfilt(sos, x) ** 2
    w = max(1, int(smooth_s * sr))
    # A centred moving average, so a hit's rise stays where the hit is.
    power = np.convolve(power, np.ones(w) / w, mode='same')
    return 10 * np.log10(power[:count * HOP].reshape(-1, HOP).mean(axis=1) + 1e-12)


def onsets(db):
    """Rise in log energy, half-wave rectified, lightly smoothed."""
    rise = np.maximum(0, np.diff(db, prepend=db[0]))
    return rise


def body_onsets(x, sr, timing_bands, body_bands, smooth_s, loud_check=True):
    """Onsets timed on timing_bands, kept only where body_bands then rise by
    BODY_RISE_DB (mean over the next 80 ms against the 60 ms before)."""
    timing = onsets(band_energy(x, sr, timing_bands, smooth_s))
    body = band_energy(x, sr, body_bands, 0.010)
    out = np.zeros_like(timing)
    at, heights = peaks(timing, sr, ripple_floor=True)
    before, after = int(0.06 * sr / HOP), int(0.08 * sr / HOP)
    # And a kick's body must be LOUD, within 12 dB of the band's loudest: in
    # a quiet band, a hat's last trickle of low end is a rise too. Not a
    # snare's: a kick starting at 200 Hz owns the snare's 150-300 Hz band,
    # and the snares fell 12 dB under it (GNARL's own loops).
    loud = np.percentile(body, 99) - 12
    for a, h in zip(at, heights):
        if a - before < 0 or a + after > len(body):
            continue
        rise = body[a:a + after].mean() - body[a - before:a - 2].mean()
        if rise >= BODY_RISE_DB and (not loud_check or body[a:a + after].max() >= loud):
            out[a] = h
    return out


def peaks(onset, sr, ripple_floor=False):
    """Onset peaks: hops and heights, at least 45 ms apart (a 1/32 at 150 bpm
    is 50 ms)."""
    at, props = signal.find_peaks(onset, distance=max(1, int(0.045 * sr / HOP)), height=0)
    heights = props['peak_heights']
    if not len(at):
        return at, heights
    # Above half the strong hits AND well above the typical rise: a held
    # bass ripples every few hops and fills the low percentiles.
    # The 75th, not the 90th: in the hat band a bar's few kick clicks and
    # snare cracks are the loudest peaks, and half of THEM dropped every
    # quieter hat (GNARL's own loops).
    # ripple_floor (the kick's timing band only): also 1.6 x the median, so a
    # held bass's ripple - most of that band's peaks - stays out. In the hat
    # band the hats ARE most of the peaks, and the same floor dropped them.
    floor = 1.6 * np.median(heights) if ripple_floor else 0.0
    keep = heights >= max(np.percentile(heights, 75) / 2, floor)
    return at[keep], heights[keep]


def hits_on_grid(onset, sr, start, bar_seconds, bars):
    """bars x TICKS: the height of a peak within SNAP of each tick, else 0.
    The kick's onsets arrive already picked (kick_onsets): every non-zero
    hop is a hit, and picking again would weigh kicks against each other."""
    if np.count_nonzero(onset) < len(onset) // 20:
        at = np.nonzero(onset)[0]
        heights = onset[at]
    else:
        at, heights = peaks(onset, sr)
    out = np.zeros((bars, TICKS))
    t = at * HOP / sr - start
    pos = t / bar_seconds * TICKS
    tick = np.rint(pos).astype(int)
    near = np.abs(pos - tick) * bar_seconds / TICKS <= SNAP_SECONDS
    for k, h, ok in zip(tick, heights, near):
        b, i = divmod(k, TICKS)
        if ok and 0 <= b < bars:
            out[b, i] = max(out[b, i], h)
    return out


def bar_phase(kick_onset, snare_onset, sr, start, bar_seconds, bars):
    """The start, within half a bar either side, that puts the most kick on
    tick 0 and the most snare on tick 24 - riddim's half-time anchor, kick on
    1 and snare on 3. Kicks alone were not enough: with kicks on 1, 2 and the
    16th after 3, shifting the bar by a beat put another kick on tick 0 and
    one in the middle, and scored higher than the truth (GNARL's own loops,
    tests/test_drums_riddimize.py). Half a bar, not a beat: a drop's start
    found by its rise in level can be two beats out."""
    # Every shift within SNAP of the truth scores the same: take the middle
    # of the best plateau. Taking the first put the grid 18 ms early, and a
    # hat 16 ms off its tick fell outside the snap (GNARL's own loops).
    scores = []
    for shift in np.arange(-bar_seconds / 2, bar_seconds / 2 + 0.001, 0.002):
        s = start + shift
        if s < 0:
            continue
        k = hits_on_grid(kick_onset, sr, s, bar_seconds, bars)
        n = hits_on_grid(snare_onset, sr, s, bar_seconds, bars)
        scores.append((s, (k[:, 0] > 0).sum() + (n[:, 24] > 0).sum()))
    if not scores:
        return start
    top = max(v for _, v in scores)
    # The longest run of top-scoring shifts, and its middle.
    runs, run = [], []
    for s, v in scores:
        if v == top:
            run.append(s)
        elif run:
            runs.append(run)
            run = []
    if run:
        runs.append(run)
    best = max(runs, key=len)
    return best[len(best) // 2]


def probabilities(grid):
    """Share of bars with a hit at each tick."""
    return (grid > 0).mean(axis=0)


def decay_ms(env_db, at):
    """Hops from the peak after `at` until 20 dB below it, in ms."""
    seg = env_db[at:at + int(0.6 * 44100 / HOP)]
    if len(seg) < 3:
        return None
    p = int(np.argmax(seg[:8]))
    below = np.nonzero(seg[p:] < seg[p] - 20)[0]
    return None if not len(below) else round(float(below[0]) * HOP / 44100 * 1000, 1)


def pitch_hz(x, sr, at_sample, offset_s, window_s=0.04):
    """Strongest frequency 30-200 Hz in a window offset_s after the hit."""
    a = at_sample + int(offset_s * sr)
    seg = x[a:a + int(window_s * sr)]
    if len(seg) < 256:
        return None
    n = 1 << 14
    spec = np.abs(np.fft.rfft(seg * signal.windows.blackmanharris(len(seg)), n))
    f = np.fft.rfftfreq(n, 1 / sr)
    band = (f >= 30) & (f <= 200)
    return round(float(f[band][np.argmax(spec[band])]), 1)


def centroid_hz(x, sr, at_sample, window_s=0.05):
    seg = x[at_sample:at_sample + int(window_s * sr)]
    if len(seg) < 256:
        return None
    spec = np.abs(np.fft.rfft(seg * signal.windows.blackmanharris(len(seg)))) ** 2
    f = np.fft.rfftfreq(len(seg), 1 / sr)
    return round(float((spec * f).sum() / (spec.sum() + 1e-20)), 0)


def character(name, perc, sr, start, bar_seconds, bars, prob, env):
    """Median sound numbers over this drum's hits on its most likely ticks."""
    ticks = [t for t in np.argsort(prob)[::-1][:4] if prob[t] >= 0.5]
    decays, pitches0, pitches1, centroids = [], [], [], []
    for b in range(bars):
        for t in ticks:
            at = int((start + (b + t / TICKS) * bar_seconds) * sr)
            hop = at // HOP
            d = decay_ms(env, hop)
            if d is not None:
                decays.append(d)
            if name == 'kick':
                p0, p1 = pitch_hz(perc, sr, at, 0.0, 0.03), pitch_hz(perc, sr, at, 0.08)
                if p0 and p1:
                    pitches0.append(p0)
                    pitches1.append(p1)
            else:
                c = centroid_hz(perc, sr, at)
                if c:
                    centroids.append(c)
    med = lambda v: None if not v else round(float(np.median(v)), 1)  # noqa: E731
    out = {'decay_ms': med(decays)}
    if name == 'kick':
        out.update(pitch_start_hz=med(pitches0), pitch_end_hz=med(pitches1))
    else:
        out['centroid_hz'] = med(centroids)
    return out


# Riddim's tempi (CLAUDE.md section 7). A beat tracker on half-time drums
# lands on a ratio of the tempo - 105 for 140 is 3/4 - so its answer is
# folded into this range by the nearest simple ratio.
RIDDIM_BPM = (135.0, 155.0)


def fold_bpm(bpm):
    for ratio in (1, 4 / 3, 3 / 4, 2, 1 / 2, 3 / 2, 2 / 3):
        b = bpm * ratio
        if RIDDIM_BPM[0] <= b <= RIDDIM_BPM[1]:
            return round(b, 2)
    return round(bpm, 2)


def measure_track(path, bpm=None):
    x, sr = measure.read_audio(path)
    if x.ndim == 1:
        x = x[:, None]
    mid = x.mean(axis=1)
    bpm = bpm or fold_bpm(isolate.auto_bpm(mid, sr))
    bar_seconds = 4 * 60 / bpm
    drops = measure.find_drops(mid, sr, bpm)[:MAX_DROPS]
    if not drops:
        # No quiet-then-loud drop found: the loudest 8 bars.
        bar = int(bar_seconds * sr)
        windows = range(0, max(1, len(mid) - DROP_BARS * bar), bar)
        loudest = max(windows, key=lambda s: np.mean(mid[s:s + DROP_BARS * bar] ** 2))
        drops = [loudest / sr]
    import librosa
    result = {'file': os.path.basename(path), 'bpm': bpm, 'drops': []}
    for d in drops:
        lo = max(0, int((d - bar_seconds / 4) * sr))
        hi = min(len(mid), int((d + (DROP_BARS + 0.25) * bar_seconds) * sr))
        seg = mid[lo:hi]
        perc = librosa.effects.percussive(np.ascontiguousarray(seg), margin=2.0)
        # Timing from the mix itself: HPSS's 2048-point frames smear a hit by
        # 46 ms, more than a tick. The percussive part is kept for the
        # sound numbers, where the bass would only get in the way.
        env = {k: band_energy(seg, sr, r, SMOOTH_SECONDS[k]) for k, r in BANDS.items()}
        on = {k: onsets(v) for k, v in env.items()}
        on['kick'] = kick_onsets(seg, sr)
        on['snare'] = snare_onsets(seg, sr)
        # For lining the bar up, every snare - also one ON a kick: riddim's
        # beat-3 snare is often layered with a kick (references/drums.json),
        # and without it the bar was lined up on kicks alone and slipped.
        snare_all = on['snare'].copy()
        # A kick's click reaches the snare's bands; riddim almost never hits
        # both at once, so a snare within 25 ms of a kick is the kick.
        near = max(1, int(0.025 * sr / HOP))
        for k in np.nonzero(on['kick'])[0]:
            on['snare'][max(0, k - near):k + near + 1] = 0
        # Hats by brightness (hat_onsets), and none within 25 ms of a kick or
        # snare: their click and crack reach the hat band too.
        on['hat'] = hat_onsets(seg, sr)
        for k in np.nonzero(on['kick'] + on['snare'])[0]:
            on['hat'][max(0, k - near):k + near + 1] = 0
        env_perc = {k: band_energy(perc, sr, r, SMOOTH_SECONDS[k]) for k, r in BANDS.items()}
        rel = d - lo / sr
        bars = int((len(seg) / sr - rel) / bar_seconds)
        bars = min(bars, DROP_BARS)
        if bars < 2:
            continue
        start = bar_phase(on['kick'], snare_all, sr, rel, bar_seconds, bars)
        drop = {'at_seconds': round(lo / sr + start, 3), 'bars': bars, 'patterns': {}, 'sound': {}}
        for k in BANDS:
            grid = hits_on_grid(on[k], sr, start, bar_seconds, bars)
            prob = probabilities(grid)
            drop['patterns'][k] = [round(float(p), 2) for p in prob]
            drop['sound'][k] = character(k, perc, sr, start, bar_seconds, bars, prob, env_perc[k])
        result['drops'].append(drop)
    return result


def show(result):
    print(f"{result['file']}  {result['bpm']} bpm")
    for d in result['drops']:
        print(f"  drop at {d['at_seconds']} s, {d['bars']} bars")
        for k, p in d['patterns'].items():
            line = ''.join('#' if v >= 0.6 else '+' if v >= 0.3 else '.' for v in p)
            # 48 ticks in four beats of 12
            print(f"    {k:5s} |{line[0:12]}|{line[12:24]}|{line[24:36]}|{line[36:48]}|  {d['sound'][k]}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('tracks', nargs='+')
    ap.add_argument('--json')
    ap.add_argument('--bpm', type=float, help='the tempo, if the tracker gets it wrong')
    args = ap.parse_args()
    results = []
    for path in args.tracks:
        r = measure_track(path, args.bpm)
        show(r)
        results.append(r)
    if args.json:
        with open(args.json, 'w') as f:
            json.dump({'schema': 1, 'ticks_per_bar': TICKS, 'tracks': results}, f, indent=1)


if __name__ == '__main__':
    main()
