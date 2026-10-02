import os
import sys
from typing import List, Optional

from PySide6 import QtCore, QtGui, QtWidgets

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
import anchor_space

class CornerPillButton(QtWidgets.QPushButton):
    def __init__(self, corner_index: int, name: str, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self.corner_index = corner_index
        self.corner_name = name
        self.frame_name = "talking_hedz"
        self.root_hz = 500.0
        self.is_active = False

        self.setCheckable(True)
        self.setMinimumHeight(38)
        self.setCursor(QtCore.Qt.CursorShape.PointingHandCursor)
        self.update_style()

    def update_info(self, frame_name: str, root_hz: float):
        self.frame_name = frame_name
        self.root_hz = root_hz
        self.update_style()

    def set_selected(self, active: bool):
        self.is_active = active
        self.setChecked(active)
        self.update_style()

    def update_style(self):
        border = "#ffffff" if self.is_active else "#222227"
        bg = "#222228" if self.is_active else "#121215"
        text_c = "#ffffff" if self.is_active else "#71717a"

        self.setStyleSheet(f"""
            QPushButton {{
                background-color: {bg};
                border: 1px solid {border};
                border-radius: 3px;
                padding: 2px 4px;
                text-align: left;
                color: {text_c};
                font-size: 9px;
            }}
            QPushButton:hover {{
                border: 1px solid #71717a;
            }}
        """)
        short_name = self.frame_name.split(" · ")[0][:14]
        self.setText(f"C{self.corner_index} [{self.corner_name}]\n{short_name}")

class HypercubePadWidget(QtWidgets.QWidget):
    corner_selected = QtCore.Signal(int)
    morph_changed = QtCore.Signal(float)
    q_changed = QtCore.Signal(float)
    bite_changed = QtCore.Signal(float)
    audition_toggled = QtCore.Signal(bool)
    sweep_toggled = QtCore.Signal(bool)
    anchor_corner_requested = QtCore.Signal(int)
    copy_morph_requested = QtCore.Signal(int)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self.active_corner: int = 0
        self.corner_buttons: List[CornerPillButton] = []

        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)

        grid = QtWidgets.QGridLayout()
        grid.setSpacing(4)

        names = ["M0 Q0", "M1 Q0", "M0 Q1", "M1 Q1"]
        positions = [(0, 0), (0, 1), (1, 0), (1, 1)]

        for i in range(4):
            btn = CornerPillButton(i, names[i], self)
            r, c = positions[i]
            grid.addWidget(btn, r, c)
            self.corner_buttons.append(btn)
            btn.clicked.connect(lambda checked=False, idx=i: self._on_corner_clicked(idx))

        layout.addLayout(grid)

        ctrl_box = QtWidgets.QFrame(self)
        ctrl_box.setStyleSheet("""
            QFrame {
                background-color: #121215;
                border: 1px solid #1f1f26;
                border-radius: 3px;
                padding: 4px;
            }
            QLabel {
                color: #71717a;
                font-size: 9px;
                font-weight: bold;
            }
            QLabel#val {
                color: #ffffff;
                font-family: 'Consolas', monospace;
                font-size: 10px;
            }
            QSlider::groove:horizontal {
                height: 4px;
                background: #1c1c22;
                border-radius: 2px;
            }
            QSlider::handle:horizontal {
                background: #ffffff;
                width: 10px;
                margin: -3px 0;
                border-radius: 5px;
            }
            QSlider::handle:horizontal:hover {
                background: #e2e8f0;
            }
        """)
        cb_layout = QtWidgets.QVBoxLayout(ctrl_box)
        cb_layout.setContentsMargins(4, 4, 4, 4)
        cb_layout.setSpacing(4)

        m_row = QtWidgets.QHBoxLayout()
        m_row.addWidget(QtWidgets.QLabel("MORPH", self))
        self.lbl_m_val = QtWidgets.QLabel("0.500", self)
        self.lbl_m_val.setObjectName("val")
        m_row.addWidget(self.lbl_m_val)
        m_row.addStretch()
        cb_layout.addLayout(m_row)

        self.slider_morph = QtWidgets.QSlider(QtCore.Qt.Orientation.Horizontal, self)
        self.slider_morph.setRange(0, 1000)
        self.slider_morph.setValue(500)
        self.slider_morph.valueChanged.connect(self._on_morph_slider)
        cb_layout.addWidget(self.slider_morph)

        q_row = QtWidgets.QHBoxLayout()
        q_row.addWidget(QtWidgets.QLabel("Q", self))
        self.lbl_q_val = QtWidgets.QLabel("0.500", self)
        self.lbl_q_val.setObjectName("val")
        q_row.addWidget(self.lbl_q_val)
        q_row.addStretch()
        cb_layout.addLayout(q_row)

        self.slider_q = QtWidgets.QSlider(QtCore.Qt.Orientation.Horizontal, self)
        self.slider_q.setRange(0, 1000)
        self.slider_q.setValue(500)
        self.slider_q.valueChanged.connect(self._on_q_slider)
        cb_layout.addWidget(self.slider_q)

        b_row = QtWidgets.QHBoxLayout()
        b_row.addWidget(QtWidgets.QLabel("BITE", self))
        self.lbl_b_val = QtWidgets.QLabel("0.18", self)
        self.lbl_b_val.setObjectName("val")
        b_row.addWidget(self.lbl_b_val)
        b_row.addStretch()
        cb_layout.addLayout(b_row)

        self.slider_bite = QtWidgets.QSlider(QtCore.Qt.Orientation.Horizontal, self)
        self.slider_bite.setRange(0, 500)
        self.slider_bite.setValue(180)
        self.slider_bite.valueChanged.connect(self._on_bite_slider)
        cb_layout.addWidget(self.slider_bite)

        btn_row = QtWidgets.QHBoxLayout()
        btn_row.setSpacing(4)

        self.btn_audition = QtWidgets.QPushButton("AUDITION", self)
        self.btn_audition.setCheckable(True)
        self.btn_audition.setStyleSheet("""
            QPushButton {
                background-color: #1a1a20;
                border: 1px solid #282834;
                color: #71717a;
                font-size: 9px;
                font-weight: bold;
                padding: 4px;
                border-radius: 2px;
            }
            QPushButton:checked {
                background-color: #ffffff;
                color: #0c0c0e;
                border: 1px solid #ffffff;
            }
        """)
        self.btn_audition.toggled.connect(self.audition_toggled.emit)
        btn_row.addWidget(self.btn_audition)

        self.btn_sweep = QtWidgets.QPushButton("TRAVERSE LFO", self)
        self.btn_sweep.setCheckable(True)
        self.btn_sweep.setStyleSheet("""
            QPushButton {
                background-color: #1a1a20;
                border: 1px solid #282834;
                color: #71717a;
                font-size: 9px;
                font-weight: bold;
                padding: 4px;
                border-radius: 2px;
            }
            QPushButton:checked {
                background-color: #ffffff;
                color: #0c0c0e;
                border: 1px solid #ffffff;
            }
        """)
        self.btn_sweep.toggled.connect(self.sweep_toggled.emit)
        btn_row.addWidget(self.btn_sweep)

        self.btn_anchor = QtWidgets.QPushButton("ANCHOR", self)
        self.btn_anchor.setStyleSheet("""
            QPushButton {
                background-color: #1a1a20;
                border: 1px solid #282834;
                color: #a1a1aa;
                font-size: 9px;
                font-weight: bold;
                padding: 4px;
                border-radius: 2px;
            }
            QPushButton:hover {
                border: 1px solid #ffffff;
                color: #ffffff;
            }
        """)
        self.btn_anchor.clicked.connect(lambda: self.anchor_corner_requested.emit(self.active_corner))
        btn_row.addWidget(self.btn_anchor)

        cb_layout.addLayout(btn_row)
        layout.addWidget(ctrl_box)

        self.set_active_corner(0)

    def _on_corner_clicked(self, idx: int):
        self.set_active_corner(idx)
        self.corner_selected.emit(idx)

    def set_active_corner(self, idx: int):
        self.active_corner = idx
        for i, b in enumerate(self.corner_buttons):
            b.set_selected(i == idx)

    def update_corner_info(self, idx: int, name: str, root_hz: float):
        if 0 <= idx < len(self.corner_buttons):
            self.corner_buttons[idx].update_info(name, root_hz)

    def _on_morph_slider(self, val: int):
        f_val = val / 1000.0
        self.lbl_m_val.setText(f"{f_val:.3f}")
        self.morph_changed.emit(f_val)

    def _on_q_slider(self, val: int):
        f_val = val / 1000.0
        self.lbl_q_val.setText(f"{f_val:.3f}")
        self.q_changed.emit(f_val)

    def _on_bite_slider(self, val: int):
        f_val = val / 1000.0
        self.lbl_b_val.setText(f"{f_val:.2f}")
        self.bite_changed.emit(f_val)

    def set_position(self, morph: float, q: float):
        self.slider_morph.blockSignals(True)
        self.slider_q.blockSignals(True)
        self.slider_morph.setValue(int(morph * 1000.0))
        self.slider_q.setValue(int(q * 1000.0))
        self.lbl_m_val.setText(f"{morph:.3f}")
        self.lbl_q_val.setText(f"{q:.3f}")
        self.slider_morph.blockSignals(False)
        self.slider_q.blockSignals(False)
