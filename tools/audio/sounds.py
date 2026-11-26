"""Every sound in FRONTIER, synthesized. Returns float arrays in -1..1.

Instruments are either single-cycle loops (N samples a cycle; the pitch register
sets the note) or one-shots recorded at a known note. SFX are one-shots.
"""
import numpy as np
from scipy import signal

rng = np.random.default_rng(1987)


def norm(x, peak=0.9):
    m = np.max(np.abs(x))
    return x * (peak / m) if m > 0 else x


def t_of(sec, sr):
    return np.arange(int(sec * sr)) / sr


def lp(x, fc, sr, order=2):
    b, a = signal.butter(order, min(fc / (sr / 2), 0.99), 'low')
    return signal.lfilter(b, a, x)


def hp(x, fc, sr, order=2):
    b, a = signal.butter(order, min(fc / (sr / 2), 0.99), 'high')
    return signal.lfilter(b, a, x)


def bp(x, lo, hi, sr, order=2):
    b, a = signal.butter(order, [lo / (sr / 2), min(hi / (sr / 2), 0.99)], 'band')
    return signal.lfilter(b, a, x)


def sweep_lp(x, f0, f1, sr):
    """A one-pole low-pass whose cutoff glides exponentially from f0 to f1."""
    n = len(x)
    fc = f0 * (f1 / f0) ** (np.arange(n) / max(1, n - 1))
    a = np.exp(-2 * np.pi * fc / sr)
    y = np.zeros(n)
    s = 0.0
    for i in range(n):
        s = (1 - a[i]) * x[i] + a[i] * s
        y[i] = s
    return y


def chirp(f0, f1, sec, sr, exp=True):
    t = t_of(sec, sr)
    if exp:
        f = f0 * (f1 / f0) ** (t / sec)
    else:
        f = f0 + (f1 - f0) * t / sec
    ph = 2 * np.pi * np.cumsum(f) / sr
    return ph, f


def env_exp(sec, sr, tau, attack=0.002):
    t = t_of(sec, sr)
    e = np.exp(-t / tau)
    a = np.minimum(1, t / max(attack, 1e-6))
    return e * a


def noise(n):
    return rng.uniform(-1, 1, n)


def brown(n):
    x = np.cumsum(rng.uniform(-1, 1, n))
    x = hp(x, 20, 16000)
    return x / (np.max(np.abs(x)) + 1e-9)


# ---------------------------------------------------------------------------
# Instruments
# ---------------------------------------------------------------------------

def cycle(harmonics, N):
    """One band-limited cycle of N samples from {harmonic: amplitude}."""
    t = np.arange(N) / N
    x = sum(a * np.sin(2 * np.pi * k * t + ph) for k, (a, ph) in harmonics.items())
    return norm(x, 0.85)


def inst_square(N=112):
    return cycle({k: (np.exp(-k / 22) / k, 0) for k in range(1, 20, 2)}, N)


def inst_saw(N=112):
    return cycle({k: (np.exp(-k / 14) / k, 0) for k in range(1, 19)}, N)


def inst_bass(N=224):
    h = {k: (0.9 * np.exp(-k / 7) / k ** 1.2, 0) for k in range(1, 26)}
    h[1] = (0.8, 0)
    return cycle(h, N)


def inst_glass(N=112):
    return cycle({1: (1.0, 0), 2: (0.35, 0.3), 3: (0.12, 0.0), 4: (0.22, 1.1), 6: (0.06, 0)}, N)


def inst_pad(N=112, K=170):
    """K cycles of a soft saw, with two copies detuned by one cycle either way
    across the loop: a slow chorus that loops seamlessly."""
    L = N * K
    t = np.arange(L) / L
    x = np.zeros(L)
    for cyc, amp in ((K, 1.0), (K + 1, 0.7), (K - 1, 0.7)):
        for k in range(1, 13):
            x += amp * np.exp(-k / 4.5) / k * np.sin(2 * np.pi * k * cyc * t + k * cyc * 0.37)
    return norm(x, 0.85)


def fm_oneshot(f0, ratio, index, sec, sr, decay, idecay, extra=None):
    t = t_of(sec, sr)
    mod = index * np.exp(-t / idecay) * np.sin(2 * np.pi * f0 * ratio * t)
    x = np.sin(2 * np.pi * f0 * t + mod) * np.exp(-t / decay)
    if extra:
        x += extra(t)
    x *= np.minimum(1, t / 0.002)
    return norm(x, 0.9)


