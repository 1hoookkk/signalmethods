
from __future__ import annotations

import json
import math
import os
from pathlib import Path

import bpy
from mathutils import Euler

FRAME_COUNT = 129
FRAME_LAST = FRAME_COUNT - 1
LOOP_PITCH_DEG = 360.0 / 31.0
SOURCE_FRAME_LAST = 256

ACCENT_SRGB_HEX = "C8E07A"
CORE_SRGB_HEX = "F0FFD2"
NAVY_SRGB_HEX = "2E3142"

ONLY_FRAMES = os.environ.get("TRENCH_RENDER_FRAMES")
RENDER_PASS = os.environ.get("TRENCH_RENDER_PASS", "both").strip().lower()
KNURL_CYCLES = float(os.environ.get("TRENCH_KNURL_CYCLES", "62"))
KNURL_STRENGTH = float(os.environ.get("TRENCH_KNURL_STRENGTH", "1.0"))
KNURL_LEAN = float(os.environ.get("TRENCH_KNURL_LEAN", "8"))
KNURL_JITTER = float(os.environ.get("TRENCH_KNURL_JITTER", "0.8"))
CHANNEL_DEPTH = float(os.environ.get("TRENCH_CHANNEL_DEPTH", "0.68"))
CHANNEL_HALFWIDTH = float(os.environ.get("TRENCH_CHANNEL_HALFWIDTH", "0.045"))
KEY_TOP_ENERGY = float(os.environ.get("TRENCH_KEY_TOP", "620"))
KEY_FRONT_ENERGY = float(os.environ.get("TRENCH_KEY_FRONT", "30"))
BOTTOM_FILL_ENERGY = float(os.environ.get("TRENCH_BOTTOM_FILL", "255"))

SOURCE_BLEND = Path(
    r"C:\Users\hooki\df2\dev\tmp\thumbwheel_blender\user_live_wheel_locked_spin_embedded_cobalt_257.blend"
)
REPO = Path(__file__).resolve().parents[1]
BASE_OUT = REPO / "dev" / "tmp" / "thumbwheel_blender" / "wheel_129"
OUTPUT_VARIANT = os.environ.get("TRENCH_RENDER_VARIANT", "").strip()
if OUTPUT_VARIANT and not OUTPUT_VARIANT.replace("_", "").isalnum():
    raise RuntimeError(f"Invalid TRENCH_RENDER_VARIANT={OUTPUT_VARIANT!r}")
OUT = BASE_OUT if not OUTPUT_VARIANT else BASE_OUT / "variants" / OUTPUT_VARIANT
WHEEL_PASS = OUT / "wheel_pass"
TRANSMISSION_PASS = OUT / "transmission_pass"

RAW_SIZE = (1200, 360)
FIXED_CROP = (25, 42, 1175, 318)

WHEEL_OBJECT = "WheelT"
ACTION_NAME = "Wheel_Locked_OnePitch_LeftToRight"
ABANDONED_OBJECTS = {"CodexCleanFaceGlowHead", "WheelGlowCore", "Light"}
ABANDONED_PREFIXES = ("WheelGlowBounce",)
OLD_LIGHTS = ("KeyT.007", "FacetGlintT.004", "EdgeT.006", "BottomRightT.001", "FillT.007")

TRANSMISSION_SECTION_CENTERS = (
    10.0, 32.0, 57.0, 86.0, 115.0, 141.0, 165.0, 211.0,
    237.0, 264.0, 285.0, 307.0, 342.0, 361.0, 381.0,
)

def _srgb_to_linear(channel: float) -> float:
    if channel <= 0.04045:
        return channel / 12.92
    return ((channel + 0.055) / 1.055) ** 2.4

def _hex_linear(hex_str: str) -> tuple[float, float, float]:
    srgb = tuple(int(hex_str[i : i + 2], 16) / 255.0 for i in (0, 2, 4))
    return tuple(_srgb_to_linear(v) for v in srgb)

def _accent_light_colour() -> tuple[float, float, float]:
    linear = _hex_linear(ACCENT_SRGB_HEX)
    peak = max(linear)
    return tuple(v / peak for v in linear)

def _set_input(node, name: str, value) -> None:
    socket = node.inputs.get(name)
    if socket is not None:
        socket.default_value = value

