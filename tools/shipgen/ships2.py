"""The rest of the catalog (civilian, UFS, FSA, Veyr). Run this file instead of
ships.py: python3 ships2.py OUTDIR [names...] [--top]"""
from ships import *
import ships

def oct_secs(secs):
    return [octagon(*s) for s in secs]

# ============================================================ civilian

def frontier_runner():
    tan = node_mat("tan", (0.36, 0.29, 0.22), panel=1.2, dirt=0.5)
    tan2 = node_mat("tan2", (0.26, 0.21, 0.16), panel=1.0, dirt=0.5)
    red = node_mat("red", (0.42, 0.05, 0.04), panel=1.2, dirt=0.5)
    dark = node_mat("dark", (0.10, 0.09, 0.09), metal=0.7, panel=0.8, dirt=0.3)
    glass = node_mat("glass", (0.05, 0.10, 0.16), glass=True)
    glow = node_mat("glow", None, emit=(1.0, 0.45, 0.15))
    b = Builder("body")
    b.rings(oct_secs([(14.0, 1.6, -0.8, 0.6, 0.5), (12.0, 2.6, -1.3, 1.3, 0.6), (4.0, 3.0, -1.5, 1.8, 0.5),
                      (1.0, 3.2, -1.5, 1.9, 0.5), (-1.5, 3.2, -1.5, 1.9, 0.5), (-12.0, 3.0, -1.4, 1.7, 0.5),
                      (-13.5, 2.4, -1.1, 1.2, 0.5)]), [tan2, tan, tan, red, tan, tan2])
    b.rings(oct_secs([(12.6, 1.4, 0.4, 0.9, 0.3), (10.6, 2.0, 0.9, 1.6, 0.4), (9.0, 1.8, 1.0, 1.7, 0.4)]), [glass])
    b.rings(oct_secs([(3.0, 1.8, 1.6, 2.6, 0.5), (-10.0, 1.9, 1.6, 2.7, 0.5), (-11.0, 1.4, 1.6, 2.2, 0.4)]), [tan2])
    greebles(b, [(-2.6, 2.6, -11.0, 3.0, 1.75), (-1.6, 1.6, -9.0, 2.0, 2.65)], [tan2, dark, tan], 30, 21)
    b.finish()
    s = Builder("side")
    s.prism([(2.4, 13.5), (5.4, 11.6), (5.4, 10.4), (2.4, 10.6)], -0.6, -0.1, tan)        # canards
    s.prism([(3.0, -4.0), (5.2, -7.0), (5.2, -12.0), (3.0, -12.0)], -0.4, 0.3, tan)
    nacelle(s, 3.8, 2.2, -4.0, -15.0, 1.25, tan2, glow, red)
    s.finish(mirror=True)
    return dict(length=30.0, px=48)

def wayfinder():
    grey = node_mat("grey", (0.40, 0.41, 0.41), panel=1.2, dirt=0.4)
    grey2 = node_mat("grey2", (0.24, 0.25, 0.26), panel=1.0, dirt=0.4)
    orange = node_mat("orange", (0.75, 0.25, 0.05), panel=1.0, dirt=0.3)
    dark = node_mat("dark", (0.07, 0.07, 0.08), metal=0.7, panel=0.8, dirt=0.3)
    glass = node_mat("glass", (0.03, 0.05, 0.08), glass=True)
    glow = node_mat("glow", None, emit=(0.2, 0.5, 1.0))
    b = Builder("body")
    b.rings(oct_secs([(11.0, 1.0, -0.6, 0.3, 0.3), (8.0, 2.4, -1.0, 1.0, 0.6), (2.0, 3.2, -1.2, 1.5, 0.8),
                      (-6.0, 3.2, -1.2, 1.5, 0.8), (-8.5, 2.5, -1.0, 1.1, 0.6)]), [grey, grey, grey, grey2])
    b.rings(oct_secs([(8.6, 0.9, 0.0, 0.6, 0.3), (6.0, 1.6, 0.8, 1.6, 0.5), (3.0, 1.5, 1.0, 1.7, 0.5)]), [glass])
    b.prism([(-0.6, 1.0), (0.6, 1.0), (0.6, -5.0), (-0.6, -5.0)], 1.5, 2.2, grey2)
    greebles(b, [(-2.4, 2.4, -7.0, 0.5, 1.5)], [grey2, dark, orange], 14, 4, hmax=0.25)
    b.finish()
    s = Builder("side")
    s.prism([(1.6, 2.6), (2.6, 2.4), (4.4, -3.5), (1.6, -3.0)], 1.5, 1.62, orange)          # orange stripe
    s.prism([(3.0, 0.0), (8.8, -5.0), (9.2, -7.0), (3.0, -6.0)], -0.3, 0.3, grey)          # wing
    s.box(8.4, 9.3, -2.0, 4.5, -0.1, 0.25, grey2)                                          # wingtip boom
    nacelle(s, 4.4, 0.4, 0.5, -9.5, 1.35, grey2, glow, dark)
    s.finish(mirror=True)
    return dict(length=21.5, px=40)

