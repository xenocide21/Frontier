"""The game's FRONTIER logo (tools/artgen/logo.py), redrawn K times larger with a transparent background for print."""
import sys, math
import os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image, ImageDraw, ImageFilter
from glyphs import GLYPHS
import numpy as np
K = 4
W, H, SS = 480 * K, 120 * K, 2
TEXT = "FRONTIER"
LW, LH, GAP, SW = 43 * K, 58 * K, 13 * K, 9.5 * K
X0 = (W - (len(TEXT) * LW + (len(TEXT) - 1) * GAP)) / 2 - 9 * K
Y0 = 18 * K
SHEAR = 0.16
def P(x, y): return ((x + (Y0 + LH - y) * SHEAR) * SS, y * SS)
big = Image.new("L", (W * SS, H * SS), 0); d = ImageDraw.Draw(big)
for i, ch in enumerate(TEXT):
    ox = X0 + i * (LW + GAP)
    for line in GLYPHS[ch]:
        pts = [P(ox + SW / 2 + x / 4 * (LW - SW), Y0 + SW / 2 + y / 6 * (LH - SW)) for x, y in line]
        d.line(pts, fill=255, width=int(SW * SS), joint="curve")
        r = SW * SS / 2
        for (x, y) in pts: d.ellipse([x - r, y - r, x + r, y + r], fill=255)
f = lambda img: np.asarray(img, dtype=np.float32) / 255.0
m = f(big.resize((W, H), Image.LANCZOS))
bl = f(big.filter(ImageFilter.GaussianBlur(3 * K * SS)).resize((W, H), Image.LANCZOS))
g1 = f(big.filter(ImageFilter.GaussianBlur(9 * K * SS)).resize((W, H), Image.LANCZOS))
g2 = f(big.filter(ImageFilter.GaussianBlur(3.5 * K * SS)).resize((W, H), Image.LANCZOS))
ol = f(big.filter(ImageFilter.MaxFilter(4 * K * SS + 1)).resize((W, H), Image.LANCZOS))
yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
t = (yy - Y0) / LH
def lerp(a, b, s): return [a[k] + (b[k] - a[k]) * s for k in range(3)]
top = lerp((0.16, 0.42, 0.95), (0.90, 0.98, 1.0), np.clip(t, 0, None) / 0.52)
bot = lerp((0.05, 0.10, 0.36), (0.40, 0.78, 1.0), np.clip((t - 0.52) / 0.48, None, 1))
c = [np.where(t < 0.52, top[k], bot[k]) for k in range(3)]
sh = 2 * K
sx = (np.roll(bl, -sh, 1) - np.roll(bl, sh, 1)); sy = (np.roll(bl, -sh, 0) - np.roll(bl, sh, 0))
e = (-sx - sy) * 1.5
c = [np.clip(v + e * np.where(e > 0, 0.9, 0.6), 0, 1) for v in c]
glow = g1 ** 1.2 * 1.1 + g2 * 0.5
gc = [0.05 * glow, 0.38 * glow, 1.0 * glow]
ly = Y0 + LH + 16 * K
dx = (xx - 34 * K) / (W - 68 * K)
inb = (dx >= 0) & (dx <= 1)
fall = np.where(inb, np.clip(np.sin(np.clip(dx, 0, 1) * math.pi), 0, 1) ** 0.6, 0)
dy = np.abs(yy - ly - 0.5 * K)
core = np.clip(1 - dy / (1.6 * K), 0, None) * fall
halo = np.clip(1 - dy / (9 * K), 0, None) ** 2 * fall * 0.55
fl = np.exp(-((xx - (34 * K + 0.80 * (W - 68 * K))) ** 2) / (300.0 * K * K)) * np.clip(1 - dy / (14 * K), 0, None) ** 2 * inb
gc = [gc[0] + core * 0.9 + halo * 0.15 + fl, gc[1] + core + halo * 0.5 + fl, gc[2] + core + halo + fl]
gc = [np.clip(v, 0, 1) for v in gc]
a = m; o = ol
dark = (0.01, 0.03, 0.10)
solid_c = [np.where(a > 0.5, c[k], dark[k] + (c[k] - dark[k]) * a * 0.9) for k in range(3)]
is_solid = (a > 0.5) | (o > 0.55)
# letters and outline opaque; the glow as colour with alpha = its strength (reads as light on a dark ground)
ga = np.clip(np.maximum.reduce(gc), 0, 1)
gcol = [np.where(ga > 0, gc[k] / np.maximum(ga, 1e-4), 0) for k in range(3)]
# soften the outline's outer edge into the glow
sa = np.clip((o - 0.3) / 0.25, 0, 1)
A = np.where(is_solid, 1.0, np.maximum(ga, sa * 0.9))
R = [np.where(is_solid, solid_c[k], np.where(sa * 0.9 > ga, dark[k], gcol[k])) for k in range(3)]
rgba = np.dstack([np.clip(R[0], 0, 1), np.clip(R[1], 0, 1), np.clip(R[2], 0, 1), A])
img = Image.fromarray((rgba * 255 + 0.5).astype(np.uint8), 'RGBA')
img = img.crop(img.getbbox())
img.save(sys.argv[1] if len(sys.argv) > 1 else 'logo_print.png')
print(img.size)
# preview over the dark ground
bg = Image.new('RGBA', img.size, (6, 8, 20, 255)); bg.alpha_composite(img); pass