def _smoked_navy_material() -> bpy.types.Material:
    material = bpy.data.materials.new("TRENCH_RenderSmokedNavy")
    material.use_nodes = True
    tree = material.node_tree
    tree.nodes.clear()

    out = tree.nodes.new("ShaderNodeOutputMaterial")
    bsdf = tree.nodes.new("ShaderNodeBsdfPrincipled")
    navy = _hex_linear(NAVY_SRGB_HEX)
    _set_input(bsdf, "Base Color", (*navy, 1.0))
    _set_input(bsdf, "Metallic", 0.12)
    _set_input(bsdf, "Roughness", 0.52)
    _set_input(bsdf, "IOR", 1.45)
    _set_input(bsdf, "Specular IOR Level", 0.32)
    tree.links.new(bsdf.outputs[0], out.inputs[0])

    coords = tree.nodes.new("ShaderNodeTexCoord")
    xyz = tree.nodes.new("ShaderNodeSeparateXYZ")
    tree.links.new(coords.outputs["Object"], xyz.inputs[0])
    theta = tree.nodes.new("ShaderNodeMath")
    theta.operation = "ARCTAN2"
    tree.links.new(xyz.outputs["Y"], theta.inputs[0])
    tree.links.new(xyz.outputs["X"], theta.inputs[1])
    freq = tree.nodes.new("ShaderNodeMath")
    freq.operation = "MULTIPLY"
    freq.inputs[1].default_value = KNURL_CYCLES
    tree.links.new(theta.outputs[0], freq.inputs[0])
    lean = tree.nodes.new("ShaderNodeMath")
    lean.operation = "MULTIPLY"
    lean.inputs[1].default_value = KNURL_LEAN
    tree.links.new(xyz.outputs["Z"], lean.inputs[0])
    noise = tree.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 3.0
    tree.links.new(coords.outputs["Object"], noise.inputs["Vector"])
    jitter = tree.nodes.new("ShaderNodeMath")
    jitter.operation = "MULTIPLY_ADD"
    jitter.inputs[1].default_value = 2.0 * KNURL_JITTER
    jitter.inputs[2].default_value = -KNURL_JITTER
    tree.links.new(noise.outputs["Fac"], jitter.inputs[0])
    phase = tree.nodes.new("ShaderNodeMath")
    phase.operation = "ADD"
    tree.links.new(freq.outputs[0], phase.inputs[0])
    tree.links.new(lean.outputs[0], phase.inputs[1])
    phase2 = tree.nodes.new("ShaderNodeMath")
    phase2.operation = "ADD"
    tree.links.new(phase.outputs[0], phase2.inputs[0])
    tree.links.new(jitter.outputs[0], phase2.inputs[1])
    below = tree.nodes.new("ShaderNodeMath")
    below.operation = "LESS_THAN"
    below.inputs[1].default_value = 0.0
    tree.links.new(xyz.outputs["Z"], below.inputs[0])
    flip = tree.nodes.new("ShaderNodeMath")
    flip.operation = "MULTIPLY_ADD"
    flip.inputs[1].default_value = math.pi
    flip.inputs[2].default_value = 0.0
    tree.links.new(below.outputs[0], flip.inputs[0])
    phase3 = tree.nodes.new("ShaderNodeMath")
    phase3.operation = "ADD"
    tree.links.new(phase2.outputs[0], phase3.inputs[0])
    tree.links.new(flip.outputs[0], phase3.inputs[1])
    sine = tree.nodes.new("ShaderNodeMath")
    sine.operation = "SINE"
    tree.links.new(phase3.outputs[0], sine.inputs[0])
    wave = tree.nodes.new("ShaderNodeMath")
    wave.operation = "MULTIPLY"
    wave.inputs[1].default_value = 1.6
    wave.use_clamp = False
    tree.links.new(sine.outputs[0], wave.inputs[0])
    clampn = tree.nodes.new("ShaderNodeMath")
    clampn.operation = "MAXIMUM"
    clampn.inputs[1].default_value = -1.0
    tree.links.new(wave.outputs[0], clampn.inputs[0])
    clampp = tree.nodes.new("ShaderNodeMath")
    clampp.operation = "MINIMUM"
    clampp.inputs[1].default_value = 1.0
    tree.links.new(clampn.outputs[0], clampp.inputs[0])
    wave = clampp

    bump = tree.nodes.new("ShaderNodeBump")
    _set_input(bump, "Strength", min(1.0, 0.65 * KNURL_STRENGTH))
    _set_input(bump, "Distance", 0.004)
    tree.links.new(wave.outputs[0], bump.inputs["Height"])
    tree.links.new(bump.outputs[0], bsdf.inputs["Normal"])

    half = tree.nodes.new("ShaderNodeMath")
    half.operation = "MULTIPLY_ADD"
    half.inputs[1].default_value = 0.5
    half.inputs[2].default_value = 0.5
    tree.links.new(wave.outputs[0], half.inputs[0])
    depth = min(1.0, 0.55 * KNURL_STRENGTH)
    gain = tree.nodes.new("ShaderNodeMath")
    gain.operation = "MULTIPLY_ADD"
    gain.inputs[1].default_value = depth
    gain.inputs[2].default_value = 1.0 - depth
    tree.links.new(half.outputs[0], gain.inputs[0])

    zn = tree.nodes.new("ShaderNodeMath")
    zn.operation = "DIVIDE"
    zn.inputs[1].default_value = CHANNEL_HALFWIDTH
    tree.links.new(xyz.outputs["Z"], zn.inputs[0])
    z2 = tree.nodes.new("ShaderNodeMath")
    z2.operation = "MULTIPLY"
    tree.links.new(zn.outputs[0], z2.inputs[0])
    tree.links.new(zn.outputs[0], z2.inputs[1])
    z4 = tree.nodes.new("ShaderNodeMath")
    z4.operation = "MULTIPLY"
    tree.links.new(z2.outputs[0], z4.inputs[0])
    tree.links.new(z2.outputs[0], z4.inputs[1])
    z8 = tree.nodes.new("ShaderNodeMath")
    z8.operation = "MULTIPLY"
    tree.links.new(z4.outputs[0], z8.inputs[0])
    tree.links.new(z4.outputs[0], z8.inputs[1])
    negz8 = tree.nodes.new("ShaderNodeMath")
    negz8.operation = "MULTIPLY"
    negz8.inputs[1].default_value = -1.0
    tree.links.new(z8.outputs[0], negz8.inputs[0])
    band = tree.nodes.new("ShaderNodeMath")
    band.operation = "EXPONENT"
    tree.links.new(negz8.outputs[0], band.inputs[0])
    chan = tree.nodes.new("ShaderNodeMath")
    chan.operation = "MULTIPLY_ADD"
    chan.inputs[1].default_value = -CHANNEL_DEPTH
    chan.inputs[2].default_value = 1.0
    tree.links.new(band.outputs[0], chan.inputs[0])
    spec = tree.nodes.new("ShaderNodeMath")
    spec.operation = "MULTIPLY"
    spec.inputs[1].default_value = 0.32
    tree.links.new(chan.outputs[0], spec.inputs[0])
    tree.links.new(spec.outputs[0], bsdf.inputs["Specular IOR Level"])
    met = tree.nodes.new("ShaderNodeMath")
    met.operation = "MULTIPLY"
    met.inputs[1].default_value = 0.12
    tree.links.new(chan.outputs[0], met.inputs[0])
    tree.links.new(met.outputs[0], bsdf.inputs["Metallic"])
    total = tree.nodes.new("ShaderNodeMath")
    total.operation = "MULTIPLY"
    tree.links.new(gain.outputs[0], total.inputs[0])
    tree.links.new(chan.outputs[0], total.inputs[1])

    navy_rgb = tree.nodes.new("ShaderNodeRGB")
    navy_rgb.outputs[0].default_value = (*navy, 1.0)
    scaled = tree.nodes.new("ShaderNodeVectorMath")
    scaled.operation = "SCALE"
    tree.links.new(navy_rgb.outputs[0], scaled.inputs[0])
    tree.links.new(total.outputs[0], scaled.inputs["Scale"])
    tree.links.new(scaled.outputs[0], bsdf.inputs["Base Color"])
    return material

