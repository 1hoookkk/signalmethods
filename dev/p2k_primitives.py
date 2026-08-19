"""Five structural tests over the 33 P2K skins (132 corners, 792 live stages).

Conventions, fixed once for every test in this file:
  LIVE_R   a conjugate root below this radius is "not there" (the encoder's
           way of writing no-pole/no-zero); see dev/p2k_diagnostics.py
  SR       39,062.5 Hz, the datum currently assigned to the P2K bank. Every
           test here is relative (ratios, deltas, orderings) except the printed
           Hz bounds in Test 2, which scale by 1.129 if the datum is 44,100.
  corners  c0 = M0_Q0, c1 = M100_Q0, c2 = M0_Q100, c3 = M100_Q100

Usage: python dev/p2k_primitives.py <library.json>
"""
import sys, json, math, collections
sys.path.insert(0, __file__.rsplit("\\", 1)[0] + r"\cell_dictionary")
import decode_lib as dl
import numpy as np

SR = 39062.5
LIVE_R = 0.45
GRID = np.array(dl.log_grid_hz(20, 20000, 512))
rng = np.random.default_rng(9)


def live(p):
    return type(p).__name__ == "Conjugate" and p.hz > 0 and p.r > LIVE_R


def cells(lib):
    """(preset, corner, stage, geometry) for every non-identity stage"""
    out = []
    for s in [x for x in lib["stage_sources"] if x["family"] == "p2k"]:
        for ci in range(s["corners"]):
            for si in range(s["stage_count"]):
                g = dl.geometry_from_words_at(tuple(s["tracks"][si][ci]), SR)
                out.append((s["name"], ci, si, g, dl.stage_is_identity(g)))
    return out


def corner_curve(lib, name, ci):
    s = [x for x in lib["stage_sources"] if x["name"] == name][0]
    geoms = [dl.geometry_from_words_at(tuple(s["tracks"][si][ci]), SR) for si in range(s["stage_count"])]
    geoms = [g for g in geoms if not dl.stage_is_identity(g)]
    d = np.asarray(dl.corner_response_db(geoms, GRID, SR))
    d = np.where(np.isfinite(d), d, -120.0)
    return d - d.mean()          # DC/level factored out


# ---------------------------------------------------------------- Test 1
def test1(lib):
    print("\n=== TEST 1  pose clustering (132 corners, 512-pt log grid, level removed) ===")
    names = [(s["name"], ci) for s in lib["stage_sources"] if s["family"] == "p2k" for ci in range(s["corners"])]
    C = np.array([corner_curve(lib, n, ci) for n, ci in names])
    n = len(C)
    D = np.sqrt(((C[:, None, :] - C[None, :, :]) ** 2).mean(axis=2))
    for thr in (1.5, 3.0, 6.0):
        parent = list(range(n))
        def find(a):
            while parent[a] != a: parent[a] = parent[parent[a]]; a = parent[a]
            return a
        for i in range(n):
            for j in range(i + 1, n):
                if D[i, j] < thr:
                    a, b = find(i), find(j)
                    if a != b: parent[a] = b
        groups = collections.defaultdict(list)
        for i in range(n): groups[find(i)].append(i)
        multi = [g for g in groups.values() if len(g) > 1]
        print(f"  at <{thr:>4} dB RMS: {len(groups):3} clusters from {n} corners   "
              f"({len(multi)} with >1 member, largest {max((len(g) for g in groups.values()))})")
    parent = list(range(n))
    def find(a):
        while parent[a] != a: parent[a] = parent[parent[a]]; a = parent[a]
        return a
    for i in range(n):
        for j in range(i + 1, n):
            if D[i, j] < 1.5:
                a, b = find(i), find(j)
                if a != b: parent[a] = b
    groups = collections.defaultdict(list)
    for i in range(n): groups[find(i)].append(i)
    print("  clusters with >1 member at <1.5 dB:")
    for g in sorted(groups.values(), key=len, reverse=True):
        if len(g) < 2: continue
        print("    " + " | ".join(f"{names[i][0]} c{names[i][1]}" for i in g))
    print(f"  nearest-neighbour distance: median {np.median(np.sort(D + np.eye(n)*999, axis=1)[:,0]):.2f} dB")


