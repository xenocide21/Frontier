"""Outer Rim Syndicate ships (Milestone 13g): home-made hulls welded together
from scrapped civilian, UFS and FSA ships. Mismatched paint, asymmetric parts,
bare girders and patch plates. Run this file instead of ships2.py:
    python3 ships3.py OUTDIR [names...] [--top]"""
from ships2 import *
import random


def ors_mats():
    return dict(
        tan=node_mat("o_tan", (0.40, 0.31, 0.20), panel=1.1, dirt=0.65),        # civilian
        orange=node_mat("o_orange", (0.55, 0.24, 0.05), panel=1.2, dirt=0.6),
        navy=node_mat("o_navy", (0.07, 0.13, 0.28), panel=1.3, dirt=0.55),      # UFS
        white=node_mat("o_white", (0.55, 0.56, 0.57), panel=1.4, dirt=0.6),
        olive=node_mat("o_olive", (0.18, 0.18, 0.13), metal=0.5, panel=1.0, dirt=0.65),   # FSA
        red=node_mat("o_red", (0.42, 0.05, 0.03), panel=1.2, dirt=0.6),
        primer=node_mat("o_primer", (0.30, 0.29, 0.27), panel=0.7, dirt=0.7),    # bare patch plate
        rust=node_mat("o_rust", (0.38, 0.13, 0.05), panel=0.6, dirt=0.7),
        dark=node_mat("o_dark", (0.07, 0.07, 0.07), metal=0.75, panel=0.8, dirt=0.35),
        glass=node_mat("glass", (0.18, 0.12, 0.03), glass=True),
        glow_o=node_mat("o_glow_o", None, emit=(1.0, 0.45, 0.12)),
        glow_b=node_mat("o_glow_b", None, emit=(0.25, 0.55, 1.0)),
        green=node_mat("o_green", None, emit=(0.4, 1.0, 0.2)))


def patches(b, region, m, count, seed, z, size=(0.4, 1.4), h=0.08):
    """Welded patch plates in clashing colours."""
    rnd = random.Random(seed)
    x0, x1, y0, y1 = region
    for _ in range(count):
        w = rnd.uniform(*size); l = rnd.uniform(*size)
        x = rnd.uniform(x0, x1 - w); y = rnd.uniform(y0, y1 - l)
        b.box(x, x + w, y, y + l, z, z + h, rnd.choice(m))


def disc(b, x, y, r, z0, z1, m, n=10):
    """A round turret base seen from above."""
    import math
    b.prism([(x + r * math.cos(2 * math.pi * k / n), y + r * math.sin(2 * math.pi * k / n)) for k in range(n)], z0, z1, m)


def girder(b, x0, y0, x1, y1, z, w, m):
    """A bare beam between two points, with cross braces."""
    import math
    L = math.hypot(x1 - x0, y1 - y0); nx, ny = -(y1 - y0) / L * w, (x1 - x0) / L * w
    b.prism([(x0 + nx, y0 + ny), (x1 + nx, y1 + ny), (x1 - nx, y1 - ny), (x0 - nx, y0 - ny)], z - w, z + w, m)


