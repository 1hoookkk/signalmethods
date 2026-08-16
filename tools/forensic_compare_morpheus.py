import json, io, math, os, random, bisect
import numpy as np
from collections import defaultdict

DATUM = 39062.5
GRID_N = 512
FLO, FHI = 40.0, 16000.0
CO_CENTS = 50.0
NULL_TRIALS = 400
R_MIN = 1e-6
random.seed(1999)

def bw_of_r(r, sr=DATUM):
    return -math.log(max(r, 1e-12)) * sr / math.pi

H = []
def rec(name, source, roots):
    H.append({"name": name, "source": source, "roots": roots})
def P(f, b=None): return ("p", float(f), b)
def Z(f, b=None): return ("z", float(f), b)

rec("fant s (2p1z)", "Fant & Martony STL-QPSR 1/1960 Fig I-7",
    [P(5800, 580), P(8000, 2500), Z(4500, 1800)])
rec("fant f", "Fant & Martony 1/1960 Fig I-7",
    [P(2700, 1500), P(15000, 4687.5), Z(2500, 1388.9)])
rec("fant sh (full)", "Fant & Martony 1/1960 Fig I-8",
    [P(1500, 468.75), P(2200, 550), P(2800, 280), P(6000, 3000), P(7000, 7000),
     Z(1000, 400), Z(1900, 593.75), Z(2500, 625), Z(5000, 2500)])
rec("fant s (full)", "Fant & Martony 1/1960 Fig I-8",
    [P(2500, 250), P(4200, 1312.5), P(5200, 945.5), P(8500, 6071.4),
     Z(2200, 687.5), Z(3000, 937.5), Z(4700, 1468.75)])
rec("martony l after a", "Martony & Fant STL-QPSR 1/1961 Table I-1",
    [P(375), P(1250), P(2400), P(3250), P(3600), Z(2700)])
rec("martony l after I", "Martony & Fant 1/1961 Table I-1",
    [P(235), P(1600), P(2300), P(2770), P(3260), Z(1900)])
rec("martony l after E", "Martony & Fant 1/1961 Table I-1",
    [P(290), P(1450), P(2250), P(2700), P(3100), Z(2000)])
rec("martony l after u:", "Martony & Fant 1/1961 Table I-1",
    [P(265), P(1600), P(2380), P(2900), P(3300), Z(2500)])
rec("martony l after oe", "Martony & Fant 1/1961 Table I-1",
    [P(280), P(1475), P(2280), P(2460), P(3250), Z(2060)])
rec("fujimura schwa", "Fujimura & Lindqvist STL-QPSR 3/1964",
    [P(480, 33), P(1450, 42), P(2300, 62), P(3500), P(4000)])
rec("fujimura nasalized schwa", "Fujimura & Lindqvist 3/1964",
    [P(517, 56), P(1230, 55), P(1830, 154), P(270, 46), P(2700), P(5000), Z(312, 60)])
rec("bell vowel I", "Bell et al. JASA 1961 Fig 9",
    [P(430, 30), P(1870, 80), P(2580, 150), P(3400, 120)])
rec("klatt nasal pair", "Klatt 1980 cascade constants",
    [P(250, 100), Z(250, 100)])
rec("klatt glottal zero", "Klatt 1980 source stage",
    [Z(1500, 6000)])
rec("kerkhoff o (bot)", "Segers & Verhoeven 2005 Table 1, steady o",
    [P(460, 100), P(950, 100), P(2200, 130), P(3800, 150)])
rec("kerkhoff a (bart)", "Segers & Verhoeven 2005 Table 1, steady a",
    [P(780, 200), P(1320, 200), P(2600, 200), P(3500, 200)])
rec("paravowel A", "Morpheus module manual p36 (peaks only)",
    [P(800), P(1150), P(2800), P(3500), P(4950)])
rec("paravowel E", "Morpheus module manual p36",
    [P(400), P(1600), P(2700), P(3300), P(4900)])
rec("paravowel O", "Morpheus module manual p36",
    [P(450), P(800), P(2830), P(3500), P(4950)])
rec("paravowel U", "Morpheus module manual p36",
    [P(325), P(700), P(2530), P(3500), P(4950)])
rec("manual.md f variant", "manual.md section 3 (deviates from 1960 paper)",
    [P(2700, 465.5), P(15000, 4545.5), Z(2500, 431.0)])
rec("manual.md s variant", "manual.md section 3 (deviates from 1960 paper)",
    [P(5800, 580), P(8000, 2666.7), Z(4500, 1800)])

cube_doc = json.load(io.open(os.path.join("ref", "morpheus", "cubes_decoded.json"), encoding="utf-8"))

