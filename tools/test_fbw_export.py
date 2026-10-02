import math
import os
import sys
import tempfile

import numpy as np
from scipy import signal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fbw_export as fx


def read_rows_like_the_app(path):
    poles, zeros = [], []
    for line in open(path, encoding="utf-8"):
        stripped = line.lstrip(" \t\r")
        if not stripped.strip():
            continue
        if stripped[0] in "#;*":
            continue
        cells = stripped.replace(",", " ").split()
        values = [float(c) for c in cells]
        assert len(values) in (2, 4), line
        assert all(math.isfinite(v) and v > 0.0 for v in values), line
        poles.append((values[0], values[1]))
        zeros.append((values[2], values[3]) if len(values) == 4 else None)
    return poles, zeros


def test_butterworth_lowpass_carries_its_pole_and_drops_the_nyquist_zeros():
    rows, report = fx.rows_from_design("butter", "lowpass", 2, [1000.0])
    assert len(rows) == 1
    z, p, _ = signal.iirfilter(2, 1000.0 / 22050.0, btype="lowpass", ftype="butter", output="zpk")
    pole = p[np.imag(p) > 0][0]
    assert abs(rows[0][0][0] - np.angle(pole) / (2 * np.pi) * 44100.0) < 1e-6
    assert abs(rows[0][0][1] - fx.bandwidth_hz(abs(pole), 44100.0)) < 1e-6
    assert rows[0][1] is None
    assert report.real_roots == 2


def test_elliptic_bandstop_zeros_sit_on_the_circle_at_the_floor_width():
    rows, report = fx.rows_from_design("ellip", "bandstop", 4, [800.0, 1200.0], rp=1.0, rs=40.0)
    assert 1 <= len(rows) <= 6
    assert all(zero is not None for _, zero in rows)
    assert all(abs(zero[1] - fx.MIN_BW_HZ) < 1e-9 for _, zero in rows)


def test_coefficients_give_one_row_with_both_widths():
    rp, rz, theta = 0.9, 0.95, 2 * math.pi * 1500.0 / 44100.0
    a = [1.0, -2 * rp * math.cos(theta), rp * rp]
    b = [1.0, -2 * rz * math.cos(theta), rz * rz]
    rows, report = fx.rows_from_ba(b, a)
    assert len(rows) == 1
    (pole, zero) = rows[0]
    assert abs(pole[0] - 1500.0) < 1e-6
    assert abs(zero[0] - 1500.0) < 1e-6
    assert abs(pole[1] - fx.bandwidth_hz(rp, 44100.0)) < 1e-6
    assert abs(zero[1] - fx.bandwidth_hz(rz, 44100.0)) < 1e-6


def test_zero_outside_the_circle_is_reflected_inside():
    theta = 2 * math.pi * 3000.0 / 44100.0
    z = [2.0 * np.exp(1j * theta), 2.0 * np.exp(-1j * theta)]
    p = [0.9 * np.exp(1j * theta), 0.9 * np.exp(-1j * theta)]
    rows, report = fx.rows_from_zpk(z, p)
    assert report.reflected == 1
    assert abs(rows[0][1][1] - fx.bandwidth_hz(0.5, 44100.0)) < 1e-6


def test_comb_ladder_spacing_matches_the_factory_idiom():
    rows, _ = fx.rows_from_comb(514.0, 1047.0, 6, 120.0)
    assert len(rows) == 6
    poles = [pole[0] for pole, _ in rows]
    zeros = [zero[0] for _, zero in rows]
    assert all(abs((b - a) - 1047.0) < 1e-9 for a, b in zip(poles, poles[1:]))
    assert all(abs(z - (p + 523.5)) < 1e-9 for p, z in zip(poles, zeros))


def test_more_than_six_poles_are_cut_to_six():
    rows, report = fx.rows_from_design("butter", "lowpass", 16, [2000.0])
    assert len(rows) == 6
    assert report.dropped_poles == 2


def test_written_file_reads_back_under_the_app_rules():
    rows, _ = fx.rows_from_design("cheby1", "bandpass", 6, [300.0, 3000.0])
    rows = fx.clamp_rows(rows)
    folder = tempfile.mkdtemp()
    path = os.path.join(folder, "probe.fbw")
    fx.write_fbw(path, "probe", rows)
    poles, zeros = read_rows_like_the_app(path)
    assert len(poles) == len(rows)
    for (pole, zero), read_pole, read_zero in zip(rows, poles, zeros):
        assert abs(read_pole[0] - pole[0]) < 1e-4 and abs(read_pole[1] - pole[1]) < 1e-4
        assert (zero is None) == (read_zero is None)
    assert open(path, encoding="utf-8").readline() == "# probe\n"


def test_cli_writes_a_file_and_returns_zero():
    folder = tempfile.mkdtemp()
    path = os.path.join(folder, "cli.fbw")
    assert fx.main(["--out", path, "comb", "--start", "500", "--step", "1000", "--bw", "100"]) == 0
    poles, zeros = read_rows_like_the_app(path)
    assert len(poles) == 6 and all(z is not None for z in zeros)


if __name__ == "__main__":
    names = [n for n in dir() if n.startswith("test_")]
    for name in names:
        globals()[name]()
        print("PASS", name)