def inst_bell(sr=22050):            # recorded at A4
    return fm_oneshot(440, 3.5, 2.2, 1.5, sr, 0.45, 0.25)


def inst_epiano(sr=22050):          # recorded at A3
    tine = lambda t: 0.25 * np.sin(2 * np.pi * 220 * 14 * t) * np.exp(-t / 0.02)
    return fm_oneshot(220, 1.0, 1.6, 1.4, sr, 0.6, 0.35, tine)


def inst_pluck(sr=22050, f0=220.0, sec=0.9):
    """Karplus-Strong, recorded at A3."""
    n = int(sec * sr)
    p = int(round(sr / f0))
    buf = noise(p) * 0.8
    buf = lp(buf, 3000, sr)
    out = np.zeros(n)
    for i in range(n):
        v = buf[i % p]
        out[i] = v
        buf[i % p] = 0.5 * (v + buf[(i + 1) % p]) * 0.996
    return norm(out * np.minimum(1, np.arange(n) / 30), 0.9)


def drum_kick(sr=22050):
    ph, f = chirp(150, 42, 0.38, sr)
    x = np.sin(ph) * env_exp(0.38, sr, 0.13)
    x[:int(0.004 * sr)] += noise(int(0.004 * sr)) * 0.4
    return norm(x, 0.95)


def drum_snare(sr=22050):
    t = t_of(0.28, sr)
    body = np.sin(2 * np.pi * 190 * t) * np.exp(-t / 0.05)
    sn = bp(noise(len(t)), 1500, 9000, sr) * np.exp(-t / 0.08)
    return norm(0.6 * body + sn, 0.9)


def drum_hat(sr=22050, sec=0.07, tau=0.018):
    t = t_of(sec, sr)
    x = hp(noise(len(t)), 7000, sr, 4) * np.exp(-t / tau)
    return norm(x, 0.8)


def drum_crash(sr=16000):
    t = t_of(1.3, sr)
    x = hp(noise(len(t)), 3500, sr, 2) * np.exp(-t / 0.45)
    x += 0.3 * bp(noise(len(t)), 3000, 6000, sr) * np.exp(-t / 0.2)
    return norm(x, 0.8)


def drum_tom(sr=22050):             # recorded at A2 (110 Hz)
    ph, f = chirp(130, 100, 0.45, sr)
    x = np.sin(ph) * env_exp(0.45, sr, 0.16)
    x += 0.15 * bp(noise(len(x)), 200, 1200, sr) * env_exp(0.45, sr, 0.03)
    return norm(x, 0.95)


# ---------------------------------------------------------------------------
# Sound effects
# ---------------------------------------------------------------------------

def sfx_laser(sr=22050):
    ph, f = chirp(2400, 320, 0.17, sr)
    x = np.tanh(3 * np.sin(ph)) * env_exp(0.17, sr, 0.06)
    x += 0.08 * noise(len(x)) * env_exp(0.17, sr, 0.02)
    return norm(lp(x, 7000, sr), 0.8)


def sfx_laser_enemy(sr=22050):
    ph, f = chirp(1400, 180, 0.2, sr)
    saw = 2 * ((ph / (2 * np.pi)) % 1) - 1
    x = saw * env_exp(0.2, sr, 0.07)
    return norm(lp(x, 4000, sr), 0.75)


def sfx_cannon(sr=22050):
    t = t_of(0.2, sr)
    x = lp(noise(len(t)), 2500, sr) * np.exp(-t / 0.035)
    ph, f = chirp(140, 55, 0.2, sr)
    x += 0.9 * np.sin(ph) * np.exp(-t / 0.07)
    return norm(x, 0.85)


def sfx_veyr_shot(sr=22050):
    t = t_of(0.28, sr)
    f = 520 * (1 - 0.5 * t / 0.28) + 90 * np.sin(2 * np.pi * 38 * t)
    ph = 2 * np.pi * np.cumsum(f) / sr
    x = np.sin(ph + 2.0 * np.sin(ph * 1.51)) * np.exp(-t / 0.09)
    x += 0.35 * bp(noise(len(t)), 600, 2500, sr) * np.exp(-t / 0.05)
    return norm(x, 0.8)


