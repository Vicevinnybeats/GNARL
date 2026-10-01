#!/usr/bin/env python3
"""Measure a WAV: the numbers Phase 3 compares (CLAUDE.md §7).

    python3 tools/measure.py track.wav [--bpm 140] [--from 30 --to 60] [--json out.json]
    python3 tools/measure.py track.mp3 --bpm auto --isolate hpss --scan 8 --json out.json

Prints a summary and, with --json, writes every number. Run it on a
reference track LOCALLY and commit only the JSON - never the audio
(CLAUDE.md §7). A reference that is not a WAV: `ffmpeg -i in.mp3 out.wav`.

What it measures, and how each avoids a mistake this project already made:

- LOUDNESS: integrated LUFS (ITU-R BS.1770-4: K-weighting, 400 ms blocks,
  absolute and relative gates), peak dBFS, crest factor.
- BANDS: the share of total POWER in sub / low / low-mid / mid / high / air.
  Power, |X|^2 from a Welch PSD - a sub balance once summed magnitudes and
  read 12-21% for what was 79%.
- STEREO: side-to-mid power ratio per band, and L/R correlation. A riddim
  sub is mono; the growl may be wide.
- MODULATION: the spectrum of the RMS envelope, 0.25-30 Hz, in 128-sample
  hops (344 frames/s at 44.1 kHz, Nyquist 172 Hz). An 8192-sample hop once
  aliased every track into a "1.2 Hz wobble". Measured on the 200-5000 Hz
  band, where the growl lives and the kick's body does not - full-band
  envelopes find the kick, as autocorrelation once did. The full-band
  spectrum is reported beside it, and with --bpm every peak is also given
  in cycles per beat (1/4 = 1, 1/8 = 2, 1/8T = 3, 1/16 = 4). The RATE is
  the fundamental of the peaks' harmonic series, not the loudest peak: a
  shaped wobble's second harmonic can be the strongest line. The WHOLE
  spectrum is in the JSON, not only the argmax.
- BRIGHTNESS: spectral centroid over time (mean, spread) and the spectrum
  of its movement, which is where a filter wobble shows even when the level
  barely moves.

- ISOLATE (optional, tools/isolate.py): --isolate hpss removes the drums
  before measuring (librosa); --isolate demucs measures Demucs's bass stem.
  --bpm auto asks librosa's beat tracker.
- SCAN: --scan N measures every N-bar window (hop N/2 bars) across the whole
  track - the rate, depth and level of each - to find the drops without
  choosing --from and --to by ear. Any format ffmpeg reads.

tests/test_measure.py checks every one of these on signals whose answers
are known, including each past mistake put back.
"""
import argparse, json, math, os, sys

import numpy as np
from scipy import signal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wavio  # noqa: E402

SCHEMA = 1
BANDS = [('sub', 20, 60), ('low', 60, 120), ('low_mid', 120, 500), ('mid', 500, 2000),
         ('high', 2000, 8000), ('air', 8000, 20000)]
# The envelope hop: 128 samples keeps the envelope's Nyquist far above any
# wobble (172 Hz at 44.1 kHz), and the frames short enough to follow 1/16s.
HOP = 128
# The growl's band for the modulation spectrum: above a kick's body and the
# sub, below the hiss.
GROWL_BAND = (200, 5000)
MOD_RANGE = (0.25, 30.0)
# A modulation peak is reported if it is within this of the strongest.
PEAK_FLOOR_DB = -12.0
# Below this depth (strongest component, relative to the mean) there is no
# wobble to name a rate for: plain noise measured 0.016, a held GNARL note
# 0.04, real wobbles 0.29 and up. Peaks are still listed.
MIN_RATE_DEPTH = 0.1


# ---------------------------------------------------------------- loudness