def scrapper():
    """Light fighter: a civilian fuselage with a UFS wing on one side and an FSA wing on the other."""
    m = ors_mats()
    b = Builder("body")
    b.rings([octagon(9.0, 0.5, -0.3, 0.3, 0.2), octagon(6.5, 1.2, -0.6, 0.8, 0.4), octagon(1.5, 1.5, -0.7, 1.2, 0.5),
             octagon(-3.5, 1.6, -0.7, 1.1, 0.5), octagon(-6.5, 1.2, -0.5, 0.7, 0.4)],
            [m["rust"], m["tan"], m["primer"], m["tan"]])
    b.rings([octagon(5.8, 0.6, 0.6, 1.0, 0.25), octagon(3.8, 0.9, 0.8, 1.5, 0.35), octagon(1.8, 0.8, 0.9, 1.4, 0.3)], [m["glass"]])
    b.box(-0.2, 0.2, 7.0, 11.5, -0.2, 0.1, m["dark"])                        # one salvaged nose gun
    b.box(0.6, 0.9, -6.0, 1.0, 1.1, 1.35, m["dark"])                          # a bare fuel line
    patches(b, (-1.2, 1.2, -5.5, 1.0), [m["primer"], m["rust"], m["orange"], m["navy"]], 10, 4, 1.1)
    # right: a clean UFS wing, swept, with a blue engine
    b.prism([(1.3, 1.0), (7.2, -5.0), (7.6, -7.0), (1.3, -4.8)], -0.25, 0.25, m["white"])
    b.prism([(3.0, -2.0), (5.8, -4.8), (6.0, -5.6), (3.0, -3.2)], 0.25, 0.32, m["navy"])
    nacelle(b, 2.3, 0.3, -1.5, -8.0, 0.8, m["white"], m["glow_b"], m["dark"])
    # left: a blunt FSA wing, shorter and thicker, with an orange engine
    b.prism([(-1.3, 2.0), (-5.6, -1.5), (-6.0, -6.0), (-1.3, -5.0)], -0.45, 0.45, m["olive"])
    b.prism([(-2.0, 0.6), (-4.6, -1.6), (-4.8, -2.6), (-2.0, -0.6)], 0.45, 0.52, m["red"])
    nacelle(b, -2.5, 0.4, -2.0, -8.8, 1.0, m["olive"], m["glow_o"], m["dark"])
    b.finish()
    return dict(length=20.0, px=38, beauty_dist=38)


def jackal():
    """Heavy fighter: an FSA gunship nose, a civilian cargo pod and UFS engines, one long cannon."""
    m = ors_mats()
    b = Builder("body")
    b.rings(oct_secs([(11.0, 1.4, -0.7, 0.5, 0.5), (7.5, 3.0, -1.3, 1.4, 0.9), (3.0, 3.2, -1.4, 1.6, 0.9)]),
            [m["olive"], m["olive"]])                                              # FSA nose
    b.rings(oct_secs([(8.0, 1.1, 0.9, 1.4, 0.35), (5.6, 1.5, 1.3, 2.2, 0.45), (4.0, 1.4, 1.4, 2.1, 0.45)]), [m["glass"]])
    b.box(-3.6, 3.6, -6.0, 3.0, -1.6, 1.9, m["orange"])                        # civilian cargo pod
    b.box(-3.7, 3.7, -2.0, -1.2, -1.7, 2.0, m["dark"])                         # weld seam band
    b.box(-3.7, 3.7, 1.6, 2.4, -1.7, 2.0, m["dark"])
    b.rings(oct_secs([(-6.0, 2.6, -1.2, 1.3, 0.8), (-10.0, 2.2, -1.0, 1.0, 0.7)]), [m["navy"]])   # UFS tail
    patches(b, (-3.4, 3.4, -5.5, 2.5), [m["primer"], m["rust"], m["tan"], m["white"]], 14, 8, 1.9, (0.5, 1.6))
    for x in (2.8, -2.8):
        nacelle(b, x, 0.2, -6.0, -12.5, 1.15, m["white"], m["glow_b"], m["dark"])
    # right: one long salvaged cannon; left: a crude missile pod on a strut
    b.rings([circle_y(13.5, 4.6, -0.2, 0.4, 8), circle_y(2.0, 4.6, -0.2, 0.4, 8)], [m["dark"]])
    b.box(3.6, 5.6, -2.0, 3.0, -0.9, 0.5, m["primer"])
    girder(b, -3.6, -1.0, -6.0, -1.0, 0.0, 0.25, m["dark"])
    b.box(-7.2, -5.6, -3.5, 2.5, -0.7, 0.7, m["red"])
    b.box(-7.0, -5.8, 2.5, 3.0, -0.5, 0.5, m["dark"])
    b.finish()
    return dict(length=27.0, px=42)


