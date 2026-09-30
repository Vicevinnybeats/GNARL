"""Reads the WAVs gnarl-render writes: 16/24-bit PCM or 32-bit IEEE float.

Python's `wave` module rejects format tag 3 (float), which is the format the
tests need - 16-bit output has a -96 dB floor and cannot see anything
quieter, such as a denormal-range tail or a small render-to-render drift.
"""
import struct
import numpy as np


def read(path):
    """Returns (samples[frames, channels] as float64, sample_rate)."""
    data = open(path, 'rb').read()
    if data[:4] != b'RIFF' or data[8:12] != b'WAVE':
        raise ValueError(f'{path}: not a RIFF/WAVE file')
    pos, fmt, raw = 12, None, None
    while pos + 8 <= len(data):
        cid, size = data[pos:pos + 4], struct.unpack('<I', data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b'fmt ':
            tag, ch, sr, _, _, bits = struct.unpack('<HHIIHH', body[:16])
            if tag == 0xFFFE:                       # WAVE_FORMAT_EXTENSIBLE
                tag = struct.unpack('<H', body[24:26])[0]
            fmt = (tag, ch, sr, bits)
        elif cid == b'data':
            raw = body
        pos += 8 + size + (size & 1)
    if fmt is None or raw is None:
        raise ValueError(f'{path}: missing fmt or data chunk')
    tag, ch, sr, bits = fmt
    if tag == 3 and bits == 32:
        x = np.frombuffer(raw, '<f4').astype(np.float64)
    elif tag == 1 and bits == 16:
        x = np.frombuffer(raw, '<i2') / 32768.0
    elif tag == 1 and bits == 24:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3).astype(np.int32)
        v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        x = np.where(v >= 1 << 23, v - (1 << 24), v) / float(1 << 23)
    else:
        raise ValueError(f'{path}: unsupported format tag {tag}, {bits} bits')
    return x[: len(x) // ch * ch].reshape(-1, ch), sr
