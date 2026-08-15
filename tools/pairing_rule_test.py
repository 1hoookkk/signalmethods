import json, glob, io, math, random
random.seed(1999)

def load():
    out = []
    for fp in sorted(glob.glob("recipes/architectures/*.json")):
        d = json.load(io.open(fp, encoding="utf-8"))
        corners = {}
        for sec in d["sections"]:
            for cl, c in sec["corners"].items():
                p = c.get("pole"); z = c.get("zero")
                if p is None or z is None or "pair" in p or "pair" in z:
                    continue
                if p["r"] <= 0 or z["r"] <= 0:
                    continue
                corners.setdefault(cl, []).append(
                    (sec["slot"], p["hz"], p["r"], z["hz"], z["r"]))
        for cl, secs in corners.items():
            if len(secs) >= 2:
                out.append((d["name"], cl, secs))
    return out

def predict(secs, dist):
    poles = sorted(range(len(secs)), key=lambda i: -secs[i][2])
    zeros = list(range(len(secs)))
    assign = {}
    for pi in poles:
        best = min(zeros, key=lambda zi: dist(secs[pi][1], secs[zi][3]))
        assign[pi] = best
        zeros.remove(best)
    return assign

def run(dist, label, corners):
    total_pairs = 0
    agree = 0
    near = 0
    exact_corners = 0
    per_corner = []
    for name, cl, secs in corners:
        a = predict(secs, dist)
        k = len(secs)
        good = sum(1 for i, j in a.items() if i == j)
        nr = sum(1 for i, j in a.items()
                 if i != j and abs(1200 * math.log2(secs[j][3] / secs[i][3])) < 30)
        total_pairs += k
        agree += good
        near += nr
        if good == k:
            exact_corners += 1
        per_corner.append((good, k, name, cl))
    trials = 2000
    ge = 0
    null_tot = 0
    for _ in range(trials):
        tot = 0
        for name, cl, secs in corners:
            k = len(secs)
            perm = list(range(k))
            random.shuffle(perm)
            tot += sum(1 for i in range(k) if perm[i] == i)
        null_tot += tot
        if tot >= agree:
            ge += 1
    print(f"[{label}]")
    print(f"  corners tested: {len(corners)} (>=2 full pole+zero sections)")
    print(f"  pairs: {total_pairs}; rule predicts factory pairing for {agree} "
          f"({100*agree/total_pairs:.1f}%)")
    print(f"  near-agreement (predicted zero within 30 cents of factory zero): +{near}")
    print(f"  corners in exact full agreement: {exact_corners}/{len(corners)}")
    print(f"  chance (random pairing, {trials} trials): mean {null_tot/trials:.1f} "
          f"pairs, p(>=observed) = {ge/trials:.4f}")
    worst = sorted(per_corner)[:4]
    best = sorted(per_corner, reverse=True)[:4]
    print("  most agreeing corners:", ", ".join(f"{n}·{c} {g}/{k}" for g, k, n, c in best))
    print("  least agreeing corners:", ", ".join(f"{n}·{c} {g}/{k}" for g, k, n, c in worst))
    print()

corners = load()
run(lambda fp, fz: abs(fp - fz), "linear frequency distance (angle-proportional)", corners)
run(lambda fp, fz: abs(math.log(fp / fz)), "log frequency distance", corners)
