"""The FRONTIER title logo (Milestone 14e): chrome letters with a bevel, a dark
outline and a blue glow, 480 x 120. It is stored as 4-bit texture in eight
15-line strips, each with its own 16-colour palette (so the chrome gradient
gets far more than 16 colours overall); glow colours carry the mask bit, so
the console adds them to the scene behind while the letters stay solid.
    python3 logo.py ../../src/logo_data.c preview.png"""
import sys, math
from PIL import Image, ImageDraw, ImageFilter, ImageChops
from glyphs import GLYPHS
W, H, SS = 480, 120, 4
TEXT = "FRONTIER"
LW, LH, GAP, SW = 43, 58, 13, 9.5          # letter box, gap, stroke (final pixels)
X0 = (W - (len(TEXT) * LW + (len(TEXT) - 1) * GAP)) / 2 - 9
Y0 = 18
SHEAR = 0.16

def P(x, y):                                # letter-space pixel -> supersampled, slanted
    return ((x + (Y0 + LH - y) * SHEAR) * SS, y * SS)

def letters():
    im = Image.new("L", (W * SS, H * SS), 0); d = ImageDraw.Draw(im)
    for i, ch in enumerate(TEXT):
        ox = X0 + i * (LW + GAP)
        for line in GLYPHS[ch]:
            pts = [P(ox + SW / 2 + x / 4 * (LW - SW), Y0 + SW / 2 + y / 6 * (LH - SW)) for x, y in line]
            d.line(pts, fill=255, width=int(SW * SS), joint="curve")
            r = SW * SS / 2
            for (x, y) in pts: d.ellipse([x - r, y - r, x + r, y + r], fill=255)
    return im

def lerp(a, b, t): return tuple(a[k] + (b[k] - a[k]) * t for k in range(3))

def build():
    big = letters()
    mask = big.resize((W, H), Image.LANCZOS)
    blur = big.filter(ImageFilter.GaussianBlur(3 * SS)).resize((W, H), Image.LANCZOS)
    glow1 = big.filter(ImageFilter.GaussianBlur(9 * SS)).resize((W, H), Image.LANCZOS)
    glow2 = big.filter(ImageFilter.GaussianBlur(3.5 * SS)).resize((W, H), Image.LANCZOS)
    outline = big.filter(ImageFilter.MaxFilter(4 * SS + 1)).resize((W, H), Image.LANCZOS)
    m, bl, g1, g2, ol = mask.load(), blur.load(), glow1.load(), glow2.load(), outline.load()
    solid = [[None] * W for _ in range(H)]; add = [[(0, 0, 0)] * W for _ in range(H)]
    # the underline: a blade of light under the word, with a flare
    ly = Y0 + LH + 16
    for y in range(H):
        for x in range(W):
            a = m[x, y] / 255.0
            t = (y - Y0) / LH
            if t < 0.52: c = lerp((0.16, 0.42, 0.95), (0.90, 0.98, 1.0), max(0, t) / 0.52)
            else:        c = lerp((0.05, 0.10, 0.36), (0.40, 0.78, 1.0), min(1, (t - 0.52) / 0.48))
            # bevel: light from the upper left, from the blurred mask's slope
            sx = (bl[min(W - 1, x + 2), y] - bl[max(0, x - 2), y]) / 255.0
            sy = (bl[x, min(H - 1, y + 2)] - bl[x, max(0, y - 2)]) / 255.0
            e = (-sx - sy) * 1.5
            c = tuple(max(0, min(1, v + e * (0.9 if e > 0 else 0.6))) for v in c)
            o = ol[x, y] / 255.0
            glow = (g1[x, y] / 255.0) ** 1.2 * 1.1 + (g2[x, y] / 255.0) * 0.5
            gcol = (0.05 * glow, 0.38 * glow, 1.0 * glow)
            # underline
            dx = (x - 34) / (W - 68)
            if 0 <= dx <= 1:
                fall = math.sin(dx * math.pi) ** 0.6
                dy = abs(y - ly - 0.5)
                core = max(0, 1 - dy / 1.6) * fall; halo = max(0, 1 - dy / 9) ** 2 * fall * 0.55
                fl = math.exp(-((x - (34 + 0.80 * (W - 68))) ** 2) / 300.0) * max(0, 1 - dy / 14) ** 2
                gcol = (gcol[0] + core * 0.9 + halo * 0.15 + fl, gcol[1] + core + halo * 0.5 + fl, gcol[2] + core + halo + fl)
            if a > 0.5:    solid[y][x] = c
            elif o > 0.55: solid[y][x] = lerp((0.01, 0.03, 0.10), c, a * 0.9)
            else:          add[y][x] = tuple(min(1, v) for v in gcol)
    return solid, add

