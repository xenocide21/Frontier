"""The SPU's ADSR envelope (psx-spx), to pick register values from times in seconds.

A rate R (7 bits; decay uses R = dr*4, release R = rr*4) gives shift = R >> 2 and
step = 7 - (R & 3) going up or -8 + (R & 3) going down. Every 1 << max(0, shift-11)
samples the level moves by step << max(0, 11-shift); exponential up is 4x slower
above 0x6000, exponential down scales the step by level/0x8000. 44100 samples/s.
"""
RATE = 44100


def _params(R, up):
    shift = R >> 2
    step = (7 - (R & 3)) if up else (-8 + (R & 3))
    cycles = 1 << max(0, shift - 11)
    step = step << max(0, 11 - shift)
    return cycles, step


def time_up(R, exp, frm=0, to=0x7FFF):
    cycles, step = _params(R, True)
    lvl, n = frm, 0
    while lvl < to:
        c = cycles * 4 if (exp and lvl > 0x6000) else cycles
        lvl += step
        n += c
        if n > RATE * 60:
            return 1e9
    return n / RATE


def time_down(R, exp, frm=0x7FFF, to=0):
    cycles, step = _params(R, False)
    lvl, n = frm, 0
    while lvl > to:
        d = (step * lvl) >> 15 if exp else step
        if d == 0:
            d = -1
        lvl += d
        n += cycles
        if n > RATE * 60:
            return 1e9
    return n / RATE


_cache = {}


def _table(kind):
    if kind not in _cache:
        if kind == 'attack':
            _cache[kind] = [(R, time_up(R, False)) for R in range(0, 128)]
        elif kind == 'decay':          # to sustain level 0 approx: measure to 1/8 of full
            _cache[kind] = [(d, time_down(d * 4, True, 0x7FFF, 0x0FFF)) for d in range(16)]
        elif kind == 'release':
            _cache[kind] = [(r, time_down(r * 4, True, 0x7FFF, 0x100)) for r in range(32)]
        elif kind == 'sustain':        # exp decrease, full to 1/32
            _cache[kind] = [(R, time_down(R, True, 0x7FFF, 0x400)) for R in range(0, 128)]
    return _cache[kind]


def pick(kind, seconds):
    return min(_table(kind), key=lambda rt: abs(rt[1] - seconds))[0]


def regs(attack=0.005, decay=0.2, sl=15, sustain=None, release=0.15):
    """Register values. sl 0..15 (sustain level (sl+1)/16). sustain: seconds to fade
    from full to near silence while held, or None to hold."""
    ar = pick('attack', attack)
    dr = pick('decay', decay)
    rr = pick('release', release)
    adsr1 = (ar << 8) | (dr << 4) | sl           # attack linear
    if sustain is None:
        adsr2 = (1 << 15) | (1 << 14) | (127 << 6) | (1 << 5) | rr     # exp down, slowest: hold
    else:
        sr = pick('sustain', sustain)
        adsr2 = (1 << 15) | (1 << 14) | (sr << 6) | (1 << 5) | rr
    return adsr1, adsr2
