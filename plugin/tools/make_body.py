#!/usr/bin/env python3
"""End-to-end body maker: measured sources in, certified .body240 out.

  python tools/make_body.py NAME M0_SOURCE M100_SOURCE [Q0M0 Q0M100 Q100M0 Q100M100 form]

Sources (2 = M axis with Q collapsed, 4 = full four-corner body):
  recording.wav              whole file
  recording.wav:1.5-3.0      time window in seconds
  head.sofa:az=90,el=0       one measured HRTF direction (left ear)

Every step delegates to trench_core.dll: ARMA fit, gain trim, pack,
certification. Outputs the body plus evidence (fit plot, report, and a
chromatic saw auditioned through the real engine at the corners and sweeps).
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "pyruntime"))
from arma_measure_lib import (DATUM, ROOT, averaged_spectrum_db, fit_arma,
                              formant_peaks, harmonic_envelope,
                              load_wav, log_grid_target, pack_and_certify,
                              roots_response_db, trim_gain_budget)
from joint_fit import certify as certify_bytes
from joint_fit import joint_refine, surface_rms

def measure(spec: str):
    if spec.lower().startswith("sum:"):
        a_spec, b_spec = spec[4:].split("|", 1)
        la, ga, da = measure(a_spec)
        lb, gb, db_ = measure(b_spec)
        lo, hi = max(ga[0], gb[0]), min(ga[-1], gb[-1])
        if hi <= lo * 1.5:
            raise SystemExit(f"sum: measurements barely overlap ({lo:.0f}-{hi:.0f} Hz)")
        grid = np.geomspace(lo, hi, 256)
        dbs = (np.interp(np.log(grid), np.log(ga), da)
               + np.interp(np.log(grid), np.log(gb), db_))
        dbs -= dbs.mean()
        return f"[{la}] plus [{lb}]", grid, dbs

    if spec.lower().startswith("diff:"):
        a_spec, b_spec = spec[5:].split("|", 1)
        la, ga, da = measure(a_spec)
        lb, gb, db_ = measure(b_spec)
        lo, hi = max(ga[0], gb[0]), min(ga[-1], gb[-1])
        if hi <= lo * 1.5:
            raise SystemExit(f"diff: measurements barely overlap ({lo:.0f}-{hi:.0f} Hz)")
        grid = np.geomspace(lo, hi, 256)
        dbs = (np.interp(np.log(grid), np.log(ga), da)
               - np.interp(np.log(grid), np.log(gb), db_))
        dbs -= dbs.mean()
        return f"[{la}] minus [{lb}]", grid, dbs

    m = re.match(r"^(.*?\.sofa):az=([-\d.]+),el=([-\d.]+)$", spec, re.I)
    if m:
        import h5py
        path, az, el = m.group(1), float(m.group(2)), float(m.group(3))
        f = h5py.File(path)
        ir = np.array(f["Data.IR"])
        pos = np.array(f["SourcePosition"])
        sr = float(np.array(f["Data.SamplingRate"]).flat[0])
        d = (np.abs((pos[:, 0] - az + 180) % 360 - 180)
             + np.abs(pos[:, 1] - el))
        i = int(np.argmin(d))
        H = np.abs(np.fft.rfft(ir[i, 0], 4096))
        fr = np.fft.rfftfreq(4096, 1.0 / sr)
        sel = (fr >= 200) & (fr <= 18_000)
        grid = np.geomspace(200, 18_000, 512)
        dbs = np.interp(np.log(grid), np.log(fr[sel]),
                        20 * np.log10(np.maximum(H[sel], 1e-9)))
        label = (f"{Path(path).stem} az{pos[i,0]:.0f} el{pos[i,1]:.0f}")
        return label, grid, dbs - dbs.mean()

    if re.search(r"\.(txt|csv)$", spec, re.I):
        rows = []
        for line in Path(spec).read_text(errors="replace").splitlines():
            m = re.match(r"^\s*([\d.eE+-]+)[\s,;]+(-?[\d.eE+-]+)", line)
            if m and not line.lstrip().startswith("*"):
                try:
                    rows.append((float(m.group(1)), float(m.group(2))))
                except ValueError:
                    pass
        if len(rows) < 16:
            raise SystemExit(f"{spec}: no freq/dB pairs found")
        a = np.array(rows)
        a = a[(a[:, 0] >= 60) & (a[:, 0] <= 16_000)]
        grid = np.geomspace(a[0, 0], a[-1, 0], 256)
        dbs = np.interp(np.log(grid), np.log(a[:, 0]), a[:, 1])
        return Path(spec).stem, grid, dbs - dbs.mean()

    m = re.match(r"^(.*?\.wav)(?::([\d.]+)-([\d.]+))?(:raw)?$", spec, re.I)
    if not m:
        raise SystemExit(f"unrecognised source spec: {spec}")
    path = m.group(1)
    x, sr = load_wav(Path(path))
    t0 = float(m.group(2)) if m.group(2) else 0.0
    t1 = float(m.group(3)) if m.group(3) else len(x) / sr
    seg = x[int(t0 * sr):int(t1 * sr)]
    if m.group(4):
        fa, dba = averaged_spectrum_db(seg, sr, nfft=32768)
        keep = (fa > 60.0) & (fa < 16_000.0)
        grid, dbs = fa[keep], dba[keep]
        dbs = dbs - dbs.mean()
        label = f"{Path(path).stem} {t0:.2f}-{t1:.2f}s raw-spectrum"
        return label, grid, dbs
    f0, hf, hdb = harmonic_envelope(seg, sr)
    if len(hf) >= 8:
        hdb = hdb + 6.0 * np.log2(hf / hf[0])
        grid, dbs, _ = log_grid_target(hf, hdb)
        label = f"{Path(path).stem} {t0:.2f}-{t1:.2f}s f0={f0:.0f}Hz tract-only"
        return label, grid, dbs

    if len(seg) < 2048:
        raise SystemExit(f"{spec}: window too short to measure")
    from scipy.signal import welch
    nseg = min(len(seg), 4096)
    fr, pxx = welch(seg, fs=sr, nperseg=nseg, noverlap=nseg // 2)
    lo, hi = 60.0, min(16_000.0, sr * 0.45)
    grid = np.geomspace(lo, hi, 256)
    ratio = (hi / lo) ** (1.0 / (len(grid) - 1))
    dbs = np.empty_like(grid)
    for i, gc in enumerate(grid):
        sel = (fr >= gc / ratio) & (fr <= gc * ratio)
        p = pxx[sel].mean() if sel.any() else np.interp(gc, fr, pxx)
        dbs[i] = 10.0 * np.log10(max(p, 1e-18))
    if len(dbs) >= 3:
        dbs[1:-1] = (dbs[:-2] + dbs[1:-1] + dbs[2:]) / 3.0
    dbs -= dbs.mean()
    label = f"{Path(path).stem} {t0:.2f}-{t1:.2f}s LTAS"
    return label, grid, dbs

def main():
    if len(sys.argv) < 4 or len(sys.argv) == 5 or len(sys.argv) > 7:
        raise SystemExit(__doc__)
    name = sys.argv[1]
    specs = sys.argv[2:]
    evidence = ROOT / "evidence" / f"body_{name}_e2e"
    evidence.mkdir(parents=True, exist_ok=True)

    report, corner_words, panels = [], [], []
    corner_roots, corner_curves = [], []
    for spec in specs:
        label, grid, dbs = measure(spec)
        pins = formant_peaks(grid, dbs)
        roots = None
        for compress in (1.0, 0.96, 0.92, 0.88):
            for use_pins in ([pins] if compress == 1.0 else []) + [[]]:
                try:
                    roots, _, metrics = fit_arma(grid, dbs * compress,
                                                 pinned_hz=use_pins)
                    pins = use_pins
                    break
                except AssertionError:
                    continue
            if roots is not None:
                break
        if roots is None:
            raise SystemExit(f"{label}: fitter refused at every fallback")
        if compress != 1.0:
            report.append(f"  NOTE: corner fitted at {compress:.2f}x contrast "
                          "(encoder refused the full-depth curve)")
        words, trimmed, peak_db = trim_gain_budget(roots)
        corner_words.append(words)
        corner_roots.append(trimmed)
        corner_curves.append((grid, dbs))
        panels.append((label, grid, dbs, roots_response_db(roots, grid),
                       metrics))
        report.append(f"--- {label}")
        report.append("  poles locked at measured peaks: "
                      + (", ".join(f"{p:.0f} Hz" for p in pins) or "none"))
        report.append(f"  fit rms {metrics[0]:.2f} dB, packed rms "
                      f"{metrics[1]:.3f} dB, sections {int(metrics[2])}, "
                      f"peak trimmed {peak_db:+.2f} dB -> unity")
        for s, p in enumerate(roots):
            if p[1] > 0 or p[3] > 0:
                report.append(f"  S{s+1}: pole {p[0]:7.1f} Hz r={p[1]:.4f} | "
                              f"zero {p[2]:7.1f} Hz r={p[3]:.4f}")

    if len(corner_words) == 2:
        corner_words = [corner_words[0], corner_words[1],
                        corner_words[0], corner_words[1]]
        corner_roots = [corner_roots[0], corner_roots[1],
                        corner_roots[0], corner_roots[1]]
        corner_curves = [corner_curves[0], corner_curves[1],
                         corner_curves[0], corner_curves[1]]
        report.append("\nQ axis collapsed (2 sources): Q100 = byte copy of Q0")
    body, max_r = pack_and_certify(sum(corner_words, []))

    import os
    template = os.environ.get("TRENCH_TEMPLATE") or None
    print("joint refine (multistart over correspondences, a few minutes)...")
    body_j, info = joint_refine(corner_roots, corner_curves, template=template)
    rms_a = surface_rms(body, info["targets"], info["freqs"], DATUM)
    report.append(f"\ninterior surface vs blended corner targets "
                  f"(7x7 grid): independent corners {rms_a:.2f} dB rms")
    if template:
        report.append(f"lane template: {template} "
                      f"(slot perm {info['slot_perm']})")
    if body_j is None:
        report.append("joint stage: encoder refused refined roots; "
                      "keeping independent-corner body")
    else:
        ok, mr_j = certify_bytes(body_j)
        rms_j = surface_rms(body_j, info["targets"], info["freqs"], DATUM)
        report.append(
            f"joint stage: {rms_j:.2f} dB rms (seed "
            f"{info['winning_seed']}, {info['seconds']:.0f}s, "
            f"certify {'PASS' if ok else 'FAIL'} max_r={mr_j:.6f})")
        if ok and rms_j < rms_a:
            body, max_r = body_j, mr_j
            report.append("joint body adopted")
        else:
            report.append("joint body rejected; keeping "
                          "independent-corner body")

    from arma_measure_lib import dc_anchor_body
    body = dc_anchor_body(body)
    ok_dc, max_r = certify_bytes(body)
    assert ok_dc, "DC anchor broke certification (should be impossible: SCALE only)"
    report.append("unity-DC anchor applied (SCALE-only multiply, all corners)")

    out = ROOT / "bodies" / "candidates" / f"{name}.body240"
    out.write_bytes(body)
    report.append(f"certify 33x33 at datum: PASS  max_r={max_r:.6f}")
    report.append(f"wrote {out}")

    fig, axes = plt.subplots(1, len(panels), figsize=(6.5 * len(panels), 5),
                             squeeze=False)
    corner_names = ["M0 Q0", "M100 Q0", "M0 Q100", "M100 Q100"]
    for ax, cname, (label, grid, dbs, fitted, metrics) in zip(
            axes[0], corner_names, panels):
        ax.semilogx(grid, dbs, lw=1.0, alpha=0.75, label="measured")
        ax.semilogx(grid, fitted, "--", lw=1.6,
                    label=f"fit rms {metrics[0]:.2f} dB")
        ax.set_ylim(-60, 30)
        ax.grid(True, which="both", alpha=0.3)
        ax.set_title(f"{cname}\n{label}", fontsize=9)
        ax.set_xlabel("Hz")
        ax.legend(fontsize=8)
    axes[0][0].set_ylabel("dB")
    fig.suptitle(f"{name} - measured vs fitted corners ({DATUM:.0f} Hz datum)")
    fig.tight_layout()
    fig.savefig(evidence / "corner_fits.png", dpi=110)

    import ctypes

    from scipy.io import wavfile
    lib = ctypes.CDLL(str(ROOT / "target" / "release" / "trench_core.dll"))
    lib.trench_engine_create.restype = ctypes.c_void_p
    lib.trench_engine_destroy.argtypes = [ctypes.c_void_p]
    lib.trench_engine_prepare.argtypes = [ctypes.c_void_p, ctypes.c_double]
    lib.trench_engine_load_body_bytes_at.argtypes = [
        ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_double]
    lib.trench_engine_load_body_bytes_at.restype = ctypes.c_int
    lib.trench_engine_set_input_mode.argtypes = [ctypes.c_void_p, ctypes.c_uint]
    lib.trench_engine_set_spatial_mode.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.trench_engine_set_agc_enabled.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.trench_engine_set_saturation_enabled.argtypes = [
        ctypes.c_void_p, ctypes.c_int]
    lib.trench_engine_set_parameters.argtypes = (
        [ctypes.c_void_p] + [ctypes.c_float] * 5)
    lib.trench_engine_process_block.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(ctypes.c_float), ctypes.c_int, ctypes.c_double,
        ctypes.c_double]
    host_sr = 48_000.0

    def render(source, morph, q):
        eng = lib.trench_engine_create()
        lib.trench_engine_prepare(eng, host_sr)
        buf = ctypes.create_string_buffer(body, 240)
        assert lib.trench_engine_load_body_bytes_at(eng, buf, 240, DATUM) == 0
        lib.trench_engine_set_input_mode(eng, 0)
        lib.trench_engine_set_spatial_mode(eng, 2)
        lib.trench_engine_set_agc_enabled(eng, 0)
        lib.trench_engine_set_saturation_enabled(eng, 0)
        morphs = np.broadcast_to(np.asarray(morph, float), source.shape)
        qs = np.broadcast_to(np.asarray(q, float), source.shape)
        out = np.zeros_like(source, dtype=np.float32)
        for start in range(0, len(source), 512):
            seg = source[start:start + 512]
            l = np.ascontiguousarray(seg, dtype=np.float32)
            r = l.copy()
            m, qq = float(morphs[start]), float(qs[start])
            lib.trench_engine_set_parameters(eng, m, qq, 0.0, 0.0, 1.0)
            lib.trench_engine_process_block(
                eng, l.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                r.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                len(seg), m, qq)
            out[start:start + len(seg)] = l
        lib.trench_engine_destroy(eng)
        return out

    def write(fname, x):
        peak = np.abs(x).max()
        if peak > 0.891:
            x = x * (0.891 / peak)
        wavfile.write(evidence / fname, int(host_sr), x.astype(np.float32))

    n = int(0.40 * host_sr)
    t = np.arange(n) / host_sr
    env = np.minimum(1.0, t / 0.01) * np.exp(-t / 0.36)
    saw = np.concatenate([
        (2.0 * ((t * 65.406 * 2.0 ** (k / 12.0)) % 1.0) - 1.0) * 0.5 * env
        for k in range(12)])
    write("chromatic_dry.wav", saw.astype(np.float32))
    write("chromatic_M0.wav", render(saw, 0.0, 0.0))
    write("chromatic_M100.wav", render(saw, 1.0, 0.0))
    write("chromatic_Msweep.wav",
          render(saw, np.linspace(0, 1, len(saw)), 0.0))
    if len(specs) == 4:
        write("chromatic_Qsweep_M50.wav",
              render(saw, 0.5, np.linspace(0, 1, len(saw))))

    text = "\n".join(report)
    (evidence / "report.txt").write_text(text)
    print(text)
    print(f"\nevidence -> {evidence}")

if __name__ == "__main__":
    main()
