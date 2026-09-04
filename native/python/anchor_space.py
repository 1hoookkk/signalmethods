from dataclasses import dataclass, field
import math
import os
import sys
from typing import List, Optional, Tuple
import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import trench_core
import frames

CORNER_NAMES_2D = ["M0 Q0", "M1 Q0", "M0 Q100", "M1 Q100"]
CORNER_NAMES_3D = [
    "M0 Q0 Z0", "M1 Q0 Z0", "M0 Q100 Z0", "M1 Q100 Z0",
    "M0 Q0 Z1", "M1 Q0 Z1", "M0 Q100 Z1", "M1 Q100 Z1"
]

@dataclass
class AnchorNode:
    corner_index: int
    name: str
    frame_name: str
    words: List[List[int]]
    root_hz: float
    locked: bool = True

    @property
    def sparkline(self) -> List[float]:
        return frames._sparkline_for_words(self.words)

class AnchorSpace:
    def __init__(self, is_3d: bool = False, datum_hz: float = 44100.0):
        self.is_3d = is_3d
        self.datum_hz = datum_hz
        self.selected_corner = 0
        self.morph = 0.0
        self.q = 0.0
        self.z = 0.0
        self.corner_count = 8 if is_3d else 4
        self.nodes: List[AnchorNode] = []
        self._init_default_nodes()

    def _init_default_nodes(self):
        self.nodes.clear()
        names = CORNER_NAMES_3D if self.is_3d else CORNER_NAMES_2D
        identity_sec = [0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF]
        for i in range(self.corner_count):
            words = [list(identity_sec) for _ in range(6)]
            node = AnchorNode(
                corner_index=i,
                name=names[i],
                frame_name="Identity",
                words=words,
                root_hz=1000.0,
                locked=True
            )
            self.nodes.append(node)

    def load_preset_body(self, path: str):
        with open(path, "rb") as f:
            data = f.read()
        body = trench_core.Body.from_bytes(data)
        for i in range(self.corner_count):
            words = [body.get_words(i, s) for s in range(6)]
            root_hz = frames._find_root_hz(words, self.datum_hz)
            self.nodes[i].words = words
            self.nodes[i].frame_name = f"Corner {i}"
            self.nodes[i].root_hz = root_hz

    def set_corner_frame(self, corner_idx: int, frame_obj: frames.Frame, auto_anchor: bool = True):
        if not (0 <= corner_idx < self.corner_count):
            return
        words = [list(sec) for sec in frame_obj.words[:6]]
        self.nodes[corner_idx].words = words
        self.nodes[corner_idx].frame_name = frame_obj.name
        self.nodes[corner_idx].root_hz = frame_obj.root_hz
        if auto_anchor:
            self.anchor_corner(corner_idx, include_q=True)

    def get_pole_hz_at(self, corner_idx: int, slot: int) -> Optional[float]:
        if not (0 <= corner_idx < self.corner_count) or not (0 <= slot < 6):
            return None
        sec = self.nodes[corner_idx].words[slot]
        geom = trench_core.TrenchSectionGeometry()
        w = (trench_core.ctypes.c_uint16 * 5)(*sec)
        trench_core._dll.trench_section_geometry_get(
            trench_core.ctypes.byref(w),
            trench_core.ctypes.c_double(self.datum_hz),
            trench_core.ctypes.byref(geom)
        )
        if geom.pole_type == 1 and geom.pole_a >= 20.0:
            return geom.pole_a
        return None

    def anchor_corner(self, corner: int, include_q: bool = True):
        if not (0 <= corner < self.corner_count):
            return
        partner_morph = corner ^ 1
        partner_q = corner ^ 2 if include_q else None

        incoming_hz = [self.get_pole_hz_at(corner, s) for s in range(6)]
        partner_morph_hz = [self.get_pole_hz_at(partner_morph, s) for s in range(6)] if partner_morph < self.corner_count else [None] * 6
        partner_q_hz = [self.get_pole_hz_at(partner_q, s) for s in range(6)] if (partner_q is not None and partner_q < self.corner_count) else [None] * 6

        row_taken = [False] * 6
        slot_taken = [False] * 6
        plan = [6] * 6

        for slot in range(6):
            pm = partner_morph_hz[slot]
            pq = partner_q_hz[slot]
            if pm is None and pq is None:
                continue
            best_row = -1
            best_cost = 999.0
            for row in range(6):
                if row_taken[row] or incoming_hz[row] is None:
                    continue
                hz = incoming_hz[row]
                total = 0.0
                terms = 0
                if pm is not None:
                    total += abs(math.log2(hz / pm))
                    terms += 1
                if pq is not None:
                    total += abs(math.log2(hz / pq))
                    terms += 1
                if terms > 0:
                    cost = total / terms
                    if cost <= 1.5 and cost < best_cost:
                        best_cost = cost
                        best_row = row
            if best_row >= 0:
                plan[best_row] = slot
                row_taken[best_row] = True
                slot_taken[slot] = True

        free_slots = [s for s in range(6) if not slot_taken[s]]
        loose = [r for r in range(6) if not row_taken[r] and incoming_hz[r] is not None]
        loose.sort(key=lambda r: incoming_hz[r] or 0.0)
        next_free = 0
        for r in loose:
            plan[r] = free_slots[next_free]
            next_free += 1
        for r in range(6):
            if plan[r] == 6:
                plan[r] = free_slots[next_free]
                next_free += 1

        reordered = [[0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF] for _ in range(6)]
        for r in range(6):
            target_slot = plan[r]
            reordered[target_slot] = list(self.nodes[corner].words[r])
        self.nodes[corner].words = reordered
        self.nodes[corner].root_hz = frames._find_root_hz(reordered, self.datum_hz)

    def anchor_square(self):
        for c in range(self.corner_count):
            self.anchor_corner(c, include_q=True)

    def to_body(self) -> trench_core.Body:
        body = trench_core.Body.from_legacy_bytes(bytes([0] * 240))
        identity = [0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF]
        for c in range(min(4, self.corner_count)):
            for s in range(6):
                w = self.nodes[c].words[s]
                body.set_words(c, s, w)
                body.set_words(c + 4, s, w)
            body.set_words(c, 6, identity)
            body.set_words(c + 4, 6, identity)
        return body

    def cascade_at_position(self, morph: float, q: float, z: float = 0.0, host_hz: float = 44100.0) -> List[List[float]]:
        body = self.to_body()
        return body.cascade(morph, q, z, self.datum_hz, host_hz)

    def get_glide_trajectories(self) -> List[List[Tuple[float, float]]]:
        trajectories: List[List[Tuple[float, float]]] = []
        for s in range(6):
            c0_hz = self.get_pole_hz_at(0, s) or 1000.0
            c1_hz = self.get_pole_hz_at(1, s) or c0_hz
            c2_hz = self.get_pole_hz_at(2, s) or c0_hz
            c3_hz = self.get_pole_hz_at(3, s) or c1_hz
            trajectories.append([
                (c0_hz, 0.0),
                (c1_hz, 1.0),
                (c2_hz, 0.0),
                (c3_hz, 1.0)
            ])
        return trajectories

    def export_body240(self) -> bytes:
        return self.to_body().to_legacy_bytes()

    def write_audition_slot(self, dest_path: Optional[str] = None):
        if dest_path is None:
            root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
            dest_path = os.path.join(root, "plugin", "patterns", "audition_slot.body240")
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        raw = self.export_body240()
        with open(dest_path, "wb") as f:
            f.write(raw)
