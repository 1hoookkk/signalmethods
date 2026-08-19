"""What the P2K datum change does to the vowel skeletons.

The datum is settled by the rate-family pointer proof in CLAUDE.md, not by this
test. This measures the consequence: with every P2K frequency 12.9% higher, how
far are the VOW presets' formant skeletons from the DVTD mouths we actually
measured, and do they cluster by vowel any harder.

Usage:  python dev/p2k_vowel_datum.py <library.json>
"""
import sys, os, json, math, collections
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "cell_dictionary"))
import decode_lib as dl
import numpy as np

RATES = [39062.5, 44100.0]
LIVE_R = 0.45
VOW = {"Ooh-To-Eee", "Eeh-To-Aah", "TalkingHedz", "UbuOrator", "DeepBouche",
       "MultiQVox"}
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def mouths():
    table = json.load(open(os.path.join(ROOT, "recipes", "tables",
                                        "dvtd_formants.json")))
    out = []
    for m in table["mouths"]:
        peaks = sorted(m["peaks"], key=lambda p: p["hz"])[:3]
        if len(peaks) == 3:
            out.append((m["name"], [p["hz"] for p in peaks]))
    return out


def skeletons(lib, sr):
    """Lowest three live poles of each M corner, per preset."""
    out = []
    for s in lib["stage_sources"]:
        if s["family"] != "p2k":
            continue
        for ci in (0, 1):
            hz = []
            for si in range(s["stage_count"]):
                g = dl.geometry_from_words_at(tuple(s["tracks"][si][ci]), sr)
                if dl.stage_is_identity(g):
                    continue
                p = g.pole
                if type(p).__name__ == "Conjugate" and p.hz > 0 and p.r > LIVE_R:
                    hz.append(p.hz)
            if len(hz) >= 3:
                out.append((s["name"], ci, sorted(hz)[:3]))
    return out


def distance(f, ref):
    return sum(abs(12 * math.log2(a / b)) for a, b in zip(f, ref)) / 3.0


def run(lib, refs, sr):
    rows = skeletons(lib, sr)
    vow, ctl, nearest = [], [], collections.defaultdict(list)
    for name, ci, f in rows:
        best = min(refs, key=lambda r: distance(f, r[1]))
        d = distance(f, best[1])
        if name in VOW:
            vow.append(d)
            nearest[name].append(best[0])
        else:
            ctl.append(d)
    return np.array(vow), np.array(ctl), nearest


def vowel_of(mouth_name):
    return mouth_name.rsplit("-", 1)[-1]


def main(path):
    lib = json.load(open(path))
    refs = mouths()
    print("DVTD mouths with three peaks: %d\n" % len(refs))
    got = {}
    for sr in RATES:
        vow, ctl, nearest = run(lib, refs, sr)
        got[sr] = (vow, ctl, nearest)
        agree = sum(1 for v in nearest.values()
                    if len(set(vowel_of(m) for m in v)) == 1)
        print("--- %g Hz" % sr)
        print("  VOW corners      n=%2d  median %.2f st  IQR %.2f  best %.2f"
              % (len(vow), np.median(vow),
                 np.percentile(vow, 75) - np.percentile(vow, 25), vow.min()))
        print("  non-VOW control  n=%2d  median %.2f st" % (len(ctl), np.median(ctl)))
        print("  separation (control - VOW): %.2f st" % (np.median(ctl) - np.median(vow)))
        print("  VOW presets whose two M corners land on one vowel: %d of %d"
              % (agree, len(nearest)))
        for name in sorted(nearest):
            print("      %-12s %s" % (name, " | ".join(nearest[name])))
        print()

    a, b = got[RATES[0]], got[RATES[1]]
    print("--- shift 39,062.5 -> 44,100 (all P2K frequencies x1.129, +2.10 st)")
    print("  VOW median      %.2f -> %.2f st  (%+.2f)"
          % (np.median(a[0]), np.median(b[0]), np.median(b[0]) - np.median(a[0])))
    print("  control median  %.2f -> %.2f st  (%+.2f)"
          % (np.median(a[1]), np.median(b[1]), np.median(b[1]) - np.median(a[1])))
    print("  separation      %.2f -> %.2f st"
          % (np.median(a[1]) - np.median(a[0]), np.median(b[1]) - np.median(b[0])))


if __name__ == "__main__":
    main(sys.argv[1])
