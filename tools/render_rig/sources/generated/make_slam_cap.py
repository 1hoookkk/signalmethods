import bpy, os
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0.0, 0.0, 1.4))
cap = bpy.context.object
cap.scale = (11.0, 5.6, 2.8)
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
bevel = cap.modifiers.new("bevel", "BEVEL")
bevel.width = 0.75
bevel.segments = 10
bevel.limit_method = "ANGLE"
bpy.ops.object.modifier_apply(modifier="bevel")
bpy.ops.object.shade_smooth_by_angle(angle=0.6)
out = r"C:\Users\hooki\trench-native\tools\render_rig\sources\generated\slam_cap.glb"
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", use_selection=True, export_apply=True)
print("CAP wrote", out, len(cap.data.vertices))