def sfx_missile(sr=22050):
    t = t_of(0.75, sr)
    n = noise(len(t))
    x = np.zeros(len(t))
    # a band of noise that climbs as the motor lights
    for i, (lo, hi) in enumerate(((300, 900), (700, 1800), (1500, 3500))):
        seg = bp(n, lo, hi, sr)
        w = np.clip((t - i * 0.12) / 0.15, 0, 1) * np.exp(-np.maximum(0, t - i * 0.12) / 0.35)
        x += seg * w
    x *= np.minimum(1, t / 0.01)
    return norm(x, 0.8)


def sfx_explode_small(sr=22050):
    t = t_of(0.75, sr)
    x = sweep_lp(noise(len(t)), 5000, 250, sr) * np.exp(-t / 0.2)
    ph, f = chirp(90, 35, 0.75, sr)
    x = 1.6 * x + 0.8 * np.sin(ph) * np.exp(-t / 0.12)
    return norm(x, 0.9)


def sfx_explode_big(sr=16000):
    t = t_of(1.9, sr)
    x = sweep_lp(brown(len(t)), 2200, 120, sr) * np.exp(-t / 0.55) * 3
    crack = np.zeros(len(t))
    for _ in range(26):
        p = int(rng.uniform(0, 0.9) * sr)
        crack[p:p + 300] += noise(min(300, len(t) - p)) * np.exp(-np.arange(min(300, len(t) - p)) / 60) * rng.uniform(0.3, 1)
    x += 0.5 * hp(crack, 600, sr) * np.exp(-t / 0.5)
    ph, f = chirp(70, 28, 1.9, sr)
    x += 0.9 * np.sin(ph) * np.exp(-t / 0.3)
    x *= np.minimum(1, t / 0.004)
    return norm(x, 0.95)


def sfx_hit_shield(sr=22050):
    t = t_of(0.3, sr)
    tone = sum(np.sin(2 * np.pi * f * t) for f in (880, 1330, 1975))
    x = tone * (0.6 + 0.4 * np.sin(2 * np.pi * 75 * t)) * np.exp(-t / 0.08)
    x += 0.4 * hp(noise(len(t)), 3000, sr) * np.exp(-t / 0.01)
    return norm(x, 0.7)


def sfx_hit_hull(sr=22050):
    t = t_of(0.24, sr)
    x = lp(noise(len(t)), 1800, sr) * np.exp(-t / 0.04)
    ph, f = chirp(160, 70, 0.24, sr)
    x += 0.8 * np.sin(ph) * np.exp(-t / 0.06)
    x += 0.25 * (np.sin(2 * np.pi * 430 * t) + np.sin(2 * np.pi * 1170 * t)) * np.exp(-t / 0.05)
    return norm(x, 0.85)


def blip(freqs, each, sr=22050, shape=0.6, tail=0.02):
    out = []
    for f in freqs:
        t = t_of(each, sr)
        s = np.tanh(2.5 * np.sin(2 * np.pi * f * t)) * shape + (1 - shape) * np.sin(2 * np.pi * f * t)
        out.append(s * np.exp(-t / max(tail, each / 2)) * np.minimum(1, t / 0.002))
    return np.concatenate(out)


def sfx_menu_move(sr=22050):
    t = t_of(0.04, sr)
    return norm(np.sin(2 * np.pi * 1250 * t) * np.exp(-t / 0.012), 0.55)


def sfx_menu_ok(sr=22050):
    return norm(blip((880, 1320), 0.065, sr), 0.6)


def sfx_menu_back(sr=22050):
    return norm(blip((990, 660), 0.06, sr), 0.55)


def sfx_error(sr=22050):
    t = t_of(0.22, sr)
    x = np.sign(np.sin(2 * np.pi * 165 * t)) * ((t % 0.11) < 0.08)
    return norm(lp(x, 2000, sr) * np.exp(-t / 0.2), 0.55)


