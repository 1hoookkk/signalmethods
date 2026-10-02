import math
import os
import bmesh
import bpy

IMPORTERS = {
    ".stl": lambda p: bpy.ops.wm.stl_import(filepath=p),
    ".obj": lambda p: bpy.ops.wm.obj_import(filepath=p),
    ".glb": lambda p: bpy.ops.import_scene.gltf(filepath=p),
    ".gltf": lambda p: bpy.ops.import_scene.gltf(filepath=p),
    ".fbx": lambda p: bpy.ops.import_scene.fbx(filepath=p),
}

AXES = {"+x": (0, 1), "-x": (0, -1), "+y": (1, 1), "-y": (1, -1), "+z": (2, 1), "-z": (2, -1)}


def load(path):
    before = set(bpy.data.objects)
    IMPORTERS[os.path.splitext(path)[1].lower()](path)
    new = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in new:
        o.select_set(True)
    bpy.context.view_layer.objects.active = new[0]
    if len(new) > 1:
        bpy.ops.object.join()
    return bpy.context.view_layer.objects.active


def _axial(co, axis, sign):
    return co[axis] * sign


def _radial(co, axis):
    others = [i for i in range(3) if i != axis]
    return math.hypot(co[others[0]], co[others[1]])


def prepare(obj, part):
    axis, sign = AXES[part["front"]]
    me = obj.data
    bm = bmesh.new()
    bm.from_mesh(me)
    cut = part.get("keep_front_of")
    if cut is not None:
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if all(_axial(v.co, axis, sign) < cut for v in f.verts)], context="FACES")
    keep = part.get("keep_within_radius")
    if keep is not None:
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if all(_radial(v.co, axis) > keep for v in f.verts)], context="FACES")
    rules = part["materials"]
    for f in bm.faces:
        c = f.calc_center_median()
        a, r = _axial(c, axis, sign), _radial(c, axis)
        for index, rule in enumerate(rules):
            if a >= rule.get("axial_min", -1e9) and a <= rule.get("axial_max", 1e9) and r >= rule.get("radius_min", 0.0) and r <= rule.get("radius_max", 1e9):
                f.material_index = index
                break
    bm.to_mesh(me)
    bm.free()
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(part.get("smooth_angle_deg", 35.0)))
    front = [0.0, 0.0, 0.0]
    front[axis] = float(sign)
    obj.rotation_mode = "QUATERNION"
    from mathutils import Vector
    obj.rotation_quaternion = Vector(front).rotation_difference(Vector((0.0, 0.0, 1.0)))
    return obj


def add_primitive(spec):
    kind = spec["type"]
    if kind == "disc":
        bpy.ops.mesh.primitive_cylinder_add(radius=spec["radius"], depth=spec.get("depth", 0.2), location=(0, 0, spec["z"]), vertices=96)
    elif kind == "dome":
        bpy.ops.mesh.primitive_uv_sphere_add(radius=spec["radius"], location=(0, 0, spec["z"]), segments=64, ring_count=32)
        bpy.context.object.scale = (1.0, 1.0, spec.get("flatten", 1.0))
    elif kind == "ring":
        bpy.ops.mesh.primitive_torus_add(major_radius=spec["radius"], minor_radius=spec["width"] / 2.0, location=(0, 0, spec["z"]),
                                         major_segments=128, minor_segments=16)
    elif kind == "box":
        w, h, d = spec["size"]
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(spec.get("x", 0.0), spec.get("y", 0.0), spec["z"] + d / 2.0))
        box = bpy.context.object
        box.scale = (w, h, d)
        bpy.ops.object.transform_apply(location=True, rotation=False, scale=True)
        if spec.get("bevel", 0.0) > 0.0:
            mod = box.modifiers.new("bevel", "BEVEL")
            mod.width = spec["bevel"]
            mod.segments = 8
            mod.limit_method = "ANGLE"
            bpy.ops.object.modifier_apply(modifier="bevel")
    elif kind == "text":
        bpy.ops.object.text_add(location=(0, 0, spec["z"]))
        t = bpy.context.object
        t.data.body = spec["text"]
        t.data.size = spec["size"]
        t.data.align_x = "CENTER"
        t.data.align_y = "CENTER"
        t.data.extrude = spec.get("depth", 0.2) / 2.0
        if spec.get("font"):
            t.data.font = bpy.data.fonts.load(spec["font"])
        bpy.ops.object.convert(target="MESH")
    bpy.context.object.visible_shadow = spec.get("casts_shadow", True)
    if kind in ("disc", "box", "text"):
        bpy.ops.object.shade_smooth_by_angle(angle=math.radians(35.0))
    else:
        bpy.ops.object.shade_smooth()
    return bpy.context.object
