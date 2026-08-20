import os
import glob
import json
import math
import struct
import xml.etree.ElementTree as ET
from collections import defaultdict, Counter

import numpy as np

TAU = 2.0 * math.pi
SR_39K = 39062.5
SR_44K = 44100.0
IDENTITY_WORDS = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF)
IDENTITY_WORDS_ALT = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xE000)
ROOT_OFF_R = 0.45
UNIT_EPS = 1e-6
CANCEL_OCT = 0.01
CANCEL_R = 0.01


def decode_u16(word):
    u = int(word) + 1
    if u == 65536:
        return 1.0
    if u == 1:
        return 0.0
    e = (u >> 12) & 0xF
    m = u & 0xFFF
    x = m / 4096.0 if e == 0 else (m + 4096.0) / 8192.0
    return x * (2.0 ** (e - 15))


def pair_geometry(d_mag, d_rsq, sr):
    q = 1.0 - d_rsq
    c = 4.0 * d_mag + d_rsq
    p = c - 2.0
    if abs(p) < 1e-12 and abs(q) < 1e-12:
        return {"type": "off", "hz": 0.0, "r": 0.0}
    disc = p * p - 4.0 * q
    if disc < 0.0:
        r = math.sqrt(max(0.0, q))
        cw = max(-1.0, min(1.0, -p / (2.0 * r))) if r > 0 else 0.0
        return {"type": "conj", "hz": math.acos(cw) / TAU * sr, "r": r}
    s = math.sqrt(disc)
    return {"type": "real", "a": (-p + s) / 2.0, "b": (-p - s) / 2.0}


def bw_hz(r, sr):
    if r <= 0.0:
        return None
    if r >= 1.0:
        return 0.05
    return max(0.05, -math.log(r) * sr / math.pi)


def r_at(bw, sr):
    return math.exp(-math.pi * bw / sr) if bw is not None else 0.0


def cancels(p, z):
    if p["r"] < ROOT_OFF_R or z["r"] < ROOT_OFF_R:
        return False
    if p["hz"] <= 0 or z["hz"] <= 0:
        return abs(p["r"] - z["r"]) < CANCEL_R and abs(p["hz"] - z["hz"]) < 1.0
    return abs(math.log2(p["hz"] / z["hz"])) < CANCEL_OCT and abs(p["r"] - z["r"]) < CANCEL_R


occ = []
inert = Counter()
parked = Counter()
real_class = []
order_hist = defaultdict(Counter)
morpheus_raw = defaultdict(set)
raw_decode = {}
p2k_words = defaultdict(set)
idle_raw = 0
unit_zero_count = Counter()

mor = json.load(open("ref/morpheus/cubes_decoded.json"))
for cube in mor["cubes"]:
    cname = cube["name"]
    for ci, corner in enumerate(cube["corners"]):
        active = 0
        for si, sec in enumerate(corner["sections"]):
            p = sec["pole"]
            z = sec["zero"]
            raw = tuple(sec["raw"])
            if raw == (1909, 2015, 0, 2047):
                idle_raw += 1
            if p["r"] < ROOT_OFF_R and z["r"] < ROOT_OFF_R:
                inert["morpheus"] += 1
                continue
            if cancels(p, z):
                parked["morpheus"] += 1
                continue
            active += 1
            if raw not in raw_decode:
                raw_decode[raw] = {"pole": dict(p), "zero": dict(z)}
            morpheus_raw[raw].add(cname)
            occ.append({
                "src": "morpheus", "sr": SR_39K, "name": cname, "corner": f"C{ci}", "stage": si,
                "pole": p, "zero": z, "gain_db": None,
            })
        order_hist["morpheus"][active] += 1

