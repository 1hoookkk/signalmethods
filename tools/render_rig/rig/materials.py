import bpy


def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def hex_linear(hex_colour):
    h = hex_colour.lstrip("#")
    return tuple(srgb_to_linear(int(h[i:i + 2], 16) / 255.0) for i in (0, 2, 4)) + (1.0,)


def _principled(name):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    return mat, next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")


def _set(bsdf, **inputs):
    for key, value in inputs.items():
        bsdf.inputs[key.replace("_", " ")].default_value = value


PRESETS = {
    "nickel":        dict(Base_Color=(0.80, 0.79, 0.76, 1), Metallic=1.0, Roughness=0.18, Anisotropic=0.4),
    "satin_nickel":  dict(Base_Color=(0.78, 0.78, 0.76, 1), Metallic=1.0, Roughness=0.46, Anisotropic=0.3),
    "chrome":        dict(Base_Color=(0.95, 0.95, 0.95, 1), Metallic=1.0, Roughness=0.05),
    "aluminium":     dict(Base_Color=(0.91, 0.92, 0.92, 1), Metallic=1.0, Roughness=0.28, Anisotropic=0.6),
    "black_anodised": dict(Base_Color=(0.02, 0.02, 0.022, 1), Metallic=1.0, Roughness=0.35),
    "black_plastic": dict(Base_Color=(0.012, 0.012, 0.013, 1), Roughness=0.42, Coat_Weight=0.3),
    "bone_plastic":  dict(Base_Color=(0.72, 0.71, 0.66, 1), Roughness=0.5),
    "sea_salt_plastic": dict(Base_Color=(0.93, 0.89, 0.78, 1), Roughness=0.62, Coat_Weight=0.0, Specular_IOR_Level=0.35),
    "legend":        dict(Base_Color=(0.92, 0.90, 0.86, 1), Roughness=0.45),
    "cavity":        dict(Base_Color=(0.01, 0.012, 0.013, 1), Roughness=0.6),
    "clear_glass":   dict(Base_Color=(1, 1, 1, 1), Transmission_Weight=1.0, Roughness=0.03, IOR=1.49),
    "smoked_glass":  dict(Base_Color=(0.30, 0.34, 0.35, 1), Transmission_Weight=1.0, Roughness=0.03, IOR=1.49),
}


def preset(name):
    mat, bsdf = _principled(name)
    _set(bsdf, **PRESETS[name])
    return mat


def led(name, colour_hex, strength, off_hex=None):
    mat, bsdf = _principled(name)
    colour = hex_linear(colour_hex)
    lit = strength > 0.0
    _set(bsdf, Base_Color=colour if lit else (hex_linear(off_hex) if off_hex else (0.03, 0.05, 0.05, 1)), Roughness=0.4,
         Emission_Color=colour, Emission_Strength=strength)
    return mat


def tinted_glass(name, colour_hex, lit, off_hex=None):
    mat, bsdf = _principled(name)
    base = hex_linear(colour_hex) if lit else (hex_linear(off_hex) if off_hex else PRESETS["smoked_glass"]["Base_Color"])
    if lit:
        base = tuple(0.5 + 0.5 * c for c in base[:3]) + (1.0,)
    _set(bsdf, Base_Color=base, Transmission_Weight=1.0, Roughness=0.03, IOR=1.49)
    return mat


def resolve(spec, state):
    kind = spec["material"]
    if kind == "led":
        return led(spec.get("name", "led"), state.get("led_colour", "#44DEDE"), state.get("led_strength", 0.0), state.get("led_off_colour"))
    if kind == "led_glass":
        return tinted_glass(spec.get("name", "led_glass"), state.get("led_colour", "#44DEDE"), state.get("led_strength", 0.0) > 0.0,
                            state.get("lens_off_colour"))
    return preset(kind)