def k_weighting(sr):
    """BS.1770 K-weighting as two biquads, designed for any sample rate."""
    def shelf():
        g, q, fc = 3.99984385397, 0.7071752369554193, 1681.9744509555319
        k = math.tan(math.pi * fc / sr)
        vh = 10 ** (g / 20)
        vb = vh ** 0.499666774155
        a0 = 1 + k / q + k * k
        b = [(vh + vb * k / q + k * k) / a0, 2 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0]
        a = [1.0, 2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0]
        return b, a

    def highpass():
        q, fc = 0.5003270373253953, 38.13547087613982
        k = math.tan(math.pi * fc / sr)
        a0 = 1 + k / q + k * k
        return [1.0, -2.0, 1.0], [1.0, 2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0]

    return [shelf(), highpass()]


def integrated_loudness(x, sr):
    """LUFS; x is (samples, channels). L and R weigh 1 (BS.1770 table 3)."""
    y = x
    for b, a in k_weighting(sr):
        y = signal.lfilter(b, a, y, axis=0)
    block, step = int(0.4 * sr), int(0.1 * sr)
    if len(y) < block:
        return float('-inf')
    starts = range(0, len(y) - block + 1, step)
    z = np.array([np.mean(y[s:s + block] ** 2, axis=0) for s in starts])  # per channel
    lk = -0.691 + 10 * np.log10(np.maximum(z.sum(axis=1), 1e-30))
    gated = z[lk > -70.0]
    if not len(gated):
        return float('-inf')
    relative = -0.691 + 10 * np.log10(gated.mean(axis=0).sum()) - 10.0
    lk_gated = -0.691 + 10 * np.log10(np.maximum(gated.sum(axis=1), 1e-30))
    final = gated[lk_gated > relative]
    return float(-0.691 + 10 * np.log10(final.mean(axis=0).sum()))


# ------------------------------------------------------------------- bands

def psd(x, sr):
    """Welch power spectral density, Hann, 1/4-second segments."""
    n = min(len(x), 1 << int(math.log2(sr / 4)))
    return signal.welch(x, sr, window='hann', nperseg=n, axis=0)


def band_power(freqs, p, lo, hi):
    sel = (freqs >= lo) & (freqs < hi)
    return float(p[sel].sum())


def bands(mid, sr):
    f, p = psd(mid, sr)
    total = band_power(f, p, 20, 20000)
    return {name: band_power(f, p, lo, hi) / total for name, lo, hi in BANDS}


def stereo(left, right, sr):
    mid, side = (left + right) / 2, (left - right) / 2
    fm, pm = psd(mid, sr)
    _, ps = psd(side, sr)
    per_band = {}
    for name, lo, hi in BANDS:
        m, s = band_power(fm, pm, lo, hi), band_power(fm, ps, lo, hi)
        per_band[name] = 10 * math.log10(max(s, 1e-30) / max(m, 1e-30))
    corr = float(np.corrcoef(left, right)[0, 1]) if np.std(left) > 0 and np.std(right) > 0 else 1.0
    return {'side_to_mid_db': per_band, 'correlation': corr}


# -------------------------------------------------------------- modulation

def blackman_harris(n):
    t = np.arange(n) / max(n - 1, 1)
    return (0.35875 - 0.48829 * np.cos(2 * np.pi * t) + 0.14128 * np.cos(4 * np.pi * t)
            - 0.01168 * np.cos(6 * np.pi * t))


def envelope(x, hop=HOP):
    frames = len(x) // hop
    return np.sqrt(np.mean(x[:frames * hop].reshape(frames, hop) ** 2, axis=1))


def modulation_spectrum(env, fps):
    """Spectrum of a (relative) envelope over MOD_RANGE, in dB re its peak."""
    rel = env / max(np.mean(env), 1e-30) - 1.0
    n = 1 << max(16, int(math.ceil(math.log2(len(rel)))) + 2)  # zero-padded: fine bins
    mag = np.abs(np.fft.rfft(rel * blackman_harris(len(rel)), n))
    f = np.fft.rfftfreq(n, 1 / fps)
    sel = (f >= MOD_RANGE[0]) & (f <= MOD_RANGE[1])
    f, mag = f[sel], mag[sel]
    db = 20 * np.log10(np.maximum(mag, 1e-30) / max(mag.max(), 1e-30))
    # Depth of the strongest component, as a fraction of the mean level.
    depth = 2 * mag.max() / np.sum(blackman_harris(len(rel)))
    return f, db, float(depth)


