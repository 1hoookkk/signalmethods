from __future__ import annotations
import ctypes as C
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DLL = ROOT / "target" / "release" / "trench_core.dll"
BLOCKS = ROOT / "ref" / "x3_menu" / "runtime_blocks"

X3_RATES = [44100, 48000, 96000, 192000]
NUM_CORNERS = 4
NUM_COEFFS = 5

CONTROL_LABELS: dict[str, tuple[str, str]] = {
    "2_pole_lowpass":        ("Freq", "Res"),
    "4_pole_lowpass":        ("Freq", "Res"),
    "6_pole_lowpass":        ("Freq", "Res"),
    "2_pole_highpass":       ("Freq", "Res"),
    "4_pole_highpass":       ("Freq", "Res"),
    "2_pole_bandpass":       ("Freq", "Res"),
    "4_pole_bandpass":       ("Freq", "Res"),
    "contrary_bandpass":     ("Freq", "Res"),
    "swept_eq_1_octave":     ("Freq", "Res"),
    "swept_eq_2_1_octave":   ("Freq", "Res"),
    "swept_eq_3_1_octave":   ("Freq", "Res"),
    "phaser_1":              ("Freq", "Res"),
    "phaser_2":              ("Freq", "Res"),
    "bat_phaser":            ("Freq", "Res"),
    "flanger_lite":          ("Freq", "Res"),
    "vocal_ah_ay_ee":        ("Freq", "Body"),
    "vocal_oo_ah":           ("Freq", "Body"),
}

class RuntimePreset:

    def __init__(self, stem: str, manifest_entry: dict):
        self.stem = stem
        self.name = manifest_entry.get("name", stem.replace("_", " ").title())
        self.active_stages = manifest_entry["active_stages"]
        self.rate_paths: dict[int, Path] = {}
        for rate_str, rel_path in manifest_entry["rates"].items():
            self.rate_paths[int(rate_str)] = ROOT / rel_path
        self.x_control, self.q_control = CONTROL_LABELS.get(
            stem, ("Freq", "Res"))

    def select_bank(self, host_rate: float) -> tuple[int, float]:
        best_idx, best_dist = 0, float("inf")
        for rate in sorted(self.rate_paths):
            dist = abs(rate - host_rate)
            if dist < best_dist:
                best_dist, best_idx = dist, rate
        return best_idx, best_dist

    def read_words(self, rate_hz: float) -> list[int] | None:
        path = self.rate_paths.get(int(rate_hz))
        if path is None or not path.exists():
            return None
        raw = path.read_bytes()
        return list(struct.unpack(f"<{len(raw)//2}H", raw))

class RuntimePresetManifest:

    def __init__(self, path: Path | None = None):
        if path is None:
            path = ROOT / "analysis" / "x3_runtime_pipeline" / "runtime_preset_manifest.json"
        with open(path) as f:
            raw = json.load(f)
        if raw.get("format") != "x3-runtime-preset-manifest-v1":
            raise ValueError(f"Unknown manifest format: {raw.get('format')}")
        self.presets: dict[str, RuntimePreset] = {}
        for entry in raw["presets"]:
            stem = entry["stem"]
            self.presets[stem] = RuntimePreset(stem, entry)

    def __getitem__(self, stem: str) -> RuntimePreset:
        return self.presets[stem]

    def __iter__(self):
        return iter(self.presets)

    def __len__(self):
        return len(self.presets)

def _get_lib():
    lib = C.CDLL(str(DLL))
    lib.trench_engine_load_runtime_preset.restype = C.c_int32
    lib.trench_engine_load_runtime_preset.argtypes = [
        C.c_void_p,
        C.c_char_p,
        C.POINTER(C.c_uint16),
        C.c_size_t,
        C.c_size_t,
        C.c_double,
        C.c_double,
    ]
    return lib

def load_runtime_preset(
    engine: C.c_void_p,
    preset: RuntimePreset,
    host_rate: float,
    boost: float = 1.0,
    lib=None,
) -> tuple[int, float]:
    if lib is None:
        lib = _get_lib()
    rate_idx, bank_rate = preset.select_bank(host_rate)
    words = preset.read_words(bank_rate)
    if words is None:
        raise FileNotFoundError(
            f"No runtime block for '{preset.stem}' at {int(bank_rate)} Hz"
        )
    name_bytes = preset.name.encode("utf-8") + b"\x00"
    w_arr = (C.c_uint16 * len(words))(*words)
    rc = lib.trench_engine_load_runtime_preset(
        engine, name_bytes, w_arr, len(words),
        preset.active_stages, float(bank_rate), float(boost),
    )
    return rc, bank_rate

def build_manifest(output_path: Path | None = None):
    if output_path is None:
        output_path = ROOT / "analysis" / "x3_runtime_pipeline" / "runtime_preset_manifest.json"
    entries = []
    for path in sorted(BLOCKS.glob("*_44100.raw")):
        stem = path.stem.replace("_44100", "")
        raw = path.read_bytes()
        word_count = len(raw) // 2
        active_stages = word_count // (NUM_CORNERS * NUM_COEFFS)
        x_label, q_label = CONTROL_LABELS.get(stem, ("Freq", "Res"))
        rates_available = {}
        for rate in X3_RATES:
            p = BLOCKS / f"{stem}_{rate}.raw"
            if p.exists():
                rates_available[str(rate)] = str(p.relative_to(ROOT).as_posix())
        entries.append({
            "stem": stem,
            "name": stem.replace("_", " ").title(),
            "active_stages": active_stages,
            "controls": {"x": x_label, "q": q_label},
            "rates": rates_available,
        })
    manifest = {
        "format": "x3-runtime-preset-manifest-v1",
        "rates": {str(r): i for i, r in enumerate(X3_RATES)},
        "note": "X3 xStream banks are distinct designs per rate — bank selection, not remapping.",
        "presets": entries,
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(manifest, indent=2))
    print(f"Wrote {output_path} ({len(entries)} presets)")
    return manifest

if __name__ == "__main__":
    build_manifest()
