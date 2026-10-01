"""Optional front ends for tools/measure.py: separate the wobble from the
drums before measuring it, and find the tempo (docs/design/phase3-02-isolate.md).

A finished riddim track is drums over bass, and a snare or a kick moves the
level far more than a wobble does: measured on a GNARL growl (1/8T, 3 per
beat) under synthetic drums at twice and four times the level of the
tests' kit, the growl band read 1.5 and 0.5 per beat. Two ways out:

- hpss: librosa's harmonic/percussive separation (median filtering of the
  spectrogram, Fitzgerald 2010). No model, no download. Margin 1 - a wider
  margin removed the wobble's own transients and failed at four times.
- demucs: Meta's Demucs (htdemucs), the 'bass' stem. A trained model,
  downloaded on first use - so it runs on a producer's own machine, not in
  this repository's sandbox, where the download is blocked. Untested here.

Neither is imported unless asked for: tools/measure.py runs on numpy and
scipy alone, as CI does.
"""
import numpy as np


def _need(module, why):
    try:
        return __import__(module)
    except ImportError:
        raise SystemExit(f'{why} needs {module}: pip install {module}')


def hpss(x, sr, margin=1.0):
    """The harmonic part of every channel: sustained tones stay, hits go."""
    librosa = _need('librosa', '--isolate hpss')
    import librosa.effects  # noqa: F401
    return np.stack([librosa.effects.harmonic(np.ascontiguousarray(x[:, c]), margin=margin)
                     for c in range(x.shape[1])], axis=1)


def demucs_bass(x, sr):
    """The bass stem, from htdemucs. Resampled to the model's rate and back."""
    torch = _need('torch', '--isolate demucs')
    _need('demucs', '--isolate demucs')
    from demucs.apply import apply_model
    from demucs.pretrained import get_model
    model = get_model('htdemucs')
    model.eval()
    wav = torch.tensor(x.T, dtype=torch.float32)
    if wav.shape[0] == 1:
        wav = wav.repeat(2, 1)
    if sr != model.samplerate:
        import torchaudio
        wav = torchaudio.functional.resample(wav, sr, model.samplerate)
    with torch.no_grad():
        stems = apply_model(model, wav[None], device='cpu')[0]
    bass = stems[model.sources.index('bass')]
    if sr != model.samplerate:
        import torchaudio
        bass = torchaudio.functional.resample(bass, model.samplerate, sr)
    return bass.numpy().T[:len(x)]


# Riddim and dubstep live at about 130-160 bpm with half-time drums, which
# a beat tracker hears as 70: tests/test_isolate.py's kit read 69.84 for 140.
# The answer is folded by octaves into this range; --bpm N overrides it.
TEMPO_RANGE = (100.0, 200.0)


def auto_bpm(mid, sr):
    """librosa's beat tracker, folded into TEMPO_RANGE."""
    librosa = _need('librosa', '--bpm auto')
    import librosa.beat  # noqa: F401
    tempo, _ = librosa.beat.beat_track(y=np.ascontiguousarray(mid), sr=sr)
    tempo = float(np.atleast_1d(tempo)[0])
    while 0 < tempo < TEMPO_RANGE[0]:
        tempo *= 2
    while tempo > TEMPO_RANGE[1]:
        tempo /= 2
    return tempo
