import math
import os
import sys
from typing import List, Optional, Tuple
import numpy as np

from PySide6 import QtCore, QtGui, QtWidgets

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
import frames
import trench_core

class LatentGridWidget(QtWidgets.QWidget):
    position_changed = QtCore.Signal(float, float)
    corner_moved = QtCore.Signal(int, float, float)
    corner_selected = QtCore.Signal(int)
    frame_assigned = QtCore.Signal(int, object)
    audition_requested = QtCore.Signal(bool)

    F1_TOP = 180.0
    F1_BOTTOM = 950.0
    F2_TOP_LEFT = 2800.0
    F2_TOP_RIGHT = 500.0
    F2_BOTTOM_LEFT = 1800.0
    F2_BOTTOM_RIGHT = 750.0
    SLANT = 0.26

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self.setMouseTracking(True)
        self.setMinimumSize(420, 360)

        self.f1: float = 500.0
        self.f2: float = 1500.0
        self.hover_frame: Optional[frames.Frame] = None
        self.hover_frame_pos: Optional[QtCore.QPointF] = None
        self.active_corner_index: int = 0
        self.dragging_corner: Optional[int] = None
        self.dragging_reticle: bool = False
        self.is_pointer_down: bool = False

        self.corner_coords: List[Tuple[float, float]] = [
            (270.0, 2290.0),
            (730.0, 1090.0),
            (390.0, 1990.0),
            (640.0, 1190.0),
        ]

        self.morph: float = 0.5
        self.q: float = 0.5

        self.library = frames.FrameLibrary()
        self.frame_points: List[Tuple[frames.Frame, float, float]] = []
        self._index_library_frames()

    def _index_library_frames(self):
        self.frame_points.clear()
        for f in self.library.frames:
            poles = []
            for s in range(min(6, len(f.words))):
                g = trench_core.TrenchSectionGeometry()
                w_arr = (trench_core.ctypes.c_uint16 * 5)(*f.words[s])
                trench_core._dll.trench_section_geometry_get(
                    trench_core.ctypes.byref(w_arr),
                    44100.0,
                    trench_core.ctypes.byref(g),
                )
                if g.pole_type == 1 and g.pole_a >= 30.0:
                    poles.append(g.pole_a)
            poles.sort()
            if len(poles) >= 2:
                f1_val = float(np.clip(poles[0], 150.0, 1200.0))
                f2_val = float(np.clip(poles[1], 400.0, 3500.0))
                self.frame_points.append((f, f1_val, f2_val))
            elif len(poles) == 1:
                f1_val = float(np.clip(poles[0], 150.0, 1200.0))
                f2_val = float(np.clip(poles[0] * 2.0, 400.0, 3500.0))
                self.frame_points.append((f, f1_val, f2_val))

    def _f2_left(self, v: float) -> float:
        return math.exp(
            math.log(self.F2_TOP_LEFT)
            + v * (math.log(self.F2_BOTTOM_LEFT) - math.log(self.F2_TOP_LEFT))
        )

    def _f2_right(self, v: float) -> float:
        return math.exp(
            math.log(self.F2_TOP_RIGHT)
            + v * (math.log(self.F2_BOTTOM_RIGHT) - math.log(self.F2_TOP_RIGHT))
        )

    def chart_rect(self) -> QtCore.QRectF:
        side = 32.0
        top = 28.0
        bottom = 24.0
        w = max(100.0, float(self.width()) - 2.0 * side)
        h = max(100.0, float(self.height()) - top - bottom)
        return QtCore.QRectF(side, top, w, h)

    def point_for(self, f1: float, f2: float) -> QtCore.QPointF:
        chart = self.chart_rect()
        f1_clamped = max(100.0, min(1400.0, f1))
        f2_clamped = max(350.0, min(3600.0, f2))

        v = (math.log(f1_clamped) - math.log(self.F1_TOP)) / (
            math.log(self.F1_BOTTOM) - math.log(self.F1_TOP)
        )
        f2_l = self._f2_left(v)
        f2_r = self._f2_right(v)

        u = (math.log(f2_clamped) - math.log(f2_l)) / (
            math.log(f2_r) - math.log(f2_l)
        )
        left = chart.left() + chart.width() * self.SLANT * v
        x = left + u * (chart.right() - left)
        y = chart.top() + v * chart.height()
        return QtCore.QPointF(x, y)

    def formants_at(self, pt: QtCore.QPointF) -> Tuple[float, float]:
        chart = self.chart_rect()
        v = (pt.y() - chart.top()) / chart.height()
        v = max(-0.15, min(1.15, v))

        left = chart.left() + chart.width() * self.SLANT * v
        span = max(10.0, chart.right() - left)
        u = (pt.x() - left) / span
        u = max(-0.15, min(1.15, u))

        f1 = self.F1_TOP * ((self.F1_BOTTOM / self.F1_TOP) ** v)
        f2_l = self._f2_left(v)
        f2_r = self._f2_right(v)
        f2 = math.exp(math.log(f2_l) + u * (math.log(f2_r) - math.log(f2_l)))
        return float(np.clip(f1, 120.0, 1200.0)), float(
            np.clip(f2, 400.0, 3600.0)
        )

    def formants_to_mq(self, f1: float, f2: float) -> Tuple[float, float]:
        c0, c1, c2, c3 = self.corner_coords
        best_m, best_q = 0.5, 0.5
        best_dist = float("inf")
        for m_t in (0.0, 0.25, 0.5, 0.75, 1.0):
            for q_t in (0.0, 0.25, 0.5, 0.75, 1.0):
                f1_t = (
                    (1.0 - m_t) * (1.0 - q_t) * c0[0]
                    + m_t * (1.0 - q_t) * c1[0]
                    + (1.0 - m_t) * q_t * c2[0]
                    + m_t * q_t * c3[0]
                )
                f2_t = (
                    (1.0 - m_t) * (1.0 - q_t) * c0[1]
                    + m_t * (1.0 - q_t) * c1[1]
                    + (1.0 - m_t) * q_t * c2[1]
                    + m_t * q_t * c3[1]
                )
                d = (f1_t - f1) ** 2 + (f2_t - f2) ** 2
                if d < best_dist:
                    best_dist = d
                    best_m, best_q = m_t, q_t

        m, q_val = best_m, best_q
        for _ in range(8):
            f1_t = (
                (1.0 - m) * (1.0 - q_val) * c0[0]
                + m * (1.0 - q_val) * c1[0]
                + (1.0 - m) * q_val * c2[0]
                + m * q_val * c3[0]
            )
            f2_t = (
                (1.0 - m) * (1.0 - q_val) * c0[1]
                + m * (1.0 - q_val) * c1[1]
                + (1.0 - m) * q_val * c2[1]
                + m * q_val * c3[1]
            )
            df1_dm = -(1.0 - q_val) * c0[0] + (1.0 - q_val) * c1[0] - q_val * c2[0] + q_val * c3[0]
            df1_dq = -(1.0 - m) * c0[0] - m * c1[0] + (1.0 - m) * c2[0] + m * c3[0]
            df2_dm = -(1.0 - q_val) * c0[1] + (1.0 - q_val) * c1[1] - q_val * c2[1] + q_val * c3[1]
            df2_dq = -(1.0 - m) * c0[1] - m * c1[1] + (1.0 - m) * c2[1] + m * c3[1]
            det = df1_dm * df2_dq - df1_dq * df2_dm
            if abs(det) < 1e-6:
                break
            err1 = f1 - f1_t
            err2 = f2 - f2_t
            dm = (df2_dq * err1 - df1_dq * err2) / det
            dq = (-df2_dm * err1 + df1_dm * err2) / det
            m = max(0.0, min(1.0, m + dm))
            q_val = max(0.0, min(1.0, q_val + dq))

        return float(m), float(q_val)

    def set_position(self, morph: float, q: float):
        self.morph = max(0.0, min(1.0, morph))
        self.q = max(0.0, min(1.0, q))

        c0 = self.corner_coords[0]
        c1 = self.corner_coords[1]
        c2 = self.corner_coords[2]
        c3 = self.corner_coords[3]

        m, q_val = self.morph, self.q
        self.f1 = (
            (1.0 - m) * (1.0 - q_val) * c0[0]
            + m * (1.0 - q_val) * c1[0]
            + (1.0 - m) * q_val * c2[0]
            + m * q_val * c3[0]
        )
        self.f2 = (
            (1.0 - m) * (1.0 - q_val) * c0[1]
            + m * (1.0 - q_val) * c1[1]
            + (1.0 - m) * q_val * c2[1]
            + m * q_val * c3[1]
        )
        self.update()

    def set_corner_coord(self, corner_idx: int, f1: float, f2: float):
        if 0 <= corner_idx < 4:
            self.corner_coords[corner_idx] = (f1, f2)
            self.set_position(self.morph, self.q)

    def mousePressEvent(self, event: QtGui.QMouseEvent):
        if event.button() == QtCore.Qt.MouseButton.LeftButton:
            self.is_pointer_down = True
            pos = event.position()

            hit_corner = None
            for i, c in enumerate(self.corner_coords):
                c_pt = self.point_for(c[0], c[1])
                if math.hypot(c_pt.x() - pos.x(), c_pt.y() - pos.y()) <= 15.0:
                    hit_corner = i
                    break

            if hit_corner is not None:
                self.dragging_corner = hit_corner
                self.active_corner_index = hit_corner
                self.corner_selected.emit(hit_corner)
                self.update()
                event.accept()
                return

            cur_pt = self.point_for(self.f1, self.f2)
            if math.hypot(cur_pt.x() - pos.x(), cur_pt.y() - pos.y()) <= 16.0:
                self.dragging_reticle = True
                self.audition_requested.emit(True)
                self.update()
                event.accept()
                return

            hit_frame = None
            for f, f1_v, f2_v in self.frame_points:
                pt = self.point_for(f1_v, f2_v)
                if math.hypot(pt.x() - pos.x(), pt.y() - pos.y()) <= 10.0:
                    hit_frame = f
                    break

            if hit_frame is not None:
                self.frame_assigned.emit(self.active_corner_index, hit_frame)
                self.update()
                event.accept()
                return

            f1, f2 = self.formants_at(pos)
            m, q_val = self.formants_to_mq(f1, f2)
            self.set_position(m, q_val)
            self.position_changed.emit(m, q_val)
            self.dragging_reticle = True
            self.audition_requested.emit(True)
            self.update()
            event.accept()

    def mouseMoveEvent(self, event: QtGui.QMouseEvent):
        pos = event.position()
        if event.buttons() & QtCore.Qt.MouseButton.LeftButton:
            f1, f2 = self.formants_at(pos)
            if self.dragging_corner is not None:
                self.set_corner_coord(self.dragging_corner, f1, f2)
                self.corner_moved.emit(self.dragging_corner, f1, f2)
            elif self.dragging_reticle:
                m, q_val = self.formants_to_mq(f1, f2)
                self.set_position(m, q_val)
                self.position_changed.emit(m, q_val)
            self.update()

        nearest = None
        nearest_pt = None
        min_dist = 14.0
        for f, f1_v, f2_v in self.frame_points:
            pt = self.point_for(f1_v, f2_v)
            d = math.hypot(pt.x() - pos.x(), pt.y() - pos.y())
            if d < min_dist:
                min_dist = d
                nearest = f
                nearest_pt = pt

        if nearest != self.hover_frame:
            self.hover_frame = nearest
            self.hover_frame_pos = nearest_pt
            self.update()

        event.accept()

    def mouseReleaseEvent(self, event: QtGui.QMouseEvent):
        self.is_pointer_down = False
        self.dragging_corner = None
        self.dragging_reticle = False
        self.audition_requested.emit(False)
        self.update()
        event.accept()

    def paintEvent(self, event: QtGui.QPaintEvent):
        painter = QtGui.QPainter(self)
        painter.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing)

        w = float(self.width())
        h = float(self.height())
        painter.fillRect(0, 0, int(w), int(h), QtGui.QColor("#08080a"))

        chart = self.chart_rect()

        p_tl = self.point_for(self.F1_TOP, self.F2_TOP_LEFT)
        p_tr = self.point_for(self.F1_TOP, self.F2_TOP_RIGHT)
        p_br = self.point_for(self.F1_BOTTOM, self.F2_BOTTOM_RIGHT)
        p_bl = self.point_for(self.F1_BOTTOM, self.F2_BOTTOM_LEFT)

        poly = QtGui.QPolygonF([p_tl, p_tr, p_br, p_bl])
        painter.setBrush(QtGui.QColor("#101014"))
        painter.setPen(QtGui.QPen(QtGui.QColor("#1e1e26"), 1.0))
        painter.drawPolygon(poly)

        painter.setPen(
            QtGui.QPen(
                QtGui.QColor("#181820"), 0.8, QtCore.Qt.PenStyle.DashLine
            )
        )
        font = QtGui.QFont("Segoe UI", 7)
        painter.setFont(font)

        for f1_grid in [250.0, 400.0, 550.0, 700.0, 850.0]:
            pt_l = self.point_for(f1_grid, self.F2_TOP_LEFT)
            pt_r = self.point_for(f1_grid, self.F2_TOP_RIGHT)
            painter.drawLine(pt_l, pt_r)
            painter.setPen(QtGui.QColor("#40404c"))
            painter.drawText(
                int(chart.right() + 4),
                int(pt_r.y() + 3),
                f"{int(f1_grid)}",
            )
            painter.setPen(
                QtGui.QPen(
                    QtGui.QColor("#181820"), 0.8, QtCore.Qt.PenStyle.DashLine
                )
            )

        for f2_grid in [600.0, 1000.0, 1500.0, 2000.0, 2500.0]:
            pt_t = self.point_for(self.F1_TOP, f2_grid)
            pt_b = self.point_for(self.F1_BOTTOM, f2_grid)
            painter.drawLine(pt_t, pt_b)
            painter.setPen(QtGui.QColor("#40404c"))
            painter.drawText(
                int(pt_t.x() - 10),
                int(chart.top() - 6),
                f"{int(f2_grid)}",
            )
            painter.setPen(
                QtGui.QPen(
                    QtGui.QColor("#181820"), 0.8, QtCore.Qt.PenStyle.DashLine
                )
            )

        painter.setPen(QtGui.QPen(QtGui.QColor("#242430"), 1.0))
        for f, f1_v, f2_v in self.frame_points:
            pt = self.point_for(f1_v, f2_v)
            painter.drawPoint(pt)

        painter.setFont(QtGui.QFont("Segoe UI", 7))
        for m_v in self.library.vowel_marks:
            if not m_v.get("man", True):
                continue
            pt = self.point_for(m_v["f1"], m_v["f2"])
            painter.setBrush(QtGui.QColor("#52525c"))
            painter.setPen(QtCore.Qt.PenStyle.NoPen)
            painter.drawEllipse(pt, 1.8, 1.8)
            lbl = m_v.get("label", "")
            painter.setPen(QtGui.QColor("#454550"))
            painter.drawText(int(pt.x() + 4), int(pt.y() - 3), lbl)

        c_pts = [self.point_for(c[0], c[1]) for c in self.corner_coords]

        quad_poly = QtGui.QPolygonF([c_pts[0], c_pts[1], c_pts[3], c_pts[2]])
        painter.setBrush(QtGui.QColor(255, 255, 255, 7))
        painter.setPen(QtGui.QPen(QtGui.QColor("#2b2b36"), 1.0))
        painter.drawPolygon(quad_poly)

        painter.setPen(
            QtGui.QPen(QtGui.QColor("#71717a"), 1.2, QtCore.Qt.PenStyle.DotLine)
        )
        painter.drawLine(c_pts[0], c_pts[1])
        painter.drawLine(c_pts[2], c_pts[3])

        painter.setPen(
            QtGui.QPen(QtGui.QColor("#454550"), 1.0, QtCore.Qt.PenStyle.DotLine)
        )
        painter.drawLine(c_pts[0], c_pts[2])
        painter.drawLine(c_pts[1], c_pts[3])

        for i, pt in enumerate(c_pts):
            is_active = i == self.active_corner_index
            color = QtGui.QColor("#ffffff") if is_active else QtGui.QColor("#71717a")
            bg = QtGui.QColor("#27272e") if is_active else QtGui.QColor("#141418")

            painter.setBrush(bg)
            painter.setPen(QtGui.QPen(color, 1.4))
            painter.drawEllipse(pt, 5.0, 5.0)

            painter.setFont(QtGui.QFont("Segoe UI", 8, QtGui.QFont.Weight.Bold))
            painter.setPen(color)
            painter.drawText(int(pt.x() + 8), int(pt.y() + 4), f"C{i}")

        if self.hover_frame and self.hover_frame_pos:
            painter.setBrush(QtGui.QColor("#ffffff"))
            painter.setPen(QtCore.Qt.PenStyle.NoPen)
            painter.drawEllipse(self.hover_frame_pos, 2.5, 2.5)

        cur_pt = self.point_for(self.f1, self.f2)
        painter.setPen(QtGui.QPen(QtGui.QColor("#ffffff"), 1.6))
        painter.drawLine(
            QtCore.QPointF(cur_pt.x() - 8, cur_pt.y()),
            QtCore.QPointF(cur_pt.x() + 8, cur_pt.y()),
        )
        painter.drawLine(
            QtCore.QPointF(cur_pt.x(), cur_pt.y() - 8),
            QtCore.QPointF(cur_pt.x(), cur_pt.y() + 8),
        )
        painter.setBrush(QtCore.Qt.BrushStyle.NoBrush)
        painter.setPen(QtGui.QPen(QtGui.QColor("#ffffff"), 1.0))
        painter.drawEllipse(cur_pt, 5.0, 5.0)

        painter.setFont(QtGui.QFont("Segoe UI", 8))
        if self.hover_frame:
            painter.setPen(QtGui.QColor("#ffffff"))
            painter.drawText(
                12,
                int(h - 8),
                f"{self.hover_frame.name} ({self.hover_frame.root_hz:.0f} Hz)",
            )
        else:
            painter.setPen(QtGui.QColor("#52525c"))
            painter.drawText(
                12,
                int(h - 8),
                f"F1 {self.f1:.0f} Hz   F2 {self.f2:.0f} Hz",
            )
