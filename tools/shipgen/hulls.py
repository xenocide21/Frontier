"""
Hull grids and default fits for the ship editor.

    python3 hulls.py RENDER_DIR SRC_DIR name:size [...]

For each ship the sprite's outline (<name>_sprite.png, written by convert.py)
is cut into an N x N grid (16 for 64 px sprites, 24 for 128, 32 for 256). A
square belongs to the hull if most of it is ship. Squares are then zoned:
  ENGINE  the rear squares of each column (engines only)
  EDGE    squares on the outline (hardpoints: weapons may go here)
  IN      the rest (internal)
  BRIDGE  one square near the front middle (the bridge, fixed)
Then a default fit is placed for the ship's class, the same rules as the game
uses (src/fit.c). Writes src/hulls_data.c.
"""
import sys, os
from PIL import Image

Z_OUT, Z_IN, Z_EDGE, Z_ENGINE, Z_BRIDGE = 0, 1, 2, 3, 4
# Must match the MT_* order and numbers in src/fit.h / fit.c: name, w, h, kind, power, mass
MODS = [("NONE", 0, 0, "", 0, 0), ("COCKPIT", 1, 1, "bridge", -1, 1), ("BRIDGE", 1, 1, "bridge", -2, 4),
        ("LIFE SUPPORT", 1, 1, "in", -1, 1),
        ("LASER", 1, 1, "wpn", 0, 2), ("CANNON", 1, 2, "wpn", 0, 4), ("RAILGUN", 2, 5, "wpn", 0, 50),
        ("LASER TURRET", 1, 1, "wpn", -1, 2), ("GUN TURRET", 2, 2, "wpn", -1, 8), ("HEAVY TURRET", 3, 3, "wpn", -3, 18),
        ("MISSILE RACK", 1, 2, "wpn", 0, 2),
        ("ENGINE", 1, 1, "eng", -2, 4), ("BIG ENGINE", 2, 2, "eng", -7, 16), ("HUGE ENGINE", 3, 3, "eng", -15, 36),
        ("WARP DRIVE S", 1, 1, "in", -1, 4), ("WARP DRIVE M", 2, 2, "in", -4, 16), ("WARP DRIVE L", 3, 3, "in", -8, 36),
        ("REACTOR", 1, 1, "in", 10, 4), ("BIG REACTOR", 2, 2, "in", 45, 16), ("HUGE REACTOR", 3, 3, "in", 110, 36),
        ("CAPACITOR", 1, 1, "in", 0, 1), ("SHIELD GEN", 1, 1, "in", -3, 4), ("BIG SHIELD GEN", 2, 2, "in", -12, 16),
        ("ARMOR", 1, 1, "in", 0, 6), ("AMMO STORE", 1, 1, "in", 0, 1), ("CARGO BAY", 1, 1, "in", 0, 1),
        ("FUEL CELL", 1, 1, "in", 0, 2)]
MT = {n.replace(" ", "_"): i for i, (n, *_ ) in enumerate(MODS)}
GRID = {64: 16, 128: 24, 256: 32}
SIZE_CLASS = {64: 1, 128: 2, 256: 3}        # small, medium, large: the warp drive a hull needs

# class -> default fit wishes: lasers, cannons, laser turrets, gun turrets, heavy turrets, racks, engines,
#          shields, armor, cargo, fuel cells, life support
CLASS_OF = dict(wayfarer="transport", raider="fighter", hauler="freighter", frontier_runner="transport",
                wayfinder="scout", rockbreaker="freighter", reclaimer="freighter", watchman="scout", talon="fighter",
                vanguard="heavy", sentinel="corvette", bastion="frigate", resolute="destroyer", sovereign="cruiser",
                ghost="scout", wasp="fighter", hammer="heavy", renegade="corvette", defiant="frigate",
                reprisal="destroyer", liberty="cruiser", skitter="fighter", mantis="fighter", reaver="heavy",
                broodguard="frigate", leviathan="cruiser",
                scrapper="fighter", jackal="heavy", chopshop="ors_corvette", marauder="ors_frigate")
WANT = dict(fighter=(2, 0, 0, 0, 0, 1, 2, 1, 1, 0, 1, 0), scout=(2, 0, 0, 0, 0, 1, 2, 1, 1, 1, 2, 0),
            transport=(1, 0, 1, 0, 0, 1, 2, 1, 3, 3, 2, 0), heavy=(2, 2, 0, 0, 0, 1, 3, 1, 3, 0, 1, 0),
            freighter=(1, 0, 2, 0, 0, 0, 3, 1, 4, 8, 3, 0), corvette=(2, 2, 2, 1, 0, 1, 4, 2, 8, 3, 2, 1),
            frigate=(3, 2, 4, 2, 0, 1, 6, 3, 14, 4, 3, 2), destroyer=(4, 3, 6, 2, 1, 2, 8, 4, 22, 4, 4, 2),
            cruiser=(6, 4, 8, 3, 2, 2, 10, 6, 30, 6, 5, 3),
            # Syndicate scrap-builds: more guns and plating, fewer shields
            ors_corvette=(1, 3, 1, 1, 1, 1, 4, 1, 10, 3, 2, 1), ors_frigate=(2, 3, 3, 2, 2, 1, 6, 2, 16, 4, 3, 2))