def peaks(f, db, bpm=None):
    idx = signal.find_peaks(db, height=PEAK_FLOOR_DB)[0]
    out = []
    for i in sorted(idx, key=lambda i: -db[i])[:8]:
        a, b, c = db[max(i - 1, 0)], db[i], db[min(i + 1, len(db) - 1)]
        denom = a - 2 * b + c
        hz = f[i] + (0.5 * (a - c) / denom * (f[1] - f[0]) if denom else 0.0)
        p = {'hz': round(float(hz), 3), 'db': round(float(b), 1)}
        if bpm:
            p['per_beat'] = round(float(hz) / (bpm / 60), 3)
        out.append(p)
    return out


def rate(peak_list, bpm=None):
    """The wobble's rate: the fundamental most peaks are whole multiples of.

    A shaped wobble is a harmonic series, and its strongest line need not be
    the first: GNARL's triangle wobble on cutoff measured 14.0 Hz at 0 dB and
    its 7.0 Hz rate at -1.4 dB. Each peak is tried as the fundamental; the one
    whose series carries the most power wins.

    A harmonic must sit within 0.03 Hz or 0.3% of its place - peaks are
    located to about 0.01 Hz. The first version allowed 1% per harmonic, and
    at C1 the note (32.7 Hz) beating against a 7 Hz wobble's harmonics put
    lines at 4.7, 11.7, 18.7 and 25.7 Hz: 4.7 Hz then "explained" 14, 18.7
    and 28 Hz and was reported as the rate.
    """
    best = None
    for cand in peak_list:
        f0 = cand['hz']
        series = [p for p in peak_list if p['hz'] >= f0 * 0.997 and
                  abs(p['hz'] - f0 * round(p['hz'] / f0)) < max(0.03, 0.003 * p['hz'])]
        power = sum(10 ** (p['db'] / 10) for p in series)
        if best is None or power > best[0]:
            best = (power, f0, len(series))
    if best is None:
        return None
    best = (best[2], best[1])
    out = {'hz': best[1], 'explains': best[0]}
    if bpm:
        out['per_beat'] = round(best[1] / (bpm / 60), 3)
    return out


def coarse(f, db, step=0.05):
    """The whole spectrum on a fixed grid, for the JSON and for compare.py."""
    grid = np.arange(MOD_RANGE[0], MOD_RANGE[1] + step / 2, step)
    return [round(float(v), 1) for v in np.interp(grid, f, db)]


def modulation(mid, sr, bpm=None, hop=HOP):
    fps = sr / hop
    sos = signal.butter(4, GROWL_BAND, btype='bandpass', fs=sr, output='sos')
    growl = signal.sosfiltfilt(sos, mid)
    result = {'hop': hop, 'frames_per_second': fps, 'grid_step_hz': 0.05, 'grid_from_hz': MOD_RANGE[0]}
    for name, x in (('growl_band', growl), ('full_band', mid)):
        f, db, depth = modulation_spectrum(envelope(x, hop), fps)
        found = peaks(f, db, bpm)
        result[name] = {'rate': rate(found, bpm) if depth >= MIN_RATE_DEPTH else None, 'peaks': found,
                        'depth': round(depth, 4),
                        'spectrum_db': coarse(f, db)}
    return result


# -------------------------------------------------------------- brightness