def null_stage(s):
    if s["raw"][0] == s["raw"][2] and s["raw"][1] == s["raw"][3]:
        return True
    return s["pole"]["r"] <= R_MIN and s["zero"]["r"] <= R_MIN

corners = {}
for cu in cube_doc["cubes"]:
    for ci, c in enumerate(cu["corners"]):
        secs = [s for s in c["sections"] if not null_stage(s)]
        if not secs:
            continue
        roots = []
        for si, s in enumerate(secs):
            if s["pole"]["r"] > R_MIN:
                roots.append(("p", s["pole"]["hz"], s["pole"]["r"], si + 1))
            if s["zero"]["r"] > R_MIN:
                roots.append(("z", s["zero"]["hz"], s["zero"]["r"], si + 1))
        if roots:
            corners[(cu["name"], f"c{ci}")] = roots

corner_keys = list(corners.keys())
n_corners = len(corner_keys)

flat = {"p": [], "z": []}
for cid, key in enumerate(corner_keys):
    for kind, hz, r, slot in corners[key]:
        if hz > 1.0:
            flat[kind].append((math.log2(hz), cid, hz, r, slot))
for k in flat:
    flat[k].sort()
logf = {k: np.array([t[0] for t in flat[k]]) for k in flat}
meta = {k: flat[k] for k in flat}
print(f"corpus: {n_corners} active corners, poles {len(flat['p'])}, zeros {len(flat['z'])}")

THR = CO_CENTS / 1200.0

def co_best(roots):
    per_corner = defaultdict(list)
    for hi, (kind, f, b) in enumerate(roots):
        lf = math.log2(f)
        arr = logf[kind]
        lo = bisect.bisect_left(arr, lf - THR)
        hi_i = bisect.bisect_right(arr, lf + THR)
        m = meta[kind]
        for j in range(lo, hi_i):
            _, cid, hz, r, slot = m[j]
            c = abs(1200.0 * (m[j][0] - lf))
            per_corner[cid].append((c, hi, j, kind, hz, r, slot))
    best_k, best_cid, best_m = 0, None, []
    for cid, cands in per_corner.items():
        hit_h = set(x[1] for x in cands)
        hit_c = set((x[3], x[2]) for x in cands)
        if min(len(hit_h), len(hit_c)) <= best_k:
            continue
        cands.sort()
        got, used, m = set(), set(), []
        for c, hi2, j, kind, hz, r, slot in cands:
            if hi2 in got or (kind, j) in used:
                continue
            got.add(hi2)
            used.add((kind, j))
            m.append((c, hi2, kind, hz, r, slot))
        if len(m) > best_k:
            best_k, best_cid, best_m = len(m), cid, m
    return best_k, best_cid, best_m

w = 2 * np.pi * (FLO * (FHI / FLO) ** (np.arange(GRID_N) / (GRID_N - 1))) / DATUM
zr, zi = np.cos(w), -np.sin(w)
z2r, z2i = np.cos(2 * w), -np.sin(2 * w)

def curve_db(num, den):
    db = np.zeros(GRID_N)
    for (b1, b2) in num:
        db += 20 * np.log10(np.maximum(np.hypot(1 + b1 * zr + b2 * z2r, b1 * zi + b2 * z2i), 1e-15))
    for (a1, a2) in den:
        db -= 20 * np.log10(np.maximum(np.hypot(1 + a1 * zr + a2 * z2r, a1 * zi + a2 * z2i), 1e-15))
    return db - db.mean()

def conj(hz, r):
    th = 2 * math.pi * hz / DATUM
    return (-2 * r * math.cos(th), r * r)

M = np.zeros((n_corners, GRID_N), dtype=np.float64)
for cid, key in enumerate(corner_keys):
    num = [conj(hz, r) for kind, hz, r, slot in corners[key] if kind == "z"]
    den = [conj(hz, r) for kind, hz, r, slot in corners[key] if kind == "p"]
    M[cid] = curve_db(num, den)

def r_of_bw(b):
    return math.exp(-math.pi * b / DATUM)

def hist_curve(roots):
    num, den = [], []
    for kind, f, b in roots:
        if b is None:
            return None
        (num if kind == "z" else den).append(conj(f, r_of_bw(b)))
    return curve_db(num, den)

def best_rms(c):
    d = M - c
    return float(np.sqrt((d * d).mean(axis=1)).min()), int(np.argmin(np.sqrt((d * d).mean(axis=1))))

def null_roots(roots):
    out = []
    for kind, f, b in roots:
        nf = math.exp(random.uniform(math.log(150), math.log(16000)))
        nb = None if b is None else math.exp(random.uniform(math.log(30), math.log(6000)))
        out.append((kind, nf, nb))
    return out