def rockbreaker():
    steel = node_mat("steel", (0.24, 0.24, 0.25), metal=0.6, panel=1.6, dirt=0.5)
    steel2 = node_mat("steel2", (0.12, 0.12, 0.13), metal=0.7, panel=1.0, dirt=0.4)
    tan = node_mat("tan", (0.42, 0.36, 0.26), panel=1.4, dirt=0.5)
    yellow = node_mat("yellow", (0.75, 0.55, 0.06), panel=0.8, dirt=0.4)
    glow = node_mat("glow", None, emit=(0.2, 0.5, 1.0))
    lamp = node_mat("lamp", None, emit=(1.0, 0.7, 0.3))
    b = Builder("body")
    b.rings([circle_y(26.0, 0, 0, 0.3, 10), circle_y(22.0, 0, 0, 2.2, 10), circle_y(20.5, 0, 0, 2.8, 10),
             circle_y(-14.0, 0, 0, 2.8, 10), circle_y(-16.0, 0, 0, 2.2, 10)], [steel2, yellow, steel, steel])
    b.box(-3.8, 3.8, 12.0, 18.0, -1.5, 2.8, tan)                                           # control block
    b.box(-3.0, 3.0, 16.6, 17.4, 2.8, 3.1, lamp)
    for y in (6.0, -2.0, -10.0):
        b.box(-2.2, 2.2, y - 3.0, y + 3.0, 2.4, 3.6, tan)
    greebles(b, [(-2.0, 2.0, -15.0, 11.0, 2.7)], [steel2, yellow, tan], 22, 8, hmax=0.6)
    b.finish(bevel=0.1)
    s = Builder("side")
    for y0, y1 in ((10.0, 2.0), (0.0, -8.0)):
        s.rings(oct_secs([(y0, 2.2, -1.8, 1.8, 0.8, 6.2), (y1, 2.2, -1.8, 1.8, 0.8, 6.2)]), [tan])  # ore pods
        s.box(4.2, 8.2, y1 + 1.0, y1 + 1.8, 1.8, 2.0, yellow)
        s.box(2.6, 4.2, y1 + 2.0, y0 - 2.0, -0.6, 0.6, steel2)
    for x0, y0, x1, y1 in ((2.5, 14.0, 6.5, 24.0), (3.0, 10.0, 9.5, 19.0)):                # spindly arms
        horn(s, [(x0, y0), ((x0 + x1) / 2 + 1.5, (y0 + y1) / 2), (x1, y1)], [0.35, 0.3, 0.2], 0.5, 0.35, steel2)
    nacelle(s, 3.4, 0.0, -12.0, -20.0, 1.5, steel, glow, steel2)
    s.finish(mirror=True, bevel=0.08)
    return dict(length=47.0, px=58)

def reclaimer():
    orange = node_mat("orange", (0.55, 0.18, 0.05), panel=1.8, dirt=0.55)
    black = node_mat("black", (0.06, 0.06, 0.07), metal=0.7, panel=1.0, dirt=0.3)
    grey = node_mat("grey", (0.35, 0.36, 0.38), metal=0.6, panel=1.0, dirt=0.4)
    glass = node_mat("glass", (0.05, 0.12, 0.16), glass=True)
    glow = node_mat("glow", None, emit=(0.2, 0.55, 1.0))
    lamp = node_mat("lamp", None, emit=(0.6, 0.9, 1.0))
    b = Builder("body")
    b.rings(oct_secs([(14.0, 6.0, -3.0, 2.5, 1.6), (10.0, 7.5, -3.5, 3.5, 1.8), (-12.0, 7.5, -3.5, 3.5, 1.8),
                      (-16.0, 6.0, -3.0, 2.8, 1.5)]), [orange, orange, black])
    b.rings(oct_secs([(12.6, 2.6, 2.0, 3.0, 0.6), (10.6, 3.2, 3.0, 4.2, 0.7), (8.6, 2.8, 3.0, 4.0, 0.6)]), [glass])
    b.box(-1.2, 1.2, -12.0, 6.0, 3.4, 4.8, black)                                          # crane track
    b.box(-0.8, 0.8, -6.0, -4.0, 4.8, 7.0, grey)
    b.box(-0.4, 0.4, -5.6, 16.0, 6.4, 7.0, grey)                                          # crane boom
    greebles(b, [(-6.5, -2.0, -14.0, 7.0, 3.5), (2.0, 6.5, -14.0, 7.0, 3.5)], [black, grey, orange], 30, 12, hmax=0.7)
    b.box(-5.0, 5.0, 8.0, 8.6, 3.5, 3.7, lamp)
    b.finish(bevel=0.12)
    s = Builder("side")
    horn(s, [(5.0, 10.0), (8.5, 17.0), (7.0, 24.0)], [0.9, 0.8, 0.6], 0.0, 0.8, grey)    # grab arms
    horn(s, [(7.0, 24.0), (5.0, 26.0), (3.6, 25.0)], [0.6, 0.5, 0.35], 0.0, 0.6, black)  # claws
    s.box(7.5, 10.5, -8.0, 6.0, -2.5, 2.5, black)                                          # side frames
    nacelle(s, 4.6, 0.0, -14.0, -22.0, 1.9, black, glow, grey)
    s.finish(mirror=True, bevel=0.1)
    return dict(length=48.0, px=60)

# ============================================================ UFS

