"""
FRONTIER ship generator: builds each ship as a low-poly 3D model in Blender
(bpy), renders a top-down sprite source and a 3/4 "beauty" view.

    python3 ships.py OUTDIR [ship ...]

Coordinates: X right, Y forward (the nose), Z up, units are metres.
Each ship is modelled from its reference picture in art/reference.
"""
import math, random, sys, os
import bpy, bmesh
from mathutils import Vector

# ---------------------------------------------------------------- scene setup

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'
    sc.cycles.device = 'CPU'
    sc.cycles.samples = 48
    sc.cycles.use_denoising = False
    sc.render.film_transparent = True
    sc.view_settings.view_transform = 'Standard'
    sc.view_settings.look = 'None'
    sc.render.image_settings.file_format = 'PNG'
    sc.render.image_settings.color_mode = 'RGBA'
    w = bpy.data.worlds.new("World"); sc.world = w
    w.use_nodes = True
    bg = w.node_tree.nodes["Background"]
    bg.inputs[0].default_value = (0.05, 0.06, 0.09, 1)
    bg.inputs[1].default_value = 1.0
    return sc

def look_dir(obj, direction):
    obj.rotation_euler = Vector(direction).to_track_quat('-Z', 'Y').to_euler()

def lights():
    key = bpy.data.lights.new("key", 'SUN'); key.energy = 5.5; key.angle = math.radians(4)
    ko = bpy.data.objects.new("key", key); bpy.context.scene.collection.objects.link(ko)
    look_dir(ko, (0.8, -1.0, -1.6))           # from upper left, ahead of the ship
    fill = bpy.data.lights.new("fill", 'SUN'); fill.energy = 0.6; fill.color = (0.7, 0.8, 1.0)
    fo = bpy.data.objects.new("fill", fill); bpy.context.scene.collection.objects.link(fo)
    look_dir(fo, (-1.0, 0.6, -1.0))

# ---------------------------------------------------------------- materials