lines = []
say = lines.append
say("# Forensic comparison: historical recipes vs the decoded Morpheus cube corpus")
say("")
say(f"Corpus: all 289 decoded Morpheus module cubes (ref/morpheus/cubes_decoded.json),")
say(f"{n_corners} non-null corners, {len(flat['p'])} poles and {len(flat['z'])} zeros")
say(f"(roots with r > {R_MIN} in non-identity stages), frequencies at the 39,062.5 Hz")
say("datum. Same recipes, metrics, thresholds, null model, and seed as the P2K run")
say("(tools/forensic_compare.py): co-occurrence within 50 cents greedy-matched per")
say("corner; whole-response rms on mean-removed dB curves, 512-point log grid")
say("40-16,000 Hz; 400 null recipes per historical recipe, frequencies log-uniform")
say("150-16,000 Hz, bandwidths log-uniform 30-6,000 Hz, seed 1999. The two ParaVowel")
say("recipes are the module manual's own p36 peak lists and act as positive controls:")
say("AEParaVowel and AOParaVowel are cubes in this corpus.")
say("")
say("## Observed matches and significance")
say("")
say("| recipe | best k / n | corner | p(co-occ) | best rms dB | corner | p(rms) |")
say("|---|---|---|---|---|---|---|")

results = []
for h in H:
    obs_k, obs_cid, obs_m = co_best(h["roots"])
    hc = hist_curve(h["roots"])
    obs_rms, rms_cid = (None, None)
    if hc is not None:
        obs_rms, rms_cid = best_rms(hc)
    ge, le = 0, 0
    for _ in range(NULL_TRIALS):
        nr = null_roots(h["roots"])
        nk, _, _ = co_best(nr)
        if nk >= obs_k:
            ge += 1
        if obs_rms is not None:
            nrm, _ = best_rms(hist_curve(nr))
            if nrm <= obs_rms:
                le += 1
    p_co = ge / NULL_TRIALS
    p_rm = (le / NULL_TRIALS) if obs_rms is not None else None
    results.append((h, obs_k, obs_cid, obs_m, p_co, obs_rms, rms_cid, p_rm))
    ck = f"{corner_keys[obs_cid][0]} · {corner_keys[obs_cid][1]}" if obs_cid is not None else "—"
    cr = f"{corner_keys[rms_cid][0]} · {corner_keys[rms_cid][1]}" if rms_cid is not None else "—"
    rm_s = f"{obs_rms:.2f}" if obs_rms is not None else "—"
    pr_s = f"{p_rm:.3f}" if p_rm is not None else "—"
    say(f"| {h['name']} | {obs_k}/{len(h['roots'])} | {ck} | {p_co:.3f} | {rm_s} | {cr} | {pr_s} |")
    print(f"{h['name']}: k={obs_k}/{len(h['roots'])} @ {ck} p={p_co:.3f}"
          + (f" | rms {obs_rms:.2f} @ {cr} p={p_rm:.3f}" if obs_rms is not None else ""))

say("")
say("## Detail of the strongest co-occurrences")
say("")
for h, obs_k, obs_cid, obs_m, p_co, obs_rms, rms_cid, p_rm in sorted(results, key=lambda t: t[4]):
    if obs_k < 3 or p_co >= 0.2:
        continue
    say(f"**{h['name']}** ({h['source']}) — {obs_k} of {len(h['roots'])} roots at "
        f"{corner_keys[obs_cid][0]} · {corner_keys[obs_cid][1]} (p = {p_co:.3f}):")
    for c, hi2, kind, hz, r, slot in sorted(obs_m):
        f = h["roots"][hi2][1]
        say(f"- {('pole' if kind=='p' else 'zero')} {f:.0f} Hz -> S{slot} {hz:.1f} Hz "
            f"r={r:.5f} (B {bw_of_r(r):.0f} Hz), {c:.1f} cents")
    say("")

say("## Verdicts")
say("")
strong = [(h["name"], k, pco, orms, prm) for h, k, cid, m, pco, orms, rcid, prm in results
          if pco < 0.05 or (prm is not None and prm < 0.05)]
if strong:
    say("Recipes whose best cube match is unlikely under the null (p < 0.05):")
    for name, k, pco, orms, prm in sorted(strong, key=lambda s: min(s[2], s[4] if s[4] is not None else 1.0)):
        say(f"- {name}: co-occurrence k={k} (p={pco:.3f})"
            + (f", whole-response rms {orms:.2f} dB (p={prm:.3f})" if prm is not None else ""))
else:
    say("No historical recipe matches the cube corpus better than randomized null")
    say("recipes at p < 0.05 on either metric.")
say("")

out = os.path.join("dev", "forensics_report_morpheus.md")
io.open(out, "w", encoding="utf-8", newline="\n").write("\n".join(lines) + "\n")
print(f"wrote {out}")
