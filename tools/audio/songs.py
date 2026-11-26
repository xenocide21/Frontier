"""FRONTIER's music, written as note data for the in-game sequencer.

Time: a tick is 1/12 of a beat; a sixteenth is 3 ticks. Durations below are in
sixteenths. Each song is a set of channels (one SPU voice each, so chords take
several channels); every channel of a song must be the same length, and the song
loops.
"""
import random

NAMES = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}


def midi(n):
    """'C#4' -> 61 (C4 = 60)."""
    k = NAMES[n[0]]
    i = 1
    while i < len(n) and n[i] in '#b':
        k += 1 if n[i] == '#' else -1
        i += 1
    return k + 12 * (int(n[i:]) + 1)


class Ch:
    def __init__(self, inst, vel=100, pan=0, gate=0.9):
        self.ev = []                 # ('n', note, dur16, gate, vel, inst) or ('r', dur16)
        self.inst, self.vel, self.pan, self.gate = inst, vel, pan, gate

    def note(self, n, dur, gate=None, vel=None, inst=None):
        self.ev.append(('n', midi(n) if isinstance(n, str) else n, dur, self.gate if gate is None else gate,
                        self.vel if vel is None else vel, self.inst if inst is None else inst))
        return self

    def rest(self, dur):
        if dur > 0:
            self.ev.append(('r', dur))
        return self

    def seq(self, text, gate=None, vel=None):
        """'A4:4 C5:2 r:2 E5:8' ; an optional !vel suffix, e.g. 'A4:4!120'."""
        for tok in text.split():
            v = vel
            if '!' in tok:
                tok, vv = tok.split('!')
                v = int(vv)
            n, d = tok.split(':')
            if n == 'r':
                self.rest(int(d))
            else:
                self.note(n, int(d), gate, v)
        return self

    def length(self):
        return sum(e[2] if e[0] == 'n' else e[1] for e in self.ev)


class Song:
    def __init__(self, name, bpm):
        self.name, self.bpm, self.chs = name, bpm, []

    def ch(self, *a, **k):
        c = Ch(*a, **k)
        self.chs.append(c)
        return c

    def check(self):
        lens = {c.length() for c in self.chs}
        assert len(lens) == 1, (self.name, [c.length() for c in self.chs])
        assert len(self.chs) <= 10, self.name


def chords_to_pad(song, chords, bars_each, inst, vel, pans=(-30, 0, 30)):
    """Three channels holding each chord (a list of 3 note names)."""
    chs = [song.ch(inst, vel, p, gate=1.0) for p in pans]
    for ch_notes in chords:
        for i in range(3):
            chs[i].note(ch_notes[i], 16 * bars_each)
    return chs


def arp(ch, tones, bars, pattern, step=2, vel=None):
    """Arpeggiate tones (note names) with an index pattern, one note per `step` sixteenths."""
    n = 16 * bars // step
    for i in range(n):
        ch.note(tones[pattern[i % len(pattern)]], step, vel=vel)


# ---------------------------------------------------------------------------
# Instruments the songs can use (indices into the instrument table; gen.py
# defines the same order).
# ---------------------------------------------------------------------------
SQUARE, SAW, BASS, PAD, GLASS, BELL, EPIANO, PLUCK, KICK, SNARE, HAT, OHAT, CRASH, TOM = range(14)


def drums(ch, pattern, bars, kit):
    """pattern: one string per sixteenth step, e.g. 'k.h.s.h.' ; kit maps letters to
    (inst, note, vel). '.' is a rest. Repeats the pattern over `bars`."""
    steps = 16 * bars
    p = pattern.replace(' ', '')
    i = 0
    while i < steps:
        c = p[i % len(p)]
        # length: until the next hit
        j = i + 1
        while j < steps and p[j % len(p)] == '.':
            j += 1
        if c == '.':
            ch.rest(j - i)
        else:
            inst, note, vel = kit[c]
            ch.note(note, j - i, gate=0.5, vel=vel, inst=inst)
        i = j


