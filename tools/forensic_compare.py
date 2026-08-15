import json, glob, io, math, os, random, bisect
from collections import defaultdict

DATUM = 39062.5
GRID_N = 512
FLO, FHI = 40.0, 16000.0
NEAR_CENTS = 30.0
CO_CENTS = 50.0
BW_RATIO = 1.5
NULL_TRIALS = 400
random.seed(1999)

def cents(f1, f2):
    return 1200.0 * math.log2(f1 / f2)

def bw_of_r(r, sr=DATUM):
    return -math.log(max(r, 1e-12)) * sr / math.pi

def r_of_bw(b, sr=DATUM):
    return math.exp(-math.pi * b / sr)

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

corners = {}
real_pair_count = 0
for fp in sorted(glob.glob(os.path.join("recipes", "architectures", "*.json"))):
    d = json.load(io.open(fp, encoding="utf-8"))
    name = d["name"]
    per_corner = defaultdict(list)
    for sec in d["sections"]:
        slot = sec["slot"]
        for cl, c in sec["corners"].items():
            entry = {"slot": slot, "scale": c.get("scale", 1.0),
                     "pole": None, "zero": None,
                     "pole_pair": None, "zero_pair": None}
            for kind in ("pole", "zero"):
                v = c.get(kind)
                if v is None:
                    continue
                if "pair" in v:
                    entry[kind + "_pair"] = tuple(v["pair"])
                    real_pair_count += 1
                elif v["r"] > 0:
                    entry[kind] = (v["hz"], v["r"])
            per_corner[cl].append(entry)
    for cl, secs in per_corner.items():
        corners[(name, cl)] = secs

def corner_roots(secs):
    out = []
    for e in secs:
        if e["pole"]:
            out.append(("p", e["pole"][0], e["pole"][1], e["slot"]))
        if e["zero"]:
            out.append(("z", e["zero"][0], e["zero"][1], e["slot"]))
    return out

all_roots = []
for (name, cl), secs in corners.items():
    for kind, hz, r, slot in corner_roots(secs):
        if hz > 1.0:
            all_roots.append((kind, hz, r, name, cl, slot))

by_kind = {"p": sorted([t for t in all_roots if t[0] == "p"], key=lambda t: t[1]),
           "z": sorted([t for t in all_roots if t[0] == "z"], key=lambda t: t[1])}
keys = {k: [t[1] for t in v] for k, v in by_kind.items()}

def nearest(kind, f):
    arr, ks = by_kind[kind], keys[kind]
    i = bisect.bisect_left(ks, f)
    best = None
    for j in (i - 1, i, i + 1):
        if 0 <= j < len(arr):
            c = abs(cents(arr[j][1], f))
            if best is None or c < best[0]:
                best = (c, arr[j])
    return best

def fmt_root(t):
    kind, hz, r, name, cl, slot = t
    return f"{name} · {cl} · S{slot} {'pole' if kind=='p' else 'zero'} {hz:.1f} Hz r={r:.5f} (B {bw_of_r(r):.0f} Hz)"

def co_occurrence(roots, secs):
    cr = corner_roots(secs)
    matched = []
    used = set()
    cands = []
    for hi, (kind, f, b) in enumerate(roots):
        for ci, (ck, chz, crr, cslot) in enumerate(cr):
            if ck != kind:
                continue
            c = abs(cents(chz, f))
            if c <= CO_CENTS:
                cands.append((c, hi, ci))
    cands.sort()
    got = set()
    for c, hi, ci in cands:
        if hi in got or ci in used:
            continue
        got.add(hi)
        used.add(ci)
        matched.append((c, hi, ci, cr[ci]))
    return matched

def resp_curve(pairs_num, pairs_den, scale_total):
    pts = []
    for i in range(GRID_N):
        f = FLO * (FHI / FLO) ** (i / (GRID_N - 1))
        w = 2 * math.pi * f / DATUM
        zr, zi = math.cos(w), -math.sin(w)
        z2r, z2i = math.cos(2 * w), -math.sin(2 * w)
        num = 1.0
        for (b1, b2) in pairs_num:
            re = 1 + b1 * zr + b2 * z2r
            im = b1 * zi + b2 * z2i
            num *= math.hypot(re, im)
        den = 1.0
        for (a1, a2) in pairs_den:
            re = 1 + a1 * zr + a2 * z2r
            im = a1 * zi + a2 * z2i
            den *= math.hypot(re, im)
        mag = scale_total * num / max(den, 1e-30)
        pts.append(20 * math.log10(max(mag, 1e-15)))
    m = sum(pts) / len(pts)
    return [v - m for v in pts]

def conj_coeffs(hz, r):
    th = 2 * math.pi * hz / DATUM
    return (-2 * r * math.cos(th), r * r)

def hist_curve(roots):
    num, den = [], []
    for kind, f, b in roots:
        if b is None:
            return None
        r = r_of_bw(b)
        (num if kind == "z" else den).append(conj_coeffs(f, r))
    return resp_curve(num, den, 1.0)

