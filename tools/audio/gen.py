#!/usr/bin/env python3
"""Builds FRONTIER's sound bank: synthesizes every instrument and effect, encodes
them to SPU ADPCM, compiles the songs, and writes src/audio_data.c and .h.

    python3 tools/audio/gen.py            (from the project root, or anywhere)

Writes WAV previews of every sound to /tmp/frontier_sounds/ as well.
"""
import os
import sys
import wave
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import adpcm
import adsr
import sounds as S
import songs

SRC = os.path.join(HERE, '..', '..', 'src')
PREVIEW = '/tmp/frontier_sounds'


def pitch_at(sr):
    return int(round(4096 * sr / 44100))


# name, sample, loop, ref_note, base_pitch, vol, adsr
def cyc(N, f_ref=440.0):
    return int(round(4096 * f_ref * N / 44100))


INSTS = [
    # name      builder                 loop  ref  pitch                  vol  envelope
    ('SQUARE', lambda: S.inst_square(), True, 69, cyc(112), 118, adsr.regs(0.004, 0.35, 10, None, 0.10)),
    ('SAW', lambda: S.inst_saw(), True, 69, cyc(112), 118, adsr.regs(0.012, 0.5, 11, None, 0.20)),
    ('BASS', lambda: S.inst_bass(), True, 45, cyc(224, 110.0), 80, adsr.regs(0.003, 0.3, 11, None, 0.07)),
    ('PAD', lambda: S.inst_pad(), True, 69, cyc(112), 100, adsr.regs(0.35, 0.6, 13, None, 0.9)),
    ('GLASS', lambda: S.inst_glass(), True, 69, cyc(112), 112, adsr.regs(0.07, 0.7, 11, None, 0.7)),
    ('BELL', lambda: S.inst_bell(), False, 69, pitch_at(22050), 118, adsr.regs(0.001, 0.05, 15, None, 0.6)),
    ('EPIANO', lambda: S.inst_epiano(), False, 57, pitch_at(22050), 120, adsr.regs(0.001, 0.05, 15, None, 0.35)),
    ('PLUCK', lambda: S.inst_pluck(), False, 57, pitch_at(22050), 115, adsr.regs(0.001, 0.05, 15, None, 0.25)),
    ('KICK', lambda: S.drum_kick(), False, 60, pitch_at(22050), 120, adsr.regs(0.001, 0.05, 15, None, 0.4)),
    ('SNARE', lambda: S.drum_snare(), False, 60, pitch_at(22050), 100, adsr.regs(0.001, 0.05, 15, None, 0.4)),
    ('HAT', lambda: S.drum_hat(), False, 60, pitch_at(22050), 105, adsr.regs(0.001, 0.05, 15, None, 0.3)),
    ('OHAT', lambda: S.drum_hat(sec=0.32, tau=0.11), False, 60, pitch_at(22050), 100, adsr.regs(0.001, 0.05, 15, None, 0.3)),
    ('CRASH', lambda: S.drum_crash(), False, 60, pitch_at(16000), 90, adsr.regs(0.001, 0.05, 15, None, 1.0)),
    ('TOM', lambda: S.drum_tom(), False, 45, pitch_at(22050), 110, adsr.regs(0.001, 0.05, 15, None, 0.4)),
]
assert [n for n, *_ in INSTS][:14] == ['SQUARE', 'SAW', 'BASS', 'PAD', 'GLASS', 'BELL', 'EPIANO', 'PLUCK',
                                       'KICK', 'SNARE', 'HAT', 'OHAT', 'CRASH', 'TOM']

# name, builder, sample rate, volume, priority
SFX = [
    ('LASER', S.sfx_laser, 22050, 72, 2),
    ('LASER_ENEMY', S.sfx_laser_enemy, 22050, 60, 1),
    ('CANNON', S.sfx_cannon, 22050, 80, 2),
    ('VEYR_SHOT', S.sfx_veyr_shot, 22050, 70, 1),
    ('MISSILE', S.sfx_missile, 22050, 90, 3),
    ('EXPLODE_S', S.sfx_explode_small, 22050, 105, 4),
    ('EXPLODE_L', S.sfx_explode_big, 16000, 127, 6),
    ('HIT_SHIELD', S.sfx_hit_shield, 22050, 78, 3),
    ('HIT_HULL', S.sfx_hit_hull, 22050, 92, 4),
    ('MENU_MOVE', S.sfx_menu_move, 22050, 60, 5),
    ('MENU_OK', S.sfx_menu_ok, 22050, 70, 5),
    ('MENU_BACK', S.sfx_menu_back, 22050, 65, 5),
    ('ERROR', S.sfx_error, 22050, 70, 5),
    ('DOCK', S.sfx_dock, 22050, 100, 6),
    ('LAUNCH', S.sfx_launch, 22050, 95, 6),
    ('JUMP', S.sfx_jump, 22050, 105, 7),
    ('BOOST', S.sfx_boost, 22050, 80, 3),
    ('CASH', S.sfx_cash, 22050, 75, 5),
    ('ALARM', S.sfx_alarm, 22050, 80, 7),
    ('SCAN', S.sfx_scan, 22050, 75, 6),
    ('BANNER', S.sfx_banner, 22050, 100, 7),
    ('SCOOP', S.sfx_scoop, 22050, 70, 4),
    ('POWERDOWN', S.sfx_powerdown, 22050, 90, 6),
]


def wav(path, x, sr):
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype('<i2').tobytes())


