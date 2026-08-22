from __future__ import annotations

import ctypes
import math
import sys
from collections import defaultdict
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))
from arma_measure_lib import lib, RT_DOUBLES  # noqa: E402

lib.trench_packed_probe_at.argtypes = [
    ctypes.c_void_p, ctypes.c_size_t, ctypes.c_double, ctypes.c_double,
    ctypes.c_double, ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_uint32),
    ctypes.POINTER(ctypes.c_uint32)]
lib.trench_packed_probe_at.restype = ctypes.c_int
lib.trench_stage_roots_from_words_at.argtypes = [
    ctypes.POINTER(ctypes.c_uint16), ctypes.c_double,
    ctypes.POINTER(ctypes.c_double)]
lib.trench_stage_roots_from_words_at.restype = ctypes.c_int

RATE = float(sys.argv[1]) if len(sys.argv) > 1 else 48_000.0
GRID = np.geomspace(20.0, 20_000.0, 700)
CORNERS = ["M0Q0", "M100Q0", "M0Q100", "M100Q100"]
BANDS = [(20.0, 250.0), (250.0, 2500.0), (2500.0, 20_000.0)]
MORPH_STEPS = 17
HELD_ST = 0.5
UNIT_ZERO_R = 0.9995
OUT = ROOT / "evidence" / "session_20260804" / "corpus"

def load_corpus():
    bodies = []
    for f in sorted((ROOT / "ref" / "presets").glob("P2k_0*.bin")):
        idx = int(f.stem.split("_")[1])
        if idx > 32:
            continue
        bodies.append((f.stem.replace("P2k_", "", 1), f.read_bytes()))
    assert len(bodies) == 33, f"expected 33 morphing bodies, got {len(bodies)}"
    return bodies

def words_of(body: bytes) -> np.ndarray:
    return np.frombuffer(body, dtype="<u2").reshape(4, 6, 5).copy()

def roots_of(row: np.ndarray):
    w = (ctypes.c_uint16 * 5)(*[int(x) for x in row])
    out = (ctypes.c_double * 5)()
    if lib.trench_stage_roots_from_words_at(w, RATE, out) != 0:
        return None
    return tuple(out)

def stage_curves(body: bytes, m: float, q: float):
    z1 = np.exp(-1j * 2.0 * np.pi * GRID / RATE)
    z2 = z1 * z1
    c = (ctypes.c_double * RT_DOUBLES)()
    mr = ctypes.c_double(); un = ctypes.c_uint32(); nf = ctypes.c_uint32()
    buf = ctypes.create_string_buffer(body, 240)
    if lib.trench_packed_probe_at(buf, 240, m, q, RATE, c, ctypes.byref(mr),
                                  ctypes.byref(un), ctypes.byref(nf)) != 0 \
       or un.value or nf.value:
        return None
    cc = np.ctypeslib.as_array(c).reshape(6, 5)
    return [20.0 * np.log10(np.maximum(np.abs(
        (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2)), 1e-12))
        for b0, b1, b2, a1, a2 in cc]

def probe_slot_poles(body: bytes, q: float):
    traj = [[] for _ in range(6)]
    for m in np.linspace(0.0, 1.0, MORPH_STEPS):
        c = (ctypes.c_double * RT_DOUBLES)()
        mr = ctypes.c_double(); un = ctypes.c_uint32(); nf = ctypes.c_uint32()
        buf = ctypes.create_string_buffer(body, 240)
        if lib.trench_packed_probe_at(buf, 240, float(m), q, RATE, c,
                                      ctypes.byref(mr), ctypes.byref(un),
                                      ctypes.byref(nf)) != 0:
            for t in traj:
                t.append(np.nan)
            continue
        cc = np.ctypeslib.as_array(c).reshape(6, 5)
        for s, (_b0, _b1, _b2, a1, a2) in enumerate(cc):
            disc = a1 * a1 - 4.0 * a2
            if disc < 0.0:
                theta = math.atan2(math.sqrt(-disc), -a1)
                traj[s].append(theta * RATE / (2.0 * math.pi))
            else:
                traj[s].append(np.nan)
    return [np.array(t) for t in traj]

def rp_db(r: float) -> float:
    return -20.0 * math.log10(max(1.0 - r, 1e-12))