def corner_curve(secs):
    num, den, scale = [], [], 1.0
    for e in secs:
        scale *= e["scale"] if e["scale"] > 0 else 1.0
        if e["pole"]:
            den.append(conj_coeffs(*e["pole"]))
        if e["pole_pair"]:
            a, b = e["pole_pair"]
            den.append((-(a + b), a * b))
        if e["zero"]:
            num.append(conj_coeffs(*e["zero"]))
        if e["zero_pair"]:
            a, b = e["zero_pair"]
            num.append((-(a + b), a * b))
    return resp_curve(num, den, scale)

def rms(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)) / len(a))

corner_curves = {k: corner_curve(v) for k, v in corners.items()}

def null_roots(roots):
    out = []
    for kind, f, b in roots:
        nf = math.exp(random.uniform(math.log(150), math.log(16000)))
        nb = None if b is None else math.exp(random.uniform(math.log(30), math.log(6000)))
        out.append((kind, nf, nb))
    return out

lines = []
say = lines.append
say("# Forensic comparison: historical recipes vs the decoded E-mu corpus")
say("")
say("Corpus: the 33 null-verified P2K architecture recipes, every decoded corner")
say(f"({len(corners)} corners, {len(all_roots)} conjugate roots; {real_pair_count} real-pair root entries")
say("participate in whole-response curves but are excluded from F/B root matching, since")
say("historical sources specify conjugate F/Q roots). No decoded Morpheus module cubes")
say("exist in this repository; the Morpheus manual contributes recipes, not corners.")
say("All corpus bandwidths decoded at the 39,062.5 Hz datum. No slot identity or")
say("pole-zero pairing is assumed anywhere; pairs are searched across all sections of")
say("a corner. manual.md values that deviate from their primary papers are encoded as")
say("separate labeled variants.")
say("")

say("## 1. Nearest individual root matches (frequency, same kind)")
say("")
best_roots = []
for h in H:
    for kind, f, b in h["roots"]:
        n = nearest(kind, f)
        if n:
            best_roots.append((n[0], h["name"], kind, f, b, n[1]))
best_roots.sort(key=lambda t: t[0])
say("| cents | historical root | corpus state |")
say("|---|---|---|")
for c, hn, kind, f, b, t in best_roots[:25]:
    bs = f" B {b:.0f}" if b else ""
    say(f"| {c:.1f} | {hn}: {'pole' if kind=='p' else 'zero'} {f:.0f} Hz{bs} | {fmt_root(t)} |")
say("")

say("## 2. Matches in both frequency and damping")
say(f"(frequency within {NEAR_CENTS:.0f} cents AND bandwidth ratio within {BW_RATIO}x)")
say("")
fb = []
for h in H:
    for kind, f, b in h["roots"]:
        if b is None:
            continue
        arr = by_kind[kind]
        for t in arr:
            c = abs(cents(t[1], f))
            if c <= NEAR_CENTS:
                ratio = max(bw_of_r(t[2]) / b, b / max(bw_of_r(t[2]), 1e-9))
                if ratio <= BW_RATIO:
                    fb.append((c, ratio, h["name"], kind, f, b, t))
fb.sort(key=lambda t: (t[0] + 30 * (t[1] - 1)))
say("| cents | B ratio | historical root | corpus state |")
say("|---|---|---|---|")
for c, ratio, hn, kind, f, b, t in fb[:20]:
    say(f"| {c:.1f} | {ratio:.2f} | {hn}: {'pole' if kind=='p' else 'zero'} {f:.0f} Hz B {b:.0f} | {fmt_root(t)} |")
if not fb:
    say("| — | — | no root matches both frequency and damping | |")
say("")

say("## 3. Pole-zero spacing matches, and the bound-pair census")
say("")
say("Historical pole-zero pairs (all combinations within a recipe, no pairing assumed)")
say("matched against all cross-section pole-zero pairs of each corner: both endpoints")
say(f"within {CO_CENTS:.0f} cents. These appear in section 4 as 2-of-k co-occurrences.")
say("")
bound = []
for (name, cl), secs in corners.items():
    cr = corner_roots(secs)
    for pk, phz, pr, ps in cr:
        if pk != "p":
            continue
        for zk, zhz, zr_, zs in cr:
            if zk != "z":
                continue
            if abs(phz - zhz) <= 200.0:
                bound.append((abs(phz - zhz), name, cl, ps, phz, pr, zs, zhz, zr_))
bound.sort()
say(f"Bound-pair signature (pole and zero within 200 Hz, any sections): {len(bound)}")
say("instances in the corpus. Tightest 10:")
say("")
say("| dF Hz | corner | pole | zero |")
say("|---|---|---|---|")
for dfq, name, cl, ps, phz, pr, zs, zhz, zr_ in bound[:10]:
    say(f"| {dfq:.0f} | {name} · {cl} | S{ps} {phz:.0f} Hz r={pr:.4f} | S{zs} {zhz:.0f} Hz r={zr_:.4f} |")
say("")

