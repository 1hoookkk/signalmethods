from __future__ import annotations
import ctypes as C
from dataclasses import dataclass, field
from pathlib import Path

from pyruntime.arma_measure_lib import RT_DOUBLES  # noqa: E402

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
GRID = 17
DEFAULT_SR = 48000.0

@dataclass
class SurfaceMetrics:
    cutoff_travel_octaves: float = 0.0
    resonance_change: float = 0.0

    bass_retention_db: float = 0.0
    axis_independence: float = 0.0
    smoothness_penalty: float = 0.0
    spectral_centroid_span_octaves: float = 0.0
    peak_count_range: tuple[int, int] = (0, 0)
    notch_count_range: tuple[int, int] = (0, 0)
    surface_area: float = 0.0
    unstable_states: int = 0

    grid_shape: tuple[int, int] = (GRID, GRID)
    cutoff_grid: np.ndarray = field(default_factory=lambda: np.zeros((GRID, GRID)))
    resonance_grid: np.ndarray = field(default_factory=lambda: np.zeros((GRID, GRID)))

    def behaviour_vector(self) -> np.ndarray:
        return np.array([self.cutoff_travel_octaves, self.resonance_change])

    def is_valid(self) -> bool:
        return self.unstable_states == 0 and np.isfinite(self.behaviour_vector()).all()

_lib = None

def _get_lib():
    global _lib
    if _lib is None:
        _lib = C.CDLL(str(ROOT / "target" / "release" / "trench_core.dll"))
        _lib.trench_packed_probe.restype = C.c_int32
        _lib.trench_packed_probe.argtypes = [
            C.c_char_p, C.c_size_t, C.c_double, C.c_double,
            C.POINTER(C.c_double), C.POINTER(C.c_double),
            C.POINTER(C.c_uint32), C.POINTER(C.c_uint32),
        ]
    return _lib

def _probe_state(body: bytes, morph: float, q: float, sr: float) -> dict | None:
    lib = _get_lib()
    out = (C.c_double * RT_DOUBLES)()
    mr = C.c_double()
    um = C.c_uint32()
    nm = C.c_uint32()
    rc = lib.trench_packed_probe(body, len(body), C.c_double(morph), C.c_double(q),
                                  out, C.byref(mr), C.byref(um), C.byref(nm))
    if rc != 0:
        return None

    rows = np.array(out).reshape(6, 5)

    freqs = np.geomspace(20, sr / 2.2, 512)
    z = np.exp(-1j * 2 * np.pi * freqs / sr)
    total = np.ones(len(freqs), dtype=complex)
    for b0, b1, b2, a1, a2 in rows:
        num = b0 + b1 * z + b2 * z * z
        den = 1.0 + a1 * z + a2 * z * z
        total *= num / (den + 1e-12)
    mag_db = 20 * np.log10(np.abs(total) + 1e-12)

    poles = []
    zeros_list = []
    for b0, b1, b2, a1, a2 in rows:
        disc = a1 * a1 - 4 * a2
        if disc < 0:
            r = np.sqrt(max(a2, 0))
            if r > 1e-6:
                hz = np.arccos(max(-1, min(1, -a1 / (2 * r)))) * sr / (2 * np.pi)
                poles.append((float(hz), float(r)))
        if abs(b0) > 1e-12:
            zdisc = b1 * b1 - 4 * b0 * b2
            if zdisc < 0:
                zr = np.sqrt(max(b2 / b0, 0))
                if zr > 1e-6:
                    zhz = np.arccos(max(-1, min(1, -b1 / (2 * b0 * zr)))) * sr / (2 * np.pi)
                    zeros_list.append((float(zhz), float(zr)))

    peak_db = float(mag_db.max())
    cutoff_idx = np.where(mag_db <= peak_db - 3.0)[0]
    cutoff_hz = float(freqs[cutoff_idx[0]]) if len(cutoff_idx) > 0 else float(freqs[-1])

    resonance_db = float(mag_db.max() - np.median(mag_db))

    linear_mag = 10 ** (mag_db / 20)
    centroid = float(np.sum(freqs * linear_mag) / (np.sum(linear_mag) + 1e-12))

    from scipy.signal import find_peaks
    peaks, _ = find_peaks(mag_db, prominence=3, height=peak_db - 12)
    notches, _ = find_peaks(-mag_db, prominence=3)

    bass_mask = (freqs >= 40) & (freqs <= 200)
    bass_db = float(mag_db[bass_mask].mean()) if bass_mask.any() else 0.0

    return {
        "morph": morph, "q": q,
        "mag_db": mag_db, "freqs": freqs,
        "cutoff_hz": cutoff_hz,
        "resonance_db": resonance_db,
        "centroid_hz": centroid,
        "bass_db": bass_db,
        "peak_count": len(peaks),
        "notch_count": len(notches),
        "poles": poles,
        "zeros": zeros_list,
        "max_r": float(mr.value),
        "unstable": int(um.value) != 0 or int(nm.value) != 0,
    }

