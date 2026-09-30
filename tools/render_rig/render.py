import argparse
import datetime
import hashlib
import json
import math
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy
from mathutils import Quaternion
from rig import materials, meshes, post, studio


def sha256(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def git_rev():
    try:
        return subprocess.check_output(["git", "-C", HERE, "rev-parse", "--short", "HEAD"], text=True).strip()
    except Exception:
        return None


def srgb(hex_colour):
    h = hex_colour.lstrip("#")
    return tuple(int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4))


def args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    p = argparse.ArgumentParser(prog="render.py")
    p.add_argument("job")
    p.add_argument("--out", default=os.path.join(HERE, "..", "..", "out", "render_rig"))
    p.add_argument("--preview", action="store_true")
    p.add_argument("--save-blend", action="store_true")
    return p.parse_args(argv)


def build_scene(job, job_dir):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    ss = 1 if job.get("_preview") else job.get("supersample", 4)
    scene = studio.build(job["frame_mm"], job["pixels"] * ss, job.get("studio"))
    if job.get("_preview"):
        scene.cycles.samples = 32
    mesh_path = os.path.normpath(os.path.join(job_dir, job["mesh"]))
    obj = meshes.prepare(meshes.load(mesh_path), job["part"])
    extras = [meshes.add_primitive(e) for e in job.get("extras", [])]
    catcher = studio.shadow_catcher(job["shadow"]["z"], job["frame_mm"]) if "shadow" in job else None
    return scene, obj, extras, catcher, mesh_path, ss


def apply_state(job, obj, extras, state):
    merged = dict(led_colour=job.get("led_colour", "#44DEDE"), lens_off_colour=job.get("lens_off_colour"), led_off_colour=job.get("led_off_colour"), **state)
    for index, rule in enumerate(job["part"]["materials"]):
        if index < len(obj.data.materials):
            obj.data.materials[index] = materials.resolve(rule, merged)
        else:
            obj.data.materials.append(materials.resolve(rule, merged))
    for spec, e in zip(job.get("extras", []), extras):
        e.data.materials.clear()
        e.data.materials.append(materials.resolve(spec, merged))
    return merged


def render_shadow(scene, path, casters, catcher, spec):
    background = next(n for n in scene.world.node_tree.nodes if n.type == "BACKGROUND")
    key = bpy.data.objects["rig_key"]
    others = [o for o in scene.objects if o.type == "LIGHT" and o is not key]
    strength = background.inputs["Strength"].default_value
    placement = key.location.copy(), key.rotation_euler.copy(), key.data.size
    background.inputs["Strength"].default_value = 0.0
    key.location, key.rotation_euler, key.data.size = spec["light"]["location"], (0.0, 0.0, 0.0), spec["light"]["size"]
    for o in others:
        o.hide_render = True
    for o in casters:
        o.visible_camera = False
    catcher.hide_render = False
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    catcher.hide_render = True
    for o in casters:
        o.visible_camera = True
    for o in others:
        o.hide_render = False
    key.location, key.rotation_euler, key.data.size = placement
    background.inputs["Strength"].default_value = strength
    shadow = post.read(path)
    shadow[..., :3] = 0.0
    shadow[..., 3] = post.blur(shadow[..., 3], spec.get("blur_mm", 0.0) * scene.render.resolution_x / scene.camera.data.ortho_scale) * spec.get("opacity", 1.0)
    return shadow


def render_frame(scene, path, ss, merged):
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    rgba = post.read(path)
    g = merged.get("glow")
    if g and merged.get("led_strength", 0.0) > 0.0:
        for layer in (g if isinstance(g, list) else [g]):
            rgba = post.glow(rgba, srgb(merged["led_colour"]), layer["radius_px"] * ss, layer["strength"])
    return post.downsample(rgba, ss)


def main():
    a = args()
    job_path = os.path.abspath(a.job)
    with open(job_path) as f:
        job = json.load(f)
    job["_preview"] = a.preview
    out_dir = os.path.abspath(os.path.join(a.out, job["name"]))
    raw_dir = os.path.join(out_dir, "raw")
    os.makedirs(raw_dir, exist_ok=True)
    scene, obj, extras, catcher, mesh_path, ss = build_scene(job, os.path.dirname(job_path))
    written = []
    if catcher:
        shadow = render_shadow(scene, os.path.join(raw_dir, "shadow.png"), [obj, *extras], catcher, job["shadow"])
        target = os.path.join(out_dir, f"{job['name']}_shadow.png")
        post.write(target, post.downsample(shadow, ss))
        written.append(target)
    for name, state in job.get("states", {"default": {}}).items():
        merged = apply_state(job, obj, extras, state)
        strip_spec = job.get("filmstrip")
        if strip_spec:
            frames = []
            for i in range(strip_spec["frames"]):
                t = i / max(1, strip_spec["frames"] - 1)
                angle = math.radians(strip_spec["start_deg"] + t * strip_spec["sweep_deg"])
                axis, sign = meshes.AXES[job["part"]["front"]]
                front = [0.0, 0.0, 0.0]
                front[axis] = float(sign)
                obj.delta_rotation_quaternion = Quaternion(front, -angle)
                frames.append(render_frame(scene, os.path.join(raw_dir, f"{name}_{i:03d}.png"), ss, merged))
            final = post.strip(frames)
        else:
            final = render_frame(scene, os.path.join(raw_dir, f"{name}.png"), ss, merged)
        target = os.path.join(out_dir, f"{job['name']}_{name}.png")
        post.write(target, final)
        written.append(target)
    if a.save_blend:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, f"{job['name']}.blend"))
    provenance = {
        "job": os.path.relpath(job_path, HERE),
        "job_sha256": sha256(job_path),
        "mesh": os.path.relpath(mesh_path, HERE),
        "mesh_sha256": sha256(mesh_path),
        "blender": bpy.app.version_string,
        "rig_git": git_rev(),
        "preview": a.preview,
        "supersample": ss,
        "rendered_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
        "outputs": [os.path.basename(p) for p in written],
    }
    with open(os.path.join(out_dir, "provenance.json"), "w") as f:
        json.dump(provenance, f, indent=2)
    print("RIG wrote", *written, sep="\n  ")


main()
