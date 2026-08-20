import os
import re
import glob
import json
import math
import struct
from collections import defaultdict, Counter

TAU = 2.0 * math.pi
SR_39K = 39062.5
SR_44K = 44100.0
IDENTITY_WORDS = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF)
IDENTITY_WORDS_ALT = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xE000)
ROOT_OFF_R = 0.45
UNIT_R = 0.99999
CANCEL_OCT = 0.01
CANCEL_R = 0.01
NEAR_ST = 6.0
BW_RATIO_OCT = 1.0
SHARP_BW_HZ = 150.0
MIN_OCC = 8
MIN_FILTERS = 3


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
    return math.exp(-math.pi * bw / sr) if bw else 0.0


def cancels(p, z):
    if p["r"] < ROOT_OFF_R or z["r"] < ROOT_OFF_R:
        return False
    if p["hz"] <= 0 or z["hz"] <= 0:
        return abs(p["r"] - z["r"]) < CANCEL_R and abs(p["hz"] - z["hz"]) < 1.0
    return abs(math.log2(p["hz"] / z["hz"])) < CANCEL_OCT and abs(p["r"] - z["r"]) < CANCEL_R


def interval_st(p, z):
    return 12.0 * math.log2(max(z["hz"], 1.0) / max(p["hz"], 1.0))


def classify(pole, zero, sr, real=False):
    if real:
        return "real pair"
    p_on = pole["r"] >= ROOT_OFF_R
    z_on = zero["r"] >= ROOT_OFF_R
    if not p_on and not z_on:
        return "idle"
    if p_on and z_on and cancels(pole, zero):
        return "parked pair"
    if p_on and not z_on:
        return "peak" if bw_hz(pole["r"], sr) <= SHARP_BW_HZ else "lift"
    if z_on and not p_on:
        return "notch" if bw_hz(zero["r"], sr) <= SHARP_BW_HZ else "dip"
    if zero["r"] >= UNIT_R:
        return "guarded peak"
    st = interval_st(pole, zero)
    if st > NEAR_ST:
        return "low shelf"
    if st < -NEAR_ST:
        return "high shelf"
    br = math.log2(bw_hz(zero["r"], sr) / bw_hz(pole["r"], sr))
    if br >= BW_RATIO_OCT:
        return "bell"
    if br <= -BW_RATIO_OCT:
        return "carved band"
    return "close pair"


rows = []

mor = json.load(open("ref/morpheus/cubes_decoded.json"))
for cube in mor["cubes"]:
    for ci, corner in enumerate(cube["corners"]):
        for si, sec in enumerate(corner["sections"]):
            rows.append({"src": "morpheus", "sr": SR_39K, "name": cube["name"], "stage": si,
                         "pole": dict(sec["pole"]), "zero": dict(sec["zero"]), "gain_db": None, "real": False})

for pf in sorted(glob.glob("ref/presets/*.bin")):
    name = os.path.splitext(os.path.basename(pf))[0]
    data = open(pf, "rb").read()
    if len(data) != 240:
        continue
    words = struct.unpack("<" + "H" * 120, data)
    for ci in range(4):
        for si in range(6):
            sw = tuple(words[(ci * 6 + si) * 5:(ci * 6 + si) * 5 + 5])
            d = [decode_u16(w) for w in sw]
            zero = pair_geometry(d[0], d[1], SR_39K)
            pole = pair_geometry(d[2], d[3], SR_39K)
            gain_db = 20.0 * math.log10(max(4.0 * d[4], 1e-9))
            real = zero["type"] == "real" or pole["type"] == "real"
            if sw in (IDENTITY_WORDS, IDENTITY_WORDS_ALT):
                real = False
            pg = {"hz": pole.get("hz", 0.0), "r": pole["r"] if pole["type"] == "conj" else 0.0}
            zg = {"hz": zero.get("hz", 0.0), "r": zero["r"] if zero["type"] == "conj" else 0.0}
            rows.append({"src": "p2k", "sr": SR_39K, "name": name, "stage": si,
                         "pole": pg, "zero": zg, "gain_db": gain_db, "real": real})