def evaluate_surface(body: bytes, sr: float = DEFAULT_SR) -> SurfaceMetrics:
    cutoff_grid = np.zeros((GRID, GRID))
    resonance_grid = np.zeros((GRID, GRID))
    centroid_grid = np.zeros((GRID, GRID))
    bass_grid = np.zeros((GRID, GRID))
    peak_counts = np.zeros((GRID, GRID), dtype=int)
    notch_counts = np.zeros((GRID, GRID), dtype=int)
    unstable = 0

    for mi in range(GRID):
        morph = mi / (GRID - 1)
        for qi in range(GRID):
            q = qi / (GRID - 1)
            state = _probe_state(body, morph, q, sr)
            if state is None:
                unstable += 1
                continue
            cutoff_grid[mi, qi] = state["cutoff_hz"]
            resonance_grid[mi, qi] = state["resonance_db"]
            centroid_grid[mi, qi] = state["centroid_hz"]
            bass_grid[mi, qi] = state["bass_db"]
            peak_counts[mi, qi] = state["peak_count"]
            notch_counts[mi, qi] = state["notch_count"]
            if state["unstable"]:
                unstable += 1

    valid_cutoffs = cutoff_grid[cutoff_grid > 20]
    if len(valid_cutoffs) >= 2:
        cutoff_travel = float(np.log2(valid_cutoffs.max() / valid_cutoffs.min()))
    else:
        cutoff_travel = 0.0

    valid_res = resonance_grid[np.isfinite(resonance_grid)]
    resonance_change = float(valid_res.max() - valid_res.min()) if len(valid_res) > 0 else 0.0

    q0_bass = bass_grid[:, 0].mean()
    q1_bass = bass_grid[:, -1].mean()
    bass_retention = float(q1_bass - q0_bass)

    morph_slice = centroid_grid[:, 0]
    q_slice = centroid_grid[GRID // 2, :]
    if len(morph_slice) > 1 and len(q_slice) > 1:
        morph_grad = np.gradient(morph_slice)
        q_grad = np.gradient(q_slice)
        morph_range = morph_slice.max() - morph_slice.min()
        q_range = q_slice.max() - q_slice.min()
        total_range = morph_range + q_range
        axis_independence = min(morph_range, q_range) / max(total_range, 1.0)
    else:
        axis_independence = 0.0

    gy, gx = np.gradient(cutoff_grid)
    smoothness = float(np.mean(np.sqrt(gx**2 + gy**2)) / max(cutoff_grid.mean(), 1.0))

    active = (cutoff_grid > 20).sum()
    surface_area = active / (GRID * GRID)

    valid_centroids = centroid_grid[centroid_grid > 100]
    centroid_span = float(np.log2(valid_centroids.max() / valid_centroids.min())) if len(valid_centroids) > 1 else 0.0

    return SurfaceMetrics(
        cutoff_travel_octaves=cutoff_travel,
        resonance_change=resonance_change,
        bass_retention_db=bass_retention,
        axis_independence=axis_independence,
        smoothness_penalty=smoothness,
        spectral_centroid_span_octaves=centroid_span,
        peak_count_range=(int(peak_counts.min()), int(peak_counts.max())),
        notch_count_range=(int(notch_counts.min()), int(notch_counts.max())),
        surface_area=surface_area,
        unstable_states=unstable,
        cutoff_grid=cutoff_grid,
        resonance_grid=resonance_grid,
    )

def evaluate_trenchsrc(src_path: Path, sr: float = DEFAULT_SR) -> SurfaceMetrics | None:
    from tools.trenchsrc import TrenchSrc
    src = TrenchSrc.from_yaml(src_path)
    body = src.compile()
    return evaluate_surface(body, sr)