for pf in sorted(glob.glob("ref/presets/*.bin")):
    name = os.path.splitext(os.path.basename(pf))[0]
    data = open(pf, "rb").read()
    if len(data) != 240:
        continue
    words = struct.unpack("<" + "H" * 120, data)
    for ci in range(4):
        active = 0
        for si in range(6):
            sw = tuple(words[(ci * 6 + si) * 5:(ci * 6 + si) * 5 + 5])
            if sw in (IDENTITY_WORDS, IDENTITY_WORDS_ALT):
                inert["p2k"] += 1
                continue
            d = [decode_u16(w) for w in sw]
            zero = pair_geometry(d[0], d[1], SR_39K)
            pole = pair_geometry(d[2], d[3], SR_39K)
            gain_db = 20.0 * math.log10(max(4.0 * d[4], 1e-9))
            if zero["type"] == "real" or pole["type"] == "real":
                real_class.append({"src": "p2k", "name": name, "stage": si, "zero": zero, "pole": pole, "gain_db": gain_db})
                active += 1
                p2k_words[sw].add(name)
                continue
            pg = {"hz": pole.get("hz", 0.0), "r": pole["r"] if pole["type"] == "conj" else 0.0}
            zg = {"hz": zero.get("hz", 0.0), "r": zero["r"] if zero["type"] == "conj" else 0.0}
            if pg["r"] < ROOT_OFF_R and zg["r"] < ROOT_OFF_R:
                inert["p2k"] += 1
                continue
            if cancels(pg, zg):
                parked["p2k"] += 1
                continue
            active += 1
            p2k_words[sw].add(name)
            occ.append({
                "src": "p2k", "sr": SR_39K, "name": name, "corner": f"M{ci & 1}Q{ci >> 1}", "stage": si,
                "pole": pg, "zero": zg, "gain_db": gain_db,
            })
        order_hist["p2k"][active] += 1

for bf in sorted(glob.glob("ref/cubes/*.body") + glob.glob("recipes/hero/*.body")):
    name = os.path.splitext(os.path.basename(bf))[0]
    data = open(bf, "rb").read()
    if len(data) != 560:
        continue
    words = struct.unpack("<" + "H" * 280, data)
    for ci in range(8):
        active = 0
        for si in range(7):
            sw = tuple(words[(ci * 7 + si) * 5:(ci * 7 + si) * 5 + 5])
            d = [decode_u16(w) for w in sw]
            zero = pair_geometry(d[0], d[1], SR_44K)
            pole = pair_geometry(d[2], d[3], SR_44K)
            gain_db = 20.0 * math.log10(max(4.0 * d[4], 1e-9))
            if zero["type"] == "real" or pole["type"] == "real":
                real_class.append({"src": "native", "name": name, "stage": si, "zero": zero, "pole": pole, "gain_db": gain_db})
                active += 1
                continue
            pg = {"hz": pole.get("hz", 0.0), "r": pole["r"] if pole["type"] == "conj" else 0.0}
            zg = {"hz": zero.get("hz", 0.0), "r": zero["r"] if zero["type"] == "conj" else 0.0}
            if pg["r"] < ROOT_OFF_R and zg["r"] < ROOT_OFF_R:
                inert["native"] += 1
                continue
            if cancels(pg, zg):
                parked["native"] += 1
                continue
            active += 1
            occ.append({
                "src": "native", "sr": SR_44K, "name": name, "corner": f"C{ci}", "stage": si,
                "pole": pg, "zero": zg, "gain_db": gain_db,
            })
        order_hist["native"][active] += 1

strata = {"pole_only": [], "pole_zero": [], "zero_only": []}
for o in occ:
    p_on = o["pole"]["r"] >= ROOT_OFF_R
    z_on = o["zero"]["r"] >= ROOT_OFF_R
    if z_on and abs(o["zero"]["r"] - 1.0) < UNIT_EPS:
        unit_zero_count[o["src"]] += 1
    hzf = lambda hz: math.log2(max(hz, 10.0))
    if p_on and z_on:
        strata["pole_zero"].append((o, [
            hzf(o["pole"]["hz"]), math.log2(bw_hz(o["pole"]["r"], o["sr"])),
            hzf(o["zero"]["hz"]), math.log2(bw_hz(o["zero"]["r"], o["sr"])),
        ]))
    elif p_on:
        strata["pole_only"].append((o, [hzf(o["pole"]["hz"]), math.log2(bw_hz(o["pole"]["r"], o["sr"]))]))
    else:
        strata["zero_only"].append((o, [hzf(o["zero"]["hz"]), math.log2(bw_hz(o["zero"]["r"], o["sr"]))]))


def kmeans(X, k, seed=7, iters=120):
    rng = np.random.default_rng(seed)
    n = X.shape[0]
    centers = [X[rng.integers(n)]]
    for _ in range(k - 1):
        d2 = np.min([np.sum((X - c) ** 2, axis=1) for c in centers], axis=0)
        probs = d2 / max(d2.sum(), 1e-12)
        centers.append(X[rng.choice(n, p=probs)])
    C = np.array(centers)
    for _ in range(iters):
        d = np.sum((X[:, None, :] - C[None, :, :]) ** 2, axis=2)
        lab = np.argmin(d, axis=1)
        newC = np.array([X[lab == j].mean(axis=0) if np.any(lab == j) else C[j] for j in range(k)])
        if np.allclose(newC, C):
            break
        C = newC
    return lab, C