def brightness(mid, sr, bpm=None):
    nfft = 2048
    f, _, z = signal.stft(mid, sr, window='hann', nperseg=nfft, noverlap=nfft - HOP, boundary=None)
    power = np.abs(z) ** 2
    total = power.sum(axis=0)
    keep = total > total.max() * 1e-6  # ignore silent frames
    centroid = (f[:, None] * power).sum(axis=0)[keep] / total[keep]
    out = {'centroid_mean_hz': float(np.mean(centroid)), 'centroid_std_hz': float(np.std(centroid))}
    if len(centroid) > 64:
        fm, db, depth = modulation_spectrum(centroid, sr / HOP)
        found = peaks(fm, db, bpm)
        out['movement'] = {'rate': rate(found, bpm) if depth >= MIN_RATE_DEPTH else None, 'peaks': found,
                           'depth': round(depth, 4),
                           'spectrum_db': coarse(fm, db)}
    return out


# -------------------------------------------------------------------- main

def measure(x, sr, bpm=None, hop=HOP):
    if x.ndim == 1:
        x = x[:, None]
    left = x[:, 0]
    right = x[:, 1] if x.shape[1] > 1 else x[:, 0]
    mid = (left + right) / 2
    peak = float(np.max(np.abs(x)))
    rms = float(np.sqrt(np.mean(x ** 2)))
    return {
        'schema': SCHEMA,
        'seconds': round(len(x) / sr, 3),
        'sample_rate': sr,
        'bpm': bpm,
        'loudness_lufs': round(integrated_loudness(x[:, :2], sr), 2),
        'peak_dbfs': round(20 * math.log10(max(peak, 1e-30)), 2),
        'crest_db': round(20 * math.log10(max(peak, 1e-30) / max(rms, 1e-30)), 2),
        'bands': {k: round(v, 5) for k, v in bands(mid, sr).items()},
        'stereo': stereo(left, right, sr),
        'modulation': modulation(mid, sr, bpm, hop),
        'brightness': brightness(mid, sr, bpm),
    }


def summary(m):
    lines = [f"{m['seconds']} s at {m['sample_rate']} Hz",
             f"loudness {m['loudness_lufs']} LUFS, peak {m['peak_dbfs']} dBFS, crest {m['crest_db']} dB",
             'power by band: ' + ', '.join(f'{k} {100 * v:.1f}%' for k, v in m['bands'].items()),
             'side/mid by band: ' + ', '.join(f'{k} {v:.1f} dB' for k, v in m['stereo']['side_to_mid_db'].items()),
             f"L/R correlation {m['stereo']['correlation']:.3f}"]
    def rate_text(r):
        if not r:
            return 'none'
        return f"{r['hz']} Hz" + (f" = {r['per_beat']}/beat" if 'per_beat' in r else '') + f" (explains {r['explains']} peaks)"

    for name in ('growl_band', 'full_band'):
        mod = m['modulation'][name]
        lines.append(f'modulation rate ({name}): {rate_text(mod["rate"])}')
        lines.append(f'  peaks (depth {mod["depth"]:.3f}): ' + ', '.join(
            f"{p['hz']} Hz {p['db']} dB" + (f" ({p['per_beat']}/beat)" if 'per_beat' in p else '')
            for p in mod['peaks']))
    b = m['brightness']
    lines.append(f"centroid {b['centroid_mean_hz']:.0f} Hz +- {b['centroid_std_hz']:.0f}")
    if 'movement' in b:
        lines.append(f'centroid movement rate: {rate_text(b["movement"]["rate"])}')
        lines.append('  peaks: ' + ', '.join(
            f"{p['hz']} Hz {p['db']} dB" + (f" ({p['per_beat']}/beat)" if 'per_beat' in p else '')
            for p in b['movement']['peaks']))
    return '\n'.join(lines)


def read_audio(path):
    """A WAV directly; anything else through ffmpeg, to a temporary WAV
    that is deleted at once - the audio is never kept (CLAUDE.md §7)."""
    if path.lower().endswith('.wav'):
        return wavio.read(path)
    import subprocess, tempfile
    with tempfile.TemporaryDirectory() as tmp:
        wav = os.path.join(tmp, 'decoded.wav')
        subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-c:a', 'pcm_f32le', wav], check=True)
        return wavio.read(wav)


