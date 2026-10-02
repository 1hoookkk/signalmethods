import math
import os
import sys
from typing import List, Optional, Tuple
import numpy as np

from PySide6 import QtCore, QtGui, QtWidgets

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
import trench_core

class CascadePlotWidget(QtWidgets.QWidget):
    NUM_POINTS = 160
    GRID_FREQS = [50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0]
    GRID_DBS = [-20.0, -10.0, 0.0, 10.0, 20.0]

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self.setMinimumSize(360, 240)

        self.freqs_hz = np.geomspace(20.0, 20000.0, self.NUM_POINTS)
        self.db_response: np.ndarray = np.zeros(self.NUM_POINTS, dtype=np.float64)
        self.formants: List[float] = [500.0, 1500.0, 2500.0, 3500.0]

    def set_cascade(self, biquads: List[List[float]], datum_hz: float = 44100.0):
        self.db_response = trench_core.cascade_response_db(biquads, self.freqs_hz, datum_hz)
        self.update()

    def set_formants(self, f1: float, f2: float, f3: float, f4: float):
        self.formants = [f1, f2, f3, f4]
        self.update()

    def _freq_to_x(self, hz: float, w: float) -> float:
        min_log = math.log10(20.0)
        max_log = math.log10(20000.0)
        cur_log = math.log10(max(20.0, min(20000.0, hz)))
        return float((cur_log - min_log) / (max_log - min_log)) * w

    def _db_to_y(self, db: float, h: float) -> float:
        clamped = max(-30.0, min(30.0, db))
        return float((30.0 - clamped) / 60.0) * h

    def paintEvent(self, event: QtGui.QPaintEvent):
        painter = QtGui.QPainter(self)
        painter.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing)

        w = float(self.width())
        h = float(self.height()) - 24.0

        painter.fillRect(0, 0, int(w), int(h + 24.0), QtGui.QColor("#0c0c0e"))

        painter.setPen(QtGui.QPen(QtGui.QColor("#18181f"), 0.8))
        for d in self.GRID_DBS:
            y = self._db_to_y(d, h)
            painter.drawLine(0, int(y), int(w), int(y))

        for f in self.GRID_FREQS:
            x = self._freq_to_x(f, w)
            painter.drawLine(int(x), 0, int(x), int(h))

        y0 = self._db_to_y(0.0, h)
        painter.setPen(QtGui.QPen(QtGui.QColor("#2d2d38"), 1.0))
        painter.drawLine(0, int(y0), int(w), int(y0))

        font = QtGui.QFont("Segoe UI", 7)
        painter.setFont(font)
        painter.setPen(QtGui.QColor("#52525c"))
        painter.drawText(6, int(y0 - 3), "0 dB")
        painter.drawText(6, int(self._db_to_y(20.0, h) - 3), "+20")
        painter.drawText(6, int(self._db_to_y(-20.0, h) - 3), "-20")

        for f in self.GRID_FREQS:
            lbl = f"{int(f/1000)}k" if f >= 1000.0 else f"{int(f)}"
            x = self._freq_to_x(f, w)
            painter.drawText(int(x + 3), int(h - 4), lbl)

        path = QtGui.QPainterPath()
        for i in range(self.NUM_POINTS):
            x = (float(i) / float(self.NUM_POINTS - 1)) * w
            y = self._db_to_y(self.db_response[i], h)
            if i == 0:
                path.moveTo(x, y)
            else:
                path.lineTo(x, y)

        pen = QtGui.QPen(QtGui.QColor("#ffffff"), 1.5)
        painter.setPen(pen)
        painter.drawPath(path)

        for f in self.formants[:4]:
            fx = self._freq_to_x(f, w)
            idx = int(np.clip(round(np.interp(f, self.freqs_hz, np.arange(self.NUM_POINTS))), 0, self.NUM_POINTS - 1))
            fy = self._db_to_y(self.db_response[idx], h)

            painter.setBrush(QtGui.QColor("#ffffff"))
            painter.setPen(QtGui.QPen(QtGui.QColor("#0c0c0e"), 1.0))
            painter.drawEllipse(QtCore.QPointF(fx, fy), 3.0, 3.0)

        readout_y = int(h + 16.0)
        painter.setFont(QtGui.QFont("Segoe UI", 8))
        painter.setPen(QtGui.QColor("#71717a"))
        f_str = f"F1 {self.formants[0]:.0f}  F2 {self.formants[1]:.0f}  F3 {self.formants[2]:.0f}  F4 {self.formants[3]:.0f}"
        painter.drawText(8, readout_y, f_str)
