import os
import json
import glob
import math
import numpy as np
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "plots", "corpus")
os.makedirs(OUT, exist_ok=True)
FS = 39062.5
NYQ = FS / 2.0
IDLE = (1909, 2015)
REPORT = []


def say(s=""):
    print(s)
    REPORT.append(s)


# ---------------------------------------------------- STEP 1: Z -> S de-warp
def dewarp(f_hz):
    """Inverse bilinear.  Omega = (2/T) tan(omega_d T / 2) is in radians/s;
    in Hz that is f_a = (fs/pi) tan(pi f_d / fs).  Near-identity at low
    frequency, diverging only as the root approaches Nyquist."""
    if f_hz <= 0.0:
        return 0.0
    x = math.pi * min(f_hz, NYQ * 0.999) / FS
    return (FS / math.pi) * math.tan(x)


def cents(a, b):
    return 1200.0 * math.log2(max(b, 1e-6) / max(a, 1e-6))


# ---------------------------------------------------------------- corpora
def load_morpheus():
    d = json.load(open(os.path.join(ROOT, "ref/morpheus/cubes_decoded.json")))
    assert d["law"]["hz_datum"] == FS
    out = []
    for c in d["cubes"]:
        for ci, cor in enumerate(c["corners"]):
            secs = []
            for si, s in enumerate(cor["sections"]):
                raw = tuple(s["raw"])
                secs.append({
                    "stage": si, "raw": raw,
                    "fp": s["pole"]["hz"], "rp": s["pole"]["r"],
                    "fz": s["zero"]["hz"], "rz": s["zero"]["r"],
                    "idle": raw[:2] == IDLE,
                })
            out.append({"corpus": "morpheus", "name": c["name"], "corner": ci,
                        "t2": ci & 1, "m": (ci >> 1) & 1, "q": (ci >> 2) & 1,
                        "sections": secs})
    return out