def band_means(db: np.ndarray):
    return [float(db[(GRID >= lo) & (GRID < hi)].mean()) for lo, hi in BANDS]

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    bodies = load_corpus()
    names = [n for n, _ in bodies]
    W = {n: words_of(b) for n, b in bodies}
    G = {n: [[roots_of(W[n][c, s]) for s in range(6)] for c in range(4)]
         for n, _ in bodies}

    md = []
    md.append(f"# CORPUS REPORT - 33 morphing P2K bodies at {RATE:.0f} Hz\n")
    md.append("Decoded via trench_core.dll on the packed path. "
              "Raw words are the authority for reuse and law checks.\n")

    md.append("\n## LAW CHECK - the SCALE word\n")
    all_same, exceptions = 0, []
    for n in names:
        same = all(len({int(W[n][c, s, 4]) for s in range(6)}) == 1
                   for c in range(4))
        if same:
            all_same += 1
        else:
            per_corner = ["".join("=" if len({int(W[n][c, s, 4])
                          for s in range(6)}) == 1 else "x") for c in range(4)]
            exceptions.append(f"{n} {'/'.join(per_corner)}")
    md.append(f"- bodies where ALL SIX stage scales are word-identical at "
              f"every corner: **{all_same} / 33**\n")
    if exceptions:
        md.append(f"- exceptions ({len(exceptions)}): "
                  + ", ".join(exceptions) + "\n")
    corner_gain = []
    for n in names:
        gains = []
        for c in range(4):
            r0 = G[n][c][0]
            scales = [G[n][c][s][4] for s in range(6) if G[n][c][s]]
            if len(scales) == 6:
                gains.append(math.prod(scales))
        if len(gains) == 4:
            corner_gain.append((n, gains))
    md.append("- corner gain (product of six scales), min..max across "
              "corners:\n")
    for n, gains in corner_gain[:8]:
        md.append(f"    - {n}: "
                  + "  ".join(f"{20*math.log10(max(g,1e-12)):+.1f}dB"
                              for g in gains) + "\n")
    md.append(f"    - (... all 33 in scale_words.csv)\n")
    with open(OUT / "scale_words.csv", "w") as f:
        f.write("body,corner,s1,s2,s3,s4,s5,s6,product_db\n")
        for n in names:
            for c in range(4):
                sc = [G[n][c][s][4] if G[n][c][s] else float("nan")
                      for s in range(6)]
                pdb = 20 * math.log10(max(math.prod(sc), 1e-12)) \
                    if not any(math.isnan(x) for x in sc) else float("nan")
                f.write(f"{n},{CORNERS[c]},"
                        + ",".join(f"{x:.6g}" for x in sc)
                        + f",{pdb:.2f}\n")

    md.append("\n## LAW CHECK - unit-circle zeros by slot (raw words)\n")
    md.append("slot | unit-circle zero rows (decoded zero_r >= "
              f"{UNIT_ZERO_R}) of 132 corner-rows\n")
    md.append("---- | ----\n")
    unit_by_slot = []
    for s in range(6):
        count = sum(1 for n in names for c in range(4)
                    if G[n][c][s] and G[n][c][s][3] >= UNIT_ZERO_R)
        unit_by_slot.append(count)
        md.append(f"S{s+1} | {count}\n")

    md.append("\n## A - REUSE GRAPH\n")
    row_owners = defaultdict(set)
    row_owners_full = defaultdict(set)
    for n in names:
        for c in range(4):
            for s in range(6):
                key = tuple(int(x) for x in W[n][c, s, :4])
                if key != (0xFFFF,) * 4 and G[n][c][s] is not None:
                    row_owners[key].add(n)
                row_owners_full[tuple(int(x) for x in W[n][c, s])].add(n)
    shared_rows = {k: v for k, v in row_owners.items() if len(v) >= 2}
    md.append(f"- distinct non-identity pole/zero rows: {len(row_owners)}; "
              f"rows appearing in >=2 bodies: **{len(shared_rows)}**\n")
    top = sorted(shared_rows.items(), key=lambda kv: -len(kv[1]))[:12]
    for key, owners in top:
        r = roots_of(np.array(list(key) + [0xFFFF], dtype=np.uint16))
        desc = (f"pole {r[0]:.0f}Hz R'{rp_db(r[1]):.0f}dB / "
                f"zero {r[2]:.0f}Hz R'{rp_db(r[3]):.0f}dB") if r else "real pair"
        md.append(f"    - {desc}: {len(owners)} bodies "
                  f"({', '.join(sorted(owners)[:6])}"
                  f"{'...' if len(owners) > 6 else ''})\n")

    corner_hash = defaultdict(set)
    for n in names:
        for c in range(4):
            corner_hash[W[n][c].tobytes()].add((n, CORNERS[c]))
    shared_corners = [v for v in corner_hash.values() if len(v) >= 2]
    cross_body = [v for v in shared_corners
                  if len({n for n, _ in v}) >= 2]
    md.append(f"- complete corners shared between DIFFERENT bodies: "
              f"**{len(cross_body)}**\n")
    for v in cross_body:
        md.append("    - " + "  ==  ".join(f"{n}:{c}" for n, c in sorted(v))
                  + "\n")
    within_body = [v for v in shared_corners if len({n for n, _ in v}) == 1]
    md.append(f"- corners duplicated WITHIN one body (dead axis / copy-pose): "
              f"{len(within_body)} cases\n")

    md.append("- body pairs sharing >=5 of 6 slot rows at the same corner:\n")
    fam = []
    for i, a in enumerate(names):
        for b in names[i + 1:]:
            for c in range(4):
                hits = sum(
                    1 for s in range(6)
                    if tuple(W[a][c, s, :4]) == tuple(W[b][c, s, :4]))
                if hits >= 5:
                    fam.append(f"{a} ~ {b} at {CORNERS[c]} ({hits}/6)")
    md.append(("    - " + "\n    - ".join(fam) + "\n") if fam
              else "    - none at exact word level\n")

    near = []
    for i, a in enumerate(names):
        for b in names[i + 1:]:
            hits = 0
            for c in range(4):
                for s in range(6):
                    ra, rb = G[a][c][s], G[b][c][s]
                    if ra and rb and ra[0] > 0 and rb[0] > 0:
                        if (abs(math.log2(max(ra[0], 1e-3) / max(rb[0], 1e-3)))
                                < 0.01
                                and abs(ra[1] - rb[1]) < 0.002
                                and abs(math.log2(max(ra[2], 1e-3)
                                                  / max(rb[2], 1e-3))) < 0.01):
                            hits += 1
            if hits >= 8:
                near.append((hits, f"{a} ~ {b} ({hits}/24 near-identical rows)"))
    md.append("- near-match families (decoded space, >=8 of 24 corner-rows "
              "within 1% Hz and 0.002 radius):\n")
    for _, line in sorted(near, reverse=True)[:15]:
        md.append(f"    - {line}\n")

    md.append("\n## B - SLOT-ROLE REPORT\n")
    md.append("slot | pole Hz (q1/med/q3) | pole R' dB med | zero Hz med | "
              "unit-zero % | real-pair % | contribution by band "
              "(LF/MF/HF mean dB at corners)\n")
    md.append("---- | ---- | ---- | ---- | ---- | ---- | ----\n")
    contrib = {s: np.zeros(3) for s in range(6)}
    contrib_n = {s: 0 for s in range(6)}
    for n, b in bodies:
        for c, (m, q) in enumerate([(0, 0), (1, 0), (0, 1), (1, 1)]):
            curves = stage_curves(b, float(m), float(q))
            if curves is None:
                continue
            for s in range(6):
                contrib[s] += np.array(band_means(curves[s]))
                contrib_n[s] += 1
    for s in range(6):
        ph = [G[n][c][s][0] for n in names for c in range(4)
              if G[n][c][s] and G[n][c][s][0] > 0]
        pr = [rp_db(G[n][c][s][1]) for n in names for c in range(4)
              if G[n][c][s]]
        zh = [G[n][c][s][2] for n in names for c in range(4)
              if G[n][c][s] and G[n][c][s][2] > 0]
        unit = 100.0 * unit_by_slot[s] / 132.0
        realp = 100.0 * sum(1 for n in names for c in range(4)
                            if G[n][c][s] is None) / 132.0
        q1, med, q3 = np.percentile(ph, [25, 50, 75])
        cb = contrib[s] / max(contrib_n[s], 1)
        md.append(f"S{s+1} | {q1:.0f}/{med:.0f}/{q3:.0f} | "
                  f"{np.median(pr):.0f} | {np.median(zh):.0f} | "
                  f"{unit:.0f}% | {realp:.0f}% | "
                  f"{cb[0]:+.1f}/{cb[1]:+.1f}/{cb[2]:+.1f}\n")

    md.append("\n## C - VOICE-LEADING REPORT (MORPH rides, both Q rows)\n")
    motion_counts = defaultdict(int)
    travel_by_slot = [[] for _ in range(6)]
    common_tones, crossings_total = 0, 0
    per_body = []
    for n, b in bodies:
        for q in (0.0, 1.0):
            traj = probe_slot_poles(b, q)
            sgn = {}
            for s in range(6):
                t = traj[s][~np.isnan(traj[s])]
                if len(t) < 2 or t.min() <= 0:
                    continue
                st = 12.0 * math.log2(t[-1] / t[0])
                travel_by_slot[s].append(abs(12.0 * math.log2(t.max() / t.min())) / 12.0)
                if abs(st) < HELD_ST:
                    sgn[s] = 0
                    common_tones += 1
                else:
                    sgn[s] = 1 if st > 0 else -1
            for s in sgn:
                for t2 in sgn:
                    if t2 <= s:
                        continue
                    a, bb = sgn[s], sgn[t2]
                    if a == 0 and bb == 0:
                        motion_counts["both held"] += 1
                    elif a == 0 or bb == 0:
                        motion_counts["oblique"] += 1
                    elif a == bb:
                        motion_counts["parallel"] += 1
                    else:
                        motion_counts["contrary"] += 1
            order0 = sorted([s for s in sgn], key=lambda s: traj[s][0])
            order1 = sorted([s for s in sgn], key=lambda s: traj[s][-1])
            cross = sum(1 for i in range(len(order0))
                        if order0[i] != order1[i])
            crossings_total += cross // 2
            per_body.append((n, "Q0" if q == 0 else "Q100",
                             {s: sgn.get(s) for s in range(6)}, cross // 2))
    total_pairs = sum(motion_counts.values())
    md.append("motion class of slot pairs across all rides "
              f"({total_pairs} pairs):\n")
    for k in ("parallel", "contrary", "oblique", "both held"):
        md.append(f"- {k}: {motion_counts[k]} "
                  f"({100.0*motion_counts[k]/max(total_pairs,1):.0f}%)\n")
    md.append(f"- common tones (slot travel < {HELD_ST} st on a ride): "
              f"{common_tones} slot-rides\n")
    md.append(f"- voice crossings: {crossings_total} across all rides\n")
    md.append("- median absolute pole travel per slot (octaves): "
              + "  ".join(f"S{s+1} {np.median(travel_by_slot[s]):.2f}"
                          for s in range(6) if travel_by_slot[s]) + "\n")

    md.append("\n## D - SEQUENTIAL CONSTRUCTION\n")
    sigs = {}
    for n, b in bodies:
        v = []
        for c, (m, q) in enumerate([(0, 0), (1, 0), (0, 1), (1, 1)]):
            curves = stage_curves(b, float(m), float(q))
            if curves is None:
                v.append(np.zeros(18))
                continue
            v.append(np.concatenate([band_means(curves[s]) for s in range(6)]))
        sigs[n] = np.concatenate(v)
    sim = []
    for i, a in enumerate(names):
        for b2 in names[i + 1:]:
            x, y = sigs[a], sigs[b2]
            r = float(np.corrcoef(x, y)[0, 1])
            sim.append((r, a, b2))
    sim.sort(reverse=True)
    md.append("most similar construction signatures "
              "(slot-band contribution vectors, all four corners):\n")
    for r, a, b2 in sim[:12]:
        md.append(f"- {a} ~ {b2}: r = {r:.3f}\n")

    (OUT / "CORPUS_REPORT.md").write_text("".join(md), encoding="utf-8")
    print("".join(md))

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(2, 3, figsize=(16, 9), sharey=True)
    for s in range(6):
        ax = axes[s // 3][s % 3]
        for n in names:
            for c in range(4):
                g = G[n][c][s]
                if g and g[0] > 0:
                    ax.semilogx(g[0], rp_db(g[1]), ".", color="#0f6f66",
                                ms=4, alpha=0.55)
                if g and g[2] > 0:
                    ax.semilogx(g[2], rp_db(g[3]), ".", color="#c0563f",
                                ms=4, alpha=0.4)
        ax.set_xlim(20, 24000)
        ax.set_ylim(0, 100)
        ax.set_title(f"S{s+1}  poles teal / zeros coral", fontsize=10)
        ax.grid(alpha=0.25)
    fig.suptitle("SLOT-ROLE FIELD - 33 bodies x 4 corners, pole/zero "
                 "placements per slot (R' dB vs Hz)")
    fig.tight_layout()
    fig.savefig(OUT / "slot_role_field.png", dpi=110)
    plt.close(fig)

    ladders = ["003_millennium", "009_tb_or_not_tb", "013_talking_hedz",
               "022_deep_bouche", "031_ear_bender"]
    fig, axes = plt.subplots(len(ladders), 6, figsize=(22, 3.0 * len(ladders)),
                             sharex=True, sharey=True)
    for row, n in enumerate(ladders):
        b = dict(bodies)[n]
        curves = stage_curves(b, 0.0, 0.0)
        cum = np.zeros_like(GRID)
        for s in range(6):
            ax = axes[row][s]
            ax.semilogx(GRID, cum, color="#999999", lw=0.9)
            cum = cum + curves[s]
            ax.semilogx(GRID, cum, color="#c0563f", lw=1.4)
            ax.semilogx(GRID, curves[s], color="#0f6f66", lw=0.8, alpha=0.7)
            ax.set_ylim(-60, 30)
            ax.set_xlim(20, 20000)
            ax.grid(alpha=0.25)
            if row == 0:
                ax.set_title(f"after S{s+1}", fontsize=10)
        axes[row][0].set_ylabel(n, fontsize=9)
    fig.suptitle("SIGNAL SO FAR at M0 Q0 - grey before, coral after, "
                 "teal the voice alone (fixed -60..+30 dB)")
    fig.tight_layout()
    fig.savefig(OUT / "signal_so_far_ladders.png", dpi=110)
    plt.close(fig)
    print(f"\nsheets: {OUT}")

if __name__ == "__main__":
    main()
