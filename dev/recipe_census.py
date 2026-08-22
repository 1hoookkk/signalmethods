"""What each Morpheus family is built from, section by section.

Every live section in every corner of every body is classified by its root
geometry (decoded at 39,062.5 Hz), and two structural devices are counted:
the follow rule (a section's zero sits on another section's pole frequency,
within 3%) and the exact cancel (same frequency AND same radius, so the pair
is inert in the cascade).
"""
import collections
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
DECODED = ROOT / "ref/morpheus_decoded"
SR = 39062.5


def classify(sec):
    p, z = sec["pole"], sec["zero"]
    if sec["span_db"] < 0.5:
        return "idle"
    pc, zc = p["kind"] == "conjugate", z["kind"] == "conjugate"
    if pc and not zc:
        return "pole-only" if z["kind"] == "degenerate" else "pole+real-zero"
    if zc and not pc:
        return "zero-only"
    if not pc and not zc:
        return "real-only"
    if z["hz"] > 0.47 * SR:
        return "lowpass"
    if z["hz"] < 30.0:
        return "highpass"
    ratio = z["hz"] / max(p["hz"], 1.0)
    if 1 / 1.35 < ratio < 1.35:
        return "bell-cut" if z["radius"] > p["radius"] else "bell-boost"
    return "split"


index = json.load(open(DECODED / "index.json"))
fam = collections.defaultdict(collections.Counter)
follow = collections.Counter()
cancel = collections.Counter()
zeros = collections.Counter()
for row in index["filters"]:
    doc = json.load(open(DECODED / row["path"]))
    f = row["family"]
    for corner in doc["corner_data"]:
        secs = corner["sections"]
        poles = [(i, s["pole"]) for i, s in enumerate(secs) if s["pole"]["kind"] == "conjugate"]
        for i, s in enumerate(secs):
            fam[f][classify(s)] += 1
            z = s["zero"]
            if z["kind"] != "conjugate" or s["span_db"] < 0.5:
                continue
            zeros[f] += 1
            hit = [(j, p) for j, p in poles if j != i and abs(p["hz"] / max(z["hz"], 1.0) - 1.0) < 0.03]
            if hit:
                follow[f] += 1
                if any(abs(p["radius"] - z["radius"]) < 0.002 for _, p in hit):
                    cancel[f] += 1

kinds = ["lowpass", "highpass", "bell-boost", "bell-cut", "split", "pole-only", "zero-only",
         "pole+real-zero", "real-only", "idle"]
print(f"{'family':<14}" + "".join(f"{k:>15}" for k in kinds))
for f, c in fam.items():
    n = sum(c.values())
    print(f"{f:<14}" + "".join(f"{100 * c[k] / n:14.1f}%" for k in kinds))
print("\nfollow rule (zero on another section's pole, 3%) and exact cancel, as share of live conjugate zeros:")
for f in fam:
    print(f"  {f:<14} follow {100 * follow[f] / max(zeros[f], 1):5.1f}%   cancel {100 * cancel[f] / max(zeros[f], 1):5.1f}%   n={zeros[f]}")
