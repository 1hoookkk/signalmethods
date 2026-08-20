import struct, math, os, random
from pathlib import Path
from collections import defaultdict

DATUM_SR = 39062.5
IDENTITY_WORDS = [0xdfff, 0xffff, 0xdfff, 0xffff, 0xdfff]
BODIES_DIR = Path(__file__).resolve().parent.parent / "ref" / "morpheus" / "bodies"

def decode(word):
    u = word + 1
    if u == 65536: return 1.0
    if u == 1: return 0.0
    e = (u >> 12) & 0xf
    m = u & 0xfff
    x = m / 4096 if e == 0 else (m + 4096) / 8192
    return x * (2.0 ** (e - 15))

def words_to_geometry(w):
    d0, d1, d2, d3, d4 = (decode(x) for x in w)
    c0 = 4*d0 + d1
    c1 = d1
    c2 = 4*d2 + d3
    c3 = d3

    rz = math.sqrt(max(0, 1 - c1))
    if rz > 1e-9:
        cos_wz = max(-1, min(1, (2 - c0) / (2 * rz)))
        zero_hz = math.acos(cos_wz) * DATUM_SR / (2 * math.pi)
    else:
        zero_hz = 0.0
        rz = 0.0

    rp = math.sqrt(max(0, 1 - c3))
    if rp > 1e-9:
        cos_wp = max(-1, min(1, (2 - c2) / (2 * rp)))
        pole_hz = math.acos(cos_wp) * DATUM_SR / (2 * math.pi)
    else:
        pole_hz = 0.0
        rp = 0.0

    return pole_hz, rp, zero_hz, rz

def load_body(path):
    data = path.read_bytes()
    if len(data) != 560:
        return None
    raw = struct.unpack("<280H", data)
    corners = []
    for ci in range(8):
        stages = []
        for si in range(7):
            idx = ci * 35 + si * 5
            w = list(raw[idx:idx+5])
            stages.append(w)
        corners.append(stages)
    return corners

def is_identity(w):
    return w == IDENTITY_WORDS

def quantiles(vals, qs=(0.25, 0.5, 0.75)):
    s = sorted(vals)
    n = len(s)
    if n == 0: return [0]*len(qs)
    out = []
    for q in qs:
        idx = q * (n - 1)
        lo = int(math.floor(idx))
        hi = min(lo + 1, n - 1)
        frac = idx - lo
        out.append(s[lo] + frac * (s[hi] - s[lo]))
    return out

def histogram(vals, edges):
    counts = [0] * (len(edges) + 1)
    for v in vals:
        placed = False
        for i, e in enumerate(edges):
            if v < e:
                counts[i] += 1
                placed = True
                break
        if not placed:
            counts[len(edges)] += 1
    return counts

