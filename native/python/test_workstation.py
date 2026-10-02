import os
import sys
import numpy as np

from PySide6 import QtCore, QtGui, QtWidgets

sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
import anchor_space
import frames
import trench_core
from workstation.latent_grid import LatentGridWidget
from workstation.cascade_plot import CascadePlotWidget
from workstation_app import TrenchWorkstationWindow

def test_latent_grid_math():
    app = QtWidgets.QApplication.instance() or QtWidgets.QApplication(sys.argv)
    widget = LatentGridWidget()
    widget.resize(600, 400)

    assert len(widget.frame_points) == 304, f"Expected 304 frames, got {len(widget.frame_points)}"
    for f, f1, f2 in widget.frame_points:
        assert np.isfinite(f1) and np.isfinite(f2)
        pt = widget.point_for(f1, f2)
        assert np.isfinite(pt.x()) and np.isfinite(pt.y())

    for f1_in, f2_in in [(300.0, 2200.0), (500.0, 1500.0), (750.0, 950.0)]:
        pt = widget.point_for(f1_in, f2_in)
        f1_out, f2_out = widget.formants_at(pt)
        assert abs(f1_in - f1_out) < 5.0, f"F1 roundtrip error: {f1_in} vs {f1_out}"
        assert abs(f2_in - f2_out) < 25.0, f"F2 roundtrip error: {f2_in} vs {f2_out}"

    widget.set_position(0.25, 0.75)
    assert np.isfinite(widget.f1) and np.isfinite(widget.f2)

    m_inv, q_inv = widget.formants_to_mq(widget.f1, widget.f2)
    assert abs(m_inv - 0.25) < 0.05, f"Bilinear inversion error for morph: {m_inv}"
    assert abs(q_inv - 0.75) < 0.05, f"Bilinear inversion error for q: {q_inv}"
    print("PASS: LatentGridWidget logarithmic formant math, bilinear inversion, and 304-frame indexing verified")

def test_cascade_plot_evaluation():
    app = QtWidgets.QApplication.instance() or QtWidgets.QApplication(sys.argv)
    preset_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "plugin", "presets", "p2k", "talking_hedz.body240"))
    body = trench_core.Body.from_file(preset_path)

    plot = CascadePlotWidget()
    bqs = body.cascade(0.5, 0.5, 0.0, 44100.0, 44100.0)
    plot.set_cascade(bqs, 44100.0)

    assert len(plot.db_response) == plot.NUM_POINTS
    assert np.all(np.isfinite(plot.db_response))

    expected_db = trench_core.cascade_response_db(bqs, plot.freqs_hz, 44100.0)
    max_err = np.max(np.abs(plot.db_response - expected_db))
    assert max_err < 1e-6, f"Cascade plot response mismatch: {max_err}"
    print(f"PASS: CascadePlotWidget response matches serial cascade evaluation to {max_err:.8f} dB")

def test_audition_slot_roundtrip():
    test_slot = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "plugin", "patterns", "test_audition_slot.body240"))
    preset_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "plugin", "presets", "p2k", "talking_hedz.body240"))

    space = anchor_space.AnchorSpace(is_3d=False, datum_hz=44100.0)
    space.load_preset_body(preset_path)
    space.write_audition_slot(test_slot)

    assert os.path.isfile(test_slot)
    with open(test_slot, "rb") as f:
        data = f.read()

    assert len(data) == 240
    b_reloaded = trench_core.Body.from_legacy_bytes(data)

    for c in range(4):
        for s in range(6):
            assert space.nodes[c].words[s] == b_reloaded.get_words(c, s)

    if os.path.isfile(test_slot):
        os.remove(test_slot)
    print("PASS: Audition slot roundtrips 240 bytes bit-exact")

def test_stripped_workstation_window():
    app = QtWidgets.QApplication.instance() or QtWidgets.QApplication(sys.argv)
    win = TrenchWorkstationWindow()
    win.setAttribute(QtCore.Qt.WidgetAttribute.WA_DontShowOnScreen, True)
    win.resize(1360, 780)
    win.show()

    assert win.combo_presets.count() > 0
    assert len(win.corner_btns) == 4

    win.slider_morph.setValue(720)
    win.slider_q.setValue(310)
    assert abs(win.anchor.morph - 0.72) < 0.005
    assert abs(win.anchor.q - 0.31) < 0.005

    win._on_corner_clicked(2)
    assert win.anchor.selected_corner == 2
    assert win.latent_grid.active_corner_index == 2

    win.btn_traverse.setChecked(True)
    assert win._traverse_timer.isActive()
    win._on_traverse_step()
    win.btn_traverse.setChecked(False)
    assert not win._traverse_timer.isActive()

    win.close()
    print("PASS: Stripped workstation window loads presets and responds to controls")

if __name__ == "__main__":
    test_latent_grid_math()
    test_cascade_plot_evaluation()
    test_audition_slot_roundtrip()
    test_stripped_workstation_window()
    print("ALL STRIPPED WORKSTATION TESTS PASSED")