def zones_for(path, n):
    im = Image.open(path).convert("RGBA"); S = im.size[0]; a = im.split()[3].load()
    occ = [[False] * n for _ in range(n)]
    for gy in range(n):
        for gx in range(n):
            x0, x1 = gx * S // n, (gx + 1) * S // n; y0, y1 = gy * S // n, (gy + 1) * S // n
            tot = (x1 - x0) * (y1 - y0); on = sum(1 for y in range(y0, y1) for x in range(x0, x1) if a[x, y] > 0)
            occ[gy][gx] = on * 100 >= tot * 35
    z = [[Z_IN if occ[y][x] else Z_OUT for x in range(n)] for y in range(n)]
    rows = [y for y in range(n) if any(occ[y])]
    top, bot = rows[0], rows[-1]
    # Rear engine bay: small hulls get the last 1-2 squares of each column, medium
    # up to 4, large up to 7 (engines behind engines still count as rear engines).
    depth = {16: 2, 24: 4, 32: 7}[n]
    frac = {16: 5, 24: 4, 32: 3}[n]                      # rear 1/5, 1/4, 1/3 of the hull
    rear_lim = bot - max(1, (bot - top) // frac)
    for x in range(n):
        col = [y for y in range(n) if occ[y][x]]
        if not col or col[-1] < rear_lim: continue
        keep = 2 if len(col) > 3 else 1
        for y in col[::-1][:max(keep, min(depth, len(col) - 3))]:
            if y >= rear_lim: z[y][x] = Z_ENGINE
    for y in range(n):
        for x in range(n):
            if z[y][x] != Z_IN: continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                X, Y = x + dx, y + dy
                if not (0 <= X < n and 0 <= Y < n) or not occ[Y][X]: z[y][x] = Z_EDGE; break
    # bridge: the occupied square nearest the middle column, a little behind the nose
    c = (n - 1) / 2; best = None
    for y in rows[: max(2, len(rows) // 3) + 1]:
        for x in range(n):
            if z[y][x] in (Z_IN, Z_EDGE):
                k = abs(x - c) * 2 + abs(y - (top + 1))
                if best is None or k < best[0]: best = (k, x, y)
    z[best[2]][best[1]] = Z_BRIDGE
    return z

def cells(m, x, y, rot):
    w, h = MODS[m][1], MODS[m][2]
    if rot & 1: w, h = h, w
    return [(x + i, y + j) for j in range(h) for i in range(w)]

def fits(z, used, m, x, y, rot):
    n = len(z); kind = MODS[m][3]; cs = cells(m, x, y, rot); edge = False
    for X, Y in cs:
        if not (0 <= X < n and 0 <= Y < n) or used[Y][X]: return False
        zz = z[Y][X]
        if kind == "bridge":
            if zz != Z_BRIDGE: return False
        elif kind == "eng":
            if zz != Z_ENGINE: return False
        elif zz == Z_ENGINE:
            if not (MODS[m][0].startswith("WARP") or "REACTOR" in MODS[m][0] or MODS[m][0] == "ARMOR"): return False
        elif zz not in (Z_IN, Z_EDGE): return False
        if zz == Z_EDGE: edge = True
    # lasers and cannons need an edge square; turrets, missile racks and railguns
    # can sit on top of or under the hull
    return edge if MODS[m][0] in ("LASER", "CANNON") else True

def put(z, used, lay, m, x, y, rot=0, arc=0):
    if not fits(z, used, m, x, y, rot): return False
    for X, Y in cells(m, x, y, rot): used[Y][X] = True
    lay.append((m, x, y, rot, arc)); return True

def power_of(lay):
    return sum(MODS[m][4] for m, *_ in lay)

def default_fit(z, cls, size_class):
    n = len(z); used = [[False] * n for _ in range(n)]; lay = []
    las, can, ltur, gtur, htur, rack, eng, shd, arm, cargo, cells, life = WANT[cls]
    c = (n - 1) / 2
    for y in range(n):
        for x in range(n):
            if z[y][x] == Z_BRIDGE:
                put(z, used, lay, MT["COCKPIT"] if size_class == 1 else MT["BRIDGE"], x, y)
    edge_front = sorted(((y, abs(x - c), x) for y in range(n) for x in range(n) if z[y][x] == Z_EDGE))
    edge_side = sorted(((-abs(x - c), y, x) for y in range(n) for x in range(n) if z[y][x] == Z_EDGE))
    inner = [(y, x) for y in range(n) for x in sorted(range(n), key=lambda x: abs(x - c))]
    middle = sorted(inner, key=lambda t: abs(t[0] - n / 2) + abs(t[1] - c))   # deep inside first
    def turret_arc(x):
        return -8 if x < c else 8 if x > c else 0           # turrets face outward
    def place(m, k, spots, arc_fn=None):
        done = 0
        for x, y in spots:
            if done >= k: break
            dx = 0
            # try the spot and, for wide modules, a little to the left so they still fit
            for ox in (0, -1, -2):
                if put(z, used, lay, m, x + ox, y, 0, arc_fn(x) if arc_fn else 0): done += 1; break
        return done
    front = [(x, y) for y, _, x in edge_front]
    side = [(x, y) for _, y, x in edge_side]
    deep = [(x, y) for y, x in middle]
    insides = [(x, y) for y, x in inner]
    place(MT["LASER"], las, front)
    place(MT["CANNON"], can, front)
    place(MT["MISSILE_RACK"], rack, front)
    place(MT["HEAVY_TURRET"], htur, side, turret_arc)
    place(MT["GUN_TURRET"], gtur, side, turret_arc)
    place(MT["LASER_TURRET"], ltur, side, turret_arc)
    # engines: the biggest that fit first on big hulls, from the back
    rear = [(x, y) for y in range(n - 1, -1, -1) for x in sorted(range(n), key=lambda x: abs(x - c))]
    left = eng
    if size_class == 3: left -= 3 * place(MT["HUGE_ENGINE"], max(1, eng // 5), rear)
    if size_class >= 2: left -= 2 * place(MT["BIG_ENGINE"], max(0, left // 3), rear)
    place(MT["ENGINE"], max(1, left), rear)
    if not any(m in (MT["ENGINE"], MT["BIG_ENGINE"], MT["HUGE_ENGINE"]) for m, *_ in lay):
        for x, y in rear:                                   # no engine squares at all: make one
            if z[y][x] in (Z_IN, Z_EDGE) and not used[y][x]:
                z[y][x] = Z_ENGINE; put(z, used, lay, MT["ENGINE"], x, y); break
    warp = {1: "WARP_DRIVE_S", 2: "WARP_DRIVE_M", 3: "WARP_DRIVE_L"}[size_class]
    if not place(MT[warp], 1, deep) and size_class > 1:
        place(MT["WARP_DRIVE_M" if size_class == 3 else "WARP_DRIVE_S"], 1, deep)   # (falls back; flagged below)
    place(MT["LIFE_SUPPORT"], life, deep)
    big_sh = shd // 3
    place(MT["BIG_SHIELD_GEN"], big_sh, deep)
    place(MT["SHIELD_GEN"], shd - big_sh, deep)
    place(MT["CAPACITOR"], 1 + las // 2 + (1 if n > 16 else 0), insides)
    if can or gtur or htur: place(MT["AMMO_STORE"], 1 + can // 2 + gtur // 2 + htur, insides)
    need = 3 + las + 2 * can
    for _ in range(40):
        if power_of(lay) >= need: break
        gap = need - power_of(lay)
        if gap > 60 and size_class == 3 and place(MT["HUGE_REACTOR"], 1, deep): continue
        if gap > 20 and size_class >= 2 and place(MT["BIG_REACTOR"], 1, deep): continue
        if not place(MT["REACTOR"], 1, deep): break
    if size_class >= 2: place(MT["REACTOR"], 1, deep)       # a spare, so one hit is not fatal
    place(MT["FUEL_CELL"], cells, insides)
    got = place(MT["ARMOR"], arm, side)                     # plates on the outline first,
    place(MT["ARMOR"], arm - got, insides)                  # then inside
    place(MT["CARGO_BAY"], cargo, insides)
    return lay

def main():
    rdir, sdir, specs = sys.argv[1], sys.argv[2], sys.argv[3:]
    out = ['/* Generated by tools/shipgen/hulls.py from the sprites. Do not edit. */', '#include "fit.h"', '#include "ship.h"', '']
    table = []
    for sp in specs:
        name, size = sp.split(":"); size = int(size); n = GRID[size]
        z = zones_for(os.path.join(rdir, f"{name}_sprite.png"), n)
        lay = default_fit(z, CLASS_OF[name], SIZE_CLASS[size])
        out.append(f'static const u8 {name}_zones[{n * n}] = {{')
        for y in range(n): out.append('    ' + ', '.join(str(v) for v in z[y]) + ',')
        out.append('};')
        out.append(f'static const Placed {name}_fit[{len(lay)}] = {{')
        for m, x, y, rot, arc in lay: out.append(f'    {{ {m}, {x}, {y}, {rot}, {arc} }},   /* {MODS[m][0]} */')
        out.append('};')
        table.append(f'    {{ {n}, {name}_zones, {name}_fit, {len(lay)} }},')
        pw = power_of(lay); mass = sum(MODS[m][5] for m, *_ in lay)
        warps = [MODS[m][0] for m, *_ in lay if MODS[m][0].startswith("WARP")]
        engs = [MODS[m][0] for m, *_ in lay if MODS[m][3] == "eng"]
        print(f"{name}: grid {n}, {sum(1 for r in z for v in r if v)} squares, {len(lay)} modules, power {pw:+d}, "
              f"mass {mass}, warp {warps}, engines {len(engs)}")
        print("   " + "\n   ".join("".join(".:#EB"[v] for v in row) for row in z))
    out.append('const HullGrid hull_grids[SHIP_DEF_COUNT] = {')
    out += table
    out.append('};')
    open(os.path.join(sdir, "hulls_data.c"), "w").write("\n".join(out) + "\n")

main()