def node_mat(name, rgb, rough=0.55, metal=0.35, panel=1.6, dirt=0.45, emit=None, glass=False):
    m = bpy.data.materials.new(name); m.use_nodes = True
    m["flat"] = list(emit if emit else rgb)       # for the low-poly 3D export (mesh.py)
    m["emit"] = 1 if emit else 0
    nt = m.node_tree; N = nt.nodes; L = nt.links
    bsdf = N["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal
    if emit:
        bsdf.inputs["Emission Color"].default_value = (*emit, 1)
        bsdf.inputs["Emission Strength"].default_value = 1.0
        bsdf.inputs["Base Color"].default_value = (*emit, 1)
        return m
    if glass:
        bsdf.inputs["Base Color"].default_value = (*rgb, 1)
        bsdf.inputs["Roughness"].default_value = 0.08
        bsdf.inputs["Metallic"].default_value = 0.6
        return m
    tc = N.new("ShaderNodeTexCoord")
    # Panels: a brick pattern in object space, about `panel` metres wide.
    br = N.new("ShaderNodeTexBrick")
    br.inputs["Color1"].default_value = (*rgb, 1)
    br.inputs["Color2"].default_value = (rgb[0] * 0.78, rgb[1] * 0.77, rgb[2] * 0.75, 1)
    br.inputs["Mortar"].default_value = (rgb[0] * 0.45, rgb[1] * 0.45, rgb[2] * 0.45, 1)
    br.inputs["Mortar Size"].default_value = 0.02
    br.inputs["Bias"].default_value = 0.1
    br.inputs["Scale"].default_value = 1.0 / panel
    br.inputs["Brick Width"].default_value = 1.0
    br.inputs["Row Height"].default_value = 0.6
    br.offset = 0.5
    L.new(tc.outputs["Object"], br.inputs["Vector"])
    # Grime: fine speckle times broad blotches.
    nz = N.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 3.0; nz.inputs["Detail"].default_value = 10
    nz.inputs["Roughness"].default_value = 0.65
    L.new(tc.outputs["Object"], nz.inputs["Vector"])
    ramp = N.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.42; ramp.color_ramp.elements[0].color = (1, 1, 1, 1)
    ramp.color_ramp.elements[1].position = 0.68
    ramp.color_ramp.elements[1].color = (1 - dirt, 1 - dirt * 1.03, 1 - dirt * 1.08, 1)
    L.new(nz.outputs["Fac"], ramp.inputs["Fac"])
    nz2 = N.new("ShaderNodeTexNoise"); nz2.inputs["Scale"].default_value = 0.35; nz2.inputs["Detail"].default_value = 3
    L.new(tc.outputs["Object"], nz2.inputs["Vector"])
    ramp2 = N.new("ShaderNodeValToRGB")
    ramp2.color_ramp.elements[0].position = 0.40; ramp2.color_ramp.elements[0].color = (1, 1, 1, 1)
    ramp2.color_ramp.elements[1].position = 0.62
    ramp2.color_ramp.elements[1].color = (0.72, 0.70, 0.66, 1)
    L.new(nz2.outputs["Fac"], ramp2.inputs["Fac"])
    m1 = N.new("ShaderNodeMix"); m1.data_type = 'RGBA'; m1.blend_type = 'MULTIPLY'; m1.inputs["Factor"].default_value = 1.0
    L.new(br.outputs["Color"], m1.inputs[6]); L.new(ramp.outputs["Color"], m1.inputs[7])
    m2 = N.new("ShaderNodeMix"); m2.data_type = 'RGBA'; m2.blend_type = 'MULTIPLY'; m2.inputs["Factor"].default_value = 1.0
    L.new(m1.outputs[2], m2.inputs[6]); L.new(ramp2.outputs["Color"], m2.inputs[7])
    L.new(m2.outputs[2], bsdf.inputs["Base Color"])
    return m

# ---------------------------------------------------------------- geometry helpers

class Builder:
    def __init__(self, name):
        self.bm = bmesh.new(); self.name = name; self.mats = []
    def mat(self, m):
        if m not in self.mats: self.mats.append(m)
        return self.mats.index(m)
    def rings(self, rings, mats, cap=True):
        """rings: list of lists of (x, y, z); consecutive rings joined. mats: one per segment."""
        bm = self.bm
        vs = [[bm.verts.new(p) for p in r] for r in rings]
        n = len(rings[0])
        for i in range(len(vs) - 1):
            mi = self.mat(mats[min(i, len(mats) - 1)])
            for k in range(n):
                f = bm.faces.new((vs[i][k], vs[i][(k + 1) % n], vs[i + 1][(k + 1) % n], vs[i + 1][k]))
                f.material_index = mi
        if cap:
            f = bm.faces.new(vs[0]); f.material_index = self.mat(mats[0])
            f = bm.faces.new(list(reversed(vs[-1]))); f.material_index = self.mat(mats[-1])
    def prism(self, outline, z0, z1, m):
        ring0 = [(x, y, z0) for x, y in outline]
        ring1 = [(x, y, z1) for x, y in outline]
        self.rings([ring0, ring1], [m])
    def box(self, x0, x1, y0, y1, z0, z1, m):
        self.prism([(x0, y0), (x1, y0), (x1, y1), (x0, y1)], z0, z1, m)
    def finish(self, mirror=False, bevel=0.06):
        me = bpy.data.meshes.new(self.name)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        self.bm.to_mesh(me); self.bm.free()
        for m in self.mats: me.materials.append(m)
        ob = bpy.data.objects.new(self.name, me)
        bpy.context.scene.collection.objects.link(ob)
        if mirror:
            md = ob.modifiers.new("mirror", 'MIRROR'); md.use_axis[0] = True; md.use_clip = False
        if bevel:
            bv = ob.modifiers.new("bevel", 'BEVEL'); bv.width = bevel; bv.segments = 1
            bv.limit_method = 'ANGLE'
        for p in me.polygons: p.use_smooth = False
        return ob

def octagon(y, w, zb, zt, c, x0=0.0):
    return [(x0 - w + c, y, zt), (x0 + w - c, y, zt), (x0 + w, y, zt - c), (x0 + w, y, zb + c),
            (x0 + w - c, y, zb), (x0 - w + c, y, zb), (x0 - w, y, zb + c), (x0 - w, y, zt - c)]

def circle_y(y, x0, z0, r, n=10):
    return [(x0 + r * math.cos(2 * math.pi * k / n), y, z0 + r * math.sin(2 * math.pi * k / n)) for k in range(n)]

def greebles(b, regions, m_list, count, seed, hmax=0.35):
    rnd = random.Random(seed)
    for _ in range(count):
        x0, x1, y0, y1, z = rnd.choice(regions)
        w = rnd.uniform(0.25, 0.9); l = rnd.uniform(0.3, 1.4)
        x = rnd.uniform(x0, x1 - w); y = rnd.uniform(y0, y1 - l)
        b.box(x, x + w, y, y + l, z, z + rnd.uniform(0.08, hmax), rnd.choice(m_list))

# ---------------------------------------------------------------- ships

def wayfarer():
    """Chad Beefsteel's light transport (the cover ship / 'Frontier Runner')."""
    hull   = node_mat("hull",   (0.46, 0.43, 0.38), panel=1.5, dirt=0.6)
    hull2  = node_mat("hull2",  (0.32, 0.30, 0.27), panel=1.2, dirt=0.6)
    dark   = node_mat("dark",   (0.16, 0.16, 0.17), metal=0.7, panel=0.8, dirt=0.3)
    brass  = node_mat("brass",  (0.55, 0.45, 0.25), metal=0.6, panel=0.9, dirt=0.4)
    red    = node_mat("red",    (0.50, 0.06, 0.05), panel=1.5, dirt=0.5)
    yellow = node_mat("yellow", (0.72, 0.50, 0.08), panel=1.5, dirt=0.5)
    glass  = node_mat("glass",  (0.03, 0.08, 0.16), glass=True)
    glow   = node_mat("glow",   None, emit=(0.15, 0.4, 1.0))

    b = Builder("body")
    secs = [(14.5, 1.2, -0.5, 0.5, 0.4), (13.0, 2.1, -1.0, 0.9, 0.6), (11.2, 2.6, -1.2, 1.4, 0.7),
            (9.6, 2.8, -1.3, 1.7, 0.8), (4.2, 3.0, -1.4, 1.9, 0.8), (2.2, 3.6, -1.5, 2.0, 0.9),
            (0.4, 3.9, -1.5, 2.1, 0.9), (-1.6, 4.1, -1.6, 2.2, 0.9), (-9.5, 4.0, -1.5, 2.1, 0.9),
            (-11.8, 3.2, -1.2, 1.6, 0.7)]
    segmats = [hull2, yellow, hull, hull, hull, red, hull, hull, hull2]
    b.rings([octagon(*s) for s in secs], segmats)
    # canopy and its frame
    b.rings([octagon(12.4, 1.3, 0.5, 1.0, 0.35), octagon(10.6, 2.1, 0.8, 2.0, 0.6),
             octagon(7.4, 2.0, 1.2, 2.25, 0.55)], [glass])
    for x in (-1.05, 0.0, 1.05):
        b.box(x - 0.09, x + 0.09, 7.4, 12.0, 1.0, 2.3 - abs(x) * 0.25, dark)
    b.box(-2.1, 2.1, 9.9, 10.1, 1.0, 2.15, dark)
    # dorsal engine housing and spine
    b.rings([octagon(-0.5, 2.4, 1.6, 2.9, 0.7), octagon(-3.0, 2.8, 1.6, 3.3, 0.8),
             octagon(-10.5, 2.8, 1.6, 3.2, 0.8), octagon(-12.0, 2.2, 1.6, 2.6, 0.6)], [hull])
    b.box(-0.08, 0.08, -1.2, -0.9, 2.2, 4.4, dark)          # antenna mast
    greebles(b, [(-2.4, 2.4, -10.0, -3.5, 3.2), (-3.4, 3.4, -9.0, 2.0, 2.05), (-2.6, 2.6, 2.4, 6.5, 1.9)],
             [hull2, dark, brass, hull], 34, 7)
    b.box(-2.9, 2.9, -6.6, -6.0, 3.25, 3.4, red)             # red band over the housing
    body = b.finish()

    # right side, mirrored: wing, wingtip pod, engine nacelle, under-wing pod
    s = Builder("side")
    s.prism([(3.4, 1.6), (9.2, -0.9), (11.0, -2.0), (11.0, -6.6), (3.4, -7.4)], -0.35, 0.45, hull)
    s.rings([octagon(-0.4, 0.9, -0.9, 0.9, 0.35, 11.2), octagon(-1.5, 1.05, -1.0, 1.1, 0.4, 11.2),
             octagon(-8.0, 1.05, -1.0, 1.1, 0.4, 11.2), octagon(-8.8, 0.85, -0.8, 0.9, 0.3, 11.2)], [hull2])
    s.box(10.25, 12.15, -2.6, -1.8, 1.05, 1.2, red)          # wingtip red stripes
    s.box(10.25, 12.15, -3.2, -2.9, 1.05, 1.2, red)
    s.rings([circle_y(-3.0, 4.9, 1.2, 1.55), circle_y(-4.0, 4.9, 1.2, 1.75), circle_y(-13.2, 4.9, 1.2, 1.75),
             circle_y(-14.4, 4.9, 1.2, 1.45)], [brass, hull, hull, dark])
    s.rings([circle_y(-14.45, 4.9, 1.2, 1.15), circle_y(-14.5, 4.9, 1.2, 0.2)], [glow])
    s.box(4.1, 5.7, -15.7, -14.4, 1.4, 2.0, glow)                # exhaust glow, seen from above
    s.box(4.0, 5.8, -9.0, -8.2, 2.9, 3.05, red)
    s.rings([octagon(4.2, 0.75, -1.4, -0.2, 0.25, 6.6), octagon(3.4, 0.9, -1.5, -0.1, 0.3, 6.6),
             octagon(-1.6, 0.9, -1.5, -0.1, 0.3, 6.6)], [brass, hull2])
    greebles(s, [(4.0, 10.0, -6.2, -1.2, 0.45)], [hull2, dark], 10, 11, hmax=0.2)
    s.finish(mirror=True)
    return dict(length=29.5, px=50, beauty_dist=58)

def raider():
    """Pirate light fighter, after the FSA Wasp silhouette, in pirate colours."""
    hull  = node_mat("hull",  (0.46, 0.46, 0.48), panel=1.0, dirt=0.55)
    hull2 = node_mat("hull2", (0.32, 0.31, 0.31), panel=0.8, dirt=0.5)
    rust  = node_mat("rust",  (0.55, 0.16, 0.07), panel=1.0, dirt=0.5)
    tan   = node_mat("tan",   (0.62, 0.48, 0.28), panel=0.9, dirt=0.45)
    dark  = node_mat("dark",  (0.12, 0.12, 0.13), metal=0.7, panel=0.6, dirt=0.3)
    glass = node_mat("glass", (0.12, 0.03, 0.02), glass=True)
    glow  = node_mat("glow",  None, emit=(1.0, 0.35, 0.15))

    b = Builder("body")
    b.rings([octagon(8.6, 0.5, -0.3, 0.3, 0.2), octagon(6.5, 1.1, -0.6, 0.8, 0.4), octagon(2.0, 1.5, -0.7, 1.3, 0.5),
             octagon(-3.5, 1.6, -0.7, 1.2, 0.5), octagon(-6.0, 1.2, -0.5, 0.8, 0.4)], [hull, hull, hull2, hull])
    b.rings([octagon(5.6, 0.6, 0.6, 1.0, 0.25), octagon(3.6, 0.9, 0.8, 1.6, 0.35), octagon(1.4, 0.8, 1.0, 1.5, 0.3)], [glass])
    for x in (-0.55, 0.55):
        b.box(x - 0.13, x + 0.13, 7.5, 11.0, -0.25, 0.05, dark)    # nose gun prongs
    greebles(b, [(-1.1, 1.1, -5.0, 0.5, 1.2)], [hull2, dark, tan], 10, 3, hmax=0.2)
    b.finish()
    s = Builder("side")
    s.prism([(1.0, 6.0), (6.8, -3.4), (7.4, -6.2), (1.2, -4.6)], -0.25, 0.35, hull)
    s.prism([(2.2, 3.0), (5.2, -1.6), (5.8, -2.9), (2.4, -1.0)], 0.35, 0.42, rust)   # rust stripe
    s.prism([(1.6, -1.8), (5.0, -3.6), (5.2, -4.3), (1.7, -3.2)], 0.35, 0.42, tan)
    s.rings([circle_y(-1.6, 6.9, 0.2, 0.75), circle_y(-2.4, 6.9, 0.2, 0.95), circle_y(-7.0, 6.9, 0.2, 0.95),
             circle_y(-7.6, 6.9, 0.2, 0.8)], [tan, hull2, hull2, dark])
    s.rings([circle_y(-7.65, 6.9, 0.2, 0.6), circle_y(-7.7, 6.9, 0.2, 0.1)], [glow])
    s.finish(mirror=True)
    return dict(length=19.0, px=38, beauty_dist=36)

def hauler():
    """Industrial freighter, after the Iron Hauler."""
    orange = node_mat("orange", (0.60, 0.22, 0.06), panel=1.8, dirt=0.55)
    orng2  = node_mat("orng2",  (0.44, 0.17, 0.05), panel=1.4, dirt=0.55)
    black  = node_mat("black",  (0.07, 0.07, 0.08), metal=0.7, panel=1.0, dirt=0.25)
    grey   = node_mat("grey",   (0.42, 0.42, 0.43), metal=0.6, panel=0.9, dirt=0.4)
    stripe = node_mat("stripe", (0.85, 0.70, 0.12), panel=0.6, dirt=0.4)
    glass  = node_mat("glass",  (0.05, 0.10, 0.14), glass=True)
    glow   = node_mat("glow",   None, emit=(0.15, 0.45, 1.0))

    b = Builder("body")
    b.box(-2.0, 2.0, -15.0, 12.0, -1.4, 1.8, black)                     # spine frame
    b.rings([octagon(19.5, 4.6, -2.2, 1.6, 1.3), octagon(17.0, 7.5, -2.8, 2.8, 1.7), octagon(11.0, 8.0, -2.8, 3.0, 1.6),
             octagon(9.6, 7.0, -2.4, 2.4, 1.3)], [orange, orange, orng2])  # forward cab, full width
    b.rings([octagon(18.2, 3.0, 1.6, 2.5, 0.6), octagon(16.0, 4.2, 2.2, 3.2, 0.7), octagon(14.2, 3.6, 2.2, 3.1, 0.6)], [glass])
    b.box(-4.1, 4.1, 15.4, 15.8, 2.2, 3.3, black)
    b.box(-5.5, 5.5, 10.4, 11.2, 3.0, 3.25, stripe)
    b.rings([circle_y(8.4, 0.0, 3.2, 2.0, 12), circle_y(7.6, 0.0, 3.2, 2.4, 12), circle_y(-6.5, 0.0, 3.2, 2.4, 12),
             circle_y(-7.3, 0.0, 3.2, 2.0, 12)], [grey, black, grey])  # tank on top
    for y in (4.0, -1.5):
        b.box(-2.7, 2.7, y - 0.4, y + 0.4, 1.0, 5.8, orng2)             # tank bands
    greebles(b, [(-1.8, 1.8, -14.5, -8.0, 1.8), (-6.0, 6.0, 9.8, 12.5, 2.9)], [grey, orange, black], 16, 5, hmax=0.6)
    b.finish(bevel=0.1)
    s = Builder("side")
    for i, (y0, y1) in enumerate(((8.8, 3.2), (2.6, -3.0), (-3.6, -9.2))):
        s.box(2.1, 9.6, y1, y0, -2.2, 2.4, orange if i != 1 else orng2)  # cargo modules
        s.box(2.6, 9.0, y1 + 0.5, y1 + 1.2, 2.4, 2.6, stripe)
        s.box(9.6, 10.2, y1 + 0.4, y0 - 0.4, -1.8, 1.8, black)
        s.box(4.0, 7.5, y0 - 2.2, y0 - 0.8, 2.4, 2.9, black)            # hatch
    s.rings([octagon(-9.8, 3.4, -2.6, 2.6, 1.0, 6.6), octagon(-11.0, 3.9, -2.8, 3.1, 1.1, 6.6),
             octagon(-17.4, 3.9, -2.8, 3.1, 1.1, 6.6), octagon(-18.6, 3.4, -2.4, 2.4, 0.9, 6.6)], [orange, orange, orng2])
    s.box(3.4, 9.8, -16.4, -15.6, 3.1, 3.35, stripe)
    for x in (5.0, 8.2):
        s.rings([circle_y(-18.5, x, 0.0, 1.3), circle_y(-19.6, x, 0.0, 1.2)], [black])
        s.rings([circle_y(-19.65, x, 0.0, 1.0), circle_y(-19.7, x, 0.0, 0.15)], [glow])
        s.box(x - 0.9, x + 0.9, -20.4, -19.4, 0.8, 1.4, glow)
    greebles(s, [(2.6, 9.0, -9.0, 8.5, 2.4), (3.6, 9.6, -17.0, -11.5, 3.1)], [orng2, black, grey], 22, 9, hmax=0.5)
    s.finish(mirror=True, bevel=0.1)
    return dict(length=40.0, px=59, beauty_dist=95)

SHIPS = {"wayfarer": wayfarer, "raider": raider, "hauler": hauler}

# ---------------------------------------------------------------- rendering

def bounds():
    lo = Vector((1e9,) * 3); hi = Vector((-1e9,) * 3)
    dg = bpy.context.evaluated_depsgraph_get()
    for ob in bpy.context.scene.objects:
        if ob.type != 'MESH': continue
        ev = ob.evaluated_get(dg)
        for v in ev.to_mesh().vertices:
            p = ob.matrix_world @ v.co
            lo = Vector(map(min, lo, p)); hi = Vector(map(max, hi, p))
    return lo, hi

def render(name, info, outdir, only_top=False):
    sc = bpy.context.scene
    lo, hi = bounds()
    cx, cy = (lo.x + hi.x) / 2, (lo.y + hi.y) / 2
    length = hi.y - lo.y
    cam = bpy.data.cameras.new("cam"); co = bpy.data.objects.new("cam", cam)
    sc.collection.objects.link(co); sc.camera = co
    # top-down: 512 px frame, ship length -> info['px'] of a 64 px sprite
    frame = info.get("frame", 64)
    cam.type = 'ORTHO'; cam.ortho_scale = length * frame / info["px"]
    cam.clip_end = 5000
    co.location = (cx, cy, max(60, length)); co.rotation_euler = (0, 0, 0)
    sc.render.resolution_x = sc.render.resolution_y = max(512, frame * 4)
    sc.render.filepath = os.path.join(outdir, f"{name}_top.png")
    bpy.ops.render.render(write_still=True)
    if only_top: return
    # beauty: 3/4 view from front left, above
    cam.type = 'PERSP'; cam.lens = 50
    d = info.get("beauty_dist", length * 1.9)
    co.location = (cx - d * 0.62, cy + d * 0.55, d * 0.42)
    look_dir(co, (cx - co.location.x, cy - 0.15 * length - co.location.y, -co.location.z))
    sc.render.resolution_x, sc.render.resolution_y = 960, 540
    sc.cycles.samples = 40
    sc.render.filepath = os.path.join(outdir, f"{name}_beauty.png")
    bpy.ops.render.render(write_still=True)

def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    only_top = "--top" in args
    args = [a for a in args if a != "--top"]
    outdir = args[0]; names = args[1:] or list(SHIPS)
    os.makedirs(outdir, exist_ok=True)
    for n in names:
        reset(); lights()
        info = SHIPS[n]()
        render(n, info, outdir, only_top)
        print("rendered", n)


# ---------------------------------------------------------------- more helpers

def nacelle(b, x, z, y0, y1, r, m_body, m_glow, m_front=None, n=10):
    """An engine pod along Y from y0 (front) to y1 (back), with a glowing nozzle
    that also shows from above."""
    b.rings([circle_y(y0, x, z, r * 0.8, n), circle_y(y0 - r * 0.4, x, z, r, n),
             circle_y(y1 + r * 0.3, x, z, r, n), circle_y(y1, x, z, r * 0.85, n)],
            [m_front or m_body, m_body, m_body])
    b.rings([circle_y(y1 - 0.02, x, z, r * 0.7, n), circle_y(y1 - 0.05, x, z, r * 0.1, n)], [m_glow])
    b.box(x - r * 0.55, x + r * 0.55, y1 - r * 0.7, y1, z + r * 0.2, z + r * 0.6, m_glow)

def horn(b, path, widths, z, thick, m, n=6):
    """A tapered blade along a polyline in the XY plane (Veyr claws and ribs).
    widths: half-width at each point."""
    rings = []
    for i, (x, y) in enumerate(path):
        xa, ya = path[max(0, i - 1)]; xb, yb = path[min(len(path) - 1, i + 1)]
        tx, ty = xb - xa, yb - ya; L = math.hypot(tx, ty) or 1
        nx, ny = -ty / L, tx / L
        w = widths[i]; t = thick * max(0.25, w / max(widths))
        ring = []
        for k in range(n):
            a = 2 * math.pi * k / n
            ring.append((x + nx * w * math.cos(a), y + ny * w * math.cos(a), z + t * math.sin(a)))
        rings.append(ring)
    b.rings(rings, [m])

def faction_mats(kind):
    """Shared palettes: 'ufs', 'fsa', 'veyr'."""
    if kind == "ufs":
        return dict(main=node_mat("navy", (0.06, 0.13, 0.30), panel=1.4, dirt=0.35),
                    light=node_mat("ufsgrey", (0.40, 0.44, 0.50), panel=1.2, dirt=0.35),
                    white=node_mat("white", (0.62, 0.64, 0.66), panel=1.6, dirt=0.3),
                    sky=node_mat("sky", (0.18, 0.36, 0.62), panel=1.2, dirt=0.3),
                    dark=node_mat("ufsdark", (0.07, 0.08, 0.10), metal=0.7, panel=0.9, dirt=0.2),
                    accent=node_mat("ufsor", (0.70, 0.30, 0.05), panel=1.0, dirt=0.3),
                    glass=node_mat("glass", (0.30, 0.22, 0.03), glass=True),
                    glow=node_mat("glow", None, emit=(0.2, 0.5, 1.0)),
                    lamp=node_mat("lamp", None, emit=(0.9, 0.95, 1.0)))
    if kind == "fsa":
        return dict(main=node_mat("fsagrey", (0.24, 0.22, 0.19), panel=1.3, dirt=0.6),
                    light=node_mat("fsatan", (0.40, 0.34, 0.26), panel=1.2, dirt=0.6),
                    red=node_mat("fsared", (0.45, 0.04, 0.03), panel=1.3, dirt=0.5),
                    dark=node_mat("fsadark", (0.06, 0.06, 0.06), metal=0.7, panel=0.9, dirt=0.3),
                    olive=node_mat("olive", (0.17, 0.17, 0.14), metal=0.5, panel=1.0, dirt=0.5),
                    glass=node_mat("glass", (0.05, 0.10, 0.16), glass=True),
                    glow=node_mat("glow", None, emit=(1.0, 0.45, 0.12)),
                    green=node_mat("green", None, emit=(0.2, 1.0, 0.4)))
    shell = node_mat("chitin", (0.30, 0.22, 0.22), rough=0.35, metal=0.8, panel=3.0, dirt=0.55)
    bone = node_mat("bone", (0.42, 0.34, 0.28), rough=0.4, metal=0.6, panel=4.0, dirt=0.5)
    shell["flat"] = [0.07, 0.035, 0.10]           # flat 3D export: the black-violet the sprites show
    bone["flat"] = [0.20, 0.13, 0.22]
    return dict(shell=shell,
                bone=bone,
                dark=node_mat("vdark", (0.08, 0.04, 0.09), rough=0.3, metal=0.8, panel=2.0, dirt=0.3),
                glow=node_mat("vglow", None, emit=(0.30, 0.08, 1.0)),
                red=node_mat("vred", None, emit=(1.0, 0.1, 0.25)))

if __name__ == "__main__":
    main()