def pca_cluster(pairs, k):
    if len(pairs) < k * 3:
        k = max(2, len(pairs) // 6) if len(pairs) >= 12 else 1
    X = np.array([f for _, f in pairs])
    mean = X.mean(axis=0)
    std = X.std(axis=0)
    std[std < 1e-9] = 1.0
    Z = (X - mean) / std
    U, S, Vt = np.linalg.svd(Z - Z.mean(axis=0), full_matrices=False)
    var = (S ** 2) / max((S ** 2).sum(), 1e-12)
    scores = (Z - Z.mean(axis=0)) @ Vt.T
    if k == 1:
        lab = np.zeros(len(pairs), dtype=int)
    else:
        lab, _ = kmeans(scores, k)
    return lab, {"variance_ratio": [round(float(v), 4) for v in var], "loadings": [[round(float(x), 3) for x in row] for row in Vt]}


def gmean(xs):
    xs = [x for x in xs if x and x > 0]
    return math.exp(sum(math.log(x) for x in xs) / len(xs)) if xs else None


def cluster_rows(pairs, lab):
    rows = []
    for j in sorted(set(lab)):
        members = [pairs[i][0] for i in range(len(pairs)) if lab[i] == j]
        filters = sorted(set(m["name"] for m in members))
        srcs = Counter(m["src"] for m in members)
        stages = Counter(m["stage"] + 1 for m in members)
        p_hz = gmean([m["pole"]["hz"] for m in members if m["pole"]["r"] >= ROOT_OFF_R])
        p_bw = gmean([bw_hz(m["pole"]["r"], m["sr"]) for m in members if m["pole"]["r"] >= ROOT_OFF_R])
        z_hz = gmean([m["zero"]["hz"] for m in members if m["zero"]["r"] >= ROOT_OFF_R])
        z_bw = gmean([bw_hz(m["zero"]["r"], m["sr"]) for m in members if m["zero"]["r"] >= ROOT_OFF_R])
        gains = [m["gain_db"] for m in members if m["gain_db"] is not None]
        unit = sum(1 for m in members if abs(m["zero"]["r"] - 1.0) < UNIT_EPS)
        rows.append({
            "occurrences": len(members),
            "distinct_filters": len(filters),
            "by_corpus": dict(srcs),
            "corpora_spanned": len(srcs),
            "pole_hz": None if p_hz is None else round(p_hz, 1),
            "pole_bw_hz": None if p_bw is None else round(p_bw, 1),
            "pole_r_at_39062_5": None if p_bw is None else round(r_at(p_bw, SR_39K), 5),
            "pole_r_at_44100": None if p_bw is None else round(r_at(p_bw, SR_44K), 5),
            "zero_hz": None if z_hz is None else round(z_hz, 1),
            "zero_bw_hz": None if z_bw is None else round(z_bw, 1),
            "unit_circle_zero_share": round(unit / len(members), 3),
            "mean_stage_gain_db": None if not gains else round(sum(gains) / len(gains), 2),
            "stage_position_histogram": {f"S{s}": c for s, c in sorted(stages.items())},
            "exemplar_filters": filters[:10],
        })
    rows.sort(key=lambda r: (r["corpora_spanned"], r["distinct_filters"], r["occurrences"]), reverse=True)
    return rows


K = {"pole_only": 16, "pole_zero": 16, "zero_only": 6}
stratum_out = {}
for sname, pairs in strata.items():
    if not pairs:
        continue
    lab, pca = pca_cluster(pairs, K[sname])
    stratum_out[sname] = {"count": len(pairs), "pca": pca, "clusters": cluster_rows(pairs, lab)}

print("=" * 100)
print("STAGE-STATE CENSUS v2 — Klatt coordinates (hz + bandwidth at own datum), PCA + k-means in score space")
print("=" * 100)
print(f"musical SOS occurrences: {len(occ)}  strata: " + "  ".join(f"{k}:{len(v)}" for k, v in strata.items()))
print(f"inert (both roots off, r<{ROOT_OFF_R}): {dict(inert)}   parked cancelling pairs: {dict(parked)}")
print(f"morpheus idle-pole raw occurrences: {idle_raw}   real-pair stages: {len(real_class)}   unit zeros: {dict(unit_zero_count)}")
print()
print("cascade order (active sections per corner):")
for src, hist in order_hist.items():
    print(f"  {src:9s} " + "  ".join(f"{k}:{v}" for k, v in sorted(hist.items())))
for sname, out in stratum_out.items():
    print()
    print("=" * 100)
    print(f"STRATUM {sname} — {out['count']} occurrences, PCA variance {out['pca']['variance_ratio']}")
    print("=" * 100)
    for r in out["clusters"][:10]:
        pole = f"pole {r['pole_hz']} Hz bw {r['pole_bw_hz']} (r39 {r['pole_r_at_39062_5']} r44 {r['pole_r_at_44100']})" if r["pole_hz"] else "pole —"
        zero = f"zero {r['zero_hz']} Hz bw {r['zero_bw_hz']}" + (f" unit {r['unit_circle_zero_share']}" if r["unit_circle_zero_share"] > 0 else "") if r["zero_hz"] else "zero —"
        gain = f" gain {r['mean_stage_gain_db']:+.1f} dB" if r["mean_stage_gain_db"] is not None else ""
        span = "+".join(f"{k}:{v}" for k, v in r["by_corpus"].items())
        print(f"  {r['distinct_filters']:3d} filters {r['occurrences']:4d} occ ({span})  {pole}  {zero}{gain}")
        print(f"      stages {','.join(f'{k}:{v}' for k, v in r['stage_position_histogram'].items())}  e.g. {', '.join(r['exemplar_filters'][:6])}")

mr = sorted(morpheus_raw.items(), key=lambda kv: len(kv[1]), reverse=True)
pw = sorted(p2k_words.items(), key=lambda kv: len(kv[1]), reverse=True)
print()
print("=" * 100)
print("EXACT VERBATIM LETTERS")
print("=" * 100)
for raw, names in mr[:8]:
    d = raw_decode[raw]
    print(f"  morpheus {raw}  pole {d['pole']['hz']:.0f}Hz r{d['pole']['r']:.4f}  zero {d['zero']['hz']:.0f}Hz r{d['zero']['r']:.4f}  in {len(names)} cubes")
for sw, names in pw[:6]:
    d = [decode_u16(w) for w in sw]
    zero = pair_geometry(d[0], d[1], SR_39K)
    pole = pair_geometry(d[2], d[3], SR_39K)
    g = 20.0 * math.log10(max(4.0 * d[4], 1e-9))
    pd = f"pole {pole['hz']:.0f}Hz r{pole['r']:.4f}" if pole["type"] == "conj" else f"pole {pole['type']}"
    zd = f"zero {zero['hz']:.0f}Hz r{zero['r']:.4f}" if zero["type"] == "conj" else f"zero {zero['type']}"
    print(f"  p2k {len(names)} presets  {pd}  {zd}  {g:+.1f} dB  e.g. {sorted(names)[:4]}")

rc = defaultdict(list)
for m in real_class:
    part = "pole" if m["pole"]["type"] == "real" else "zero"
    root = m["pole"] if part == "pole" else m["zero"]
    rc[(part, round(root["a"], 2), round(root["b"], 2))].append(m)

emu_root = r"C:\Users\hooki\OneDrive\Documents\Creative Professional\Emulator X Family\Templates\Filter"
xml_states = defaultdict(set)
type_hist = Counter()
nfiles = 0
XML_TYPES = {0: "off", 1: "eq", 2: "lowpass", 3: "highpass"}
for xf in sorted(glob.glob(os.path.join(emu_root, "*.xml"))):
    try:
        tree = ET.parse(xf)
    except Exception:
        continue
    filt = tree.getroot().find(".//filter")
    if filt is None:
        continue
    nfiles += 1
    bname = os.path.splitext(os.path.basename(xf))[0]
    for ds in filt.findall("designer-section"):
        st = int(ds.findtext("type", "0").strip())
        lf = int(ds.findtext("low-freq", "0").strip())
        hf = int(ds.findtext("high-freq", "0").strip())
        lg = int(ds.findtext("low-gain", "0").strip())
        hg = int(ds.findtext("high-gain", "0").strip())
        if lf == 0 and hf == 0 and lg == 0 and hg == 0:
            continue
        type_hist[st] += 1
        xml_states[(st, lf, hf, lg, hg)].add(bname)

facts = {
    "contract": "stage-state-census-v2: recurring second-order-section states across the E-mu corpus, derived by the Klatt/PCA method — each SOS is represented by its formant coordinates (center frequency Hz and bandwidth Hz, both physical at the row's own corpus datum sample rate; bandwidth law bw = -ln(r)*sr/pi), stratified into pole_only / pole_zero / zero_only, standardized, reduced by PCA, and clustered by k-means in score space. Full frequency-response curves were never compared. Raw encoded angles were never compared across datums.",
    "generated_by": "dev/census_stage_states.py",
    "corpora": {
        "morpheus": {"filters": len(mor["cubes"]), "datum_sr_hz": SR_39K, "source": "ref/morpheus/cubes_decoded.json", "stages_per_corner": 7, "corners": 8, "per_stage_gain": False},
        "p2k": {"filters": len(glob.glob("ref/presets/*.bin")), "datum_sr_hz": SR_39K, "source": "ref/presets/*.bin", "stages_per_corner": 6, "corners": 4, "per_stage_gain": True},
        "native": {"filters": len(glob.glob("ref/cubes/*.body") + glob.glob("recipes/hero/*.body")), "datum_sr_hz": SR_44K, "source": "ref/cubes/*.body + recipes/hero/*.body", "stages_per_corner": 7, "corners": 8, "per_stage_gain": True},
        "emulator_x_xml": {"filters": nfiles, "source": "Emulator X Templates/Filter/*.xml", "note": "typed Morph Designer cards serialized as 0..127 codes, now decoded to physical Hz/dB via xml_display_laws; still excluded from root-geometry strata because cards compile to roots only through the typed-card compiler"},
    },
    "sentinels": {
        "definition": f"a root is OFF below r {ROOT_OFF_R} (the corpus idle convention, e.g. the r=0.125 idle pole is acoustically flat); a stage is inert when both roots are off, and parked when pole and zero cancel exactly (same hz within {CANCEL_OCT} octave, r within {CANCEL_R}) — correspondence scaffolding kept so lanes can travel apart in other corners. Both are excluded from strata.",
        "inert_stage_rows_by_corpus": dict(inert),
        "parked_cancelling_pairs_by_corpus": dict(parked),
        "morpheus_idle_pole_raw_1909_2015_0_2047": idle_raw,
        "unit_circle_zero_stages_by_corpus": dict(unit_zero_count),
    },
    "cascade_order_histogram": {src: {str(k): v for k, v in sorted(h.items())} for src, h in order_hist.items()},
    "lower_order_note": "cascade order per corner = active (non-inert, non-parked) sections; real-pair stages count as active and are listed in real_pair_letters",
    "strata": stratum_out,
    "verbatim_letters": {
        "morpheus_raw_codes": [
            {"raw": list(raw), "cube_count": len(names), "pole": raw_decode[raw]["pole"], "zero": raw_decode[raw]["zero"], "exemplar_cubes": sorted(names)[:8]}
            for raw, names in mr[:24]
        ],
        "p2k_word_states": [
            (lambda d: {
                "words": list(sw), "preset_count": len(names),
                "zero": pair_geometry(d[0], d[1], SR_39K), "pole": pair_geometry(d[2], d[3], SR_39K),
                "gain_db": round(20.0 * math.log10(max(4.0 * d[4], 1e-9)), 2), "exemplar_presets": sorted(names)[:8],
            })([decode_u16(w) for w in sw])
            for sw, names in pw[:24]
        ],
    },
    "real_pair_letters": [
        {"part": kk[0], "roots": [kk[1], kk[2]], "occurrences": len(ms), "filter_count": len(set(m["name"] for m in ms)), "exemplars": sorted(set(m["name"] for m in ms))[:6]}
        for kk, ms in sorted(rc.items(), key=lambda kv: len(kv[1]), reverse=True)[:16]
    ],
    "xml_display_laws": {
        "frequency": "hz = 83 * 2^(code/18) — 1/18 octave per code, code 0 = 83 Hz, code 124 = 9824 Hz; derived from Morph Designer display anchors (83, 202, 1188, 6186, 9824 Hz) all landing on integer codes, with the 202->1188 EQ pair present verbatim as codes {23,69} in a shipped template",
        "gain": "dB = 0.375 * (code - 64) — 48 dB over 128 steps, -24.0 .. +23.625",
        "q": "percent = code / 1.27",
        "types": {"0": "off", "1": "eq", "2": "lowpass", "3": "highpass"},
    },
    "xml_card_states": [
        {"type": XML_TYPES.get(t, t),
         "freq_hz_low": round(83.0 * 2 ** (lf / 18.0), 1), "freq_hz_high": round(83.0 * 2 ** (hf / 18.0), 1),
         "gain_db_low": round(0.375 * (lg - 64), 2), "gain_db_high": round(0.375 * (hg - 64), 2),
         "freq_code_low": lf, "freq_code_high": hf, "gain_code_low": lg, "gain_code_high": hg,
         "template_count": len(names), "exemplars": sorted(names)[:6]}
        for (t, lf, hf, lg, hg), names in sorted(xml_states.items(), key=lambda kv: len(kv[1]), reverse=True)[:24]
    ],
}
json.dump(facts, open("ref/stage_state_census.json", "w"), indent=1)
print()
print(f"machine-readable facts -> ref/stage_state_census.json")
