import glob, json, math, os, struct, zipfile
from collections import defaultdict
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
P2K_RATE = 44100.0
MORPHEUS_RATE = 39062.5
IDENTITY = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF)

P2K_TYPES = {
    "LPF": ["megasweepz", "early_rizer", "millennium", "klub_klassik", "bassbox_303"],
    "EQ+": ["dj_alkaline", "ace_of_bass", "tb_or_not_tb", "boland_bass", "bass_tracer", "rogue_hertz"],
    "EQ-": ["razor_blades", "radio_craze"],
    "VOW": ["multi_q_vox", "ooh_to_eee", "talking_hedz", "eeh_to_aah", "ubu_orator", "deep_bouche"],
    "PHA": ["freak_shifta", "cruz_pusher"],
    "FLG": ["angelz_hairz", "dream_weava"],
    "REZ": ["meaty_gizmo", "dead_ringer", "zoom_peaks", "acid_ravage", "bass_o_matic", "lucifer_s_q", "tooth_comb"],
    "WAH": ["ear_bender"],
    "DST": ["fuzzi_face"],
    "SFX": ["klang_kling"],
}


def decode_word(w):
    u = w + 1
    if u >= 65536:
        return 1.0
    e = (u >> 12) & 0xF
    m = u & 0xFFF
    x = m / 4096.0 if e == 0 else (m + 4096.0) / 8192.0
    return math.ldexp(x, e - 15)


def pole_of_words(words, rate):
    d2, d3 = decode_word(words[2]), decode_word(words[3])
    a1, a2 = 4.0 * d2 + d3 - 2.0, 1.0 - d3
    if a2 <= 0.0 or a1 * a1 - 4.0 * a2 >= 0.0:
        return None
    r = math.sqrt(a2)
    hz = math.acos(max(-1.0, min(1.0, -a1 / (2.0 * r)))) / (2.0 * math.pi) * rate
    return hz, r


def p2k_poles():
    z = zipfile.ZipFile(os.path.join(ROOT, "evidence", "factory-data", "p2k", "bodies", "p2k.zip"))
    by_type = defaultdict(list)
    type_of = {n: t for t, names in P2K_TYPES.items() for n in names}
    for info in z.infolist():
        base = os.path.basename(info.filename)
        if not base.endswith(".bin") or info.file_size != 240:
            continue
        name = base[8:-4]
        t = type_of.get(name, "?")
        words = struct.unpack("<120H", z.read(info))
        for corner in range(4):
            for section in range(6):
                w = words[(corner * 6 + section) * 5:(corner * 6 + section) * 5 + 5]
                if tuple(w) == IDENTITY:
                    continue
                p = pole_of_words(w, P2K_RATE)
                if p and 20.0 < p[0] < 0.49 * P2K_RATE and p[1] > 0.3:
                    by_type[t].append((name, corner, section, p[0], p[1]))
    return by_type


def morpheus_poles():
    base = os.path.join(ROOT, "evidence", "factory-data", "morpheus", "decoded")
    category = {}
    for cat in sorted(os.listdir(base)):
        d = os.path.join(base, cat)
        if os.path.isdir(d):
            for f in os.listdir(d):
                if f[:3].isdigit():
                    category[int(f[:3])] = cat
    cubes = json.load(open(os.path.join(ROOT, "evidence", "factory-data", "morpheus", "raw", "cubes_decoded.json")))["cubes"]
    by_type = defaultdict(list)
    for cube in cubes:
        cat = category.get(cube["index"], "unlisted")
        for ci, corner in enumerate(cube["corners"]):
            for si, sec in enumerate(corner["sections"]):
                hz, r = sec["pole"]["hz"], sec["pole"]["r"]
                if r > 0.3 and 20.0 < hz < 0.49 * MORPHEUS_RATE:
                    by_type[cat].append((cube["name"], ci, si, hz, r))
    return by_type


def armadillo(hz, r, rate):
    theta = 2.0 * math.pi * hz / rate
    rp = min(100.0, 20.0 * math.log10(1.0 / max(1.0 - r, 1e-5)))
    tp = math.pi * (10.0 + math.log2(theta / math.pi)) / 10.0
    return rp, tp


def recurring(poles, rate):
    bins = defaultdict(set)
    where = defaultdict(list)
    for name, corner, section, hz, r in poles:
        rp, _ = armadillo(hz, r, rate)
        key = (round(math.log2(hz) * 6.0), round(rp / 2.0))
        bins[key].add(name)
        where[key].append((name, corner, section, hz, r))
    states = []
    for key, names in bins.items():
        pts = where[key]
        hz = float(np.exp(np.mean([math.log(p[3]) for p in pts])))
        r = float(np.mean([p[4] for p in pts]))
        states.append({"hz": round(hz, 1), "r": round(r, 5), "bw_hz": round(-math.log(r) * rate / math.pi, 1),
                       "bodies": len(names), "hits": len(pts), "names": sorted(names)})
    states.sort(key=lambda s: (-s["bodies"], -s["hits"]))
    return states