def ps1(c, stp):
    r, g, b = (max(0, min(31, round(v * 31))) for v in c)
    v = r | g << 5 | b << 10
    if not stp and v == 0: v = 0x0421                  # solid black must not be the clear colour
    return v | (0x8000 if stp else 0)

def quant(cols, n):
    if not cols: return []
    im = Image.new("RGB", (len(cols), 1)); im.putdata([tuple(round(v * 255) for v in c) for c in cols])
    q = im.quantize(colors=n, method=Image.MEDIANCUT, dither=Image.NONE)
    pal = q.getpalette()[:n * 3]
    return [tuple(pal[i * 3 + k] / 255 for k in range(3)) for i in range(len(set(q.getdata())) and n)]

def nearest(pal, c): return min(range(len(pal)), key=lambda i: sum((pal[i][k] - c[k]) ** 2 for k in range(3)))

if __name__ == "__main__":
    out, prev = sys.argv[1], sys.argv[2]
    solid, add = build()
    idx = Image.new("P", (W, H), 0); cluts = []; view = Image.new("RGB", (W, H), (6, 8, 20))
    for s in range(8):
        ys = range(s * 15, s * 15 + 15)
        sc = [solid[y][x] for y in ys for x in range(W) if solid[y][x]]
        ac = [add[y][x] for y in ys for x in range(W) if not solid[y][x] and max(add[y][x]) > 0.03]
        ns = 8 if sc else 0
        sp = quant(sc, ns) if sc else []; ap = quant(ac, 15 - len(sp)) if ac else []
        clut = [0] + [ps1(c, 0) for c in sp] + [ps1(c, 1) for c in ap]; clut += [0] * (16 - len(clut)); cluts.append(clut)
        for y in ys:
            for x in range(W):
                if solid[y][x]:
                    i = 1 + nearest(sp, solid[y][x]); c = sp[i - 1]
                    view.putpixel((x, y), tuple(round(v * 255) for v in c))
                elif max(add[y][x]) > 0.03 and ap:
                    j = nearest(ap, add[y][x]); i = 1 + len(sp) + j
                    if clut[i] & 0x7FFF == 0: i = 0
                    b = view.getpixel((x, y)); c = ap[j]
                    view.putpixel((x, y), tuple(min(255, b[k] + round(c[k] * 255)) for k in range(3)))
                else: i = 0
                idx.putpixel((x, y), i)
    with open(out, "w") as f:
        f.write('/* Generated by tools/artgen/logo.py: the title logo, two 240 x 120 halves, eight palettes. */\n#include "common.h"\n\n')
        for name, x0 in (("logo_left_pix", 0), ("logo_right_pix", 240)):
            f.write(f"const u16 {name}[] __attribute__((aligned(4))) = {{\n")
            words = []
            for y in range(H):
                for x in range(x0, x0 + 240, 4):
                    words.append(sum(idx.getpixel((x + k, y)) << (4 * k) for k in range(4)))
            for i in range(0, len(words), 20): f.write("    " + ",".join("0x%04x" % v for v in words[i:i + 20]) + ",\n")
            f.write("};\n")
        f.write("const u16 logo_cluts[8 * 16] __attribute__((aligned(4))) = {\n")
        for c in cluts: f.write("    " + ", ".join("0x%04x" % v for v in c) + ",\n")
        f.write("};\n")
    view.resize((960, 240), Image.LANCZOS).save(prev)