def main():
    bodies = sorted(BODIES_DIR.glob("*.body"))
    print(f"Corpus: {len(bodies)} bodies in {BODIES_DIR}\n")

    all_paired = []
    all_pole_only = []
    all_zero_only = []
    all_dc_pole = []
    all_traveling_null = []
    total_stages = 0
    total_identity = 0
    corner_data = []

    for bp in bodies:
        corners = load_body(bp)
        if corners is None:
            continue
        name = bp.stem
        for ci, stages in enumerate(corners):
            corner_poles = []
            corner_zeros = []
            corner_pairs = []
            for si, w in enumerate(stages):
                if is_identity(w):
                    total_identity += 1
                    total_stages += 1
                    continue
                total_stages += 1
                ph, pr, zh, zr = words_to_geometry(w)

                has_pole = pr > 1e-6
                has_zero = zr > 1e-6
                dc_pole = has_pole and ph < 5.0

                if dc_pole:
                    all_dc_pole.append((name, ci, si, ph, pr, zh, zr))
                if has_zero and zr > 0.9999:
                    all_traveling_null.append((name, ci, si, zh, zr))

                if has_pole and has_zero and not dc_pole and ph > 5.0 and zh > 5.0:
                    dst = 12 * math.log2(zh / ph) if ph > 0 and zh > 0 else 0
                    dr = zr - pr
                    all_paired.append({
                        "name": name, "ci": ci, "si": si,
                        "pole_hz": ph, "pole_r": pr,
                        "zero_hz": zh, "zero_r": zr,
                        "dst": dst, "dr": dr,
                    })
                    corner_pairs.append({"si": si, "ph": ph, "pr": pr, "zh": zh, "zr": zr, "dst": dst})
                    corner_poles.append((si, ph))
                    corner_zeros.append((si, zh))
                elif has_pole and not has_zero:
                    all_pole_only.append((name, ci, si, ph, pr))
                    corner_poles.append((si, ph))
                elif has_zero and not has_pole:
                    all_zero_only.append((name, ci, si, zh, zr))
                    corner_zeros.append((si, zh))

            if corner_pairs:
                corner_data.append({
                    "name": name, "ci": ci,
                    "pairs": corner_pairs,
                    "poles": corner_poles,
                    "zeros": corner_zeros,
                })

    print("=" * 72)
    print("STAGE INVENTORY")
    print("=" * 72)
    print(f"  Total stage slots:    {total_stages}")
    print(f"  Identity (bypassed):  {total_identity}")
    active = total_stages - total_identity
    print(f"  Active:               {active}")
    print(f"    Paired (pole+zero): {len(all_paired)}")
    print(f"    Pole-only:          {len(all_pole_only)}")
    print(f"    Zero-only:          {len(all_zero_only)}")
    print(f"    DC poles (<5 Hz):   {len(all_dc_pole)}")
    print(f"    Traveling nulls:    {len(all_traveling_null)}")
    print()

    if not all_paired:
        print("No paired stages found.")
        return

    dsts = [p["dst"] for p in all_paired]
    drs = [p["dr"] for p in all_paired]
    abs_dsts = [abs(d) for d in dsts]

    print("=" * 72)
    print(f"Δst DISTRIBUTION (semitone interval zero→pole, n={len(dsts)})")
    print("=" * 72)
    q25, med, q75 = quantiles(dsts)
    mean = sum(dsts) / len(dsts)
    std = math.sqrt(sum((d - mean)**2 for d in dsts) / len(dsts))
    print(f"  Mean:   {mean:+.2f} st")
    print(f"  Median: {med:+.2f} st")
    print(f"  Std:    {std:.2f} st")
    print(f"  Q25:    {q25:+.2f} st")
    print(f"  Q75:    {q75:+.2f} st")
    print(f"  Min:    {min(dsts):+.2f} st")
    print(f"  Max:    {max(dsts):+.2f} st")
    print()
    within_2 = sum(1 for d in abs_dsts if d <= 2)
    within_6 = sum(1 for d in abs_dsts if d <= 6)
    beyond_12 = sum(1 for d in abs_dsts if d > 12)
    beyond_24 = sum(1 for d in abs_dsts if d > 24)
    print(f"  |Δst| ≤  2 st: {within_2:5d}  ({100*within_2/len(dsts):.1f}%)")
    print(f"  |Δst| ≤  6 st: {within_6:5d}  ({100*within_6/len(dsts):.1f}%)")
    print(f"  |Δst| > 12 st: {beyond_12:5d}  ({100*beyond_12/len(dsts):.1f}%)")
    print(f"  |Δst| > 24 st: {beyond_24:5d}  ({100*beyond_24/len(dsts):.1f}%)")

    edges = [-48, -36, -24, -18, -12, -6, -3, -1, 0, 1, 3, 6, 12, 18, 24, 36, 48]
    counts = histogram(dsts, edges)
    print()
    print("  Histogram (Δst bins):")
    labels = [f"<{edges[0]}"] + [f"{edges[i]}..{edges[i+1]}" for i in range(len(edges)-1)] + [f">{edges[-1]}"]
    for label, count in zip(labels, counts):
        bar = "#" * min(60, max(1, round(count / max(1, len(dsts)) * 200)))
        if count > 0:
            print(f"    {label:>10s}: {count:5d}  {bar}")

    print()
    print("=" * 72)
    print(f"Δr DISTRIBUTION (zero_r − pole_r, n={len(drs)})")
    print("=" * 72)
    q25r, medr, q75r = quantiles(drs)
    meanr = sum(drs) / len(drs)
    stdr = math.sqrt(sum((d - meanr)**2 for d in drs) / len(drs))
    print(f"  Mean:   {meanr:+.6f}")
    print(f"  Median: {medr:+.6f}")
    print(f"  Std:    {stdr:.6f}")
    print(f"  Q25:    {q25r:+.6f}")
    print(f"  Q75:    {q75r:+.6f}")
    print()
    zr_gt_pr = sum(1 for d in drs if d > 0)
    zr_eq_pr = sum(1 for d in drs if abs(d) < 0.01)
    zr_unit = sum(1 for p in all_paired if p["zero_r"] > 0.9999)
    print(f"  zero_r > pole_r:       {zr_gt_pr:5d}  ({100*zr_gt_pr/len(drs):.1f}%)")
    print(f"  |Δr| < 0.01:          {zr_eq_pr:5d}  ({100*zr_eq_pr/len(drs):.1f}%)")
    print(f"  zero_r > 0.9999 (null): {zr_unit:5d}  ({100*zr_unit/len(drs):.1f}%)")

    print()
    print("=" * 72)
    print("CROSS-STAGE NEAREST NEIGHBOR")
    print("=" * 72)
    own_dsts = []
    cross_dsts = []
    tighter_own = 0
    tighter_cross = 0
    equal_or_no_cross = 0

    for cd in corner_data:
        for pair in cd["pairs"]:
            own_dst = abs(pair["dst"])
            own_dsts.append(own_dst)
            best_cross = float("inf")
            for (other_si, other_zh) in cd["zeros"]:
                if other_si == pair["si"]:
                    continue
                if other_zh > 5.0 and pair["ph"] > 5.0:
                    d = abs(12 * math.log2(other_zh / pair["ph"]))
                    if d < best_cross:
                        best_cross = d
            for (other_si, other_zh_from_pair) in [(p["si"], p["zh"]) for p in cd["pairs"]]:
                if other_si == pair["si"]:
                    continue
                if other_zh_from_pair > 5.0 and pair["ph"] > 5.0:
                    d = abs(12 * math.log2(other_zh_from_pair / pair["ph"]))
                    if d < best_cross:
                        best_cross = d

            if best_cross < float("inf"):
                cross_dsts.append(best_cross)
                if own_dst < best_cross:
                    tighter_own += 1
                else:
                    tighter_cross += 1
            else:
                equal_or_no_cross += 1

    if own_dsts:
        mean_own = sum(own_dsts) / len(own_dsts)
        print(f"  Mean |Δst| own-stage pairing:    {mean_own:.2f} st  (n={len(own_dsts)})")
    if cross_dsts:
        mean_cross = sum(cross_dsts) / len(cross_dsts)
        print(f"  Mean nearest cross-stage |Δst|:   {mean_cross:.2f} st  (n={len(cross_dsts)})")
    print()
    total_compared = tighter_own + tighter_cross
    if total_compared:
        print(f"  Factory own-stage tighter:  {tighter_own:5d}  ({100*tighter_own/total_compared:.1f}%)")
        print(f"  Cross-stage tighter:        {tighter_cross:5d}  ({100*tighter_cross/total_compared:.1f}%)")
        print(f"  No cross-stage alternative: {equal_or_no_cross:5d}")

    print()
    print("=" * 72)
    print("RANDOMIZATION TEST (1000 permutations)")
    print("=" * 72)
    random.seed(42)
    n_perms = 1000
    factory_means = []
    perm_means_below = 0
    corners_tested = 0

    for cd in corner_data:
        pairs = cd["pairs"]
        if len(pairs) < 2:
            continue
        corners_tested += 1
        factory_abs_dst = sum(abs(p["dst"]) for p in pairs) / len(pairs)
        factory_means.append(factory_abs_dst)
        poles = [p["ph"] for p in pairs]
        zeros = [p["zh"] for p in pairs]
        below = 0
        for _ in range(n_perms):
            shuffled_zeros = zeros[:]
            random.shuffle(shuffled_zeros)
            perm_dst = 0
            for ph, zh in zip(poles, shuffled_zeros):
                if ph > 0 and zh > 0:
                    perm_dst += abs(12 * math.log2(zh / ph))
            perm_dst /= len(pairs)
            if perm_dst < factory_abs_dst:
                below += 1
        perm_means_below += below

    if corners_tested:
        overall_factory = sum(factory_means) / len(factory_means)
        avg_p = perm_means_below / (corners_tested * n_perms)
        print(f"  Corners tested:             {corners_tested}")
        print(f"  Mean factory |Δst|:         {overall_factory:.2f} st")
        print(f"  Avg p-value (fraction of")
        print(f"    permutations < factory):  {avg_p:.4f}")
        print()
        if avg_p < 0.05:
            print(f"  RESULT: Factory pairings are SIGNIFICANTLY tighter than random")
            print(f"          (p = {avg_p:.4f} < 0.05)")
        elif avg_p > 0.95:
            print(f"  RESULT: Factory pairings are significantly LOOSER than random")
            print(f"          (p = {avg_p:.4f} > 0.95)")
        else:
            print(f"  RESULT: Factory pairings are NOT significantly different from random")
            print(f"          (p = {avg_p:.4f})")

        p_dist = []
        for cd in corner_data:
            pairs = cd["pairs"]
            if len(pairs) < 2: continue
            factory_abs_dst = sum(abs(p["dst"]) for p in pairs) / len(pairs)
            poles = [p["ph"] for p in pairs]
            zeros = [p["zh"] for p in pairs]
            below = 0
            for _ in range(n_perms):
                sz = zeros[:]
                random.shuffle(sz)
                pd = sum(abs(12 * math.log2(z / p)) for p, z in zip(poles, sz) if p > 0 and z > 0) / len(pairs)
                if pd < factory_abs_dst:
                    below += 1
            p_dist.append(below / n_perms)

        sig_tight = sum(1 for p in p_dist if p < 0.05)
        sig_loose = sum(1 for p in p_dist if p > 0.95)
        not_sig = len(p_dist) - sig_tight - sig_loose
        print()
        print(f"  Per-corner breakdown:")
        print(f"    Significantly tight (p<0.05): {sig_tight:5d}  ({100*sig_tight/len(p_dist):.1f}%)")
        print(f"    Not significant:              {not_sig:5d}  ({100*not_sig/len(p_dist):.1f}%)")
        print(f"    Significantly loose (p>0.95): {sig_loose:5d}  ({100*sig_loose/len(p_dist):.1f}%)")

    print()

if __name__ == "__main__":
    main()
