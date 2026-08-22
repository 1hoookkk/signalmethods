
from __future__ import annotations

import argparse
from pathlib import Path
import sys

import bpy

def srgb_channel(value: int) -> float:
    channel = value / 255.0
    if channel <= 0.04045:
        return channel / 12.92
    return ((channel + 0.055) / 1.055) ** 2.4

def linear_colour(hex_value: str) -> tuple[float, float, float]:
    value = hex_value.strip().lstrip("#")
    if len(value) != 6:
        raise ValueError(f"expected RRGGBB, got {hex_value!r}")
    return tuple(srgb_channel(int(value[i:i + 2], 16)) for i in (0, 2, 4))

def enable_optix() -> None:
    preferences = bpy.context.preferences.addons["cycles"].preferences
    try:
        preferences.compute_device_type = "OPTIX"
        preferences.refresh_devices()
        for device in preferences.devices:
            device.use = device.type in {"OPTIX", "CUDA"}
    except Exception as error:
        print(f"[ss3] OPTIX unavailable, using configured Cycles device: {error}")

def parse_args() -> argparse.Namespace:
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    destination = parser.add_mutually_exclusive_group(required=True)
    destination.add_argument("--output", type=Path)
    destination.add_argument("--sequence-dir", type=Path)
    parser.add_argument("--frame", type=int, default=136)
    parser.add_argument("--frame-count", type=int, default=129)
    parser.add_argument("--accent", default="#FFC229")
    parser.add_argument("--samples", type=int, default=64)
    parser.add_argument("--core-width", type=float, default=3.25)
    parser.add_argument("--core-strength", type=float, default=7.0)
    parser.add_argument("--bounce-energy", type=float, default=55.0)
    return parser.parse_args(args)

def main() -> None:
    args = parse_args()
    enable_optix()

    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "GPU"
    scene.cycles.samples = args.samples
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 1200
    scene.render.resolution_y = 360
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.color_depth = "8"

    colour = linear_colour(args.accent)
    material = bpy.data.materials["WheelGlow_EmbeddedCobalt"]
    emission = next(node for node in material.node_tree.nodes if node.type == "EMISSION")
    emission.inputs["Color"].default_value = (*colour, 1.0)
    emission.inputs["Strength"].default_value = args.core_strength

    core = bpy.data.objects["WheelGlowCore"]
    core.scale.x *= args.core_width
    for light in bpy.data.objects:
        if light.type == "LIGHT" and light.name.startswith("WheelGlowBounce_"):
            light.data.color = colour
            light.data.energy = args.bounce_energy

    if args.sequence_dir is not None:
        args.sequence_dir.mkdir(parents=True, exist_ok=True)
        if args.frame_count < 2:
            raise ValueError("frame-count must be at least 2")
        for index in range(args.frame_count):
            source_frame = round(index * 256 / (args.frame_count - 1))
            scene.frame_set(source_frame)
            output = args.sequence_dir / f"frame_{index:03d}.png"
            scene.render.filepath = str(output.resolve())
            bpy.ops.render.render(write_still=True)
            print(
                f"[ss3] rendered {index + 1}/{args.frame_count} "
                f"(source frame {source_frame})",
                flush=True,
            )
        destination = args.sequence_dir
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        scene.frame_set(args.frame)
        scene.render.filepath = str(args.output.resolve())
        bpy.ops.render.render(write_still=True)
        destination = args.output

    print(
        f"[ss3] integrated render -> {destination} "
        f"accent={args.accent} core_width={args.core_width:.2f} "
        f"core_strength={args.core_strength:.2f} bounce={args.bounce_energy:.2f}",
        flush=True,
    )

if __name__ == "__main__":
    main()