def watchman():
    m = faction_mats("ufs")
    b = Builder("body")
    # the sensor dome: flat discs stacked
    pts = [(5.6 * math.cos(2 * math.pi * k / 20), 5.6 * math.sin(2 * math.pi * k / 20)) for k in range(20)]
    b.prism(pts, -0.8, 1.2, m["main"])
    pts2 = [(4.0 * math.cos(2 * math.pi * k / 20), 4.0 * math.sin(2 * math.pi * k / 20)) for k in range(20)]
    b.prism(pts2, 1.2, 1.5, m["white"])
    pts3 = [(2.4 * math.cos(2 * math.pi * k / 16), 2.4 * math.sin(2 * math.pi * k / 16)) for k in range(16)]
    b.prism(pts3, 1.5, 1.8, m["main"])
    b.rings(oct_secs([(13.0, 1.0, -0.6, 0.6, 0.3), (8.0, 1.8, -0.9, 1.0, 0.5), (-12.0, 1.6, -0.8, 0.9, 0.5)]), [m["light"], m["main"]])
    b.rings(oct_secs([(10.0, 0.8, 0.6, 1.0, 0.3), (7.0, 1.3, 0.8, 1.5, 0.4), (5.6, 1.2, 0.8, 1.4, 0.4)]), [m["glass"]])
    b.finish()
    s = Builder("side")
    s.prism([(1.0, 3.0), (11.0, 9.0), (11.6, 8.0), (2.5, 0.0)], -0.3, 0.3, m["light"])      # forward X blades
    s.prism([(1.0, -3.0), (11.0, -11.0), (11.8, -10.0), (2.5, -0.5)], -0.3, 0.3, m["main"])  # rear X blades
    s.box(10.6, 11.4, -11.6, -9.0, -0.3, 0.5, m["accent"])
    s.box(13.0, 13.6, -6.0, 12.0, 0.2, 0.7, m["white"])                                      # sensor boom
    s.box(11.0, 13.4, 8.2, 9.0, 0.2, 0.6, m["white"])
    nacelle(s, 1.6, -0.4, -9.0, -14.0, 0.9, m["light"], m["glow"])
    s.finish(mirror=True)
    return dict(length=27.0, px=44)

def talon():
    m = faction_mats("ufs")
    b = Builder("body")
    b.rings(oct_secs([(9.5, 0.4, -0.3, 0.2, 0.15), (6.5, 1.5, -0.7, 0.8, 0.5), (0.0, 2.0, -0.8, 1.1, 0.6),
                      (-6.5, 1.6, -0.7, 0.8, 0.5)]), [m["white"], m["white"], m["light"]])
    b.rings(oct_secs([(5.5, 0.7, 0.5, 0.9, 0.25), (3.0, 1.1, 0.8, 1.5, 0.35), (1.0, 1.0, 0.8, 1.4, 0.35)]), [m["glass"]])
    greebles(b, [(-1.4, 1.4, -6.0, -0.5, 1.0)], [m["light"], m["dark"]], 8, 2, hmax=0.2)
    b.finish()
    s = Builder("side")
    s.prism([(1.4, 1.0), (7.5, -5.5), (7.8, -7.5), (1.4, -5.0)], -0.25, 0.25, m["white"])
    s.prism([(3.0, -2.2), (6.0, -5.2), (6.2, -6.0), (3.0, -3.4)], 0.25, 0.32, m["sky"])
    nacelle(s, 2.2, 0.3, -1.5, -8.5, 0.85, m["light"], m["glow"], m["dark"])
    s.finish(mirror=True)
    return dict(length=18.5, px=30)

def vanguard():
    m = faction_mats("ufs")
    b = Builder("body")
    b.rings(oct_secs([(13.0, 0.6, -0.4, 0.3, 0.2), (9.0, 1.8, -0.9, 1.0, 0.6), (0.0, 2.4, -1.1, 1.4, 0.7),
                      (-9.0, 2.2, -1.0, 1.2, 0.7), (-11.0, 1.6, -0.8, 0.9, 0.5)]), [m["dark"], m["sky"], m["main"], m["dark"]])
    b.rings(oct_secs([(10.5, 0.8, 0.3, 0.7, 0.25), (8.0, 1.3, 0.8, 1.4, 0.4), (5.5, 1.2, 0.9, 1.5, 0.4)]), [m["glass"]])
    greebles(b, [(-2.0, 2.0, -9.0, 4.0, 1.3)], [m["light"], m["dark"], m["sky"]], 18, 6, hmax=0.3)
    b.finish()
    s = Builder("side")
    s.prism([(2.0, 3.0), (9.5, -3.5), (11.0, -9.0), (9.6, -9.0), (2.0, -6.0)], -0.3, 0.3, m["main"])
    s.prism([(4.0, 0.0), (8.0, -3.6), (8.4, -5.0), (4.0, -2.0)], 0.3, 0.38, m["sky"])
    s.box(9.4, 11.2, -9.6, -8.8, 0.3, 0.45, m["accent"])
    for x in (4.6, 7.0):
        s.box(x - 0.3, x + 0.3, -2.0, 4.0, -0.7, -0.1, m["dark"])                             # gun pods
    nacelle(s, 2.8, 0.4, -5.0, -12.5, 1.0, m["light"], m["glow"], m["dark"])
    s.finish(mirror=True)
    return dict(length=26.0, px=40)