def sfx_dock(sr=22050):
    t = t_of(1.0, sr)
    x = np.zeros(len(t))
    for at, amp in ((0.0, 1.0), (0.32, 0.7)):
        k = int(at * sr)
        n = len(t) - k
        tt = np.arange(n) / sr
        x[k:] += amp * (lp(noise(n), 1500, sr) * np.exp(-tt / 0.03) + 0.8 * np.sin(2 * np.pi * 85 * tt) * np.exp(-tt / 0.08)
                        + 0.3 * np.sin(2 * np.pi * 610 * tt) * np.exp(-tt / 0.05))
    hiss = hp(noise(len(t)), 2500, sr) * np.clip((t - 0.4) / 0.05, 0, 1) * np.exp(-np.maximum(0, t - 0.4) / 0.2) * 0.35
    return norm(x + hiss, 0.85)


def sfx_launch(sr=22050):
    t = t_of(1.1, sr)
    n = noise(len(t))
    x = sweep_lp(n, 300, 4000, sr) * np.sin(np.pi * t / 1.1) ** 1.2
    ph, f = chirp(90, 380, 1.1, sr)
    x += 0.4 * np.tanh(2 * np.sin(ph)) * np.sin(np.pi * t / 1.1)
    return norm(x, 0.8)


def sfx_jump(sr=22050):
    t = t_of(1.9, sr)
    ph, f = chirp(120, 2600, 1.6, sr)
    ph = np.concatenate([ph, ph[-1] + 2 * np.pi * 2600 * np.arange(1, len(t) - len(ph) + 1) / sr])
    tone = np.sin(ph) + 0.4 * np.sin(2 * ph) + 0.2 * np.sin(3.01 * ph)
    trem = 0.7 + 0.3 * np.sin(2 * np.pi * 22 * t)
    swell = np.clip(t / 1.4, 0, 1) ** 2 * np.exp(-np.maximum(0, t - 1.55) / 0.08)
    x = tone * trem * swell
    x += 0.5 * sweep_lp(noise(len(t)), 400, 6000, sr) * swell
    return norm(lp(x, 8000, sr), 0.8)


def sfx_boost(sr=22050):
    t = t_of(0.55, sr)
    x = bp(noise(len(t)), 500, 2200, sr) * np.minimum(1, t / 0.03) * np.exp(-t / 0.2)
    ph, f = chirp(70, 140, 0.55, sr)
    x += 0.5 * np.sin(ph) * np.exp(-t / 0.2)
    return norm(x, 0.8)


def sfx_cash(sr=22050):
    t = t_of(0.5, sr)
    a = np.sin(2 * np.pi * 1568 * t) * np.exp(-t / 0.08)
    k = int(0.07 * sr)
    b = np.zeros(len(t))
    tt = np.arange(len(t) - k) / sr
    b[k:] = (np.sin(2 * np.pi * 2093 * tt) + 0.3 * np.sin(2 * np.pi * 4186 * tt)) * np.exp(-tt / 0.15)
    return norm(a + b, 0.6)


def sfx_alarm(sr=22050):
    return norm(lp(blip((880, 660, 880, 660), 0.12, sr, shape=0.9, tail=0.2), 3500, sr), 0.6)


def sfx_scan(sr=22050):
    t = t_of(0.7, sr)
    ping = np.sin(2 * np.pi * 1400 * t) * np.exp(-t / 0.07)
    k = int(0.22 * sr)
    echo = np.zeros(len(t))
    echo[k:] = 0.35 * ping[:len(t) - k]
    return norm(ping + echo, 0.6)


def sfx_banner(sr=22050):
    t = t_of(1.8, sr)
    x = np.zeros(len(t))
    for f in (110, 164.8, 261.6, 220 * 1.498):
        ph = 2 * np.pi * f * t
        x += sum(np.sin(k * ph) / k for k in range(1, 8))
    x = lp(x, 1800, sr) * np.minimum(1, t / 0.35) * np.exp(-np.maximum(0, t - 0.35) / 0.6)
    x += 0.5 * np.sin(2 * np.pi * 880 * t) * np.exp(-t / 0.5) * np.minimum(1, t / 0.005)
    return norm(x, 0.8)


def sfx_scoop(sr=22050):
    return norm(blip((1047, 1319, 1568, 2093), 0.06, sr, shape=0.3), 0.55)


def sfx_powerdown(sr=22050):
    t = t_of(0.9, sr)
    ph, f = chirp(700, 55, 0.9, sr)
    x = np.tanh(2 * np.sin(ph)) * np.exp(-t / 0.35)
    return norm(lp(x, 2500, sr), 0.7)