for bf in sorted(glob.glob("ref/cubes/*.body") + glob.glob("recipes/hero/*.body")):
    name = os.path.splitext(os.path.basename(bf))[0]
    data = open(bf, "rb").read()
    if len(data) != 560:
        continue
    words = struct.unpack("<" + "H" * 280, data)
    for ci in range(8):
        for si in range(7):
            sw = tuple(words[(ci * 7 + si) * 5:(ci * 7 + si) * 5 + 5])
            d = [decode_u16(w) for w in sw]
            zero = pair_geometry(d[0], d[1], SR_44K)
            pole = pair_geometry(d[2], d[3], SR_44K)
            gain_db = 20.0 * math.log10(max(4.0 * d[4], 1e-9))
            real = zero["type"] == "real" or pole["type"] == "real"
            pg = {"hz": pole.get("hz", 0.0), "r": pole["r"] if pole["type"] == "conj" else 0.0}
            zg = {"hz": zero.get("hz", 0.0), "r": zero["r"] if zero["type"] == "conj" else 0.0}
            rows.append({"src": "native", "sr": SR_44K, "name": name, "stage": si,
                         "pole": pg, "zero": zg, "gain_db": gain_db, "real": real})

for r in rows:
    r["shape"] = classify(r["pole"], r["zero"], r["sr"], r["real"])

SHAPES = {
    "idle": {
        "structure": "both roots off (r below 0.45): the exact identity row the corpus parks in unused slots, seated as the idle pole at 9382 Hz r 0.125 with the off zero",
        "free": [],
        "freedom": [],
        "slaving": None,
    },
    "parked pair": {
        "structure": "conjugate pole and conjugate zero on the same frequency within 0.01 octave and the same radius within 0.01 — they cancel; correspondence scaffolding that lets the lane travel apart at other corners",
        "free": ["freq"],
        "freedom": ["pole_hz"],
        "slaving": {"zero_hz": "rides the pole", "interval_st_median": 0.0, "zero_r_follows_pole": True},
    },
    "real pair": {
        "structure": "one part is a real-axis root pair rather than a conjugate pair — the shelf and rolloff letters the conjugate schema cannot express; carried verbatim",
        "free": [],
        "freedom": [],
        "slaving": None,
    },
    "peak": {
        "structure": "conjugate pole alone, zero off; bandwidth at or under 150 Hz — a resonance",
        "free": ["freq", "resonance", "gain"],
        "freedom": ["pole_hz", "pole_r"],
        "slaving": None,
    },
    "lift": {
        "structure": "conjugate pole alone, zero off; bandwidth over 150 Hz — a broad band lift or lowpass corner",
        "free": ["freq", "resonance", "gain"],
        "freedom": ["pole_hz", "pole_r"],
        "slaving": None,
    },
    "notch": {
        "structure": "conjugate zero alone, pole off; bandwidth at or under 150 Hz — a narrow null",
        "free": ["freq", "resonance", "gain"],
        "freedom": ["zero_hz", "zero_r"],
        "slaving": None,
    },
    "dip": {
        "structure": "conjugate zero alone, pole off; bandwidth over 150 Hz — a broad scoop",
        "free": ["freq", "resonance", "gain"],
        "freedom": ["zero_hz", "zero_r"],
        "slaving": None,
    },
    "guarded peak": {
        "structure": "conjugate pole with a zero on the unit circle — the traveling null riding beside the resonance; the zero radius is exactly 1.0 and stays there",
        "free": ["freq", "resonance", "gain"],
        "freedom": ["pole_hz", "pole_r"],
        "slaving": {"zero_hz": "rides the pole at the measured interval", "zero_r_fixed": 1.0},
    },
    "bell": {
        "structure": "conjugate pole with a conjugate zero within 6 semitones, the zero at least twice as wide — level comes from the tight pole inside the wide zero",
        "free": ["freq", "resonance", "gain"],
        "freedom": ["pole_hz", "pole_r", "zero_r"],
        "slaving": {"zero_hz": "rides the pole at the measured interval"},
    },
    "carved band": {
        "structure": "conjugate pole with a conjugate zero within 6 semitones, the zero at least twice as narrow — a sharp null cut into a broad band",
        "free": ["freq", "resonance", "gain"],
        "freedom": ["pole_hz", "pole_r", "zero_r"],
        "slaving": {"zero_hz": "rides the pole at the measured interval"},
    },
    "close pair": {
        "structure": "conjugate pole and zero within 6 semitones at comparable width — nearly cancelling, a shallow tilt",
        "free": ["freq", "resonance", "gain"],
        "freedom": ["pole_hz", "pole_r", "zero_r"],
        "slaving": {"zero_hz": "rides the pole at the measured interval"},
    },
    "low shelf": {
        "structure": "conjugate pole with its conjugate zero more than 6 semitones above it — lows held up against a high null",
        "free": ["freq", "resonance", "zero freq", "zero resonance", "gain"],
        "freedom": ["pole_hz", "pole_r", "zero_hz", "zero_r"],
        "slaving": None,
    },
    "high shelf": {
        "structure": "conjugate pole with its conjugate zero more than 6 semitones below it — highs held up against a low null",
        "free": ["freq", "resonance", "zero freq", "zero resonance", "gain"],
        "freedom": ["pole_hz", "pole_r", "zero_hz", "zero_r"],
        "slaving": None,
    },
}