def chopshop():
    """Corvette: a civilian hauler spine with a UFS armoured prow welded on, mismatched side pods."""
    m = ors_mats()
    b = Builder("body")
    # UFS prow (a wedge of armour)
    b.prism([(0.0, 56.0), (12.0, 38.0), (13.0, 26.0), (-13.0, 26.0), (-12.0, 38.0)], -5.0, 5.0, m["navy"])
    b.prism([(0.0, 52.0), (8.0, 40.0), (9.0, 30.0), (-9.0, 30.0), (-8.0, 40.0)], 5.0, 5.8, m["white"])
    b.box(-3.0, 3.0, 36.0, 44.0, 5.8, 7.0, m["glass"])
    # civilian spine: stacked boxes, each its own colour
    cols = [m["orange"], m["tan"], m["primer"], m["orange"], m["rust"]]
    for i, y in enumerate((26.0, 14.0, 2.0, -10.0, -22.0)):
        w = 9.0 + (i % 2) * 2.0
        b.box(-w, w, y - 11.5, y, -4.5, 4.5 + (i % 3), cols[i])
    b.box(-10.5, 10.5, -36.0, -33.5, -5.0, 5.0, m["dark"])
    # left: an FSA pod with a red chevron; right: a white UFS pod, set further back
    b.box(-24.0, -14.0, -6.0, 20.0, -3.5, 3.5, m["olive"])
    b.prism([(-23.0, 14.0), (-15.0, 18.0), (-15.0, 14.0), (-23.0, 10.0)], 3.5, 4.0, m["red"])
    b.box(14.0, 22.0, -26.0, 6.0, -3.0, 3.0, m["white"])
    b.box(15.0, 21.0, -12.0, -8.0, 3.0, 3.5, m["navy"])
    for y in (16.0, 0.0, -18.0):                                             # bare girders holding the pods
        girder(b, -14.0, y - 2.0, -9.0, y - 2.0, 0.0, 0.8, m["dark"])
    for y in (2.0, -20.0):
        girder(b, 9.0, y, 14.0, y, 0.0, 0.8, m["dark"])
    # engines: two different kinds
    nacelle(b, -19.0, 0.0, -6.0, -34.0, 4.0, m["olive"], m["glow_o"], m["dark"])
    nacelle(b, 18.0, 0.0, -26.0, -44.0, 3.4, m["white"], m["glow_b"], m["dark"])
    nacelle(b, 0.0, 0.0, -36.0, -50.0, 5.0, m["primer"], m["glow_o"])
    # turrets: a heavy dome on top, a smaller one on the prow
    disc(b, 0.0, 2.0, 5.0, 4.5, 8.0, m["dark"])
    b.box(-4.0, 4.0, -2.0, 6.0, 5.5, 9.0, m["primer"]); b.box(-0.8, 0.8, 6.0, 16.0, 7.0, 8.4, m["dark"])
    b.box(-2.5, 2.5, 30.0, 34.0, 5.8, 8.0, m["dark"]); b.box(-0.5, 0.5, 34.0, 40.0, 6.8, 7.4, m["dark"])
    patches(b, (-10.0, 10.0, -32.0, 24.0), [m["primer"], m["rust"], m["navy"], m["olive"], m["white"]],
            70, 31, 5.0, (1.2, 4.0), 0.5)
    b.box(-1.0, 1.0, -30.0, -26.0, 6.0, 7.0, m["green"])                     # a hangar light
    b.finish(bevel=0.3)
    return dict(length=106.0, px=96, frame=128)