def compile_channel(ch):
    out = bytearray([0x80, ch.inst, 0x81, ch.vel, 0x83, ch.pan + 64])
    inst, vel = ch.inst, ch.vel
    for e in ch.ev:
        if e[0] == 'r':
            w = e[1] * 3
            while w > 0:
                out += bytes([0x82, min(w, 255)])
                w -= min(w, 255)
            continue
        _, note, dur, gate, v, i = e
        if i != inst:
            out += bytes([0x80, i])
            inst = i
        if v != vel:
            out += bytes([0x81, v])
            vel = v
        w = dur * 3
        ln = max(1, min(255, int(round(w * gate))))
        out += bytes([note, ln, min(w, 255)])
        w -= min(w, 255)
        while w > 0:
            out += bytes([0x82, min(w, 255)])
            w -= min(w, 255)
    out.append(0xFF)
    return out


def main():
    os.makedirs(PREVIEW, exist_ok=True)
    bank = bytearray()
    inst_rows, sfx_rows = [], []

    def add(data):
        off = len(bank)
        bank.extend(data)
        return off

    for name, build, loop, ref, pitch, vol, (a1, a2) in INSTS:
        x = build()
        wav(f'{PREVIEW}/inst_{name.lower()}.wav', np.tile(x, max(1, 22050 // len(x))) if loop else x, 44100 if loop else 22050)
        data = adpcm.encode(x * 32767, loop=loop, first_filter0=loop)
        inst_rows.append((name, add(data), pitch, ref, vol, a1, a2, len(data)))
    for name, build, sr, vol, prio in SFX:
        x = build(sr)
        wav(f'{PREVIEW}/sfx_{name.lower()}.wav', x, sr)
        data = adpcm.encode(x * 32767)
        frames = int(np.ceil(len(x) / sr * 60)) + 2
        sfx_rows.append((name, add(data), pitch_at(sr), vol, prio, frames, len(data)))
    while len(bank) % 64:
        bank.append(0)

    seq = bytearray()
    song_rows = []
    for s in songs.SONGS:
        offs = []
        for c in s.chs:
            offs.append(len(seq))
            seq += compile_channel(c)
        song_rows.append((s.name, s.bpm, offs))

    # --- header ---
    h = ['/* Generated by tools/audio/gen.py: do not edit. */',
         '#ifndef FRONTIER_AUDIO_DATA_H', '#define FRONTIER_AUDIO_DATA_H', '', '#include "common.h"', '',
         f'#define AUD_BANK_BYTES {len(bank)}', f'#define AUD_INSTS {len(inst_rows)}',
         f'#define AUD_SFX {len(sfx_rows)}', f'#define AUD_SONGS {len(song_rows)}', '#define AUD_SONG_CH 10', '',
         'typedef struct { u32 off; u16 pitch; u8 ref, vol; u16 adsr1, adsr2; } AudInst;',
         'typedef struct { u32 off; u16 pitch; u8 vol, prio; u16 frames; } AudSfx;',
         'typedef struct { u16 bpm; u8 nch; u16 off[AUD_SONG_CH]; } AudSong;', '',
         'enum { ' + ', '.join(f'SFX_{n}' for n, *_ in sfx_rows) + ' };',
         'enum { SONG_NONE = -1, ' + ', '.join(f'SONG_{n}' for n, *_ in song_rows) + ' };', '',
         'extern const u32 aud_bank[];', 'extern const AudInst aud_inst[];', 'extern const AudSfx aud_sfx[];',
         'extern const AudSong aud_song[];', 'extern const u8 aud_seq[];', '', '#endif', '']
    open(os.path.join(SRC, 'audio_data.h'), 'w').write('\n'.join(h))

    # --- data ---
    c = ['/* Generated by tools/audio/gen.py: do not edit. The sound bank is SPU ADPCM. */',
         '#include "audio_data.h"', '']
    words = np.frombuffer(bytes(bank), dtype='<u4')
    c.append(f'const u32 aud_bank[{len(words)}] = {{')
    for i in range(0, len(words), 8):
        c.append('    ' + ','.join(f'0x{w:08x}' for w in words[i:i + 8]) + ',')
    c.append('};')
    c.append('const AudInst aud_inst[AUD_INSTS] = {')
    for n, off, p, ref, vol, a1, a2, ln in inst_rows:
        c.append(f'    {{ {off}, {p}, {ref}, {vol}, 0x{a1:04x}, 0x{a2:04x} }},   /* {n} ({ln} bytes) */')
    c.append('};')
    c.append('const AudSfx aud_sfx[AUD_SFX] = {')
    for n, off, p, vol, prio, fr, ln in sfx_rows:
        c.append(f'    {{ {off}, {p}, {vol}, {prio}, {fr} }},   /* {n} ({ln} bytes) */')
    c.append('};')
    c.append('const AudSong aud_song[AUD_SONGS] = {')
    for n, bpm, offs in song_rows:
        c.append(f'    {{ {bpm}, {len(offs)}, {{ {", ".join(map(str, offs))} }} }},   /* {n} */')
    c.append('};')
    c.append(f'const u8 aud_seq[{len(seq)}] = {{')
    for i in range(0, len(seq), 24):
        c.append('    ' + ','.join(str(b) for b in seq[i:i + 24]) + ',')
    c.append('};')
    open(os.path.join(SRC, 'audio_data.c'), 'w').write('\n'.join(c) + '\n')

    print(f'bank {len(bank)} bytes ({len(bank) / 1024:.0f} KB), sequences {len(seq)} bytes')
    for n, off, p, ref, vol, a1, a2, ln in inst_rows:
        print(f'  inst {n:8s} {ln:6d} B  pitch {p}')
    for n, off, p, vol, prio, fr, ln in sfx_rows:
        print(f'  sfx  {n:12s} {ln:6d} B  {fr} frames')


if __name__ == '__main__':
    main()
