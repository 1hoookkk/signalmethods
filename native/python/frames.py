from dataclasses import dataclass
import json
import math
import os
import sys
from typing import Dict, List, Optional, Tuple
import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import trench_core

SPARKLINE_GRID_HZ = np.geomspace(40.0, 16000.0, 48)

@dataclass
class Frame:
    id: int
    kind: str
    group: str
    name: str
    detail: str
    root_hz: float
    words: List[List[int]]
    sparkline: List[float]

    @property
    def log_pitch(self) -> float:
        return math.log2(max(20.0, self.root_hz))

def _find_root_hz(words: List[List[int]], datum_hz: float = 44100.0) -> float:
    lowest = 20000.0
    for sec in words[:6]:
        geom = trench_core.TrenchSectionGeometry()
        w_arr = (trench_core.ctypes.c_uint16 * 5)(*sec)
        trench_core._dll.trench_section_geometry_get(
            trench_core.ctypes.byref(w_arr),
            trench_core.ctypes.c_double(datum_hz),
            trench_core.ctypes.byref(geom)
        )
        if geom.pole_type == 1 and geom.pole_a >= 30.0:
            if geom.pole_a < lowest:
                lowest = geom.pole_a
    if lowest < 20000.0:
        return lowest
    for sec in words[:6]:
        geom = trench_core.TrenchSectionGeometry()
        w_arr = (trench_core.ctypes.c_uint16 * 5)(*sec)
        trench_core._dll.trench_section_geometry_get(
            trench_core.ctypes.byref(w_arr),
            trench_core.ctypes.c_double(datum_hz),
            trench_core.ctypes.byref(geom)
        )
        if geom.pole_type == 1 and geom.pole_a > 0.0:
            return geom.pole_a
    return 1000.0

def _sparkline_for_words(words: List[List[int]], datum_hz: float = 44100.0) -> List[float]:
    body = trench_core.Body.from_legacy_bytes(bytes([0] * 240))
    for s in range(min(6, len(words))):
        body.set_words(0, s, words[s])
    bqs = body.cascade(0.0, 0.0, 0.0, datum_hz, datum_hz)
    db = trench_core.cascade_response_db(bqs, SPARKLINE_GRID_HZ, datum_hz)
    return [round(float(np.clip(v, -30.0, 30.0)), 2) for v in db]