def marauder():
    """Frigate: an FSA prow, a spine of welded civilian cargo containers and a UFS engine block,
    heavy turrets on top and a lone outrigger engine on one side."""
    m = ors_mats()
    b = Builder("body")
    b.prism([(0.0, 104.0), (22.0, 72.0), (26.0, 56.0), (-26.0, 56.0), (-22.0, 72.0)], -8.0, 7.0, m["olive"])   # FSA prow
    b.prism([(0.0, 98.0), (11.0, 76.0), (12.0, 62.0), (-12.0, 62.0), (-11.0, 76.0)], 7.0, 8.0, m["red"])
    b.box(-5.0, 5.0, 66.0, 76.0, 8.0, 11.0, m["glass"])
    # container spine
    cols = [m["orange"], m["navy"], m["tan"], m["orange"], m["white"], m["rust"], m["tan"]]
    y = 56.0
    for i, c in enumerate(cols):
        l = 15.0 + (i % 3) * 3.0
        w = 19.0 + ((i * 7) % 6)
        b.box(-w, w, y - l, y, -7.0, 6.0 + (i % 2) * 2.0, c)
        b.box(-w - 0.5, w + 0.5, y - 1.2, y, -7.5, 6.5 + (i % 2) * 2.0, m["dark"])   # weld ring
        y -= l + 1.0
    # a second row of containers welded along the left flank
    fy = 48.0
    for i, c in enumerate([m["tan"], m["red"], m["orange"], m["primer"], m["olive"]]):
        l = 16.0 + (i % 2) * 6.0
        b.box(-42.0 + (i % 2) * 3.0, -18.0, fy - l, fy, -6.0, 4.0, c)
        b.box(-42.5 + (i % 2) * 3.0, -18.0, fy - 1.0, fy, -6.5, 4.5, m["dark"])
        fy -= l + 1.0
    # UFS engine block
    b.box(-26.0, 26.0, y - 26.0, y, -8.0, 8.0, m["navy"])
    b.box(-16.0, 16.0, y - 20.0, y - 4.0, 8.0, 9.0, m["white"])
    ye = y - 26.0
    nacelle(b, -11.0, 0.0, ye + 6.0, ye - 10.0, 6.0, m["navy"], m["glow_b"], m["dark"])
    nacelle(b, 11.0, 0.0, ye + 6.0, ye - 12.0, 6.5, m["primer"], m["glow_o"], m["dark"])
    nacelle(b, 0.0, 3.0, ye + 2.0, ye - 6.0, 4.0, m["olive"], m["glow_o"])
    # one outrigger: a civilian engine on a bare girder, right side only
    girder(b, 22.0, -30.0, 40.0, -42.0, 0.0, 1.4, m["dark"])
    girder(b, 22.0, 10.0, 40.0, -20.0, 0.0, 1.0, m["dark"])
    nacelle(b, 42.0, 0.0, -14.0, -62.0, 6.0, m["orange"], m["glow_o"], m["tan"])
    # heavy turrets (domes with long barrels) and a spinal mass driver
    for ty, tx in ((40.0, -6.0), (10.0, 6.0), (-24.0, -5.0)):
        disc(b, tx, ty, 6.0, 7.0, 10.0, m["dark"])
        b.box(tx - 5.0, tx + 5.0, ty - 5.0, ty + 5.0, 8.0, 12.0, m["primer"])
        b.box(tx - 1.6, tx - 0.6, ty + 5.0, ty + 18.0, 10.0, 11.4, m["dark"])
        b.box(tx + 0.6, tx + 1.6, ty + 5.0, ty + 18.0, 10.0, 11.4, m["dark"])
    b.box(-2.0, 2.0, 56.0, 110.0, -9.0, -6.0, m["dark"])                       # rail barrel under the prow
    patches(b, (-18.0, 18.0, y + 2.0, 54.0), [m["primer"], m["rust"], m["olive"], m["white"], m["navy"]],
            110, 47, 6.0, (2.0, 6.0), 0.8)
    b.box(-1.0, 1.0, y - 3.0, y - 1.0, 9.0, 10.0, m["green"])
    b.finish(bevel=0.4)
    return dict(length=220.0, px=124, frame=128)


SHIPS.update(scrapper=scrapper, jackal=jackal, chopshop=chopshop, marauder=marauder)

if __name__ == "__main__":
    main()