def _aim_x_rotation(y: float, z: float) -> float:
    return math.atan2(-y, z)

def _front_rig(scene: bpy.types.Scene) -> list[bpy.types.Object]:
    spec = [
        ("TRENCH_KeyTop", (0.0, -2.9, 3.4), (3.0, 0.4), KEY_TOP_ENERGY),
        ("TRENCH_KeyFront", (0.0, -3.4, 0.75), (7.0, 2.5), KEY_FRONT_ENERGY),
        ("TRENCH_BottomFill", (0.0, -3.0, -2.6), (3.0, 0.5), BOTTOM_FILL_ENERGY),
    ]
    rig = []
    for name, loc, (sx, sy), energy in spec:
        data = bpy.data.lights.new(name, type="AREA")
        obj = bpy.data.objects.new(name, data)
        scene.collection.objects.link(obj)
        obj.location = loc
        obj.rotation_euler = Euler((_aim_x_rotation(loc[1], loc[2]), 0.0, 0.0), "XYZ")
        data.shape = "RECTANGLE"
        data.size = sx
        data.size_y = sy
        data.energy = energy
        data.color = (0.94, 0.97, 1.06)
        if hasattr(data, "use_shadow"):
            data.use_shadow = True
        rig.append(obj)
    return rig

def _transmission_bank(scene: bpy.types.Scene) -> list[bpy.types.Object]:
    lights = []
    for index, center in enumerate(TRANSMISSION_SECTION_CENTERS):
        name = f"TRENCH_RenderTransmissionSection_{index:02d}"
        data = bpy.data.lights.new(name, type="POINT")
        obj = bpy.data.objects.new(name, data)
        scene.collection.objects.link(obj)
        obj.location = ((center / 400.0 - 0.5) * 1.0015, -0.55, 0.0)
        data.energy = 0.0
        data.color = _accent_light_colour()
        data.shadow_soft_size = 0.04
        obj.hide_render = True
        lights.append(obj)
    return lights

