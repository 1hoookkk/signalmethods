from __future__ import annotations

import math
import sys

import numpy as np

from .audio import Audition
from .board import BAND, HIT_PX, lane_y, set_pair, y_to_bw
from .core import (GRID, LANES, OUT_ROOT, Sheet, bw_from_radius, closest_approach,
                   equal_steps, fit_endpoint, probe, response_db, section_db,
                   travel_ruler)
from .corpus import blend, by_distance, factory_bodies
from .theme import AMBER, BAD, BG, DIM, OX, PANEL, RULE, TEXT

# every section panel on one fixed scale, like the cascade inspector plate:
# a big slot must read as big against a small one.
SECTION_DB = 100.0


def run_ui(sheet: Sheet, source: bytes) -> int:
    import pyqtgraph as pg
    from PySide6 import QtCore, QtWidgets

    pg.setConfigOptions(background=BG, foreground=TEXT, antialias=True)
    app = QtWidgets.QApplication.instance() or QtWidgets.QApplication(sys.argv)

    handles: list[dict] = []

    class Board(pg.PlotWidget):
        def __init__(self):
            super().__init__()
            self.setMouseEnabled(False, False)
            self.hideButtons()
            self.setLogMode(True, False)
            self.setXRange(math.log10(20.0), math.log10(20000.0))
            self.setYRange(-0.25, sheet.lanes - 1 + BAND + 0.25)
            self.showGrid(x=True, y=False, alpha=0.25)
            self.getAxis("left").setTicks(
                [[(sheet.lanes - 1 - i + BAND / 2, f"S{i+1}") for i in range(sheet.lanes)]])
            self.setLabel("bottom", "Hz")
            self.grabbed = None
            self.setCursor(QtCore.Qt.CursorShape.PointingHandCursor)

        def _pick(self, pos):
            vb = self.plotItem.vb
            p = vb.mapSceneToView(pos)
            best = None
            for h in handles:
                if not h["live"]:
                    continue
                dx = (math.log10(max(h["hz"], 1.0)) - p.x())
                dy = h["y"] - p.y()
                sx = dx / max(vb.viewRect().width(), 1e-9) * self.width()
                sy = dy / max(vb.viewRect().height(), 1e-9) * self.height()
                d = math.hypot(sx, sy)
                if d < HIT_PX and (best is None or d < best[0]):
                    best = (d, h)
            return best[1] if best else None

        def mousePressEvent(self, e):
            self.grabbed = self._pick(e.position())
            e.accept()

        def mouseMoveEvent(self, e):
            if self.grabbed is None:
                e.accept()
                return
            p = self.plotItem.vb.mapSceneToView(e.position())
            h = self.grabbed
            bad = set_pair(h["row"], h["kind"], 10.0 ** p.x(),
                           y_to_bw(h["lane"], p.y(), sheet.lanes), sheet.rate)
            if not pending.isActive():
                pending.start()
            if bad:
                status.setText(bad)
                status.setStyleSheet(f"color:{BAD};font-weight:700")
            e.accept()

        def mouseReleaseEvent(self, e):
            self.grabbed = None
            e.accept()

    win = QtWidgets.QWidget()
    win.setWindowTitle("word sheet")
    win.setStyleSheet(
        f"QWidget{{background:{BG};color:{TEXT};font-size:11px}}"
        f"QComboBox{{background:{PANEL};border:1px solid {RULE};"
        "min-height:24px;padding:1px 4px}"
        f"QPushButton{{background:{PANEL};border:1px solid {RULE};"
        "min-height:26px;padding:2px 12px}")
    outer = QtWidgets.QHBoxLayout(win)
    outer.setContentsMargins(10, 8, 10, 8)
    outer.setSpacing(10)

    left = QtWidgets.QVBoxLayout()
    left.setSpacing(6)

    board_row = QtWidgets.QHBoxLayout()
    locks = QtWidgets.QVBoxLayout()
    locks.setSpacing(0)
    locks.addSpacing(6)
    lockboxes = []
    for lane in range(sheet.lanes):
        c = QtWidgets.QCheckBox()
        c.setChecked(sheet.locked[lane])
        c.setToolTip("this lane does not move")
        lockboxes.append(c)
        locks.addWidget(c, 1)
    locks.addSpacing(24)
    board_row.addLayout(locks)
    board = Board()
    board.setMinimumSize(620, 300)
    board_row.addWidget(board, 1)
    left.addLayout(board_row, 1)

    facts = by_distance(source, factory_bodies())
    bar = QtWidgets.QHBoxLayout()
    pick = QtWidgets.QComboBox()
    pick.addItems([n for n, _ in facts])
    slide = QtWidgets.QSlider(QtCore.Qt.Orientation.Horizontal)
    slide.setRange(0, 100)
    tlab = QtWidgets.QLabel("0%")
    tlab.setMinimumWidth(32)
    bar.addWidget(QtWidgets.QLabel("pull toward"))
    bar.addWidget(pick, 2)
    bar.addWidget(slide, 1)
    bar.addWidget(tlab)
    left.addLayout(bar)

    tgt = QtWidgets.QComboBox()
    tgt.addItems(["(no target)"] + [n for n, _ in facts])
    fit0 = QtWidgets.QPushButton("fit MORPH 0")
    fit1 = QtWidgets.QPushButton("fit MORPH 100")
    fit0.setStyleSheet(f"color:{OX};font-weight:700")
    fit1.setStyleSheet(f"color:{AMBER};font-weight:700")
    fitbar = QtWidgets.QHBoxLayout()
    fitbar.addWidget(QtWidgets.QLabel("fit toward"))
    fitbar.addWidget(tgt, 2)
    fitbar.addWidget(fit0)
    fitbar.addWidget(fit1)
    left.addLayout(fitbar)

    # Re-pairing: which root at MORPH 0 travels to which root at MORPH 100.
    # Both endpoints stay bit-identical; only the middle of the wheel moves.
    pa = QtWidgets.QComboBox()
    pb = QtWidgets.QComboBox()
    for c in (pa, pb):
        c.addItems([f"S{i+1}" for i in range(sheet.lanes)])
    pb.setCurrentIndex(min(1, sheet.lanes - 1))
    swap = QtWidgets.QPushButton("swap far ends")
    byfreq = QtWidgets.QPushButton("pair by distance")
    byfreq.setToolTip("shortest total move in ARMAdillo space - angle AND radius")
    pairbar = QtWidgets.QHBoxLayout()
    pairbar.addWidget(QtWidgets.QLabel("re-pair"))
    pairbar.addWidget(pa, 1)
    pairbar.addWidget(pb, 1)
    pairbar.addWidget(swap)
    pairbar.addWidget(byfreq)
    left.addLayout(pairbar)
    meet = QtWidgets.QLabel("")
    meet.setStyleSheet(f"color:{AMBER}")
    left.addWidget(meet)

    ear = Audition()
    play = QtWidgets.QPushButton("play")
    play.setCheckable(True)
    wheel = QtWidgets.QSlider(QtCore.Qt.Orientation.Horizontal)
    wheel.setRange(0, 100)
    mlab = QtWidgets.QLabel("MORPH 0")
    mlab.setMinimumWidth(70)
    mlab.setStyleSheet(f"color:{OX};font-weight:700")
    even = QtWidgets.QCheckBox("even steps")
    even.setToolTip("space the wheel by how much the sound actually moves, "
                    "not by the stored word value (Martens, PALETTE 1985)")
    hear = QtWidgets.QHBoxLayout()
    hear.addWidget(play)
    hear.addWidget(wheel, 1)
    hear.addWidget(mlab)
    hear.addWidget(even)
    left.addLayout(hear)
    ruler = QtWidgets.QLabel("")
    ruler.setStyleSheet(f"color:{DIM}")
    left.addWidget(ruler)

    god = QtWidgets.QGridLayout()
    god.setHorizontalSpacing(10)
    switches = {}
    for i, name in enumerate(("agc", "saturation", "dc block")):
        cb = QtWidgets.QCheckBox(name)
        cb.setChecked(True)
        cb.toggled.connect(lambda on, n=name: ear.set_switch(n, on))
        god.addWidget(cb, 0, i)
        switches[name] = cb
    amounts = {}
    for i, (name, lo, hi, init) in enumerate(
            (("input", 0, 100, 0), ("bite", 0, 100, 0), ("agc drive", 0, 400, 180))):
        lab = QtWidgets.QLabel(name)
        lab.setStyleSheet(f"color:{DIM}")
        sl = QtWidgets.QSlider(QtCore.Qt.Orientation.Horizontal)
        sl.setRange(lo, hi)
        sl.setValue(init)
        val = QtWidgets.QLabel(f"{init / 100:.2f}")
        val.setMinimumWidth(34)
        sl.valueChanged.connect(
            lambda v, n=name, w=val: (ear.set_amount(n, v / 100.0),
                                      w.setText(f"{v / 100:.2f}")))
        god.addWidget(lab, i + 1, 0)
        god.addWidget(sl, i + 1, 1)
        god.addWidget(val, i + 1, 2)
        amounts[name] = sl
    left.addLayout(god)

    status = QtWidgets.QLabel("")
    status.setMinimumHeight(18)
    left.addWidget(status)
    save = QtWidgets.QPushButton("write body240")
    left.addWidget(save)
    outer.addLayout(left, 0)

    right = QtWidgets.QVBoxLayout()
    right.setSpacing(4)
    whole = pg.PlotWidget()
    whole.setLogMode(True, False)
    whole.setLabel("bottom", "Hz")
    whole.setLabel("left", "dB")
    whole.showGrid(x=True, y=True, alpha=0.25)
    whole.setMinimumHeight(300)
    whole.setMouseEnabled(False, False)
    whole.hideButtons()
    right.addWidget(whole, 1)

    strip = QtWidgets.QHBoxLayout()
    strip.setSpacing(6)
    secs, caps = [], []
    for i in range(sheet.lanes):
        col = QtWidgets.QVBoxLayout()
        col.setSpacing(1)
        t = QtWidgets.QLabel(f"S{i+1}")
        t.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        t.setStyleSheet(f"color:{DIM};font-weight:700")
        col.addWidget(t)
        p = pg.PlotWidget()
        p.setLogMode(True, False)
        p.setMouseEnabled(False, False)
        p.hideButtons()
        p.hideAxis("bottom")
        p.setFixedHeight(210)
        p.getViewBox().setBorder(pg.mkPen(RULE))
        col.addWidget(p)
        cap = QtWidgets.QLabel("")
        cap.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        cap.setStyleSheet(f"color:{DIM}")
        col.addWidget(cap)
        strip.addLayout(col)
        secs.append(p)
        caps.append(cap)
    right.addLayout(strip, 0)
    outer.addLayout(right, 1)

    curve_items = {m: whole.plot([], [], pen=pg.mkPen(c, width=2))
                   for m, c in ((0.0, OX), (1.0, AMBER))}
    ghost_items = [whole.plot([], [], pen=pg.mkPen(RULE, width=1))
                   for _ in range(2)]
    sec_items = {(i, m): secs[i].plot([], [], pen=pg.mkPen(c, width=1.6))
                 for i in range(sheet.lanes) for m, c in ((0.0, OX), (1.0, AMBER))}
    ghost_cache: dict = {}

    def apply_blend() -> None:
        t = slide.value() / 100.0
        tlab.setText(f"{slide.value()}%")
        body = blend(source, facts[pick.currentIndex()][1], t) if t > 0 \
            else source
        fresh = Sheet.from_body(body, sheet.rate)
        for lane in range(sheet.lanes):
            for end in (0, 1):
                sheet.rows[end][lane].w = list(fresh.rows[end][lane].w)
        redraw()

    def redraw() -> None:
        handles.clear()
        for it in list(board.listDataItems()):
            board.removeItem(it)
        for lane in range(sheet.lanes):
            sheet.locked[lane] = lockboxes[lane].isChecked()
        for lane in range(sheet.lanes):
            for end, colour in ((0, OX), (1, AMBER)):
                live = end == 0 or not sheet.locked[lane]
                row = sheet.row(lane, end)
                for kind, getter in ((1, row.pole), (0, row.zero)):
                    g = getter(sheet.rate)
                    if not g:
                        continue
                    y = lane_y(lane, bw_from_radius(g[1], sheet.rate), sheet.lanes)
                    handles.append({"lane": lane, "end": end, "kind": kind,
                                    "row": row, "hz": g[0], "y": y,
                                    "live": live})
                    board.plot([g[0]], [y], pen=None, symbol="o",
                               symbolSize=13 if live else 9,
                               symbolPen=pg.mkPen(colour, width=2),
                               symbolBrush=(colour if kind else None))
        for lane in range(sheet.lanes):
            board.addItem(pg.InfiniteLine(pos=sheet.lanes - 1 - lane, angle=0,
                                          pen=pg.mkPen(RULE, width=1)))
        body = sheet.body()
        curves: dict[int, list] = {i: [] for i in range(sheet.lanes)}
        lo = hi = 0.0
        refused = []
        ti = tgt.currentIndex()
        for k, morph in enumerate((0.0, 1.0)):
            if ti <= 0:
                ghost_items[k].setData([], [])
                continue
            key = (ti, morph)
            if key not in ghost_cache:
                ghost_cache[key] = response_db(facts[ti - 1][1], morph, sheet.rate)
            g = ghost_cache[key]
            ghost_items[k].setData(GRID, g if g is not None else [])
        for morph, colour in ((0.0, OX), (1.0, AMBER)):
            cs = probe(body, morph, sheet.rate)
            if cs is None:
                refused.append(int(morph * 100))
                continue
            tot = np.zeros_like(GRID)
            for i, c in enumerate(cs):
                d = section_db(c, sheet.rate)
                tot = tot + d
                curves[i].append((d, colour))
            curve_items[morph].setData(GRID, tot)
            lo, hi = min(lo, float(tot.min())), max(hi, float(tot.max()))
        if lo != hi:
            whole.setYRange(lo - 5, hi + 5)
        ear.load(body)
        # every section on ONE fixed scale, so a big slot reads as big against a
        # small one. Auto-ranging each panel hides exactly that comparison.
        for i, p in enumerate(secs):
            for d, colour in curves[i]:
                m = 0.0 if colour == OX else 1.0
                sec_items[(i, m)].setData(GRID, d)
            p.setYRange(-SECTION_DB, SECTION_DB)
            def geom(end):
                pl = sheet.row(i, end).pole(sheet.rate)
                ze = sheet.row(i, end).zero(sheet.rate)
                if not pl or not ze or pl[0] <= 0:
                    return "-"
                return (f"pole {pl[0]:,.0f} Hz r{pl[1]:.3f}\n"
                        f"zero {ze[0]:,.0f} Hz r{ze[1]:.3f}")
            caps[i].setText(geom(0) if sheet.locked[i] else f"{geom(0)}\n{geom(1)}")
        pos, frac = travel_ruler(body, sheet.rate)
        steps["pos"] = equal_steps(body, sheet.rate, 9)
        half = float(np.interp(0.5, pos, frac)) * 100.0 if pos else 50.0
        ruler.setText(f"MORPH 50 is {half:.0f}% of the way through the change"
                      + ("" if 45 <= half <= 55 else "  — the wheel is uneven"))
        st, mm, hz, la, lb = closest_approach(body, sheet.rate)
        meet.setText(f"S{la} and S{lb} pass {st:.1f} st apart at MORPH {mm*100:.0f}"
                     f"  ·  {hz:,.0f} Hz" if st < 6.0 else
                     f"closest pass {st:.1f} st — nothing meets")
        if refused:
            status.setText("engine refused MORPH "
                           + " and ".join(str(m) for m in refused))
            status.setStyleSheet(f"color:{BAD};font-weight:700")
        else:
            status.setText("")
            status.setStyleSheet(f"color:{DIM}")

    def do_fit(end: int) -> None:
        ti = tgt.currentIndex()
        if ti <= 0:
            status.setText("pick a target to fit toward")
            status.setStyleSheet(f"color:{BAD};font-weight:700")
            return
        goal = response_db(facts[ti - 1][1], float(end), sheet.rate)
        if goal is None:
            status.setText("that target refuses to decode at this endpoint")
            status.setStyleSheet(f"color:{BAD};font-weight:700")
            return
        msg = fit_endpoint(sheet, end, goal, sheet.rate)
        redraw()
        status.setText(msg)
        status.setStyleSheet(f"color:{TEXT};font-weight:700")

    def write() -> None:
        out = OUT_ROOT / "bodies" / "candidates" / "word_sheet.body240"
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_bytes(sheet.body())
        status.setText(f"wrote {out.name}")
        status.setStyleSheet(f"color:{TEXT};font-weight:700")

    steps: dict = {"pos": None}

    def wheel_morph(v: int) -> float:
        # with "even steps" on, the slider walks equal amounts of CHANGE
        if not even.isChecked() or steps["pos"] is None:
            return v / 100.0
        return float(np.interp(v / 100.0,
                               np.linspace(0.0, 1.0, len(steps["pos"])),
                               steps["pos"]))

    def on_wheel(v: int) -> None:
        m = wheel_morph(v)
        ear.morph = m
        mlab.setText(f"MORPH {m*100:3.0f}")
        mlab.setStyleSheet(f"color:{OX if m < 0.5 else AMBER};font-weight:700")

    def on_play(down: bool) -> None:
        if down:
            err = ear.start()
            if err:
                play.setChecked(False)
                status.setText(err)
                status.setStyleSheet(f"color:{BAD};font-weight:700")
                return
            play.setText("stop")
        else:
            ear.stop()
            play.setText("play")

    for c in lockboxes:
        c.stateChanged.connect(redraw)
    def do_swap() -> None:
        sheet.swap_pairing(pa.currentIndex(), pb.currentIndex())
        redraw()

    def do_byfreq() -> None:
        sheet.pair_by_distance()
        redraw()

    even.toggled.connect(lambda _: on_wheel(wheel.value()))
    swap.clicked.connect(do_swap)
    byfreq.clicked.connect(do_byfreq)
    fit0.clicked.connect(lambda: do_fit(0))
    fit1.clicked.connect(lambda: do_fit(1))
    tgt.currentIndexChanged.connect(redraw)
    slide.valueChanged.connect(apply_blend)
    pick.currentIndexChanged.connect(apply_blend)
    wheel.valueChanged.connect(on_wheel)
    play.toggled.connect(on_play)
    save.clicked.connect(write)
    app.aboutToQuit.connect(ear.close)

    pending = QtCore.QTimer()
    pending.setSingleShot(True)
    pending.setInterval(16)
    pending.timeout.connect(redraw)

    QtCore.QTimer.singleShot(0, redraw)
    win.resize(1500, 650)
    win.show()
    return app.exec()