# ---------------------------------------------------------------- Test 2
def test2(cs):
    print("\n=== TEST 2  per-stage role specialisation ===")
    print(f"  {'stage':6}{'n':>5}{'pole Hz p10..p90':>22}{'pole BW Hz med':>16}{'zero live':>11}{'unit-circle Z':>15}")
    perstage = collections.defaultdict(list)
    for name, ci, si, g, ident in cs:
        if ident: continue
        perstage[si].append(g)
    for si in sorted(perstage):
        gs = perstage[si]
        pf = np.array([g.pole.hz for g in gs if live(g.pole)])
        pbw = np.array([(-math.log(min(g.pole.r, 0.999999))) * SR / math.pi for g in gs if live(g.pole)])
        zl = sum(1 for g in gs if live(g.zero))
        uz = sum(1 for g in gs if type(g.zero).__name__ == "Conjugate" and g.zero.r >= 0.99999)
        print(f"  S{si+1:<5}{len(gs):>5}{f'{np.percentile(pf,10):.0f}..{np.percentile(pf,90):.0f}':>22}"
              f"{np.median(pbw):>16.0f}{100*zl/len(gs):>10.0f}%{100*uz/len(gs):>14.0f}%")
    print("\n  stage-frequency overlap (fraction of S_i poles inside S_j's p10..p90):")
    bounds = {}
    for si in sorted(perstage):
        pf = np.array([g.pole.hz for g in perstage[si] if live(g.pole)])
        bounds[si] = (np.percentile(pf, 10), np.percentile(pf, 90))
    hdr = "      " + "".join(f"S{j+1:<6}" for j in sorted(perstage))
    print(hdr)
    for si in sorted(perstage):
        pf = np.array([g.pole.hz for g in perstage[si] if live(g.pole)])
        row = "".join(f"{np.mean((pf>=bounds[sj][0])&(pf<=bounds[sj][1])):<7.2f}" for sj in sorted(perstage))
        print(f"  S{si+1}  {row}")


# ---------------------------------------------------------------- Test 3
def test3(cs):
    print("\n=== TEST 3  pole-zero coupling modes ===")
    modes = collections.Counter()
    dtheta = []
    for name, ci, si, g, ident in cs:
        if ident: continue
        p, z = g.pole, g.zero
        pz = live(p); zz = live(z)
        unit = type(z).__name__ == "Conjugate" and z.r >= 0.99999
        if pz and not zz: modes["pure pole (no zero)"] += 1
        elif zz and not pz: modes["zero only (no pole)"] += 1
        elif not pz and not zz: modes["neither live"] += 1
        else:
            d = abs(12 * math.log2(z.hz / p.hz))
            dtheta.append(d)
            if unit and p.r < 0.9: modes["unit-circle notch, broad pole"] += 1
            elif unit: modes["unit-circle notch, tight pole"] += 1
            elif d < 3: modes["co-located (<3 st)"] += 1
            elif d < 12: modes["asymmetric skirt (3-12 st)"] += 1
            else: modes["opposed (>12 st)"] += 1
    tot = sum(modes.values())
    for k, v in modes.most_common():
        print(f"  {k:32} {v:5}  {100*v/tot:5.1f}%")
    d = np.array(dtheta)
    print(f"\n  |pole->zero| semitones: p25 {np.percentile(d,25):.1f}  median {np.median(d):.1f}  p75 {np.percentile(d,75):.1f}")
    print(f"  zero ABOVE its pole: {100*np.mean([1 for name,ci,si,g,i in cs if not i and live(g.pole) and live(g.zero) and g.zero.hz>g.pole.hz]) if True else 0:.0f}%"
          if False else "")
    above = [1 if g.zero.hz > g.pole.hz else 0 for name, ci, si, g, i in cs if not i and live(g.pole) and live(g.zero)]
    print(f"  zero sits ABOVE its pole in {100*np.mean(above):.0f}% of coupled stages")