def _compile_type_rows(rows: List[dict], datum_hz: float = 44100.0) -> List[List[int]]:
    k_sections = 6
    count = min(len(rows), k_sections)
    if count == 0:
        return [[0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF] for _ in range(6)]

    ceiling = (rows[count - 1].get("type", "").upper() in ["LP", "LOWPASS"])
    slot_map: List[Optional[dict]] = [None] * k_sections
    next_idx = 0
    for i in range(count):
        target = (k_sections - 1) if (ceiling and i + 1 == count) else next_idx
        if not (ceiling and i + 1 == count):
            next_idx += 1
        if target < k_sections and slot_map[target] is None:
            slot_map[target] = rows[i]

    out_words: List[List[int]] = []
    for s in range(k_sections):
        row = slot_map[s]
        if row is None:
            out_words.append([0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF])
            continue
        rtype = str(row.get("type", "EQ")).upper()
        hz = float(row.get("hz", 1000.0))
        bw_hz = max(1.0, float(row.get("bw_hz", 50.0)))
        gain_db = float(row.get("gain_db", 0.0))

        r_pole = math.exp(-math.pi * bw_hz / datum_hz)
        if rtype in ["LP", "LOWPASS"]:
            pole_geom = (1, hz, r_pole)
            zero_geom = (1, 20000.0, math.exp(-math.pi * 50.0 / datum_hz))
            scale = 1.0
            g = trench_core.TrenchSectionGeometry(
                pole_type=1, pole_a=hz, pole_b=r_pole,
                zero_type=0, zero_a=0.0, zero_b=0.0,
                scale=1.0
            )
            w = (trench_core.ctypes.c_uint16 * 5)()
            trench_core._dll.trench_section_geometry_set(
                trench_core.ctypes.byref(g),
                trench_core.ctypes.c_double(datum_hz),
                trench_core.ctypes.byref(w)
            )
            zero_rsq = 0x01F0
            zero_mag = trench_core.p2k_mag_word_for(20000.0, zero_rsq)
            words = [zero_mag, zero_rsq, int(w[2]), int(w[3]), 0xDFFF]
            out_words.append(words)
        elif rtype in ["HP", "HIGHPASS"]:
            g = trench_core.TrenchSectionGeometry(
                pole_type=1, pole_a=hz, pole_b=r_pole,
                zero_type=2, zero_a=1.0, zero_b=1.0,
                scale=1.0
            )
            w = (trench_core.ctypes.c_uint16 * 5)()
            trench_core._dll.trench_section_geometry_set(
                trench_core.ctypes.byref(g),
                trench_core.ctypes.c_double(datum_hz),
                trench_core.ctypes.byref(w)
            )
            out_words.append([int(w[i]) for i in range(5)])
        elif rtype in ["POLE"]:
            g = trench_core.TrenchSectionGeometry(
                pole_type=1, pole_a=hz, pole_b=r_pole,
                zero_type=0, zero_a=0.0, zero_b=0.0,
                scale=1.0
            )
            w = (trench_core.ctypes.c_uint16 * 5)()
            trench_core._dll.trench_section_geometry_set(
                trench_core.ctypes.byref(g),
                trench_core.ctypes.c_double(datum_hz),
                trench_core.ctypes.byref(w)
            )
            out_words.append([int(w[0]), int(w[1]), int(w[2]), int(w[3]), 0xDFFF])
        else:
            bw_zero = min(19845.0, max(20.0, bw_hz * (10.0 ** (gain_db / 20.0))))
            r_zero = math.exp(-math.pi * bw_zero / datum_hz)
            g = trench_core.TrenchSectionGeometry(
                pole_type=1, pole_a=hz, pole_b=r_pole,
                zero_type=1, zero_a=hz, zero_b=r_zero,
                scale=1.0
            )
            w = (trench_core.ctypes.c_uint16 * 5)()
            trench_core._dll.trench_section_geometry_set(
                trench_core.ctypes.byref(g),
                trench_core.ctypes.c_double(datum_hz),
                trench_core.ctypes.byref(w)
            )
            out_words.append([int(w[i]) for i in range(5)])
    return out_words

