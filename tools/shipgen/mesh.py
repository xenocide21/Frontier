"""Low-poly 3D ships for the shipyard preview (Milestone 14d).

Builds each ship from the same Blender models as the sprites (ships3.py), drops
greebles and small loose parts, decimates to a triangle budget, and writes
src/meshes_data.c: per ship, s8 vertices (x right, y forward, z up, scaled so
the ship spans about +-120) and faces of 3 vertex indices + flat RGB + flags.

    python3 mesh.py OUTDIR/meshes_data.c name:size ...   (same spec as convert.py)
"""
import sys, math
from ships3 import *
import bmesh

def tri_budget(size):
    return {64: 260, 128: 520, 256: 700}.get(size, 400)

def srgb(c):
    return max(0, min(255, int(255 * (max(0.0, c) ** (1 / 1.9)))))

def build(name, size):
    reset()
    SHIPS[name]()
    dg = bpy.context.evaluated_depsgraph_get()
    bm = bmesh.new()
    colours = []
    for ob in list(bpy.context.scene.objects):
        if ob.type != 'MESH':
            continue
        for md in list(ob.modifiers):
            if md.type == 'BEVEL':
                ob.modifiers.remove(md)
    dg = bpy.context.evaluated_depsgraph_get()
    for ob in list(bpy.context.scene.objects):
        if ob.type != 'MESH':
            continue
        ev = ob.evaluated_get(dg)
        me = ev.to_mesh()
        base = len(colours)
        for m in me.materials:
            colours.append((tuple(m.get("flat", (0.5, 0.5, 0.5))), int(m.get("emit", 0))) if m else ((0.5, 0.5, 0.5), 0))
        if not me.materials:
            colours.append(((0.5, 0.5, 0.5), 0))
        tmp = bmesh.new(); tmp.from_mesh(me)
        bmesh.ops.transform(tmp, matrix=ob.matrix_world, verts=tmp.verts)
        for f in tmp.faces:
            f.material_index = base + f.material_index
        me2 = bpy.data.meshes.new("tmp"); tmp.to_mesh(me2); tmp.free()
        bm.from_mesh(me2)
        ev.to_mesh_clear()
    # Islands (loose parts) and their sizes; drop the small ones (greebles, patch plates),
    # raising the bar until the ship fits the triangle budget.
    bm.verts.ensure_lookup_table(); bm.faces.ensure_lookup_table()
    xs = [v.co.x for v in bm.verts]; ys = [v.co.y for v in bm.verts]
    length = max(max(ys) - min(ys), max(xs) - min(xs))
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=length * 0.002)
    bm.faces.ensure_lookup_table()
    seen = set(); islands = []
    for f in bm.faces:
        if f.index in seen:
            continue
        isl = []; stack = [f]; seen.add(f.index)
        while stack:
            g = stack.pop(); isl.append(g)
            for e in g.edges:
                for h in e.link_faces:
                    if h.index not in seen:
                        seen.add(h.index); stack.append(h)
        co = [v.co for g in isl for v in g.verts]
        d = (max(c.x for c in co) - min(c.x for c in co)) + (max(c.y for c in co) - min(c.y for c in co)) + \
            (max(c.z for c in co) - min(c.z for c in co))
        tris = sum(len(g.verts) - 2 for g in isl)
        islands.append((d, tris, isl))
    budget = tri_budget(size)
    frac = 0.035
    while True:
        keep = sum(t for d, t, _ in islands if d >= length * frac)
        if keep <= budget * 1.15 or frac > 0.5:
            break
        frac *= 1.25
    drop = [g for d, t, isl in islands if d < length * frac for g in isl]
    bmesh.ops.delete(bm, geom=list(set(drop)), context='FACES')
    bmesh.ops.dissolve_limit(bm, angle_limit=0.02, verts=bm.verts, edges=bm.edges)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    me = bpy.data.meshes.new("joined"); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new("joined", me); bpy.context.scene.collection.objects.link(ob)
    for attempt in range(6):          # last resort: collapse what's left
        if len(me.polygons) <= budget:
            break
        md = ob.modifiers.new("dec", 'DECIMATE'); md.ratio = budget / len(me.polygons) * (0.95 ** attempt)
        md.use_collapse_triangulate = True
        dg = bpy.context.evaluated_depsgraph_get()
        newme = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
        ob.modifiers.clear(); ob.data = newme; me = newme
    bm = bmesh.new(); bm.from_mesh(me); bmesh.ops.triangulate(bm, faces=bm.faces)
    bm.verts.ensure_lookup_table(); bm.faces.ensure_lookup_table()
    # scale: the longest half-extent to 118, centred on the bounding box
    xs = [v.co.x for v in bm.verts]; ys = [v.co.y for v in bm.verts]; zs = [v.co.z for v in bm.verts]
    cx, cy, cz = (max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2, (max(zs) + min(zs)) / 2
    half = max(max(xs) - cx, max(ys) - cy, max(zs) - cz) or 1
    k = 118 / half
    verts = [(round((v.co.x - cx) * k), round((v.co.y - cy) * k), round((v.co.z - cz) * k)) for v in bm.verts]
    faces = []
    for f in bm.faces:
        (rgb, emit) = colours[f.material_index] if f.material_index < len(colours) else ((0.5, 0.5, 0.5), 0)
        a, b, c = (v.index for v in f.verts)
        faces.append((a, b, c, srgb(rgb[0]), srgb(rgb[1]), srgb(rgb[2]), emit))
    bm.free()
    assert len(verts) <= 1000 and len(faces) <= 1000, (name, len(verts), len(faces))
    # Face normals in model space (length 127), from the rounded vertices, so the console
    # lights a face with three multiplies.
    normals = []
    for (a, b, c, *_rest) in faces:
        ax, ay, az = verts[a]; bx, by, bz = verts[b]; cx2, cy2, cz2 = verts[c]
        ux, uy, uz = bx - ax, by - ay, bz - az; vx, vy, vz = cx2 - ax, cy2 - ay, cz2 - az
        nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
        L = math.sqrt(nx * nx + ny * ny + nz * nz) or 1
        normals.append((round(nx / L * 127), round(ny / L * 127), round(nz / L * 127)))
    while len(verts) % 3: verts.append((0, 0, 0))      # the GTE transforms three at a time
    return verts, faces, normals

def station():
    """A ring station, seen from above in flight: hub, ring, spokes, pods, solar wings."""
    m = dict(hull=node_mat("st_hull", (0.34, 0.37, 0.42)), light=node_mat("st_light", (0.55, 0.57, 0.60)),
             dark=node_mat("st_dark", (0.10, 0.11, 0.14)), blue=node_mat("st_blue", (0.05, 0.12, 0.34)),
             orange=node_mat("st_or", (0.60, 0.26, 0.05)), lamp=node_mat("st_lamp", None, emit=(0.5, 0.9, 1.0)),
             red=node_mat("st_red", None, emit=(1.0, 0.2, 0.15)))
    b = Builder("body")
    def ngon(r, n, a0=0.0): return [(r * math.cos(a0 + 2 * math.pi * k / n), r * math.sin(a0 + 2 * math.pi * k / n)) for k in range(n)]
    b.prism(ngon(11, 8, math.pi / 8), -5, 6, m["light"])                 # hub
    b.prism(ngon(7, 8, math.pi / 8), 6, 9, m["hull"])
    b.prism(ngon(3, 6), 9, 11, m["lamp"])                                # the beacon
    for k in range(12):                                                  # the ring
        a0, a1 = 2 * math.pi * k / 12, 2 * math.pi * (k + 1) / 12
        pts = [(34 * math.cos(a0), 34 * math.sin(a0)), (34 * math.cos(a1), 34 * math.sin(a1)),
               (27 * math.cos(a1), 27 * math.sin(a1)), (27 * math.cos(a0), 27 * math.sin(a0))]
        b.prism(pts, -3, 3, m["hull"] if k % 3 else m["light"])
    for k in range(4):                                                   # spokes and docking pods
        a = math.pi / 4 + math.pi / 2 * k; c, s_ = math.cos(a), math.sin(a)
        def P(r, t): return (r * c - t * s_, r * s_ + t * c)
        b.prism([P(10, -2), P(28, -2), P(28, 2), P(10, 2)], -1.5, 1.5, m["dark"])
        b.prism([P(33, -6), P(41, -6), P(41, 6), P(33, 6)], -4, 4, m["orange"] if k % 2 == 0 else m["light"])
        b.prism([P(41, -2), P(43, -2), P(43, 2), P(41, 2)], -1, 1, m["red"] if k % 2 else m["lamp"])
    for sgn in (1, -1):                                                  # solar wings, east and west
        b.prism([(sgn * 34, -1.5), (sgn * 46, -1.5), (sgn * 46, 1.5), (sgn * 34, 1.5)], -1, 1, m["dark"])
        for j in (-1, 1):
            b.prism([(sgn * 46, j * 3), (sgn * 62, j * 3), (sgn * 62, j * 17), (sgn * 46, j * 17)], -0.5, 0.5, m["blue"])
    b.finish(bevel=0)
    return dict(length=124.0)

SHIPS["station"] = station

def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    out = args[0]; specs = [a.split(":") for a in args[1:]]
    body = []; table = []
    total = 0
    for i, (name, size) in enumerate(specs):
        v, f, n = build(name, int(size))
        total += len(v) * 8 + len(f) * 14
        body.append("static const s16 v_%s[] __attribute__((aligned(4))) = {%s};" % (name, ",".join("%d,%d,%d,0" % t for t in v)))
        body.append("static const u16 f_%s[] = {%s};" % (name, ",".join("%d,%d,%d" % t[:3] for t in f)))
        body.append("static const u8 c_%s[] = {%s};" % (name, ",".join("%d,%d,%d,%d" % t[3:] for t in f)))
        body.append("static const s8 n_%s[] = {%s};" % (name, ",".join("%d,%d,%d,0" % t for t in n)))
        table.append("    { %d, %d, v_%s, f_%s, c_%s, n_%s },   /* %d %s */" % (len(v), len(f), name, name, name, name, i, name))
        print("%-12s %3d verts %3d tris" % (name, len(v), len(f)))
    with open(out, "w") as fh:
        fh.write("/* Generated by tools/shipgen/mesh.py: low-poly ships for the shipyard preview. */\n")
        fh.write('#include "model.h"\n\n')
        fh.write("\n".join(body) + "\n\n")
        fh.write("const Mesh ship_meshes[%d] = {\n%s\n};\n" % (len(table), "\n".join(table)))
    print("wrote", out, total, "bytes of data")

if __name__ == "__main__":
    main()
