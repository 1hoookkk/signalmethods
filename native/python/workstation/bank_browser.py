import os
import sys
from typing import List, Optional

from PySide6 import QtCore, QtGui, QtWidgets

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
import frames

class SparklineWidget(QtWidgets.QWidget):
    def __init__(self, points: List[float], parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self.points = points
        self.setFixedSize(80, 24)

    def set_points(self, points: List[float]):
        self.points = points
        self.update()

    def paintEvent(self, event: QtGui.QPaintEvent):
        painter = QtGui.QPainter(self)
        painter.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing)

        w = float(self.width())
        h = float(self.height())

        painter.fillRect(self.rect(), QtGui.QColor("#101014"))
        painter.setPen(QtGui.QColor("#1f1f26"))
        painter.drawRect(0, 0, int(w) - 1, int(h) - 1)

        if not self.points or len(self.points) < 2:
            return

        path = QtGui.QPainterPath()
        n = len(self.points)
        for i, val in enumerate(self.points):
            clamped = max(-30.0, min(30.0, float(val)))
            x = (i / float(n - 1)) * w
            y = ((30.0 - clamped) / 60.0) * h
            if i == 0:
                path.moveTo(x, y)
            else:
                path.lineTo(x, y)

        pen = QtGui.QPen(QtGui.QColor("#ffffff"), 1.2)
        painter.setPen(pen)
        painter.drawPath(path)

class FrameRowWidget(QtWidgets.QFrame):
    land_clicked = QtCore.Signal(object)
    hear_clicked = QtCore.Signal(object)

    def __init__(self, frame_obj: frames.Frame, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self.frame_obj = frame_obj

        self.setObjectName("frameRow")
        self.setStyleSheet("""
            #frameRow {
                background-color: #121215;
                border: 1px solid #1c1c22;
                border-radius: 2px;
            }
            #frameRow:hover {
                border: 1px solid #3f3f4e;
                background-color: #16161a;
            }
            QLabel#title {
                color: #ffffff;
                font-size: 10px;
            }
            QLabel#meta {
                color: #71717a;
                font-size: 8px;
            }
            QPushButton {
                background-color: #1a1a20;
                border: 1px solid #282834;
                color: #a1a1aa;
                font-size: 8px;
                font-weight: bold;
                border-radius: 2px;
                padding: 2px 4px;
            }
            QPushButton:hover {
                background-color: #ffffff;
                color: #0c0c0e;
                border: 1px solid #ffffff;
            }
        """)

        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(4, 2, 4, 2)
        layout.setSpacing(4)

        info_box = QtWidgets.QVBoxLayout()
        info_box.setSpacing(1)

        lbl_title = QtWidgets.QLabel(frame_obj.name, self)
        lbl_title.setObjectName("title")
        info_box.addWidget(lbl_title)

        lbl_meta = QtWidgets.QLabel(f"{frame_obj.group} · {frame_obj.root_hz:.0f} Hz", self)
        lbl_meta.setObjectName("meta")
        info_box.addWidget(lbl_meta)

        layout.addLayout(info_box, stretch=2)

        self.sparkline = SparklineWidget(frame_obj.sparkline, self)
        layout.addWidget(self.sparkline)

        btns_box = QtWidgets.QHBoxLayout()
        btns_box.setSpacing(2)

        btn_hear = QtWidgets.QPushButton("H", self)
        btn_hear.setToolTip("Hear one-shot preview")
        btn_hear.setFixedWidth(20)
        btn_hear.clicked.connect(lambda: self.hear_clicked.emit(self.frame_obj))
        btns_box.addWidget(btn_hear)

        btn_land = QtWidgets.QPushButton("LAND", self)
        btn_land.clicked.connect(lambda: self.land_clicked.emit(self.frame_obj))
        btns_box.addWidget(btn_land)

        layout.addLayout(btns_box)

class BankBrowserWidget(QtWidgets.QWidget):
    frame_land_requested = QtCore.Signal(object)
    frame_hear_requested = QtCore.Signal(object)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self.library = frames.FrameLibrary()
        self.current_group: str = "ALL"
        self.target_words: Optional[List[List[int]]] = None

        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(3)

        ctrl_row = QtWidgets.QHBoxLayout()
        ctrl_row.setSpacing(3)

        self.search_edit = QtWidgets.QLineEdit(self)
        self.search_edit.setPlaceholderText("Filter 304 frames...")
        self.search_edit.setStyleSheet("""
            QLineEdit {
                background-color: #121215;
                border: 1px solid #1f1f26;
                border-radius: 2px;
                color: #ffffff;
                padding: 3px 5px;
                font-size: 9px;
            }
            QLineEdit:focus {
                border: 1px solid #ffffff;
            }
        """)
        self.search_edit.textChanged.connect(self.refresh_list)
        ctrl_row.addWidget(self.search_edit)

        self.lbl_count = QtWidgets.QLabel("304", self)
        self.lbl_count.setStyleSheet("color: #52525c; font-size: 9px;")
        ctrl_row.addWidget(self.lbl_count)
        layout.addLayout(ctrl_row)

        self.list_widget = QtWidgets.QListWidget(self)
        self.list_widget.setStyleSheet("""
            QListWidget {
                background-color: #0c0c0e;
                border: 1px solid #1a1a20;
                border-radius: 3px;
                padding: 2px;
            }
            QScrollBar:vertical {
                background: #0c0c0e;
                width: 6px;
            }
            QScrollBar::handle:vertical {
                background: #24242c;
                border-radius: 3px;
            }
        """)
        layout.addWidget(self.list_widget)

        self.refresh_list()

    def set_target_words(self, words: List[List[int]]):
        self.target_words = words

    def refresh_list(self):
        self.list_widget.clear()
        query = self.search_edit.text().strip()

        items = self.library.sorted_by_log_frequency(self.current_group, query)
        self.lbl_count.setText(str(len(items)))

        for frame_obj in items:
            list_item = QtWidgets.QListWidgetItem(self.list_widget)
            list_item.setSizeHint(QtCore.QSize(200, 36))
            row = FrameRowWidget(frame_obj, self.list_widget)
            row.land_clicked.connect(self.frame_land_requested.emit)
            row.hear_clicked.connect(self.frame_hear_requested.emit)
            self.list_widget.setItemWidget(list_item, row)
