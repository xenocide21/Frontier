"""PlayStation SPU ADPCM encoder (and a decoder to check it).

Each 16-byte block holds 28 samples: byte 0 = shift | filter << 4, byte 1 = flags
(bit 0 loop end, bit 1 repeat, bit 2 loop start), then 14 bytes of 4-bit nibbles.
Decoding (what the SPU does):  s = (nibble << 12) >> shift;  s += (h1*k1 + h2*k2 + 32) >> 6.
"""
import numpy as np

FILTERS = [(0, 0), (60, 0), (115, -52), (98, -55), (122, -60)]


def _clamp(v):
    return -32768 if v < -32768 else 32767 if v > 32767 else v


def _encode_block(x, h1, h2, filt, shift):
    k1, k2 = FILTERS[filt]
    nibs, out, err = [], [], 0
    for s in x:
        pred = (h1 * k1 + h2 * k2 + 32) >> 6
        r = s - pred
        step = 1 << (12 - shift)
        n = int(round(r / step))
        n = -8 if n < -8 else 7 if n > 7 else n
        d = _clamp(((n << 12) >> shift) + pred)
        err += (d - s) * (d - s)
        nibs.append(n & 15)
        out.append(d)
        h2, h1 = h1, d
    return nibs, err, h1, h2


def encode(samples, loop=False, first_filter0=False):
    """samples: int16-range ints. Returns bytes, padded to whole blocks.
    loop=True: the whole sample loops (loop start on block 0, end+repeat on the last).
    loop=False: one-shot; a silent self-looping block is appended so the voice idles."""
    x = [int(v) for v in np.clip(np.round(samples), -32768, 32767)]
    if loop:
        assert len(x) % 28 == 0, "a looped sample must be whole blocks"
    while len(x) % 28:
        x.append(0)
    nblocks = len(x) // 28
    out = bytearray()
    h1 = h2 = 0
    for b in range(nblocks):
        blk = x[b * 28:(b + 1) * 28]
        best = None
        filters = [0] if (first_filter0 and b == 0) else range(5)
        for f in filters:
            k1, k2 = FILTERS[f]
            # estimate the shift from the residual against the true history
            hh1, hh2, mx = h1, h2, 0
            for s in blk:
                r = s - ((hh1 * k1 + hh2 * k2 + 32) >> 6)
                mx = max(mx, abs(r))
                hh2, hh1 = hh1, s
            bits = 0
            while bits < 12 and (7 << bits) < mx:
                bits += 1
            for sh in {12 - bits, max(0, 12 - bits - 1)}:
                nibs, err, n1, n2 = _encode_block(blk, h1, h2, f, sh)
                if best is None or err < best[0]:
                    best = (err, f, sh, nibs, n1, n2)
        _, f, sh, nibs, h1, h2 = best
        flags = 0
        if loop:
            if b == 0:
                flags |= 4
            if b == nblocks - 1:
                flags |= 3
        out.append(sh | (f << 4))
        out.append(flags)
        for i in range(0, 28, 2):
            out.append(nibs[i] | (nibs[i + 1] << 4))
    if not loop:
        out += bytes([0, 7]) + bytes(14)      # silent block that loops on itself
    return bytes(out)


def decode(data):
    out, h1, h2 = [], 0, 0
    for b in range(0, len(data), 16):
        sh, f = data[b] & 15, data[b] >> 4
        k1, k2 = FILTERS[min(f, 4)]
        for i in range(28):
            byte = data[b + 2 + i // 2]
            n = (byte >> ((i & 1) * 4)) & 15
            if n >= 8:
                n -= 16
            s = _clamp(((n << 12) >> sh) + ((h1 * k1 + h2 * k2 + 32) >> 6))
            out.append(s)
            h2, h1 = h1, s
    return np.array(out)