KIT = {'k': (KICK, 'C4', 120), 's': (SNARE, 'C4', 100), 'h': (HAT, 'C4', 55), 'H': (HAT, 'C4', 80),
       'o': (OHAT, 'C4', 60), 'c': (CRASH, 'C4', 90), 't': (TOM, 'A2', 105), 'T': (TOM, 'D3', 100),
       'u': (TOM, 'E2', 110), 'x': (SNARE, 'C4', 60)}


# ---------------------------------------------------------------------------
# TITLE: "Frontier". D minor, epic and slow. 16 bars.
# ---------------------------------------------------------------------------
def song_title():
    s = Song('TITLE', 84)
    prog = [['A3', 'D4', 'F4'], ['Bb3', 'D4', 'F4'], ['A3', 'C4', 'F4'], ['G3', 'C4', 'E4'],
            ['A3', 'D4', 'F4'], ['Bb3', 'D4', 'F4'], ['G3', 'Bb3', 'D4'], ['A3', 'C#4', 'E4']]
    chords_to_pad(s, prog, 2, PAD, 70)
    bass = s.ch(BASS, 105, 0, gate=0.95)
    roots = ['D2', 'Bb1', 'F2', 'C2', 'D2', 'Bb1', 'G1', 'A1']
    for i, r in enumerate(roots):
        if i < 4:
            bass.note(r, 32)
        else:                               # the second half pulses in eighths
            for _ in range(16):
                bass.note(r, 2, gate=0.7)
    bell = s.ch(BELL, 62, 25)
    arps = [['D4', 'F4', 'A4', 'D5'], ['D4', 'F4', 'Bb4', 'D5'], ['C4', 'F4', 'A4', 'C5'], ['C4', 'E4', 'G4', 'C5'],
            ['D4', 'F4', 'A4', 'D5'], ['D4', 'F4', 'Bb4', 'D5'], ['D4', 'G4', 'Bb4', 'D5'], ['C#4', 'E4', 'A4', 'C#5']]
    for tones in arps:
        arp(bell, tones, 2, [0, 1, 2, 3, 2, 1, 2, 3])
    lead = s.ch(SAW, 92, -10, gate=0.95)
    lead.rest(64)
    lead.seq('A4:8 C5:4 F5:4  E5:8 D5:4 C5:4  D5:12 E5:4  C5:8 G4:8 '
             'D5:8 F5:4 A5:4  G5:6 F5:2 E5:4 D5:4  F5:12 D5:4  Bb4:8 C5:4 D5:4 '
             'G5:8 F5:4 D5:4  Bb4:6 C5:2 D5:8  E5:8 C#5:4 A4:4  A4:12 r:4')
    dr = s.ch(KICK, 100, 0)
    dr.rest(128)
    drums(dr, 'c.......k.......' + 'k.......k.......' * 6, 7, KIT)
    drums(dr, 'k.......k...tTtu', 1, KIT)
    return s


