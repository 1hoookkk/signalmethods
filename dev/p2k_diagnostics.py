"""Every P2K structural diagnostic from the 2026-08-19 session, run at both
candidate datums so the rate-sensitive results are separated from the invariant
ones.

The 39,062.5 Hz datum is validated for the Morpheus cubes only (CLAUDE.md line
151, LPFlange.4 against its manual entry). The P2K .bin skins inherited it via
`author::extrude::AUTHORING_SR`, which holds the legacy rate despite its name.
Proteus 2000 is a later instrument; 44,100 is the competing candidate.

Usage:  python dev/p2k_diagnostics.py <library.json>
        (library.json = the /api/library payload from author-server)
"""
import sys, json, math, glob, collections
sys.path.insert(0, __file__.rsplit("\\", 1)[0] + r"\cell_dictionary")
import decode_lib as dl
import numpy as np

RATES = [39062.5, 44100.0]
XML_DIR = r"C:\Users\hooki\trench-x3-clean\plugin\presets\bodies"
VOW = {"Ooh-To-Eee", "Eeh-To-Aah", "TalkingHedz", "UbuOrator", "DeepBouche", "MultiQVox"}
HZ = np.array(dl.log_grid_hz(40, 16000, 1024))
db = lambda x: 20 * math.log10(max(x, 1e-12))


def conj(p):
    return type(p).__name__ == "Conjugate" and p.hz > 0 and p.r > 0


def bwhz(r, sr):
    if r <= 0: return 0.0
    if r >= 1: return 0.01
    return max(0.01, (-math.log(r)) * sr / math.pi)


def rk(p, sr):
    n = type(p).__name__
    if n == "Degenerate": return ("off",)
    if n == "RealPair": return ("real", round(p.root_a / 5e-4), round(p.root_b / 5e-4))
    if not conj(p): return ("off",)
    return ("conj", round(1200 * math.log2(max(p.hz, 1e-6)) / 10.0),
            round(1200 * math.log2(bwhz(p.r, sr)) / 85.0))


def cancellation(geoms, sr):
    live = [g for g in geoms if not dl.stage_is_identity(g)]
    if len(live) < 2: return None
    tot = np.asarray(dl.corner_response_db(live, HZ, sr)); tot = tot[np.isfinite(tot)]
    t = tot.max() - tot.min()
    if t < 1: return None
    sp = 0.0
    for g in live:
        d = np.asarray(dl.stage_response_db(dl.stage_biquad(g, sr), HZ, sr)); d = d[np.isfinite(d)]
        sp += d.max() - d.min()
    return sp / t


def run(lib, sr, refs):
    p2k = [x for x in lib["stage_sources"] if x["family"] == "p2k"]
    out = {}
    sec = collections.defaultdict(list); pol = collections.defaultdict(list); zer = collections.defaultdict(list)
    nulls, cancels, vow_d, ctl_d, uniform, ncorner = [], [], [], [], 0, 0
    ivs, s6null = [], 0
    for s in p2k:
        for ci in range(s["corners"]):
            geoms = [dl.geometry_from_words_at(tuple(s["tracks"][si][ci]), sr) for si in range(s["stage_count"])]
            live = [g for g in geoms if not dl.stage_is_identity(g)]
            if not live: continue
            ncorner += 1
            d = [db(g.scale) for g in live]
            uniform += (max(d) - min(d) < 0.02)
            c = cancellation(geoms, sr)
            if c: cancels.append(c)
            poles = []
            for si, g in enumerate(geoms):
                if dl.stage_is_identity(g): continue
                sec[(rk(g.pole, sr), rk(g.zero, sr))].append(s["name"])
                pol[rk(g.pole, sr)].append(s["name"]); zer[rk(g.zero, sr)].append(s["name"])
                if conj(g.pole): poles.append(g.pole.hz)
                if conj(g.zero) and g.zero.r >= 0.99999:
                    nulls.append((si + 1, g.zero.hz, g.zero.hz / (sr / 2)))
                    if si + 1 == 6: s6null += 1
                if conj(g.pole) and conj(g.zero):
                    ivs.append(abs(12 * math.log2(g.zero.hz / g.pole.hz)))
            if ci in (0, 1) and len(poles) >= 3:
                f = sorted(poles)[:3]
                d3 = min(sum(abs(12 * math.log2(f[i] / r[i])) for i in range(3)) / 3 for r in refs)
                (vow_d if s["name"] in VOW else ctl_d).append(d3)
    ns = sum(len(v) for v in sec.values())
    out["sections"] = ns
    out["sec_distinct"] = len(sec); out["sec_collapse"] = 100 * (1 - len(sec) / ns)
    out["pol_distinct"] = len(pol); out["zer_distinct"] = len(zer)
    out["pol_cross"] = sum(1 for v in pol.values() if len(set(v)) >= 2)
    out["zer_cross"] = sum(1 for v in zer.values() if len(set(v)) >= 2)
    out["uniform_gain"] = f"{uniform}/{ncorner}"
    out["cancel_median"] = np.median(cancels)
    out["vow_median"] = np.median(vow_d); out["ctl_median"] = np.median(ctl_d)
    out["interval_median"] = np.median(ivs)
    out["unit_nulls"] = len(nulls); out["unit_at_s6"] = s6null
    top = collections.Counter(round(h) for _, h, _ in nulls).most_common(1)[0]
    out["null_parked_hz"] = top[0]; out["null_parked_n"] = top[1]
    out["null_parked_nyq"] = top[0] / (sr / 2)
    return out


def main(path):
    lib = json.load(open(path))
    tab = json.load(open(__file__.rsplit("\\", 2)[0] + r"\recipes\tables\phonetic_formants.json"))["vowels"]
    refs = [[v.get("f1"), v.get("f2"), v.get("f3")] for v in tab.values() if v.get("f3")]
    res = {sr: run(lib, sr, refs) for sr in RATES}

    xml = []
    for f in sorted(glob.glob(XML_DIR + r"\xml_*.body240")):
        for st in dl.decode_p2k_body(open(f, "rb").read(), 44100.0):
            c = cancellation(list(st), 44100.0)
            if c: xml.append(c)

    keys = [k for k in res[RATES[0]]]
    print(f"{'diagnostic':22}{'39,062.5':>14}{'44,100':>14}   moves?")
    for k in keys:
        a, b = res[RATES[0]][k], res[RATES[1]][k]
        fa = f"{a:.2f}" if isinstance(a, float) else str(a)
        fb = f"{b:.2f}" if isinstance(b, float) else str(b)
        same = fa == fb
        print(f"{k:22}{fa:>14}{fb:>14}   {'-' if same else 'YES'}")
    print(f"\nXML hand-built cancellation median (44.1k, unchanged reference): {np.median(xml):.2f}")


if __name__ == "__main__":
    main(sys.argv[1])