say("## 4. Multi-root co-occurrence within a corner")
say(f"(each historical root matched to a distinct corner root of the same kind, within {CO_CENTS:.0f} cents)")
say("")
co_results = []
for h in H:
    best = None
    for key, secs in corners.items():
        m = co_occurrence(h["roots"], secs)
        if best is None or len(m) > len(best[1]):
            best = (key, m)
    co_results.append((h, best))
co_results.sort(key=lambda t: -len(t[1][1]))
for h, (key, m) in co_results:
    k = len(m)
    n = len(h["roots"])
    if k < 3:
        continue
    say(f"**{h['name']}** ({h['source']}): best corner matches {k} of {n} roots —")
    say(f"{key[0]} · {key[1]}")
    for c, hi, ci, (ck, chz, crr, cslot) in sorted(m):
        kind, f, b = h["roots"][hi]
        say(f"- {('pole' if kind=='p' else 'zero')} {f:.0f} Hz -> S{cslot} {chz:.1f} Hz "
        f"r={crr:.5f} (B {bw_of_r(crr):.0f} Hz), {c:.1f} cents")
    say("")

say("## 5. Whole-response similarity")
say("(mean-removed dB curves at the datum; only recipes whose every root has a published")
say("bandwidth — frequency-only recipes are skipped rather than given invented damping)")
say("")
wr_results = []
for h in H:
    hc = hist_curve(h["roots"])
    if hc is None:
        continue
    ranked = sorted(((rms(hc, cc), key) for key, cc in corner_curves.items()))
    wr_results.append((h, ranked[:3]))
wr_results.sort(key=lambda t: t[1][0][0])
say("| historical recipe | best corner | rms dB | runners-up |")
say("|---|---|---|---|")
for h, top in wr_results:
    r0 = top[0]
    ru = "; ".join(f"{k[0]} · {k[1]} ({v:.1f})" for v, k in top[1:])
    say(f"| {h['name']} | {r0[1][0]} · {r0[1][1]} | {r0[0]:.2f} | {ru} |")
say("")

say("## 6. Significance against randomized null recipes")
say(f"({NULL_TRIALS} null recipes per historical recipe, same root counts and kinds,")
say("frequencies log-uniform 150-16000 Hz, bandwidths log-uniform 30-6000 Hz, seed 1999;")
say("p = fraction of nulls matching the corpus at least as well)")
say("")
say("| historical recipe | co-occurrence best k | p(co-occ) | best rms dB | p(rms) |")
say("|---|---|---|---|---|")
signif = []
for h in H:
    obs_best = 0
    for key, secs in corners.items():
        obs_best = max(obs_best, len(co_occurrence(h["roots"], secs)))
    hc = hist_curve(h["roots"])
    obs_rms = None
    if hc is not None:
        obs_rms = min(rms(hc, cc) for cc in corner_curves.values())
    ge = 0
    le = 0
    for _ in range(NULL_TRIALS):
        nr = null_roots(h["roots"])
        nb = 0
        for key, secs in corners.items():
            nb = max(nb, len(co_occurrence(nr, secs)))
            if nb >= obs_best:
                break
        if nb >= obs_best:
            ge += 1
        if obs_rms is not None:
            ncur = hist_curve(nr)
            nrm = min(rms(ncur, cc) for cc in corner_curves.values())
            if nrm <= obs_rms:
                le += 1
    p_co = ge / NULL_TRIALS
    p_rm = (le / NULL_TRIALS) if obs_rms is not None else None
    signif.append((h["name"], obs_best, p_co, obs_rms, p_rm))
    rm_s = f"{obs_rms:.2f}" if obs_rms is not None else "—"
    pr_s = f"{p_rm:.3f}" if p_rm is not None else "—"
    say(f"| {h['name']} | {obs_best} | {p_co:.3f} | {rm_s} | {pr_s} |")
say("")

say("## 7. Ranked verdicts")
say("")
strong = [s for s in signif if s[2] < 0.05 or (s[4] is not None and s[4] < 0.05)]
if strong:
    say("Recipes whose best corpus match is unlikely under the null (p < 0.05):")
    for name, k, pco, orms, prm in sorted(strong, key=lambda s: min(s[2], s[4] if s[4] is not None else 1.0)):
        say(f"- {name}: co-occurrence k={k} (p={pco:.3f})"
            + (f", whole-response rms {orms:.2f} dB (p={prm:.3f})" if prm is not None else ""))
else:
    say("No historical recipe matches the corpus better than randomized null recipes at")
    say("p < 0.05 on either metric. The corpus does not carry these recipes' numbers.")
say("")

out = os.path.join("dev", "forensics_report.md")
os.makedirs("dev", exist_ok=True)
io.open(out, "w", encoding="utf-8", newline="\n").write("\n".join(lines) + "\n")
print(f"wrote {out}")
print(f"corners {len(corners)}, roots {len(all_roots)}, historical recipes {len(H)}")
for name, k, pco, orms, prm in signif:
    rm_s = f" rms {orms:.2f} p={prm:.3f}" if orms is not None else ""
    print(f"  {name}: k={k} p={pco:.3f}{rm_s}")