# ---------------------------------------------------------------------------
# FLIGHT: "Drift". E minor, ambient, 32 bars (the melody comes in the second half).
# ---------------------------------------------------------------------------
def song_flight():
    s = Song('FLIGHT', 70)
    prog = [['G3', 'B3', 'F#4'], ['G3', 'B3', 'E4'], ['G3', 'B3', 'D4'], ['F#3', 'A3', 'D4'],
            ['E3', 'G3', 'B3'], ['G3', 'C4', 'E4'], ['G3', 'B3', 'E4'], ['F#3', 'B3', 'D#4']]
    chords_to_pad(s, prog * 2, 2, PAD, 64)
    bass = s.ch(BASS, 95, 0, gate=1.0)
    for r in ['E2', 'C2', 'G1', 'D2', 'E2', 'A1', 'C2', 'B1'] * 2:
        bass.note(r, 32)
    bell = s.ch(BELL, 48, 30)
    tones = [['E5', 'F#5', 'G5', 'B5'], ['E5', 'G5', 'B5', 'C6'], ['D5', 'G5', 'B5', 'D6'], ['D5', 'F#5', 'A5', 'D6'],
             ['E5', 'G5', 'B5', 'E6'], ['E5', 'G5', 'A5', 'C6'], ['E5', 'G5', 'B5', 'C6'], ['D#5', 'F#5', 'B5', 'D#6']]
    rnd = random.Random(7)
    for rep in range(2):
        for t in tones:
            for bar in range(2):
                # two or three soft notes a bar, on eighths
                slots = sorted(rnd.sample(range(8), rnd.choice((2, 3))))
                pos = 0
                for sl in slots:
                    bell.rest(sl * 2 - pos)
                    bell.note(rnd.choice(t), 2, vel=rnd.choice((38, 46, 54)))
                    pos = sl * 2 + 2
                bell.rest(16 - pos)
    glass = s.ch(GLASS, 70, -20, gate=0.97)
    glass.rest(256)
    glass.seq('B4:12 A4:4 G4:16  E4:16 G4:8 B4:8  D5:24 B4:8  A4:16 F#4:16 '
              'G4:8 A4:8 B4:16  C5:16 E5:8 D5:8  B4:24 G4:8  F#4:16 D#4:8 F#4:8')
    hat = s.ch(HAT, 30, 15)
    hat.rest(256)
    drums(hat, 'h...x.......h.h.', 16, {'h': (HAT, 'C4', 28), 'x': (OHAT, 'C4', 22)})
    return s


# ---------------------------------------------------------------------------
# COMBAT: "Engage". A minor, driving. 16 bars.
# ---------------------------------------------------------------------------
def song_combat():
    s = Song('COMBAT', 138)
    prog = [['A3', 'C4', 'E4'], ['A3', 'C4', 'F4'], ['G3', 'B3', 'D4'], ['G3', 'B3', 'E4'],
            ['A3', 'C4', 'E4'], ['A3', 'C4', 'F4'], ['A3', 'D4', 'F4'], ['G#3', 'B3', 'E4']]
    chords_to_pad(s, prog, 2, PAD, 52)
    bass = s.ch(BASS, 112, 0)
    for r in ['A1', 'F1', 'G1', 'E1', 'A1', 'F1', 'D1', 'E1']:
        lo, hi = midi(r), midi(r) + 12
        for _ in range(2):
            for n in (lo, lo, hi, lo, lo, lo, hi, lo):
                bass.note(n, 2, gate=0.6)
    lead = s.ch(SQUARE, 88, -15, gate=0.85)
    lead.seq('A4:2 C5:2 E5:2 A5:2 G5:2 E5:2 C5:2 E5:2  D5:4 C5:2 B4:2 A4:8 '
             'F4:2 A4:2 C5:2 F5:2 E5:2 C5:2 A4:2 C5:2  B4:4 A4:2 G4:2 F4:8 '
             'G4:2 B4:2 D5:2 G5:2 F5:2 D5:2 B4:2 D5:2  C5:4 B4:2 A4:2 G4:8 '
             'B4:6 G4:2 E4:8  E5:6 D5:2 B4:8')
    lead.inst = SAW
    lead.vel = 84
    lead.seq('E5:8 A5:8  G5:4 E5:4 C5:8  F5:8 C6:8  A5:4 F5:4 C5:8 '
             'D5:4 F5:4 A5:8  G5:4 F5:4 D5:8  E5:8 G#5:8  B5:8 G#5:4 E5:4', gate=0.92)
    kick = s.ch(KICK, 100, 0)
    drums(kick, 'c...s...k.k.s...' + ('k...s...k.k.s...' * 6) + 'k...s...k.k.sss.' + 'c...s...k.k.s...' + ('k...s...k.k.s...' * 6) + 'k...s.s.tTtusss.', 16, KIT)
    hat = s.ch(HAT, 60, 20)
    drums(hat, 'HhHhHhHoHhHhHhHo', 16, {'H': (HAT, 'C4', 62), 'h': (HAT, 'C4', 38), 'o': (OHAT, 'C4', 48)})
    return s