def sentinel():
    m = faction_mats("ufs")
    b = Builder("body")
    b.rings(oct_secs([(48.0, 3.0, -3.0, 1.0, 1.0), (36.0, 12.0, -5.0, 4.0, 3.0), (-30.0, 16.0, -6.0, 6.0, 4.0),
                      (-44.0, 14.0, -5.0, 5.0, 3.5)]), [m["main"], m["main"], m["dark"]])
    for y in (16.0, 10.0):
        b.box(-11.0, 11.0, y, y + 2.5, 4.0, 4.6, m["white"])                                  # white bands
    b.box(-6.0, 6.0, -16.0, 4.0, 5.0, 9.0, m["light"])                                       # bridge block
    b.box(-3.0, 3.0, -14.0, 0.0, 9.0, 11.0, m["accent"])
    b.box(-0.6, 0.6, -24.0, -4.0, 6.0, 7.2, m["white"])
    greebles(b, [(-13.0, 13.0, -40.0, 30.0, 5.0)], [m["light"], m["dark"], m["main"]], 70, 31, hmax=1.4)
    b.finish(bevel=0.3)
    s = Builder("side")
    s.rings(oct_secs([(6.0, 4.0, -4.0, 3.0, 1.5, 17.0), (-36.0, 4.5, -4.5, 3.5, 1.6, 17.0),
                      (-46.0, 3.5, -3.5, 2.5, 1.2, 17.0)]), [m["main"], m["dark"]])
    s.box(13.0, 21.0, -20.0, -17.0, 3.0, 3.8, m["white"])
    nacelle(s, 17.0, 0.0, -42.0, -50.0, 3.2, m["dark"], m["glow"])
    s.finish(mirror=True, bevel=0.3)
    return dict(length=98.0, px=92, frame=128)