def _smoothstep(edge0: float, edge1: float, value: float) -> float:
    if edge0 == edge1:
        return 1.0 if value >= edge1 else 0.0
    t = max(0.0, min(1.0, (value - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)

def _transmission_amplitudes(frame: int) -> list[float]:
    if frame <= 0:
        return [0.0] * len(TRANSMISSION_SECTION_CENTERS)
    authored = frame * (SOURCE_FRAME_LAST / FRAME_LAST)
    count = len(TRANSMISSION_SECTION_CENTERS)
    last_start = SOURCE_FRAME_LAST - 5.0
    starts = [i * last_start / max(1, count - 1) for i in range(count)]
    lead_index = (authored / float(SOURCE_FRAME_LAST)) * (count - 1)
    amplitudes = []
    for index, start in enumerate(starts):
        ramp = _smoothstep(start, start + 5.0, float(authored))
        age = max(0.0, authored - (start + 5.0))
        trail = math.exp(-age / 78.0)
        base = ramp * (0.035 + 0.965 * trail)
        lead = math.exp(-(((index - lead_index) / 0.72) ** 2))
        amplitudes.append(min(1.0, base * 0.90 + lead * 0.10))
    return amplitudes

def _set_engine(scene: bpy.types.Scene) -> None:
    for engine in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
        try:
            scene.render.engine = engine
            return
        except TypeError:
            continue
    raise RuntimeError("No EEVEE engine id accepted")

def main() -> None:
    if not SOURCE_BLEND.exists():
        raise FileNotFoundError(SOURCE_BLEND)

    scene = bpy.context.scene
    wheel = bpy.data.objects.get(WHEEL_OBJECT)
    if wheel is None or wheel.type != "MESH":
        raise RuntimeError(f"Expected mesh object {WHEEL_OBJECT!r}")
    if wheel.animation_data is None or wheel.animation_data.action is None:
        raise RuntimeError(f"{WHEEL_OBJECT} has no authored action")
    if wheel.animation_data.action.name != ACTION_NAME:
        raise RuntimeError(f"Unexpected action {wheel.animation_data.action.name!r}")

    scene.frame_set(0)
    bpy.context.view_layer.update()
    base_rotation = tuple(float(v) for v in wheel.rotation_euler)
    start_deg = ((math.degrees(base_rotation[2] + math.pi) + 180.0) % 360.0) - 180.0
    if abs(start_deg - 23.0) > 0.75:
        raise RuntimeError(f"Authored base pose drifted: {start_deg} vs 23.0")
    checkpoints = {"0": start_deg, "128": start_deg + LOOP_PITCH_DEG}

    OUT.mkdir(parents=True, exist_ok=True)
    WHEEL_PASS.mkdir(parents=True, exist_ok=True)
    TRANSMISSION_PASS.mkdir(parents=True, exist_ok=True)

    wheel.animation_data.action = None
    wheel.data.materials.clear()
    wheel.data.materials.append(_smoked_navy_material())

    for obj in scene.objects:
        if obj.name in ABANDONED_OBJECTS or obj.name.startswith(ABANDONED_PREFIXES):
            obj.hide_render = True
    for name in OLD_LIGHTS:
        obj = bpy.data.objects.get(name)
        if obj is not None:
            obj.hide_render = True

    rig = _front_rig(scene)
    bank = _transmission_bank(scene)

    _set_engine(scene)
    if hasattr(scene.eevee, "use_shadows"):
        scene.eevee.use_shadows = True
    scene.render.resolution_x, scene.render.resolution_y = RAW_SIZE
    scene.render.resolution_percentage = 100
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.color_depth = "8"
    scene.render.film_transparent = True
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    scene.view_settings.exposure = 0.0

    if ONLY_FRAMES is not None:
        render_frames = sorted({int(v) for v in ONLY_FRAMES.split(",")})
    else:
        render_frames = list(range(FRAME_COUNT))
    if any(f < 0 or f > FRAME_LAST for f in render_frames):
        raise RuntimeError(f"Invalid TRENCH_RENDER_FRAMES={ONLY_FRAMES}")
    if RENDER_PASS not in {"both", "wheel", "transmission"}:
        raise RuntimeError(f"Invalid TRENCH_RENDER_PASS={RENDER_PASS!r}")

    def pose(frame: int) -> None:
        scene.frame_set(frame)
        z = base_rotation[2] + math.pi + math.radians(LOOP_PITCH_DEG) * frame / FRAME_LAST
        wheel.rotation_euler = Euler((base_rotation[0], base_rotation[1], z), wheel.rotation_mode)
        bpy.context.view_layer.update()

    if RENDER_PASS in {"both", "wheel"}:
        for light in bank:
            light.hide_render = True
        for frame in render_frames:
            pose(frame)
            scene.render.filepath = str(WHEEL_PASS / f"frame_{frame:03d}.png")
            bpy.ops.render.render(write_still=True)
            if frame % 16 == 0 or frame == FRAME_LAST:
                print(f"[wheel-129] wheel_pass {frame}/{FRAME_LAST}")

    if RENDER_PASS in {"both", "transmission"}:
        for frame in render_frames:
            for light, amplitude in zip(bank, _transmission_amplitudes(frame)):
                light.hide_render = amplitude <= 0.0005
                light.data.energy = 2.4 * amplitude
            pose(frame)
            scene.render.filepath = str(TRANSMISSION_PASS / f"frame_{frame:03d}.png")
            bpy.ops.render.render(write_still=True)
            if frame % 16 == 0 or frame == FRAME_LAST:
                print(f"[wheel-129] transmission_pass {frame}/{FRAME_LAST}")

    manifest = {
        "source_blend": str(SOURCE_BLEND),
        "scene": scene.name,
        "object": WHEEL_OBJECT,
        "action": ACTION_NAME,
        "frame_count": FRAME_COUNT,
        "loop_pitch_deg": LOOP_PITCH_DEG,
        "opposite_degrees": checkpoints,
        "raw_size": list(RAW_SIZE),
        "fixed_crop": list(FIXED_CROP),
        "render_engine": scene.render.engine,
        "view_transform": "Standard",
        "material": {
            "name": "TRENCH_RenderSmokedNavy",
            "navy_srgb_hex": NAVY_SRGB_HEX,
            "knurl_cycles": KNURL_CYCLES,
            "knurl_strength": KNURL_STRENGTH,
            "channel_depth": CHANNEL_DEPTH,
            "channel_halfwidth": CHANNEL_HALFWIDTH,
        },
        "lights": {
            "rig": "x=0 front-on/top-down (KeyTop / KeyFront / BottomFill)",
            "key_top": KEY_TOP_ENERGY,
            "key_front": KEY_FRONT_ENERGY,
            "bottom_fill": BOTTOM_FILL_ENERGY,
        },
        "transmission_light": {
            "type": "stationary_point_section_bank",
            "peak_energy_each": 2.4,
            "accent_srgb_hex": ACCENT_SRGB_HEX,
            "core_srgb_hex": CORE_SRGB_HEX,
            "screen_centers": list(TRANSMISSION_SECTION_CENTERS),
            "progression": "257-pipeline X3 rise and trail at authored positions",
        },
        "passes": {"wheel": str(WHEEL_PASS), "transmission": str(TRANSMISSION_PASS)},
    }
    (OUT / "render_manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(json.dumps({"render_manifest": str(OUT / "render_manifest.json"), "checkpoints": checkpoints}))

if __name__ == "__main__":
    main()