class FrameLibrary:
    def __init__(self, root_dir: Optional[str] = None):
        if root_dir is None:
            root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
        self.root_dir = root_dir
        self.frames: List[Frame] = []
        self.vowel_marks: List[dict] = []
        self._load()

    def _load(self):
        self.frames.clear()
        self.vowel_marks.clear()
        fid = 0

        fx3_path = os.path.join(self.root_dir, "native", "app", "templates", "frames_x3.json")
        if os.path.isfile(fx3_path):
            with open(fx3_path, "r", encoding="utf-8") as f:
                data = json.load(f)
            for item in data.get("frames", []):
                b_name = item.get("body", "").replace("_", " ").title()
                c_name = item.get("corner", "")
                words = item.get("sections", [])
                root_hz = _find_root_hz(words, 44100.0)
                spark = _sparkline_for_words(words, 44100.0)
                frame = Frame(
                    id=fid,
                    kind="bank",
                    group="BANK",
                    name=f"{b_name} · {c_name}",
                    detail=item.get("type", "P2K"),
                    root_hz=root_hz,
                    words=words,
                    sparkline=spark
                )
                self.frames.append(frame)
                fid += 1

        pt_path = os.path.join(self.root_dir, "native", "app", "templates", "pole_templates_by_type.json")
        if os.path.isfile(pt_path):
            with open(pt_path, "r", encoding="utf-8") as f:
                data = json.load(f)
            for fam_key, fam_name in [("p2k_44100", "X3"), ("morpheus_39062_5", "MORPHEUS")]:
                for t_key, t_val in data.get(fam_key, {}).items():
                    states = t_val.get("states", [])
                    chosen = sorted(states, key=lambda s: s.get("bodies", 0), reverse=True)
                    filtered = []
                    for s in chosen:
                        close = any(abs(math.log2(o["hz"] / s["hz"])) < 0.25 for o in filtered)
                        if close:
                            continue
                        filtered.append(s)
                        if len(filtered) == 6:
                            break
                    if len(filtered) < 3:
                        continue
                    filtered.sort(key=lambda s: s["hz"])
                    rows = []
                    for i in range(len(filtered) - 1):
                        rows.append({"type": "EQ", "hz": filtered[i]["hz"], "bw_hz": filtered[i]["bw_hz"], "gain_db": 12.0})
                    rows.append({"type": "LP", "hz": filtered[-1]["hz"], "bw_hz": filtered[-1]["bw_hz"], "gain_db": 0.0})
                    words = _compile_type_rows(rows, 44100.0)
                    root_hz = _find_root_hz(words, 44100.0)
                    spark = _sparkline_for_words(words, 44100.0)
                    frame = Frame(
                        id=fid,
                        kind="type",
                        group="TYPES",
                        name=f"{fam_name} · {t_key}",
                        detail=f"{len(t_val.get('bodies', []))} bodies",
                        root_hz=root_hz,
                        words=words,
                        sparkline=spark
                    )
                    self.frames.append(frame)
                    fid += 1

            for l_key, l_val in data.get("ladders", {}).items():
                root_hz = float(l_val.get("default_root_hz", 64.3))
                ratios = l_val.get("ratios", [])
                bw_fracs = l_val.get("bw_fraction", [])
                z_ratios = l_val.get("zero_bw_ratio", [])
                rows = []
                for i in range(min(6, len(ratios))):
                    hz = root_hz * ratios[i]
                    bw_hz = hz * (bw_fracs[i] if i < len(bw_fracs) else 0.03)
                    z_ratio = z_ratios[i] if i < len(z_ratios) else 4.0
                    gain_db = 20.0 * math.log10(max(1.0, z_ratio))
                    rows.append({"type": "EQ", "hz": hz, "bw_hz": bw_hz, "gain_db": gain_db})
                words = _compile_type_rows(rows, 44100.0)
                root_hz = _find_root_hz(words, 44100.0)
                spark = _sparkline_for_words(words, 44100.0)
                frame = Frame(
                    id=fid,
                    kind="type",
                    group="TYPES",
                    name=f"LADDER · {l_key}",
                    detail=f"{len(ratios)} rungs",
                    root_hz=root_hz,
                    words=words,
                    sparkline=spark
                )
                self.frames.append(frame)
                fid += 1

        kfa_path = os.path.join(self.root_dir, "native", "app", "templates", "keyframes_from_audio.json")
        if os.path.isfile(kfa_path):
            with open(kfa_path, "r", encoding="utf-8") as f:
                data = json.load(f)
            for item in data.get("frames", []):
                group = item.get("group", "AUDIO")
                name = item.get("name", "")
                rows = item.get("rows", [])
                words = _compile_type_rows(rows, 44100.0)
                root_hz = _find_root_hz(words, 44100.0)
                spark = _sparkline_for_words(words, 44100.0)
                src = os.path.basename(item.get("source", ""))
                mode = item.get("mode", "")
                frame = Frame(
                    id=fid,
                    kind="keyframe",
                    group=group,
                    name=name,
                    detail=f"{mode} · {src}" if src else mode,
                    root_hz=root_hz,
                    words=words,
                    sparkline=spark
                )
                self.frames.append(frame)
                fid += 1

                if group == "VOWEL H95" and len(rows) >= 3:
                    self.vowel_marks.append({
                        "name": name,
                        "label": name.split()[0] if name else "",
                        "man": name.endswith(" man"),
                        "f1": rows[0]["hz"],
                        "f2": rows[1]["hz"],
                        "f3": rows[2]["hz"],
                    })

    def sorted_by_log_frequency(self, group: Optional[str] = None, search: str = "") -> List[Frame]:
        res = self.frames
        if group and group != "ALL":
            res = [f for f in res if f.group == group]
        if search:
            q = search.strip().lower()
            res = [f for f in res if q in f.name.lower() or q in f.detail.lower() or q in f.group.lower()]
        return sorted(res, key=lambda f: f.root_hz)

    def sorted_by_glide_distance(self, target_words: List[List[int]], group: Optional[str] = None, search: str = "") -> List[Tuple[Frame, float]]:
        target_roots = []
        for s in range(6):
            g = trench_core.TrenchSectionGeometry()
            w = (trench_core.ctypes.c_uint16 * 5)(*target_words[s])
            trench_core._dll.trench_section_geometry_get(trench_core.ctypes.byref(w), 44100.0, trench_core.ctypes.byref(g))
            if g.pole_type == 1 and g.pole_a >= 30.0:
                target_roots.append(g.pole_a)

        candidates = self.sorted_by_log_frequency(group, search)
        scored = []
        for f in candidates:
            f_roots = []
            for s in range(6):
                g = trench_core.TrenchSectionGeometry()
                w = (trench_core.ctypes.c_uint16 * 5)(*f.words[s])
                trench_core._dll.trench_section_geometry_get(trench_core.ctypes.byref(w), 44100.0, trench_core.ctypes.byref(g))
                if g.pole_type == 1 and g.pole_a >= 30.0:
                    f_roots.append(g.pole_a)
            if not target_roots or not f_roots:
                dist = abs(math.log2(f.root_hz / max(20.0, target_roots[0] if target_roots else 1000.0)))
            else:
                dist = 0.0
                terms = 0
                for tr in target_roots:
                    best = min(abs(math.log2(fr / tr)) for fr in f_roots)
                    dist += best * best
                    terms += 1
                dist = math.sqrt(dist / max(1, terms))
            scored.append((f, dist))
        scored.sort(key=lambda x: x[1])
        return scored

    def formants_at(self, f1: float, f2: float) -> Tuple[float, float, float, float]:
        log_f1 = math.log(max(1.0, f1))
        log_f2 = math.log(max(1.0, f2))
        w_sum = 0.0
        v_sum = 0.0
        for m in self.vowel_marks:
            if not m["man"]:
                continue
            dx = log_f1 - math.log(m["f1"])
            dy = log_f2 - math.log(m["f2"])
            reach = math.sqrt(dx * dx + dy * dy)
            if reach < 1e-9:
                return f1, f2, m["f3"], 1.4 * m["f3"]
            w = 1.0 / reach
            w_sum += w
            v_sum += w * m["f3"]
        f3 = v_sum / w_sum if w_sum > 0.0 else 2500.0
        f4 = 1.4 * f3
        return f1, f2, f3, f4

    def frame_from_vowel(self, f1: float, f2: float, f3: Optional[float] = None, f4: Optional[float] = None) -> Frame:
        if f3 is None or f4 is None:
            f1, f2, f3, f4 = self.formants_at(f1, f2)
        rows = [
            {"type": "EQ", "hz": f1, "bw_hz": 50.0 + f1 / 20.0, "gain_db": 14.0},
            {"type": "EQ", "hz": f2, "bw_hz": 50.0 + f2 / 20.0, "gain_db": 10.0},
            {"type": "EQ", "hz": f3, "bw_hz": 50.0 + f3 / 20.0, "gain_db": 6.0},
            {"type": "EQ", "hz": f4, "bw_hz": 50.0 + f4 / 20.0, "gain_db": 3.0},
        ]
        words = _compile_type_rows(rows, 44100.0)
        root_hz = _find_root_hz(words, 44100.0)
        spark = _sparkline_for_words(words, 44100.0)
        return Frame(
            id=-1,
            kind="vowel",
            group="VOWEL",
            name=f"Vowel {int(f1)}/{int(f2)}",
            detail=f"F1:{int(f1)} F2:{int(f2)} F3:{int(f3)}",
            root_hz=root_hz,
            words=words,
            sparkline=spark
        )

    def frame_from_audio(self, mono_samples: np.ndarray, sample_rate_hz: float, mode: str = "speech") -> Frame:
        if mode.lower() == "speech":
            poles = trench_core.speech_poles(mono_samples, sample_rate_hz, 6)
            rows = [{"type": "POLE", "hz": p[0], "bw_hz": p[1], "gain_db": 0.0} for p in poles]
        else:
            res = trench_core.audio_resonances(mono_samples, sample_rate_hz, 6)
            rows = [{"type": "EQ", "hz": r[0], "bw_hz": r[1], "gain_db": min(24.0, max(6.0, r[2]))} for r in res]
            if len(rows) > 0:
                rows[-1]["type"] = "LP"
        words = _compile_type_rows(rows, 44100.0)
        root_hz = _find_root_hz(words, 44100.0)
        spark = _sparkline_for_words(words, 44100.0)
        return Frame(
            id=-2,
            kind="audio",
            group="AUDIO",
            name=f"Audio {mode.upper()}",
            detail=f"{len(rows)} sections",
            root_hz=root_hz,
            words=words,
            sparkline=spark
        )