# ---------------------------------------------------------------------------
# DOCK: "Port of Call". F major, laid back. 16 bars.
# ---------------------------------------------------------------------------
def song_dock():
    s = Song('DOCK', 92)
    prog = [['A3', 'C4', 'E4'], ['G3', 'B3', 'D4'], ['F3', 'A3', 'C4'], ['E3', 'G3', 'B3'],
            ['A3', 'D4', 'F4'], ['G3', 'C4', 'E4'], ['F3', 'Bb3', 'D4'], ['E3', 'G3', 'Bb3']]
    roots = ['F2', 'E2', 'D2', 'C2', 'Bb1', 'A1', 'G1', 'C2']
    keys = [s.ch(EPIANO, 62, p, gate=0.9) for p in (-25, 0, 25)]
    for ch3 in prog:
        for bar in range(2):
            for i in range(3):              # comp: beat one, then the and of two
                keys[i].note(ch3[i], 6).note(ch3[i], 6, vel=50).rest(4)
    bass = s.ch(BASS, 100, 0, gate=0.85)
    for i, r in enumerate(roots):
        nr = midi(roots[(i + 1) % len(roots)])
        R = midi(r)
        bass.note(R, 4).note(R + 7, 4).note(R + 12, 4).note(R + 10, 4)
        bass.note(R, 4).note(R + 4 if i not in (1, 3, 5) else R + 3, 4).note(R + 7, 4).note(nr - 1, 4)
    pl = s.ch(PLUCK, 80, 18)
    pl.rest(128)
    pl.seq('D5:4 F5:4 A5:8  G5:4 F5:4 D5:8  C5:4 E5:4 G5:8  E5:16 '
           'F5:4 D5:4 Bb4:8  A4:4 Bb4:4 C5:8  E5:8 G5:8  Bb5:8 G5:8')
    dr = s.ch(KICK, 90, 0)
    drums(dr, 'k...x.....k.x...', 16, {'k': (KICK, 'C4', 85), 'x': (SNARE, 'C4', 48)})
    hat = s.ch(HAT, 40, 22)
    drums(hat, 'h.hhh.hhh.hhh.hh', 16, {'h': (HAT, 'C4', 34)})
    return s


# ---------------------------------------------------------------------------
# VEYR: "The Swarm". C# phrygian, dark, pulsing. 16 bars.
# ---------------------------------------------------------------------------
def song_veyr():
    s = Song('VEYR', 96)
    roots = ['C#2', 'C#2', 'C#2', 'C#2', 'D2', 'D2', 'C#2', 'C#2', 'G1', 'G1', 'G1', 'G1', 'D2', 'D2', 'C#2', 'C#2']
    pads = {'C#2': ['C#3', 'G#3', 'D4'], 'D2': ['D3', 'A3', 'Eb4'], 'G1': ['G2', 'C#3', 'F3']}
    chs = [s.ch(PAD, 66, p, gate=1.0) for p in (-30, 0, 30)]
    for r in roots:
        for i in range(3):
            chs[i].note(pads[r][i], 16)
    sub = s.ch(BASS, 108, 0, gate=1.0)
    for r in roots:
        sub.note(r, 16)
    pulse = s.ch(PLUCK, 70, -18, gate=0.6)
    acc = [1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0]
    for r in roots:
        R = midi(r) + 12
        for i in range(16):
            pulse.note(R + (12 if acc[i] and i in (6, 14) else 0), 1, vel=84 if acc[i] else 46)
    glass = s.ch(GLASS, 72, 20, gate=0.98)
    glass.rest(64)
    glass.seq('G#4:16  A4:16  G#4:8 E4:8  D4:16  G4:24 F4:8  C#4:16  D5:8 C#5:8  Bb4:12 A4:4  '
              'G#4:16  r:16  r:16')
    dr = s.ch(TOM, 100, 0)
    drums(dr, 'u..u..t.u...s...' * 3 + 'u..u..t.u.tTtTss', 16, KIT)
    return s


SONGS = [song_title(), song_flight(), song_combat(), song_dock(), song_veyr()]
for _s in SONGS:
    _s.check()