def bastion():
    m = faction_mats("ufs")
    b = Builder("body")
    b.rings(oct_secs([(92.0, 8.0, -6.0, 4.0, 3.0), (80.0, 14.0, -9.0, 8.0, 4.0), (-80.0, 14.0, -9.0, 8.0, 4.0),
                      (-90.0, 11.0, -7.0, 6.0, 3.0)]), [m["white"], m["light"], m["dark"]])
    for y in range(-70, 70, 14):
        b.box(-12.0, 12.0, y, y + 10, 8.0, 11.0, m["main"] if (y // 14) % 2 else m["sky"])  # module blocks
        b.box(-3.0, 3.0, y + 3, y + 7, 11.0, 12.0, m["accent"])
    b.box(-8.0, 8.0, 60.0, 76.0, 8.0, 16.0, m["light"])                                       # bridge
    b.box(-6.0, 6.0, 70.0, 74.0, 16.0, 17.0, m["lamp"])
    greebles(b, [(-13.0, 13.0, -78.0, 58.0, 11.0)], [m["light"], m["dark"], m["white"]], 90, 41, hmax=2.0)
    b.finish(bevel=0.4)
    s = Builder("side")
    for y in (40.0, 0.0, -40.0):
        s.box(14.0, 22.0, y - 12, y + 12, -6.0, 6.0, m["main"])                               # side blisters
        s.box(17.0, 20.0, y - 4, y + 4, 6.0, 9.0, m["dark"])                                  # turrets
        s.box(18.0, 19.0, y + 4, y + 12, 7.0, 8.0, m["light"])
    nacelle(s, 9.0, 0.0, -78.0, -96.0, 6.0, m["dark"], m["glow"])
    s.finish(mirror=True, bevel=0.4)
    return dict(length=190.0, px=122, frame=128)

def resolute():
    m = faction_mats("ufs")
    b = Builder("body")
    b.rings(oct_secs([(170.0, 6.0, -6.0, 4.0, 3.0), (140.0, 22.0, -14.0, 12.0, 7.0), (-150.0, 26.0, -16.0, 14.0, 8.0),
                      (-168.0, 20.0, -12.0, 10.0, 6.0)]), [m["sky"], m["light"], m["dark"]])
    for i, y in enumerate(range(-130, 120, 22)):
        w = 20 if i % 3 else 26
        b.box(-w, w, y, y + 16, 12.0, 20.0, [m["sky"], m["white"], m["main"]][i % 3])
        b.box(-w + 4, w - 4, y + 4, y + 12, 20.0, 22.0, m["lamp"] if i % 4 == 1 else m["dark"])
    b.rings([circle_y(-60.0, 0, 0, 30.0, 20), circle_y(-70.0, 0, 0, 30.0, 20)], [m["light"]])  # ring section
    greebles(b, [(-20.0, 20.0, -140.0, 120.0, 20.0)], [m["light"], m["dark"], m["white"], m["main"]], 160, 51, hmax=4.0)
    b.finish(bevel=0.6)
    s = Builder("side")
    s.prism([(24.0, 110.0), (52.0, 40.0), (56.0, -120.0), (24.0, -140.0)], -8.0, 6.0, m["sky"])   # flank decks
    for y in (60.0, 0.0, -60.0, -110.0):
        s.box(34.0, 50.0, y - 14, y + 14, 6.0, 12.0, m["main"])
        s.box(38.0, 46.0, y - 6, y + 6, 12.0, 17.0, m["dark"])
        s.box(41.0, 43.0, y + 6, y + 28, 13.0, 15.0, m["light"])                               # big guns
    nacelle(s, 14.0, 0.0, -150.0, -178.0, 9.0, m["dark"], m["glow"])
    s.finish(mirror=True, bevel=0.6)
    return dict(length=350.0, px=180, frame=256)

def sovereign():
    m = faction_mats("ufs")
    b = Builder("body")
    b.rings(oct_secs([(310.0, 4.0, -4.0, 2.0, 2.0), (250.0, 30.0, -16.0, 12.0, 9.0), (100.0, 55.0, -26.0, 20.0, 16.0),
                      (-260.0, 60.0, -28.0, 22.0, 18.0), (-300.0, 46.0, -22.0, 16.0, 14.0)]),
            [m["white"], m["light"], m["sky"], m["dark"]])
    for i, y in enumerate(range(-240, 200, 30)):
        w = 40 - abs(i - 7) * 2
        b.box(-w, w, y, y + 22, 20.0, 30.0, [m["light"], m["white"], m["sky"]][i % 3])
    b.box(-20.0, 20.0, -200.0, -120.0, 30.0, 52.0, m["light"])                                # command tower
    b.box(-14.0, 14.0, -150.0, -140.0, 52.0, 56.0, m["lamp"])
    greebles(b, [(-50.0, 50.0, -250.0, 200.0, 30.0)], [m["light"], m["dark"], m["white"], m["main"]], 260, 61, hmax=7.0)
    b.finish(bevel=1.0)
    s = Builder("side")
    s.prism([(50.0, 60.0), (95.0, -150.0), (95.0, -230.0), (55.0, -250.0)], -10.0, 10.0, m["main"])  # wing decks
    for y in (20.0, -60.0, -140.0, -200.0):
        s.box(66.0, 82.0, y - 10, y + 10, 10.0, 18.0, m["dark"])
        s.box(72.0, 76.0, y + 10, y + 38, 13.0, 16.0, m["light"])
    for x in (20.0, 44.0):
        nacelle(s, x, 0.0, -270.0, -312.0, 13.0, m["dark"], m["glow"])
    s.finish(mirror=True, bevel=1.0)
    return dict(length=630.0, px=250, frame=256)

SHIPS.update(frontier_runner=frontier_runner, wayfinder=wayfinder, rockbreaker=rockbreaker, reclaimer=reclaimer,
             watchman=watchman, talon=talon, vanguard=vanguard, sentinel=sentinel, bastion=bastion,
             resolute=resolute, sovereign=sovereign)

# ============================================================ FSA

def ghost():
    silver = node_mat("silver", (0.38, 0.40, 0.43), metal=0.7, panel=1.2, dirt=0.45)
    dark = node_mat("dark", (0.10, 0.10, 0.11), metal=0.7, panel=0.8, dirt=0.3)
    rust = node_mat("rust", (0.30, 0.16, 0.08), panel=1.0, dirt=0.5)
    glass = node_mat("glass", (0.05, 0.10, 0.16), glass=True)
    glow = node_mat("glow", None, emit=(1.0, 0.45, 0.12))
    b = Builder("body")
    b.box(-2.2, 2.2, -4.0, 4.0, -1.2, 1.6, dark)                                            # cage frame
    b.box(-1.4, 1.4, -2.6, 2.6, 1.6, 2.6, silver)
    b.rings(oct_secs([(6.0, 1.2, -0.8, 0.8, 0.4), (4.0, 1.6, -1.0, 1.1, 0.5), (1.0, 1.6, -1.0, 1.1, 0.5)]), [glass, silver])
    greebles(b, [(-2.0, 2.0, -4.0, 4.0, 1.6)], [dark, rust, silver], 12, 3, hmax=0.4)
    b.finish()
    s = Builder("side")
    s.rings([circle_y(10.0, 3.6, 0.0, 1.3, 12), circle_y(9.0, 3.6, 0.0, 1.8, 12), circle_y(-9.0, 3.6, 0.0, 1.8, 12),
             circle_y(-10.5, 3.6, 0.0, 1.5, 12)], [dark, silver, rust])                    # twin drive tubes
    for y in (10.05, -10.55):
        s.rings([circle_y(y, 3.1, 0.3, 0.55, 8), circle_y(y + (0.02 if y > 0 else -0.02), 3.1, 0.3, 0.1, 8)], [dark])
    s.box(2.6, 4.6, -11.6, -10.4, 0.6, 1.2, glow)
    greebles(s, [(2.4, 4.8, -8.0, 8.0, 1.6)], [dark, rust], 10, 5, hmax=0.4)
    s.finish(mirror=True)
    return dict(length=23.0, px=40)

def wasp():
    m = faction_mats("fsa")
    b = Builder("body")
    b.rings(oct_secs([(8.0, 0.5, -0.3, 0.3, 0.2), (5.5, 1.1, -0.6, 0.8, 0.4), (-3.5, 1.5, -0.7, 1.0, 0.5),
                      (-6.5, 1.0, -0.5, 0.7, 0.3)]), [m["light"], m["main"], m["main"]])
    b.rings(oct_secs([(5.0, 0.6, 0.5, 0.9, 0.25), (3.0, 0.9, 0.8, 1.4, 0.35), (1.0, 0.8, 0.8, 1.3, 0.3)]), [m["glass"]])
    b.finish()
    s = Builder("side")
    s.prism([(1.0, 3.0), (6.5, -4.0), (6.5, -5.5), (1.0, -4.5)], -0.25, 0.3, m["light"])
    s.prism([(2.2, 0.0), (5.0, -3.2), (5.0, -4.0), (2.2, -1.8)], 0.3, 0.38, m["red"])
    nacelle(s, 6.6, 0.1, -1.5, -7.5, 0.95, m["main"], m["glow"], m["dark"])
    s.finish(mirror=True)
    return dict(length=16.5, px=28)

def hammer():
    m = faction_mats("fsa")
    b = Builder("body")
    b.rings(oct_secs([(10.0, 1.6, -0.8, 0.6, 0.6), (7.0, 3.4, -1.4, 1.6, 1.0), (-6.0, 4.0, -1.6, 2.0, 1.2),
                      (-9.0, 3.0, -1.2, 1.4, 0.9)]), [m["olive"], m["olive"], m["dark"]])
    b.rings(oct_secs([(7.4, 1.2, 0.9, 1.5, 0.4), (5.0, 1.6, 1.4, 2.4, 0.5), (3.0, 1.5, 1.5, 2.3, 0.5)]), [m["glass"]])
    greebles(b, [(-3.0, 3.0, -8.0, 2.5, 1.9)], [m["olive"], m["dark"], m["main"]], 18, 9, hmax=0.4)
    b.finish()
    s = Builder("side")
    s.prism([(3.5, 4.0), (8.0, 1.0), (8.5, -6.0), (3.5, -7.0)], -0.6, 0.6, m["olive"])
    for x, y0, r in ((5.0, 13.0, 0.35), (7.4, 11.0, 0.45)):                                # long cannons
        s.rings([circle_y(y0, x, -0.2, r, 8), circle_y(y0 - 9.0, x, -0.2, r, 8)], [m["dark"]])
        s.box(x - 0.8, x + 0.8, y0 - 12.0, y0 - 8.0, -0.9, 0.5, m["olive"])
    nacelle(s, 3.2, 2.2, -2.0, -11.0, 1.4, m["olive"], m["glow"], m["dark"])
    s.finish(mirror=True)
    return dict(length=25.0, px=40)

def renegade():
    m = faction_mats("fsa")
    b = Builder("body")
    b.rings(oct_secs([(54.0, 6.0, -4.0, 2.0, 2.0), (44.0, 13.0, -6.0, 5.0, 3.5), (-36.0, 15.0, -7.0, 6.0, 4.0),
                      (-50.0, 12.0, -6.0, 4.0, 3.0)]), [m["light"], m["light"], m["main"]])
    b.box(-13.0, 13.0, 20.0, 26.0, 5.5, 6.4, m["red"])                                     # red chevron band
    b.box(-6.0, 6.0, -10.0, 12.0, 6.0, 10.0, m["main"])
    b.box(-4.0, 4.0, 30.0, 40.0, 4.0, 6.5, m["glass"])
    for y in (16.0, -20.0):
        b.box(-2.0, 2.0, y, y + 4.0, 10.0, 12.0, m["dark"]); b.box(-0.6, 0.6, y + 4.0, y + 12.0, 10.6, 11.4, m["dark"])
    greebles(b, [(-14.0, 14.0, -46.0, 40.0, 6.0)], [m["main"], m["dark"], m["light"]], 90, 17, hmax=1.4)
    b.finish(bevel=0.3)
    s = Builder("side")
    s.prism([(14.0, 10.0), (30.0, -8.0), (30.0, -26.0), (14.0, -30.0)], -2.0, 2.0, m["light"])
    s.prism([(16.0, 4.0), (26.0, -6.0), (26.0, -10.0), (16.0, -2.0)], 2.0, 2.4, m["red"])
    nacelle(s, 30.0, 2.0, -6.0, -32.0, 4.5, m["main"], node_mat("bglow", None, emit=(0.25, 0.5, 1.0)), m["dark"])
    nacelle(s, 9.0, 0.0, -40.0, -56.0, 4.0, m["main"], m["glow"])
    s.finish(mirror=True, bevel=0.3)
    return dict(length=112.0, px=96, frame=128)

def defiant():
    m = faction_mats("fsa")
    b = Builder("body")
    b.prism([(0.0, 104.0), (18.0, 70.0), (22.0, -96.0), (-22.0, -96.0), (-18.0, 70.0)], -10.0, 8.0, m["dark"])
    b.prism([(0.0, 96.0), (14.0, 66.0), (16.0, -20.0), (-16.0, -20.0), (-14.0, 66.0)], 8.0, 9.0, m["red"])
    for i in range(5):
        y = -10.0 - i * 15.0
        b.box(-14.0, 14.0, y - 11.0, y, 8.0, 10.0, m["main"])                                # hangar doors
        b.box(-12.0, 12.0, y - 2.0, y - 1.0, 10.0, 10.4, m["green"])
    b.box(-6.0, 6.0, -90.0, -78.0, 9.0, 20.0, m["main"]); b.box(-1.0, 1.0, -88.0, -60.0, 16.0, 18.0, m["dark"])
    greebles(b, [(-14.0, 14.0, 0.0, 60.0, 9.0)], [m["dark"], m["main"], m["red"]], 60, 23, hmax=2.0)
    b.finish(bevel=0.4)
    s = Builder("side")
    s.box(22.0, 26.0, -80.0, 40.0, -6.0, 4.0, m["main"])
    nacelle(s, 12.0, 0.0, -90.0, -104.0, 5.0, m["dark"], m["glow"])
    s.finish(mirror=True, bevel=0.4)
    return dict(length=210.0, px=124, frame=128)

def reprisal():
    m = faction_mats("fsa")
    b = Builder("body")
    b.prism([(0.0, 182.0), (26.0, 120.0), (34.0, -150.0), (-34.0, -150.0), (-26.0, 120.0)], -16.0, 8.0, m["main"])
    b.prism([(0.0, 170.0), (20.0, 116.0), (26.0, -40.0), (-26.0, -40.0), (-20.0, 116.0)], 8.0, 16.0, m["light"])
    for i, y in enumerate(range(-30, 150, 30)):
        b.box(-22.0 + i, 22.0 - i, y, y + 12.0, 16.0, 19.0, m["red"] if i % 2 else m["dark"])
    b.box(-14.0, 14.0, -140.0, -60.0, 8.0, 28.0, m["dark"]); b.box(-10.0, 10.0, -120.0, -110.0, 28.0, 32.0, m["red"])
    greebles(b, [(-28.0, 28.0, -140.0, 120.0, 16.0)], [m["dark"], m["main"], m["red"], m["light"]], 170, 29, hmax=4.0)
    b.finish(bevel=0.6)
    s = Builder("side")
    s.prism([(30.0, 120.0), (62.0, 30.0), (66.0, -130.0), (34.0, -150.0)], -10.0, 6.0, m["main"])  # side gun decks
    s.prism([(40.0, 60.0), (58.0, 10.0), (58.0, -20.0), (40.0, 20.0)], 6.0, 7.0, m["red"])
    for y in (40.0, -10.0, -60.0, -110.0):
        s.box(44.0, 54.0, y - 7.0, y + 7.0, 6.0, 13.0, m["dark"]); s.box(48.0, 50.0, y + 7.0, y + 30.0, 9.0, 11.0, m["dark"])
    for x in (12.0, 26.0):
        nacelle(s, x, 0.0, -140.0, -184.0, 8.0, m["dark"], m["glow"])
    s.finish(mirror=True, bevel=0.6)
    return dict(length=370.0, px=186, frame=256)

def liberty():
    m = faction_mats("fsa")
    b = Builder("body")
    b.rings(oct_secs([(300.0, 10.0, -10.0, 6.0, 6.0), (260.0, 50.0, -24.0, 16.0, 14.0), (-260.0, 70.0, -30.0, 24.0, 18.0),
                      (-290.0, 60.0, -26.0, 20.0, 16.0)]), [m["main"], m["olive"], m["dark"]])
    for i, y in enumerate(range(-240, 240, 40)):
        b.box(-46.0, 46.0, y, y + 30.0, 22.0, 30.0, [m["main"], m["olive"], m["light"]][i % 3])
        b.box(-6.0, 6.0, y + 10.0, y + 20.0, 30.0, 33.0, m["red"])
    b.box(-24.0, 24.0, -200.0, -130.0, 30.0, 56.0, m["main"])
    greebles(b, [(-60.0, 60.0, -260.0, 240.0, 30.0)], [m["dark"], m["main"], m["olive"], m["light"]], 280, 37, hmax=8.0)
    b.finish(bevel=1.0)
    s = Builder("side")
    s.box(70.0, 84.0, -200.0, 120.0, -14.0, 12.0, m["olive"])
    for x in (18.0, 46.0):
        nacelle(s, x, 0.0, -270.0, -310.0, 14.0, m["dark"], m["glow"])
    s.finish(mirror=True, bevel=1.0)
    return dict(length=610.0, px=246, frame=256)

# ============================================================ Veyr

def vhorn(b, path, widths, z, thick, m):
    """Veyr blades: broad, curved, flattened."""
    horn(b, path, [w * 2.6 for w in widths], z, thick * 1.1, m, n=8)

def veyr_body(b, m, length, width, height, glow_spots, seed):
    L, W, H = length, width * 1.4, height
    b.rings(oct_secs([(L * 0.50, W * 0.10, -H * 0.10, H * 0.10, W * 0.07), (L * 0.36, W * 0.26, -H * 0.30, H * 0.35, W * 0.18),
                      (L * 0.05, W * 0.34, -H * 0.40, H * 0.50, W * 0.24), (-L * 0.30, W * 0.30, -H * 0.36, H * 0.40, W * 0.20),
                      (-L * 0.50, W * 0.08, -H * 0.12, H * 0.10, W * 0.06)]), [m["shell"], m["dark"], m["shell"], m["bone"]])
    rnd = random.Random(seed)
    for _ in range(glow_spots):
        y = rnd.uniform(-L * 0.3, L * 0.3); x = rnd.uniform(-W * 0.18, W * 0.18); r = W * rnd.uniform(0.012, 0.026)
        b.box(x - r, x + r, y - r * 2, y + r * 2, H * 0.45, H * 0.52, m["glow"])

def skitter():
    m = faction_mats("veyr")
    b = Builder("body"); veyr_body(b, m, 12.0, 6.0, 3.0, 4, 1)
    b.box(-0.5, 0.5, 4.6, 5.4, 0.4, 0.7, m["red"])
    b.finish(bevel=0.05)
    s = Builder("side")
    vhorn(s, [(1.5, 3.0), (6.0, 7.0), (10.5, 6.0), (11.0, 2.0)], [0.9, 0.8, 0.5, 0.15], 0.2, 0.6, m["shell"])
    vhorn(s, [(1.5, -1.0), (7.0, 1.0), (11.5, -3.0), (10.5, -7.0)], [0.9, 0.8, 0.5, 0.15], 0.0, 0.6, m["bone"])
    vhorn(s, [(1.0, -4.0), (4.5, -8.0), (5.0, -11.0)], [0.6, 0.4, 0.1], 0.0, 0.4, m["dark"])
    s.finish(mirror=True, bevel=0.04)
    return dict(length=22.0, px=34)

def mantis():
    m = faction_mats("veyr")
    b = Builder("body"); veyr_body(b, m, 22.0, 6.0, 3.0, 5, 2)
    b.box(-0.5, 0.5, 9.0, 10.0, 0.6, 0.9, m["glow"])
    b.finish(bevel=0.05)
    s = Builder("side")
    vhorn(s, [(1.5, 5.0), (7.0, 3.0), (12.0, -2.0), (13.0, -9.0)], [1.4, 1.2, 0.8, 0.15], 0.3, 0.7, m["shell"])   # blade wings
    vhorn(s, [(1.0, 8.0), (3.0, 12.0), (2.0, 15.0)], [0.6, 0.4, 0.1], 0.0, 0.4, m["bone"])                         # front claws
    vhorn(s, [(1.5, -6.0), (5.0, -10.0), (6.0, -14.0)], [0.6, 0.4, 0.1], 0.0, 0.4, m["dark"])
    s.box(5.0, 6.0, 0.5, 2.0, 0.9, 1.1, m["glow"])
    s.finish(mirror=True, bevel=0.04)
    return dict(length=30.0, px=40)

def reaver():
    m = faction_mats("veyr")
    b = Builder("body"); veyr_body(b, m, 34.0, 12.0, 6.0, 8, 3)
    vhorn(b, [(0.0, 10.0), (0.0, -2.0), (0.0, -14.0)], [1.0, 1.6, 0.6], 3.2, 1.0, m["bone"])   # dorsal ridge
    b.finish(bevel=0.08)
    s = Builder("side")
    vhorn(s, [(4.0, 8.0), (12.0, 10.0), (18.0, 4.0), (19.0, -6.0)], [2.0, 1.6, 1.0, 0.2], 0.4, 1.2, m["shell"])
    vhorn(s, [(4.0, -4.0), (11.0, -6.0), (14.0, -14.0), (12.0, -20.0)], [1.8, 1.4, 0.9, 0.2], 0.0, 1.0, m["bone"])
    vhorn(s, [(2.0, 14.0), (5.0, 20.0), (3.5, 24.0)], [1.0, 0.7, 0.15], 0.0, 0.7, m["dark"])
    for y in (4.0, -4.0):
        s.box(5.0, 6.6, y - 0.8, y + 0.8, 1.6, 2.0, m["glow"])
    s.finish(mirror=True, bevel=0.06)
    return dict(length=46.0, px=54)

def broodguard():
    m = faction_mats("veyr")
    b = Builder("body"); veyr_body(b, m, 300.0, 90.0, 50.0, 22, 4)
    for y in (60.0, 0.0, -60.0):
        vhorn(b, [(-30.0, y - 10.0), (0.0, y + 8.0), (30.0, y - 10.0)], [6.0, 8.0, 6.0], 26.0, 6.0, m["bone"])   # arches
    b.finish(bevel=0.5)
    s = Builder("side")
    for y in (40.0, -20.0, -80.0):                                                           # tethered brood
        vhorn(s, [(30.0, y + 10.0), (44.0, y), (48.0, y - 14.0)], [6.0, 5.0, 1.5], 0.0, 4.0, m["shell"])
        s.box(38.0, 44.0, y - 4.0, y + 4.0, 3.0, 5.0, m["glow"])
    vhorn(s, [(20.0, 110.0), (40.0, 140.0), (30.0, 168.0)], [7.0, 5.0, 1.0], 0.0, 5.0, m["dark"])
    vhorn(s, [(24.0, -110.0), (46.0, -130.0), (52.0, -160.0)], [7.0, 5.0, 1.0], 0.0, 5.0, m["shell"])
    s.finish(mirror=True, bevel=0.5)
    return dict(length=340.0, px=124, frame=128)

def leviathan():
    m = faction_mats("veyr")
    b = Builder("body"); veyr_body(b, m, 880.0, 260.0, 160.0, 40, 5)
    for y in (220.0, 80.0, -60.0, -200.0):
        vhorn(b, [(-90.0, y - 30.0), (0.0, y + 30.0), (90.0, y - 30.0)], [16.0, 22.0, 16.0], 80.0, 16.0, m["bone"])
    b.finish(bevel=1.0)
    s = Builder("side")
    vhorn(s, [(60.0, 300.0), (130.0, 340.0), (150.0, 440.0)], [24.0, 16.0, 3.0], 0.0, 14.0, m["shell"])   # mandibles
    vhorn(s, [(100.0, 140.0), (190.0, 60.0), (200.0, -120.0), (150.0, -300.0)], [30.0, 26.0, 18.0, 3.0], 10.0, 18.0, m["shell"])
    vhorn(s, [(80.0, -200.0), (150.0, -330.0), (130.0, -440.0)], [20.0, 14.0, 3.0], 0.0, 12.0, m["dark"])
    for y in (100.0, 0.0, -100.0):
        s.box(150.0, 168.0, y - 12.0, y + 12.0, 26.0, 34.0, m["glow"])
    s.finish(mirror=True, bevel=1.0)
    return dict(length=900.0, px=250, frame=256)

SHIPS.update(ghost=ghost, wasp=wasp, hammer=hammer, renegade=renegade, defiant=defiant, reprisal=reprisal,
             liberty=liberty, skitter=skitter, mantis=mantis, reaver=reaver, broodguard=broodguard, leviathan=leviathan)

if __name__ == "__main__":
    main()
