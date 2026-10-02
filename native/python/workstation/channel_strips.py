import math
import os
import sys
from typing import List, Optional

from PySide6 import QtCore, QtGui, QtWidgets

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
import trench_core

class CompactStageWidget(QtWidgets.QFrame):
    stage_changed = QtCore.Signal(int, list)

    def __init__(self, stage_index: int, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self.stage_index = stage_index
        self.datum_hz = 44100.0
        self._words: List[int] = [0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF]

        self.setObjectName("compactStage")
        self.setStyleSheet("""
            #compactStage {
                background-color: #121215;
                border: 1px solid #1f1f26;
                border-radius: 3px;
                padding: 4px;
            }
            QLabel {
                color: #71717a;
                font-size: 9px;
            }
            QLabel#badge {
                font-weight: bold;
                font-size: 10px;
                color: #ffffff;
            }
            QLabel#val {
                color: #e4e4e7;
                font-family: 'Consolas', monospace;
                font-size: 10px;
            }
            QLabel#hex {
                color: #52525c;
                font-family: 'Consolas', monospace;
                font-size: 8px;
            }
        """)

        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)
        layout.setSpacing(2)

        hdr_row = QtWidgets.QHBoxLayout()
        tag = f"ST {stage_index + 1}"
        if stage_index == 0:
            tag += " · THROAT"
        elif stage_index == 5:
            tag += " · NOTCH"
        self.lbl_badge = QtWidgets.QLabel(tag, self)
        self.lbl_badge.setObjectName("badge")
        hdr_row.addWidget(self.lbl_badge)
        hdr_row.addStretch()
        layout.addLayout(hdr_row)

        self.lbl_pole = QtWidgets.QLabel("P: ---", self)
        self.lbl_pole.setObjectName("val")
        layout.addWidget(self.lbl_pole)

        self.lbl_zero = QtWidgets.QLabel("Z: ---", self)
        self.lbl_zero.setObjectName("val")
        layout.addWidget(self.lbl_zero)

        self.lbl_hex1 = QtWidgets.QLabel("0000 0000", self)
        self.lbl_hex1.setObjectName("hex")
        layout.addWidget(self.lbl_hex1)

        self.lbl_hex2 = QtWidgets.QLabel("0000 0000 0000", self)
        self.lbl_hex2.setObjectName("hex")
        layout.addWidget(self.lbl_hex2)

        layout.addStretch()

    def set_words(self, words: List[int], datum_hz: float = 44100.0):
        if len(words) != 5:
            return
        self._words = list(words)
        self.datum_hz = datum_hz

        geom = trench_core.TrenchSectionGeometry()
        w_arr = (trench_core.ctypes.c_uint16 * 5)(*words)
        trench_core._dll.trench_section_geometry_get(
            trench_core.ctypes.byref(w_arr),
            trench_core.ctypes.c_double(datum_hz),
            trench_core.ctypes.byref(geom)
        )

        p_hz = geom.pole_a if geom.pole_type == 1 else 0.0
        p_r = geom.pole_b if geom.pole_type == 1 else 0.0
        bw_p = -math.log(max(1e-6, min(0.9999, p_r))) / math.pi * datum_hz
        q_p = p_hz / max(1.0, bw_p)

        if p_hz >= 1000.0:
            p_str = f"P {p_hz/1000.0:.2f}k · r{p_r:.3f} · Q{q_p:.1f}"
        else:
            p_str = f"P {p_hz:.0f}Hz · r{p_r:.3f} · Q{q_p:.1f}"
        self.lbl_pole.setText(p_str)

        z_hz = geom.zero_a if geom.zero_type == 1 else 0.0
        z_r = geom.zero_b if geom.zero_type == 1 else 0.0
        if z_r > 0.001:
            if z_hz >= 1000.0:
                z_str = f"Z {z_hz/1000.0:.2f}k · r{z_r:.3f}"
            else:
                z_str = f"Z {z_hz:.0f}Hz · r{z_r:.3f}"
        else:
            z_str = "Z bypass"
        self.lbl_zero.setText(z_str)

        self.lbl_hex1.setText(f"{words[0]:04X} {words[1]:04X}")
        self.lbl_hex2.setText(f"{words[2]:04X} {words[3]:04X} {words[4]:04X}")

class ChannelStripsWidget(QtWidgets.QWidget):
    words_changed = QtCore.Signal(int, list)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self.strips: List[CompactStageWidget] = []

        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)

        for s in range(6):
            strip = CompactStageWidget(s, self)
            self.strips.append(strip)
            layout.addWidget(strip)

    def load_sections(self, sections: List[List[int]], datum_hz: float = 44100.0):
        for s in range(min(6, len(sections))):
            self.strips[s].set_words(sections[s], datum_hz)