def scan(x, sr, bpm, bars):
    """Every `bars`-bar window, hop half that: rate, depth and level."""
    window = int(bars * 4 * 60 / bpm * sr)
    hop = window // 2
    out = []
    for start in range(0, max(1, len(x) - window + 1), hop):
        part = x[start:start + window]
        mid = part.mean(axis=1)
        rms = float(np.sqrt(np.mean(mid ** 2)))
        if rms < 1e-4:
            continue
        mod = modulation(mid, sr, bpm)['growl_band']
        bright = brightness(mid, sr, bpm).get('movement', {})
        out.append({'from': round(start / sr, 2), 'to': round((start + len(part)) / sr, 2),
                    'rms_dbfs': round(20 * math.log10(rms), 2),
                    'growl_rate': mod['rate'], 'growl_depth': mod['depth'], 'growl_peaks': mod['peaks'][:4],
                    'brightness_rate': bright.get('rate'), 'brightness_depth': bright.get('depth')})
    return out


def scan_summary(windows):
    lines = []
    for w in windows:
        def text(r):
            return '-' if not r else (f"{r['per_beat']}/beat" if 'per_beat' in r else f"{r['hz']} Hz")
        lines.append(f"{w['from']:7.1f}-{w['to']:6.1f} s  {w['rms_dbfs']:6.1f} dBFS  "
                     f"growl {text(w['growl_rate']):>11} (depth {w['growl_depth']:.2f})  "
                     f"brightness {text(w['brightness_rate']):>11} (depth {w['brightness_depth'] or 0:.2f})")
    return '\n'.join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('wav', help='a WAV, or anything ffmpeg reads')
    ap.add_argument('--bpm', help="the tempo, or 'auto'")
    ap.add_argument('--from', dest='start', type=float, default=0.0, help='seconds')
    ap.add_argument('--to', dest='end', type=float, help='seconds')
    ap.add_argument('--isolate', choices=['hpss', 'demucs'], help='remove the drums first (tools/isolate.py)')
    ap.add_argument('--scan', type=float, metavar='BARS', help='measure every BARS-bar window instead')
    ap.add_argument('--json', help='write every number here')
    args = ap.parse_args()
    x, sr = read_audio(args.wav)
    if x.ndim == 1:
        x = x[:, None]
    x = x[int(args.start * sr):int(args.end * sr) if args.end else None]
    import isolate
    # The tempo from the whole mix: the drums are what a beat tracker hears.
    raw_mid = x.mean(axis=1)
    if args.isolate == 'hpss':
        x = isolate.hpss(x, sr)
    elif args.isolate == 'demucs':
        x = isolate.demucs_bass(x, sr)
    bpm = None
    if args.bpm == 'auto':
        bpm = round(isolate.auto_bpm(raw_mid, sr), 2)
        print(f'tempo (librosa beat tracker, folded into {isolate.TEMPO_RANGE[0]:.0f}-{isolate.TEMPO_RANGE[1]:.0f}): {bpm} bpm')
    elif args.bpm:
        bpm = float(args.bpm)
    source = {'file': os.path.basename(args.wav), 'from': args.start, 'to': args.end, 'isolate': args.isolate}
    if args.scan:
        if not bpm:
            raise SystemExit('--scan needs --bpm')
        m = {'schema': SCHEMA, 'bpm': bpm, 'scan_bars': args.scan, 'source': source,
             'windows': scan(x, sr, bpm, args.scan)}
        print(scan_summary(m['windows']))
    else:
        m = measure(x, sr, bpm)
        m['source'] = source
        print(summary(m))
    if args.json:
        json.dump(m, open(args.json, 'w'), indent=1)


if __name__ == '__main__':
    main()