def load_p2k():
    out = []
    for fn in sorted(glob.glob(os.path.join(ROOT, "recipes/architectures/*.json"))):
        d = json.load(open(fn))
        assert d["datum_sr_hz"] == FS
        for ci, cn in enumerate(["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]):
            secs = []
            for si, sec in enumerate(d["sections"]):
                g = sec["corners"][cn]
                def rd(x):
                    if "pair" in x:
                        a, b = x["pair"]
                        return (0.3 if (a + b) >= 0 else NYQ,
                                min(math.sqrt(abs(a * b)), 0.99999))
                    return (x["hz"], min(x["r"], 0.99999))
                fp, rp = rd(g["pole"])
                fz, rz = rd(g["zero"])
                secs.append({"stage": si, "raw": None, "fp": fp, "rp": rp,
                             "fz": fz, "rz": rz, "idle": False})
            out.append({"corpus": "p2k", "name": d["name"], "corner": ci,
                        "t2": 0, "m": ci & 1, "q": (ci >> 1) & 1,
                        "sections": secs})
    return out


CORNERS = load_morpheus() + load_p2k()
MOR = [c for c in CORNERS if c["corpus"] == "morpheus"]
P2K = [c for c in CORNERS if c["corpus"] == "p2k"]


POSE_FMAX = 16000.0


def live_poles(cor, thr=0.3, fmax=POSE_FMAX):
    """Active poles inside the audio band. Roots parked at/above 16 kHz are
    the corpus's structural top-shelf and idle roots; de-warping sends them
    to hundreds of kHz, so they are not part of an acoustic pose."""
    return sorted(s["fp"] for s in cor["sections"]
                  if s["rp"] > thr and not s["idle"] and s["fp"] < fmax)


say("=" * 78)
say("SECTION-LEVEL ROOT DE-WARPING AND ACOUSTIC POSE CROSS-HASHING")
say(f"  corpora: {len({c['name'] for c in MOR})} Morpheus cubes "
    f"({len(MOR)} corners, 7 sections = 14th order) + "
    f"{len({c['name'] for c in P2K})} P2K architectures "
    f"({len(P2K)} corners, 6 sections = 12th order)")
say(f"  both stored at the native {FS:g} Hz datum")
say()
say("STEP 1 — inverse-bilinear root normalisation  Omega = 2 fs tan(pi f / fs)")
allp = [(s["fp"], s["rp"]) for c in CORNERS for s in c["sections"]
        if s["rp"] > 0.3 and not s["idle"]]
dw = np.array([dewarp(f) for f, _ in allp])
zz = np.array([f for f, _ in allp])
shift = np.array([cents(f, dewarp(f)) for f, _ in allp if f > 20])
say(f"  live poles de-warped: {len(dw)}")
say(f"  z-plane -> acoustic shift (cents): median {np.median(shift):+.0f}, "
    f"p90 {np.percentile(shift, 90):+.0f}, max {shift.max():+.0f}")
for lo, hi in ((0, 1000), (1000, 4000), (4000, 8000), (8000, 16000),
               (16000, 19600)):
    m = (zz >= lo) & (zz < hi)
    if m.any():
        say(f"     {lo:5d}-{hi:5d} Hz  n={int(m.sum()):5d}  "
            f"median warp {np.median([cents(f, dewarp(f)) for f in zz[m]]):+6.0f} cents")

# ------------------------------------- STEP 2: E-mu's own pose vocabulary
say()
say("STEP 2 — Q0 base-plane pose vocabulary, hashed against ITSELF")
say("  (no external phonetic targets; this maps the E-mu corpus internally)")
nshelf = sum(1 for c in CORNERS for s in c["sections"]
             if s["rp"] > 0.3 and not s["idle"] and s["fp"] >= POSE_FMAX)
say(f"  excluded {nshelf} structural poles at/above {POSE_FMAX:.0f} Hz "
    f"(top-shelf and Nyquist-parked roots)")


def pose_key(pose, q_cents=50.0):
    return tuple(round(cents(20.0, f) / q_cents) for f in pose)


def intervals(pose):
    return [12 * math.log2(pose[i + 1] / pose[i]) for i in range(len(pose) - 1)]


POSES = defaultdict(list)
for c in CORNERS:
    if c["q"] != 0:
        continue
    pose = [dewarp(f) for f in live_poles(c)]
    if len(pose) < 2:
        continue
    POSES[(len(pose), pose_key(pose))].append((c["corpus"], c["name"],
                                               c["corner"], pose))
n_corners = sum(len(v) for v in POSES.values())
shared = {k: v for k, v in POSES.items()
          if len({(a, b) for a, b, _, _ in v}) > 1}
say(f"  Q0 corners with >=2 live poles: {n_corners}")
say(f"  distinct poses (1/2-semitone bins): {len(POSES)}")
say(f"  poses used by more than one filter: {len(shared)} covering "
    f"{sum(len(v) for v in shared.values())} corners "
    f"({100*sum(len(v) for v in shared.values())/n_corners:.0f}%)")
say(f"  pole-count distribution of Q0 poses:")
for n, cnt in sorted(Counter(k[0] for k in POSES).items()):
    say(f"     {n} poles: {cnt} distinct poses")
say()
say("  most reused Q0 poses (the factory's own pose library):")
for (n, key), v in sorted(shared.items(), key=lambda t: -len(t[1]))[:8]:
    pose = v[0][3]
    fl = sorted({f"{a}:{b}" for a, b, _, _ in v})
    say(f"     x{len(v):3d} corners / {len(fl):2d} filters  {n} poles  "
        f"[{', '.join(f'{p:.0f}' for p in pose)}] Hz")
    say(f"          steps {', '.join(f'{i:+.1f}st' for i in intervals(pose))}")
    say(f"          {', '.join(fl[:5])}{'...' if len(fl) > 5 else ''}")

allint = [i for v in POSES.values() for i in intervals(v[0][3])]
ai = np.array(allint)
say()
say(f"  interval structure of every Q0 pose ({len(ai)} adjacent pole steps):")
for lo, hi, lab in ((0, 3, "<3 st"), (3, 6, "3-6"), (6, 9, "6-9"),
                    (9, 13, "9-13 (octave)"), (13, 20, "13-20"),
                    (20, 99, ">20 st")):
    m = (ai >= lo) & (ai < hi)
    say(f"     {lab:16s} {int(m.sum()):5d} ({100*m.mean():5.1f}%)")
say(f"  median step {np.median(ai):.1f} st; "
    f"within 0.5 st of an octave: {100*(np.abs(ai-12)<0.5).mean():.1f}%; "
    f"of a fifth: {100*(np.abs(ai-7)<0.5).mean():.1f}%")

# ------------------------------- STEP 3: section reuse graph / scaffolds
say()
say("STEP 3 — section graph and scaffold topology")


def skey(s, q=2.0):
    """Acoustic section hash on de-warped roots: 1/2-semitone frequency bins,
    radius to 3 decimals."""
    def part(f, r):
        if r <= 0.05:
            return "off"
        return f"{round(cents(20.0, dewarp(f)) / (100.0 * q / 2))}:{r:.3f}"
    return part(s["fp"], s["rp"]) + "|" + part(s["fz"], s["rz"])


ORDERED = []
for c in CORNERS:
    seq = [(s["stage"], skey(s)) for s in c["sections"]
           if not s["idle"] and (s["rp"] > 0.05 or s["rz"] > 0.05)]
    if seq:
        ORDERED.append((c["corpus"], c["name"], c["corner"], seq))

use = defaultdict(list)
for corp, nm, ci, seq in ORDERED:
    for st, k in seq:
        use[k].append((corp, nm, ci, st))
tot = sum(len(v) for v in use.values())
multi = {k: v for k, v in use.items() if len({(a, b) for a, b, _, _ in v}) > 1}
reused = sum(len(v) for v in multi.values())
say(f"  active sections hashed: {tot}   distinct section states: {len(use)}")
say(f"  states used by more than one filter: {len(multi)} covering "
    f"{reused} sections = {100*reused/tot:.1f}%")
for tag in ("morpheus", "p2k"):
    t = sum(1 for v in use.values() for x in v if x[0] == tag)
    r = sum(1 for v in multi.values() for x in v if x[0] == tag)
    say(f"     {tag}: {r}/{t} = {100*r/max(t,1):.1f}% shared with another filter")

say()
say("  MODULES — contiguous runs of sections reused inside cascades")
say("  n   instances  distinct  recurring  cross-filter   % of runs that recur")
NGRAM = {}
for n in (2, 3, 4, 5):
    g = defaultdict(list)
    for corp, nm, ci, seq in ORDERED:
        ks = [k for _, k in seq]
        sts = [st for st, _ in seq]
        for i in range(len(ks) - n + 1):
            if sts[i + n - 1] - sts[i] != n - 1:
                continue
            g[tuple(ks[i:i + n])].append((corp, nm, ci, sts[i]))
    NGRAM[n] = g
    inst = sum(len(v) for v in g.values())
    rec = {k: v for k, v in g.items() if len(v) > 1}
    xf = {k: v for k, v in g.items() if len({(a, b) for a, b, _, _ in v}) > 1}
    say(f"  {n}   {inst:8d}  {len(g):8d}  {len(rec):9d}  {len(xf):12d}   "
        f"{100*sum(len(v) for v in rec.values())/max(inst,1):5.1f}%")

for n in (3, 4):
    g = NGRAM[n]
    xf = sorted(((k, v) for k, v in g.items()
                 if len({(a, b) for a, b, _, _ in v}) > 1),
                key=lambda t: -len(t[1]))
    say()
    say(f"  top {n}-section modules shared across filters:")
    for k, v in xf[:6]:
        fl = sorted({f"{a}:{b}" for a, b, _, _ in v})
        st = Counter(x[3] for x in v)
        say(f"     x{len(v):3d} in {len(fl):2d} filters, entry stage "
            f"{'/'.join(f'S{p+1}:{c}' for p, c in sorted(st.items()))}")
        say(f"          {', '.join(fl[:5])}{'...' if len(fl) > 5 else ''}")

say()
say("  SUBSTITUTION POINTS — modules identical but for one section")
for n in (3, 4):
    g = NGRAM[n]
    keep = {k: v for k, v in g.items() if len(v) >= 2}
    buckets = defaultdict(list)
    for k in keep:
        for i in range(n):
            buckets[(i, k[:i] + ("*",) + k[i + 1:])].append(k)
    subs = [(pos, ctx, ks) for (pos, ctx), ks in buckets.items() if len(ks) > 1]
    slots = Counter(pos for pos, _, _ in subs)
    say(f"  n={n}: {len(subs)} contexts admit more than one filler; "
        f"position histogram " + " ".join(f"p{p}:{c}" for p, c in sorted(slots.items())))
    best = sorted(subs, key=lambda t: -len(t[2]))[:3]
    for pos, ctx, ks in best:
        users = sorted({f"{a}:{b}" for k in ks for a, b, _, _ in g[k]})
        say(f"     position {pos+1} of {n}: {len(ks)} alternative fillers, "
            f"{len(users)} filters  e.g. {', '.join(users[:4])}")

# ---------------------------- STEP 4: carver zero differential profiling
say()
say("STEP 4 — carver zero differential profiling  dst = 12 log2(fz/fp)")
rows = []
for c in CORNERS:
    for s in c["sections"]:
        if s["idle"] or s["rp"] <= 0.3:
            continue
        wp, wz = dewarp(s["fp"]), dewarp(s["fz"])
        rows.append((c["corpus"], c["name"], c["corner"], s["stage"],
                     wp, s["rp"], wz, s["rz"],
                     12 * math.log2(max(wz, 1e-6) / max(wp, 1e-6))
                     if s["rz"] > 0.05 else None))
have_z = [r for r in rows if r[8] is not None]
no_z = [r for r in rows if r[8] is None]
say(f"  live-pole sections: {len(rows)}   with a live zero: {len(have_z)}   "
    f"zero dissolved (rz<=0.05): {len(no_z)} = {100*len(no_z)/len(rows):.0f}%")
dst = np.array([r[8] for r in have_z])
rz = np.array([r[7] for r in have_z])
carver = (rz >= 0.98) & (dst >= 6) & (dst <= 12)
below = (rz >= 0.98) & (dst <= -6) & (dst >= -12)
say(f"  carver zeros (rz>=0.98, dst in [+6,+12] st): {int(carver.sum())} "
    f"= {100*carver.mean():.1f}% of zero-bearing sections")
say(f"  mirror below the pole (dst in [-12,-6]):     {int(below.sum())} "
    f"= {100*below.mean():.1f}%")
say(f"  dst distribution: median {np.median(dst):+.1f} st, "
    f"below pole {100*(dst<0).mean():.0f}%, above {100*(dst>0).mean():.0f}%")
say(f"  unit-circle zeros (rz>=0.999): {int((rz>=0.999).sum())} "
    f"= {100*(rz>=0.999).mean():.0f}% of zero-bearing sections")
for lo, hi, lab in ((-99, -12, "< -12 st"), (-12, -6, "-12..-6"),
                    (-6, -1, "-6..-1"), (-1, 1, "on the pole"),
                    (1, 6, "+1..+6"), (6, 12, "+6..+12 CARVER"),
                    (12, 99, "> +12 st")):
    m = (dst >= lo) & (dst < hi)
    say(f"     {lab:16s} n={int(m.sum()):5d} ({100*m.mean():5.1f}%)  "
        f"median rz {np.median(rz[m]) if m.any() else float('nan'):.3f}")

with open(os.path.join(OUT, "pose_hash_report.txt"), "w") as f:
    f.write("\n".join(REPORT))
with open(os.path.join(OUT, "carver_zeros.tsv"), "w") as f:
    f.write("corpus\tfilter\tcorner\tstage\tpole_acoustic_hz\trp\t"
            "zero_acoustic_hz\trz\tdelta_st\n")
    for r in rows:
        f.write("\t".join(str(x) if x is not None else "" for x in r) + "\n")
print("\nwrote", OUT)