def panel(ax, poles, rate, title, colour, min_bodies):
    states = recurring(poles, rate)
    bodies = len({p[0] for p in poles})
    for s in states:
        if s["bodies"] < min_bodies:
            continue
        rp, tp = armadillo(s["hz"], s["r"], rate)
        ax.scatter(rp * math.cos(tp), rp * math.sin(tp), s=8 + 6.0 * s["bodies"], c=colour, alpha=0.7, edgecolors="none")
    for db in (20, 40, 60, 80, 100):
        t = np.linspace(0, math.pi, 120)
        ax.plot(db * np.cos(t), db * np.sin(t), color="#c8c8c8", lw=0.5)
    for hz in (50, 100, 200, 500, 1000, 2000, 5000, 10000):
        if hz >= 0.49 * rate:
            continue
        _, tp = armadillo(hz, 0.9, rate)
        ax.plot([0, 100 * math.cos(tp)], [0, 100 * math.sin(tp)], color="#e6e6e6", lw=0.5)
        ax.text(106 * math.cos(tp), 106 * math.sin(tp), f"{hz}", fontsize=5.5, ha="center", va="center", color="#777")
    ax.set_aspect("equal")
    ax.set_xlim(-114, 114)
    ax.set_ylim(-4, 114)
    ax.set_xticks([]); ax.set_yticks([])
    shared = sum(1 for s in states if s["bodies"] >= min_bodies)
    ax.set_title(f"{title}\n{bodies} bodies, {len(poles)} poles, {shared} shared states", fontsize=8)
    return states


def main():
    p2k = p2k_poles()
    mor = morpheus_poles()
    p2k_order = [t for t in P2K_TYPES if t in p2k]
    mor_order = sorted(mor)
    n = len(p2k_order) + len(mor_order)
    cols = 4
    rows = (n + cols - 1) // cols
    fig, axes = plt.subplots(rows, cols, figsize=(4.6 * cols, 3.2 * rows))
    axes = axes.flatten()
    templates = {"schema": "pole state templates by E-mu filter type; hz and bw_hz at the datum; r is the pole radius at that datum",
                 "p2k_44100": {}, "morpheus_39062_5": {}}
    k = 0
    for t in p2k_order:
        states = panel(axes[k], p2k[t], P2K_RATE, f"X3 type {t}  (44.1 kHz)", "#c0392b", 2 if len(P2K_TYPES[t]) > 1 else 1)
        templates["p2k_44100"][t] = {"bodies": P2K_TYPES[t], "states": [s for s in states if s["bodies"] >= (2 if len(P2K_TYPES[t]) > 1 else 1)][:40]}
        k += 1
    for cat in mor_order:
        states = panel(axes[k], mor[cat], MORPHEUS_RATE, f"Morpheus {cat}  (39062.5 Hz)", "#1f77b4", 3)
        templates["morpheus_39062_5"][cat] = {"bodies": sorted({p[0] for p in mor[cat]}), "states": [s for s in states if s["bodies"] >= 3][:60]}
        k += 1
    for ax in axes[k:]:
        ax.axis("off")
    fig.suptitle("recurring pole states by filter type, poles only, ARMAdillo plane (R' = 20 log10 1/(1-R), angle = log2 f); dot size = bodies sharing the state", fontsize=11)
    fig.tight_layout()
    out_png = os.path.join(ROOT, "evidence", "research-results", "pole_states_by_type.png")
    fig.savefig(out_png, dpi=110)
    out_json = os.path.join(ROOT, "evidence", "research-results", "pole_templates_by_type.json")
    json.dump(templates, open(out_json, "w"), indent=1)
    print(out_png); print(out_json)
    for t in p2k_order:
        st = templates["p2k_44100"][t]["states"]
        print(f"== X3 {t}: {len(st)} template states; top: " + "; ".join(f"{s['hz']:.0f} Hz r{s['r']:.3f} x{s['bodies']}" for s in st[:6]))
    for cat in mor_order:
        st = templates["morpheus_39062_5"][cat]["states"]
        print(f"== Morpheus {cat}: {len(st)} template states; top: " + "; ".join(f"{s['hz']:.0f} Hz r{s['r']:.3f} x{s['bodies']}" for s in st[:6]))


if __name__ == "__main__":
    main()