# ---------------------------------------------------------------- Test 4
def test4(lib):
    print("\n=== TEST 4  Q-axis action vector (c0->c2 and c1->c3) ===")
    dfreq, dbw, dzf, dzr, npairs = [], [], [], [], 0
    ceiling_hits = 0
    for s in [x for x in lib["stage_sources"] if x["family"] == "p2k"]:
        if s["corners"] < 4: continue
        for lo, hi in ((0, 2), (1, 3)):
            for si in range(s["stage_count"]):
                a = dl.geometry_from_words_at(tuple(s["tracks"][si][lo]), SR)
                b = dl.geometry_from_words_at(tuple(s["tracks"][si][hi]), SR)
                if dl.stage_is_identity(a) or dl.stage_is_identity(b): continue
                if live(a.pole) and live(b.pole):
                    npairs += 1
                    dfreq.append(12 * math.log2(b.pole.hz / a.pole.hz))
                    fa = (-math.log(min(a.pole.r, 0.999999))) * SR / math.pi
                    fb = (-math.log(min(b.pole.r, 0.999999))) * SR / math.pi
                    dbw.append(fb / fa)
                    if b.pole.r >= 0.999: ceiling_hits += 1
                if live(a.zero) and live(b.zero):
                    dzf.append(12 * math.log2(b.zero.hz / a.zero.hz))
                    dzr.append(b.zero.r - a.zero.r)
    dfreq = np.array(dfreq); dbw = np.array(dbw)
    print(f"  pole pairs compared: {npairs}")
    print(f"  A  pole FREQUENCY moves?   |delta| median {np.median(np.abs(dfreq)):.2f} st   within 10 cents: {100*np.mean(np.abs(dfreq)<0.1):.0f}%")
    print(f"  C  pole BANDWIDTH narrows? ratio median {np.median(dbw):.3f}   narrower in {100*np.mean(dbw<1):.0f}% of pairs")
    print(f"     pole radius reaches >=0.999 at Q100 in {100*ceiling_hits/npairs:.0f}% of pairs")
    if dzf:
        print(f"  C  zero FREQUENCY moves?   |delta| median {np.median(np.abs(np.array(dzf))):.2f} st")
        print(f"     zero radius delta median {np.median(np.array(dzr)):+.4f}")


# ---------------------------------------------------------------- Test 5
def test5(lib):
    print("\n=== TEST 5  morph trajectory (c0->c1 at Q0) ===")
    vel, cross, lanes, presets_cross = [], 0, 0, set()
    for s in [x for x in lib["stage_sources"] if x["family"] == "p2k"]:
        tr = []
        for si in range(s["stage_count"]):
            a = dl.geometry_from_words_at(tuple(s["tracks"][si][0]), SR)
            b = dl.geometry_from_words_at(tuple(s["tracks"][si][1]), SR)
            if dl.stage_is_identity(a) or dl.stage_is_identity(b): continue
            if not (live(a.pole) and live(b.pole)): continue
            tr.append((si, a.pole.hz, b.pole.hz))
            vel.append(12 * math.log2(b.pole.hz / a.pole.hz))
        for i in range(len(tr)):
            for j in range(i + 1, len(tr)):
                lanes += 1
                _, a0, a1 = tr[i]; _, b0, b1 = tr[j]
                if (a0 - b0) * (a1 - b1) < 0:
                    cross += 1
                    presets_cross.add(s["name"])
    v = np.array(vel)
    print(f"  lanes with live poles at both M ends: {len(v)}")
    print(f"  morph velocity (semitones, + = rises): median {np.median(v):+.2f}   rises in {100*np.mean(v>0):.0f}%")
    print(f"     |velocity| p25 {np.percentile(np.abs(v),25):.2f}  median {np.median(np.abs(v)):.2f}  p90 {np.percentile(np.abs(v),90):.2f}")
    print(f"  lane PAIRS that cross during the morph: {cross} of {lanes}  ({100*cross/lanes:.0f}%)")
    print(f"  presets containing at least one crossing: {len(presets_cross)} of 33")


def main(path):
    lib = json.load(open(path))
    cs = cells(lib)
    test1(lib); test2(cs); test3(cs); test4(lib); test5(lib)


if __name__ == "__main__":
    main(sys.argv[1])