def gmean(xs):
    xs = [x for x in xs if x and x > 0]
    return math.exp(sum(math.log(x) for x in xs) / len(xs)) if xs else None


def span(xs, digits=1):
    xs = sorted(x for x in xs if x is not None)
    if not xs:
        return None
    return {"min": round(xs[0], digits), "median": round(xs[len(xs) // 2], digits), "max": round(xs[-1], digits)}


alphabet = json.load(open("recipes/alphabet.json"))["anchors"]
stem_parts = defaultdict(dict)
letter_positions = defaultdict(set)
for lname, a in alphabet.items():
    for m in re.finditer(r"\bS([1-7])\b", a.get("source", "")):
        letter_positions[lname].add(int(m.group(1)))
    m = re.match(r"^(.*?)-(pole|zero)(-\d+)?$", lname)
    if m:
        stem_parts[m.group(1) + (m.group(3) or "")][m.group(2)] = lname
    else:
        stem_parts[lname]["solo"] = lname

letter_shape = {}
for stem, parts in stem_parts.items():
    pole = alphabet[parts["pole"]] if "pole" in parts else {"hz": 0.3, "r": 0.0}
    zero = alphabet[parts["zero"]] if "zero" in parts else {"hz": 0.3, "r": 0.0}
    if "solo" in parts:
        a = alphabet[parts["solo"]]
        if "zero" in parts["solo"] or "notch" in parts["solo"]:
            pole, zero = {"hz": 0.3, "r": 0.0}, a
        else:
            pole, zero = a, {"hz": 0.3, "r": 0.0}
    sh = classify(pole, zero, SR_39K)
    for lname in parts.values():
        letter_shape[lname] = sh

census = json.load(open("ref/stage_state_census.json"))
cluster_shape = []
for sname, st in census["strata"].items():
    for i, cl in enumerate(st["clusters"]):
        pole = {"hz": cl["pole_hz"] or 0.3, "r": cl["pole_r_at_39062_5"] or 0.0}
        zbw = cl["zero_bw_hz"]
        zero = {"hz": cl["zero_hz"] or 0.3, "r": r_at(zbw, SR_39K) if zbw else 0.0}
        cluster_shape.append({
            "id": f"{sname}-{i}",
            "shape": classify(pole, zero, SR_39K),
            "occurrences": cl["occurrences"],
            "positions": {k: v for k, v in cl["stage_position_histogram"].items()},
        })

by_pos = defaultdict(lambda: defaultdict(list))
for r in rows:
    by_pos[r["stage"] + 1][r["shape"]].append(r)

positions = {}
dropped = []
coverage = {}
for pos in sorted(by_pos):
    total = sum(len(v) for v in by_pos[pos].values())
    entries = []
    kept = 0
    for shname, members in sorted(by_pos[pos].items(), key=lambda kv: -len(kv[1])):
        filters = sorted(set(m["name"] for m in members))
        srcs = Counter(m["src"] for m in members)
        if len(members) < MIN_OCC or len(filters) < MIN_FILTERS:
            dropped.append({"position": f"S{pos}", "shape": shname, "occurrences": len(members), "distinct_filters": len(filters)})
            continue
        kept += len(members)
        p_on = [m for m in members if m["pole"]["r"] >= ROOT_OFF_R]
        z_on = [m for m in members if m["zero"]["r"] >= ROOT_OFF_R]
        p_hz = gmean([m["pole"]["hz"] for m in p_on])
        p_bw = gmean([bw_hz(m["pole"]["r"], m["sr"]) for m in p_on])
        z_hz = gmean([m["zero"]["hz"] for m in z_on])
        z_bw = gmean([bw_hz(m["zero"]["r"], m["sr"]) for m in z_on])
        both = [m for m in members if m["pole"]["r"] >= ROOT_OFF_R and m["zero"]["r"] >= ROOT_OFF_R]
        ints = sorted(interval_st(m["pole"], m["zero"]) for m in both)
        gains = [m["gain_db"] for m in members if m["gain_db"] is not None]
        seat = {
            "pole_hz": round(p_hz, 2) if p_hz else 0.3,
            "pole_r": round(r_at(p_bw, SR_39K), 6) if p_bw else 0.0,
            "zero_hz": round(z_hz, 2) if z_hz else 0.3,
            "zero_r": round(r_at(z_bw, SR_39K), 6) if z_bw else 0.0,
        }
        if shname == "guarded peak":
            seat["zero_r"] = 1.0
        if shname == "idle":
            seat = {"pole_hz": 9381.77, "pole_r": 0.125031, "zero_hz": 0.3, "zero_r": 0.0}
        ranges = {}
        if p_on:
            ranges["pole_hz"] = span([m["pole"]["hz"] for m in p_on])
            ranges["pole_bw_hz"] = span([bw_hz(m["pole"]["r"], m["sr"]) for m in p_on])
        if z_on:
            ranges["zero_hz"] = span([m["zero"]["hz"] for m in z_on])
            ranges["zero_bw_hz"] = span([bw_hz(m["zero"]["r"], m["sr"]) for m in z_on])
        if gains:
            ranges["gain_db"] = span(gains, 2)
        entry = {
            "name": shname,
            "seat": seat,
            "ranges": ranges,
            "evidence": {
                "occurrences": len(members),
                "distinct_filters": len(filters),
                "by_corpus": dict(srcs),
                "corpora_spanned": len(srcs),
                "exemplar_filters": filters[:8],
                "clusters": [c["id"] for c in cluster_shape if c["shape"] == shname and f"S{pos}" in c["positions"]],
                "letters": sorted(l for l, s in letter_shape.items() if s == shname and (not letter_positions[l] or pos in letter_positions[l])),
            },
        }
        if ints:
            entry["slaving"] = {
                "interval_st_median": round(ints[len(ints) // 2], 2),
                "interval_st_p25": round(ints[len(ints) // 4], 2),
                "interval_st_p75": round(ints[(3 * len(ints)) // 4], 2),
            }
        entries.append(entry)
    positions[f"S{pos}"] = {"stage_instances": total, "shapes": entries}
    coverage[f"S{pos}"] = {"instances": total, "classified": kept, "share": round(kept / total, 4)}

out = {
    "contract": "stage-vocabulary-v1: the shapes E-mu actually seated at each cascade position, derived from the same corpora as the stage-state census by geometry alone (root presence, radius, the pole/zero interval in semitones and the width ratio). Every measured figure is physical at the row's own corpus datum; seats and ranges are stated at 39,062.5 Hz. Shapes are the picker vocabulary; alphabet letters and census clusters are presets of a shape at the positions the factory used them.",
    "generated_by": "dev/derive_stage_vocabulary.py",
    "sources": ["ref/morpheus/cubes_decoded.json", "ref/presets/*.bin", "ref/cubes/*.body", "recipes/hero/*.body", "ref/stage_state_census.json", "recipes/alphabet.json"],
    "cuts": {
        "root_off_r": ROOT_OFF_R,
        "unit_zero_r": UNIT_R,
        "near_interval_st": NEAR_ST,
        "width_ratio_octaves": BW_RATIO_OCT,
        "sharp_bw_hz": SHARP_BW_HZ,
        "menu_floor": {"occurrences": MIN_OCC, "distinct_filters": MIN_FILTERS},
    },
    "shapes": SHAPES,
    "positions": positions,
    "coverage": coverage,
    "below_menu_floor": dropped,
    "letter_shapes": letter_shape,
    "cluster_shapes": cluster_shape,
}
json.dump(out, open("ref/stage_vocabulary.json", "w"), indent=1)

print("=" * 92)
print("STAGE VOCABULARY — shapes per cascade position")
print("=" * 92)
print(f"stage instances: {len(rows)}   shapes defined: {len(SHAPES)}")
for pos in sorted(positions):
    cov = coverage[pos]
    print(f"\n{pos}  {cov['instances']} instances  coverage {cov['share'] * 100:.2f}%  ({len(positions[pos]['shapes'])} shapes)")
    for e in positions[pos]["shapes"]:
        ev = e["evidence"]
        print(f"  {e['name']:14s} {ev['occurrences']:5d} occ  {ev['distinct_filters']:3d} filters  {ev['corpora_spanned']} corpora  "
              f"seat p{e['seat']['pole_hz']:.0f}Hz r{e['seat']['pole_r']:.4f} z{e['seat']['zero_hz']:.0f}Hz r{e['seat']['zero_r']:.4f}  "
              f"{len(ev['letters'])} letters {len(ev['clusters'])} clusters")
print("\nbelow the menu floor (escape path only):")
for d in dropped:
    print(f"  {d['position']} {d['shape']}: {d['occurrences']} occ, {d['distinct_filters']} filters")
tot = sum(c["instances"] for c in coverage.values())
cls = sum(c["classified"] for c in coverage.values())
print(f"\ntotal {cls}/{tot} = {cls / tot * 100:.2f}%   letters classified {len(letter_shape)}/{len(alphabet)}   clusters {len(cluster_shape)}/38")
print("-> ref/stage_vocabulary.json")
