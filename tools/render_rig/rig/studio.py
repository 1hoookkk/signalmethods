import math
import os
import bpy

STUDIO = {
    "hdri": "studio.exr",
    "hdri_strength": 1.1,
    "hdri_rotation_deg": 135.0,
    "world_grey": 0.015,
    "softboxes": [
        {"name": "overhead", "size": (140.0, 140.0), "location": (0.0, 0.0, 90.0), "rotation_deg": (0.0, 0.0, 0.0), "strength": 3.0, "gradient_deg": 135.0},
        {"name": "strip_left", "size": (12.0, 90.0), "location": (-45.0, 0.0, 25.0), "rotation_deg": (0.0, -60.0, 0.0), "strength": 6.0},
        {"name": "strip_right", "size": (8.0, 90.0), "location": (45.0, 0.0, 20.0), "rotation_deg": (0.0, 65.0, 0.0), "strength": 2.0},
    ],
    "key": {"energy": 2500.0, "size": 6.0, "shape": "SQUARE", "location": (-22.0, 22.0, 30.0), "rotation_deg": (40.0, -35.0, 0.0)},
    "fill": {"energy": 400.0, "size": 20.0, "location": (25.0, -20.0, 25.0), "rotation_deg": (-40.0, 40.0, 0.0)},
    "samples": 256,
    "lens_mm": None,
    "view_transform": "AgX",
}


def bundled_hdri(name):
    return os.path.join(bpy.utils.resource_path("LOCAL"), "datafiles", "studiolights", "world", name)


def _area(scene, name, spec):
    light = bpy.data.lights.new(name, "AREA")
    light.energy = spec["energy"]
    light.shape = spec.get("shape", "SQUARE")
    light.size = spec["size"]
    obj = bpy.data.objects.new(name, light)
    scene.collection.objects.link(obj)
    obj.location = spec["location"]
    obj.rotation_euler = tuple(math.radians(a) for a in spec["rotation_deg"])
    return obj


def _softbox(scene, spec):
    bpy.ops.mesh.primitive_plane_add(size=1.0, location=spec["location"])
    card = bpy.context.object
    card.name = "rig_softbox_" + spec["name"]
    card.scale = (spec["size"][0], spec["size"][1], 1.0)
    card.rotation_euler = tuple(math.radians(a) for a in spec["rotation_deg"])
    card.visible_camera = False
    mat = bpy.data.materials.new(card.name)
    mat.use_nodes = True
    nt = mat.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    emit = nt.nodes.new("ShaderNodeEmission")
    emit.inputs["Strength"].default_value = spec["strength"]
    nt.links.new(emit.outputs["Emission"], out.inputs["Surface"])
    if "gradient_deg" in spec:
        coords = nt.nodes.new("ShaderNodeTexCoord")
        mapping = nt.nodes.new("ShaderNodeMapping")
        mapping.inputs["Location"].default_value = (0.5, 0.5, 0.0)
        mapping.inputs["Rotation"].default_value = (0.0, 0.0, math.radians(spec["gradient_deg"]))
        centre = nt.nodes.new("ShaderNodeMapping")
        centre.inputs["Location"].default_value = (-0.5, -0.5, 0.0)
        ramp = nt.nodes.new("ShaderNodeValToRGB")
        ramp.color_ramp.elements[0].position = 0.15
        ramp.color_ramp.elements[0].color = (0.0, 0.0, 0.0, 1.0)
        ramp.color_ramp.elements[1].position = 0.85
        gradient = nt.nodes.new("ShaderNodeTexGradient")
        nt.links.new(coords.outputs["UV"], centre.inputs["Vector"])
        nt.links.new(centre.outputs["Vector"], mapping.inputs["Vector"])
        nt.links.new(mapping.outputs["Vector"], gradient.inputs["Vector"])
        nt.links.new(gradient.outputs["Fac"], ramp.inputs["Fac"])
        nt.links.new(ramp.outputs["Color"], emit.inputs["Color"])
    card.data.materials.append(mat)
    return card


def shadow_catcher(z, frame_mm):
    bpy.ops.mesh.primitive_plane_add(size=frame_mm * 4.0, location=(0.0, 0.0, z))
    plane = bpy.context.object
    plane.name = "rig_shadow_catcher"
    plane.is_shadow_catcher = True
    plane.hide_render = True
    return plane


def build(frame_mm, pixels, overrides=None):
    cfg = dict(STUDIO, **(overrides or {}))
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = cfg["samples"]
    scene.cycles.use_denoising = True
    scene.cycles.max_bounces = 16
    scene.cycles.transmission_bounces = 12
    scene.render.film_transparent = True
    scene.render.resolution_x = scene.render.resolution_y = pixels
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.color_depth = "8"
    scene.view_settings.view_transform = cfg["view_transform"]

    cam_data = bpy.data.cameras.new("rig_camera")
    cam = bpy.data.objects.new("rig_camera", cam_data)
    scene.collection.objects.link(cam)
    if cfg["lens_mm"]:
        cam_data.type = "PERSP"
        cam_data.sensor_fit = "HORIZONTAL"
        cam_data.lens = cfg["lens_mm"]
        cam_data.clip_start = 0.1
        half_fov = math.atan(cam_data.sensor_width / (2.0 * cam_data.lens))
        cam.location = (0.0, 0.0, cfg.get("front_z_mm", 6.0) + (frame_mm / 2.0) / math.tan(half_fov))
    else:
        cam_data.type = "ORTHO"
        cam_data.ortho_scale = frame_mm
        cam.location = (0.0, 0.0, 200.0)
    scene.camera = cam

    world = bpy.data.worlds.new("rig_world")
    scene.world = world
    world.use_nodes = True
    nt = world.node_tree
    background = next(n for n in nt.nodes if n.type == "BACKGROUND")
    if not cfg["hdri"]:
        g = cfg["world_grey"]
        background.inputs["Color"].default_value = (g, g, g, 1.0)
        for box in cfg["softboxes"]:
            _softbox(scene, box)
        _area(scene, "rig_key", cfg["key"])
        return scene
    env = nt.nodes.new("ShaderNodeTexEnvironment")
    env.image = bpy.data.images.load(bundled_hdri(cfg["hdri"]), check_existing=True)
    mapping = nt.nodes.new("ShaderNodeMapping")
    coords = nt.nodes.new("ShaderNodeTexCoord")
    mapping.inputs["Rotation"].default_value = (0.0, 0.0, math.radians(cfg["hdri_rotation_deg"]))
    nt.links.new(coords.outputs["Generated"], mapping.inputs["Vector"])
    nt.links.new(mapping.outputs["Vector"], env.inputs["Vector"])
    nt.links.new(env.outputs["Color"], background.inputs["Color"])
    background.inputs["Strength"].default_value = cfg["hdri_strength"]

    _area(scene, "rig_key", cfg["key"])
    _area(scene, "rig_fill", cfg["fill"])
    return scene
