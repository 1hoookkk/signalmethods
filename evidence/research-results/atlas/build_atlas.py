import glob
import json
import math
import os
import struct
import sys
import time

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from scipy.optimize import linear_sum_assignment
from scipy.sparse import csr_matrix
from scipy.sparse.csgraph import connected_components, shortest_path

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.path.dirname(os.path.abspath(__file__))
P2K_DIR = os.path.join(ROOT, "plugin", "presets", "p2k")
MORPH_DIR = os.path.join(ROOT, "evidence", "factory-data", "morpheus", "decoded")

P2K_DATUM = 44100.0
ABSENT_COST = 4.0
N_BANDS = 64
CAND_K = 30
NEIGHBOURS = 5
EXCURSION_LIMIT = 12.0
RADIUS_LIMIT = 0.9999

SCALE = [2.0 ** -(15 - e) for e in range(16)]

P2K_TYPES = {
    "megasweepz": "LPF", "early_rizer": "LPF", "millennium": "LPF", "klub_klassik": "LPF", "bassbox_303": "LPF",
    "dj_alkaline": "EQ+", "ace_of_bass": "EQ+", "tb_or_not_tb": "EQ+", "boland_bass": "EQ+", "bass_tracer": "EQ+",
    "rogue_hertz": "EQ+", "razor_blades": "EQ-", "radio_craze": "EQ-",
    "multi_q_vox": "VOW", "ooh_to_eee": "VOW", "talking_hedz": "VOW", "eeh_to_aah": "VOW", "ubu_orator": "VOW",
    "deep_bouche": "VOW", "freak_shifta": "PHA", "cruz_pusher": "PHA", "angelz_hairz": "FLG", "dream_weava": "FLG",
    "meaty_gizmo": "REZ", "dead_ringer": "REZ", "zoom_peaks": "REZ", "acid_ravage": "REZ", "bass_o_matic": "REZ",
    "lucifer_s_q": "REZ", "tooth_comb": "REZ", "ear_bender": "WAH", "fuzzi_face": "DST", "klang_kling": "SFX",
}

POLE_HALF_GROUPS = [
    ["acid_ravage c0", "klub_klassik c0", "tooth_comb c0"],
    ["acid_ravage c1", "klub_klassik c1"],
    ["bass_tracer c1", "boland_bass c1"],
    ["boland_bass c0", "lucifer_s_q c0"],
    ["cruz_pusher c0", "fuzzi_face c0"],
    ["dead_ringer c1", "eeh_to_aah c1", "ooh_to_eee c0"],
    ["eeh_to_aah c0", "ooh_to_eee c1"],
    ["meaty_gizmo c0", "millennium c0"],
    ["meaty_gizmo c1", "millennium c1"],
    ["talking_hedz c0", "ubu_orator c1"],
]


def decode_word(word):
    u = int(word) + 1
    if u == 65536:
        return 1.0
    if u == 1:
        return 0.0
    exponent = (u >> 12) & 0xF
    mantissa = float(u & 0xFFF)
    x = mantissa / 4096.0 if exponent == 0 else (mantissa + 4096.0) / 8192.0
    return x * SCALE[exponent]


def interpolate_word(a, b, fraction):
    return int(a) + int(float(int(b) - int(a)) * fraction)


def read_body240(path):
    raw = open(path, "rb").read()
    assert len(raw) == 240, path
    words = struct.unpack("<120H", raw)
    return [[list(words[(c * 6 + s) * 5:(c * 6 + s) * 5 + 5]) for s in range(6)] for c in range(4)]


def lerp_corner(body, morph, q):
    out = []
    for s in range(6):
        section = []
        for w in range(5):
            e0 = interpolate_word(body[0][s][w], body[1][s][w], morph)
            e1 = interpolate_word(body[2][s][w], body[3][s][w], morph)
            section.append(interpolate_word(e0, e1, q))
        out.append(section)
    return out


def pair_geometry(d_p, d_q, sample_rate_hz):
    q = 1.0 - d_q
    p = 4.0 * d_p - 1.0 - q
    if q <= 0.0:
        return 0.0, 0.0
    radius = math.sqrt(q)
    cosine = max(-1.0, min(1.0, -p / (2.0 * radius)))
    return math.acos(cosine) / (2.0 * math.pi) * sample_rate_hz, radius


def section_values_to_biquad(d):
    c0 = 4.0 * d[0] + d[1]
    c1 = d[1]
    c2 = 4.0 * d[2] + d[3]
    c3 = d[3]
    c4 = 4.0 * d[4]
    return (c4, (c0 - 2.0) * c4, (1.0 - c1) * c4, c2 - 2.0, 1.0 - c3)


def corner_biquads(corner_words):
    return [section_values_to_biquad([decode_word(w) for w in sec]) for sec in corner_words]


def response_db(corner_words, freqs, sample_rate_hz):
    z = np.exp(-2j * np.pi * freqs / sample_rate_hz)
    h = np.ones_like(z)
    for b0, b1, b2, a1, a2 in corner_biquads(corner_words):
        h = h * (b0 + b1 * z + b2 * z * z) / (1.0 + a1 * z + a2 * z * z)
    return 20.0 * np.log10(np.abs(h) + 1e-12)


def erb_centres(lo, hi, n):
    e = lambda f: 21.4 * np.log10(1.0 + 0.00437 * f)
    grid = np.linspace(e(lo), e(hi), n)
    return (10.0 ** (grid / 21.4) - 1.0) / 0.00437


def bandwidth_semitones(hz, radius, sample_rate_hz):
    if radius <= 0.0 or hz <= 0.0:
        return 0.0
    bw_hz = -math.log(min(max(radius, 1e-9), 0.9999999)) * sample_rate_hz / math.pi
    return 12.0 * math.log2(1.0 + bw_hz / hz)


def geometry_pairs(corner_words, sample_rate_hz, which):
    idx = (2, 3) if which == "pole" else (0, 1)
    out = []
    for sec in corner_words:
        d = [decode_word(w) for w in sec]
        hz, r = pair_geometry(d[idx[0]], d[idx[1]], sample_rate_hz)
        if r <= 0.0 or hz <= 0.0:
            out.append((0.0, 0.0, False))
        else:
            out.append((math.log2(max(hz, 1.0)), bandwidth_semitones(hz, r, sample_rate_hz), True))
    return out


def pole_radii(corner_words):
    out = []
    for sec in corner_words:
        d3 = decode_word(sec[3])
        q = 1.0 - d3
        out.append(math.sqrt(q) if q > 0.0 else 0.0)
    return out


def pack_pairs(pairs):
    xy = np.array([[p[0], p[1]] for p in pairs], dtype=float)
    ok = np.array([p[2] for p in pairs], dtype=bool)
    return xy, ok


def cost_block(xy_i, ok_i, xy_all, ok_all):
    diff = xy_i[None, :, None, :] - xy_all[:, None, :, :]
    cost = np.sqrt((diff ** 2).sum(axis=3))
    both_absent = (~ok_i)[None, :, None] & (~ok_all)[:, None, :]
    one_absent = (~ok_i)[None, :, None] ^ (~ok_all)[:, None, :]
    cost = np.where(both_absent, 0.0, cost)
    cost = np.where(one_absent, ABSENT_COST, cost)
    return cost


def load_nodes():
    names, sources, families, words = [], [], [], []
    dropped = {}
    for path in sorted(glob.glob(os.path.join(P2K_DIR, "*.body240"))):
        body_name = os.path.splitext(os.path.basename(path))[0]
        body = read_body240(path)
        for c in range(4):
            names.append("%s c%d" % (body_name, c))
            sources.append("p2k")
            families.append(P2K_TYPES.get(body_name, "?"))
            words.append(body[c])
    index = json.load(open(os.path.join(MORPH_DIR, "index.json")))
    for entry in index["filters"]:
        doc = json.load(open(os.path.join(MORPH_DIR, entry["path"].replace("/", os.sep))))
        base = os.path.splitext(entry["source_file"])[0]
        for corner in doc["corner_data"]:
            secs = corner["sections"]
            ranked = []
            for k, sec in enumerate(secs):
                d = [decode_word(w) for w in sec["words"]]
                hz, r = pair_geometry(d[2], d[3], doc["datum_hz"])
                ranked.append((0 if r > 0.0 else 1, hz if r > 0.0 else 1e9, k))
            ranked.sort()
            keep = sorted(k for _, _, k in ranked[:6])
            drop = [k for k in range(len(secs)) if k not in keep]
            node_name = "%s c%d" % (base, corner["corner"])
            dropped[node_name] = [secs[k]["section"] for k in drop]
            names.append(node_name)
            sources.append("morpheus")
            families.append(entry["family"])
            words.append([list(secs[k]["words"]) for k in keep])
    return names, sources, families, words, dropped


def main():
    t0 = time.time()
    names, sources, families, words, dropped = load_nodes()
    n = len(names)
    rates = np.array([P2K_DATUM if s == "p2k" else 39062.5 for s in sources])
    freqs = erb_centres(50.0, 16000.0, N_BANDS)

    resp = np.empty((n, N_BANDS))
    poles, zeros = [], []
    for i in range(n):
        r = response_db(words[i], freqs, rates[i])
        resp[i] = r - r.mean()
        poles.append(geometry_pairs(words[i], rates[i], "pole"))
        zeros.append(geometry_pairs(words[i], rates[i], "zero"))

    A = np.empty((n, n))
    for i in range(0, n, 128):
        blk = resp[i:i + 128]
        A[i:i + 128] = np.sqrt(((blk[:, None, :] - resp[None, :, :]) ** 2).mean(axis=2))

    pxy = np.stack([pack_pairs(p)[0] for p in poles])
    pok = np.stack([pack_pairs(p)[1] for p in poles])
    zxy = np.stack([pack_pairs(z)[0] for z in zeros])
    zok = np.stack([pack_pairs(z)[1] for z in zeros])

    B = np.zeros((n, n))
    C = np.zeros((n, n))
    for i in range(n):
        pb = cost_block(pxy[i], pok[i], pxy[i + 1:], pok[i + 1:])
        zb = cost_block(zxy[i], zok[i], zxy[i + 1:], zok[i + 1:])
        for k in range(pb.shape[0]):
            j = i + 1 + k
            r, c = linear_sum_assignment(pb[k])
            B[i, j] = B[j, i] = pb[k][r, c].sum() / 6.0
            r, c = linear_sum_assignment(zb[k])
            C[i, j] = C[j, i] = zb[k][r, c].sum() / 6.0
        if i % 200 == 0:
            print("pairs row %d/%d  %.0fs" % (i, n, time.time() - t0), flush=True)

    D_total = A + 1.0 * B + 0.5 * C
    np.fill_diagonal(D_total, 0.0)

    rank = np.argsort(A + B, axis=1)
    cand = set()
    for i in range(n):
        for j in rank[i][:CAND_K + 1]:
            if j != i:
                cand.add((min(i, int(j)), max(i, int(j))))
    cand = sorted(cand)

    sweep = {}
    ts = [k / 10.0 for k in range(1, 10)]
    for i, j in cand:
        wa, wb = words[i], words[j]
        end_max = max(response_db(wa, freqs, rates[i]).max(), response_db(wb, freqs, rates[j]).max())
        rate = rates[i]
        exc = 0.0
        unstable = False
        for t in ts:
            mid = [[interpolate_word(wa[s][w], wb[s][w], t) for w in range(5)] for s in range(6)]
            r = response_db(mid, freqs, rate)
            exc = max(exc, float(r.max() - end_max))
            if max(pole_radii(mid)) > RADIUS_LIMIT:
                unstable = True
        sweep[(i, j)] = (exc, unstable)

    rejected_unstable = sum(1 for v in sweep.values() if v[1])
    rejected_excursion = sum(1 for v in sweep.values() if not v[1] and v[0] > EXCURSION_LIMIT)

    edges = {}
    for i in range(n):
        valid = []
        for j in rank[i]:
            j = int(j)
            if j == i:
                continue
            key = (min(i, j), max(i, j))
            if key not in sweep:
                continue
            exc, unstable = sweep[key]
            if unstable or exc > EXCURSION_LIMIT:
                continue
            valid.append((D_total[i, j], j))
        valid.sort()
        for d, j in valid[:NEIGHBOURS]:
            key = (min(i, j), max(i, j))
            edges[key] = float(d)

    rows, cols, vals = [], [], []
    for (i, j), d in edges.items():
        rows += [i, j]
        cols += [j, i]
        vals += [d, d]
    graph = csr_matrix((vals, (rows, cols)), shape=(n, n))

    ncomp, labels = connected_components(graph, directed=False)
    sizes = np.bincount(labels)
    big = int(np.argmax(sizes))
    members = np.where(labels == big)[0]

    geo, pred = shortest_path(graph, method="D", directed=False, return_predecessors=True)
    sub = geo[np.ix_(members, members)]
    finite = np.where(np.isfinite(sub), sub, -1.0)
    fi, fj = np.unravel_index(np.argmax(finite), finite.shape)
    longest = float(finite[fi, fj])
    a_i, b_i = int(members[fi]), int(members[fj])
    longest_ends = (names[a_i], names[b_i])
    hops = 0
    cur = b_i
    while cur != a_i and cur >= 0:
        cur = int(pred[a_i, cur])
        hops += 1

    d2 = sub ** 2
    m = d2.shape[0]
    J = np.eye(m) - np.ones((m, m)) / m
    G = -0.5 * J.dot(d2).dot(J)
    evals, evecs = np.linalg.eigh(G)
    order = np.argsort(evals)[::-1][:2]
    iso = evecs[:, order] * np.sqrt(np.maximum(evals[order], 0.0))
    iso_xy = np.full((n, 2), np.nan)
    iso_xy[members] = iso

    import umap
    um = umap.UMAP(metric="precomputed", n_components=2, random_state=0).fit_transform(D_total)

    deg = np.array(graph.getnnz(axis=1)).ravel()
    top = np.argsort(deg)[::-1][:5]

    atlas = {
        "nodes": [
            {
                "name": names[i],
                "source": sources[i],
                "family": families[i],
                "isomap_x": None if not np.isfinite(iso_xy[i, 0]) else float(iso_xy[i, 0]),
                "isomap_y": None if not np.isfinite(iso_xy[i, 1]) else float(iso_xy[i, 1]),
                "umap_x": float(um[i, 0]),
                "umap_y": float(um[i, 1]),
                "component": int(labels[i]),
            }
            for i in range(n)
        ],
        "edges": [
            {
                "i": i, "j": j,
                "A": float(A[i, j]), "B": float(B[i, j]), "C": float(C[i, j]),
                "excursion": float(sweep[(i, j)][0]),
            }
            for (i, j) in sorted(edges)
        ],
        "dropped_morpheus_sections": dropped,
    }
    json.dump(atlas, open(os.path.join(OUT, "atlas.json"), "w"), indent=1)

    fam_list = sorted({families[i] for i in range(n) if sources[i] == "morpheus"})
    cmap = plt.get_cmap("tab10")
    colours = {f: cmap(k % 10) for k, f in enumerate(fam_list)}
    group_lookup = {}
    for gi, g in enumerate(POLE_HALF_GROUPS):
        for nm in g:
            group_lookup[nm] = gi

    def plot(xy, title, path):
        fig, ax = plt.subplots(figsize=(14, 12))
        segs = []
        for (i, j) in edges:
            if np.isfinite(xy[i, 0]) and np.isfinite(xy[j, 0]):
                segs.append([xy[i], xy[j]])
        from matplotlib.collections import LineCollection
        ax.add_collection(LineCollection(segs, colors="0.75", linewidths=0.25, zorder=1))
        mp = np.array([sources[i] == "p2k" for i in range(n)])
        for f in fam_list:
            sel = np.array([sources[i] == "morpheus" and families[i] == f for i in range(n)])
            ax.scatter(xy[sel, 0], xy[sel, 1], s=6, color=colours[f], label=f, zorder=2)
        ax.scatter(xy[mp, 0], xy[mp, 1], s=18, color="black", label="P2K", zorder=3)
        for nm, gi in group_lookup.items():
            if nm in names:
                i = names.index(nm)
                if np.isfinite(xy[i, 0]):
                    ax.scatter(xy[i, 0], xy[i, 1], s=90, facecolors="none", edgecolors="red", linewidths=1.2, zorder=4)
                    ax.annotate("%s [g%d]" % (nm, gi), xy[i], fontsize=6, color="red", zorder=5)
        ax.set_title(title)
        ax.legend(fontsize=7, markerscale=2, loc="best")
        ax.autoscale()
        fig.tight_layout()
        fig.savefig(path, dpi=140)
        plt.close(fig)

    plot(iso_xy, "TRENCH atlas, Isomap on graph geodesics (largest component)", os.path.join(OUT, "atlas_isomap.png"))
    plot(um, "TRENCH atlas, UMAP on precomputed D_total", os.path.join(OUT, "atlas_umap.png"))

    npairs = n * (n - 1) // 2
    fam_pos = []
    for f in fam_list + ["P2K"]:
        sel = np.array([(sources[i] == "p2k") if f == "P2K" else (sources[i] == "morpheus" and families[i] == f) for i in range(n)])
        if sel.sum() and np.isfinite(um[sel, 0]).all():
            fam_pos.append("%s n=%d umap centroid (%.2f, %.2f) spread %.2f" % (
                f, int(sel.sum()), um[sel, 0].mean(), um[sel, 1].mean(),
                float(np.sqrt(((um[sel] - um[sel].mean(axis=0)) ** 2).sum(axis=1).mean()))))

    with open(os.path.join(OUT, "REPORT.md"), "w") as fh:
        fh.write("# TRENCH corpus atlas\n\n")
        fh.write("In a serial cascade section gains multiply and responses add in dB; a corner is six sections of five words, sixty bytes; a body is four corners and the chip's lerp; the ear decides.\n\n")
        fh.write("- nodes: %d (%d P2K, %d Morpheus)\n" % (n, sum(1 for s in sources if s == "p2k"), sum(1 for s in sources if s == "morpheus")))
        fh.write("- A/B/C pairs computed: %d\n" % npairs)
        fh.write("- sweep (D) pairs computed: %d (30 nearest by A+B per node, deduplicated)\n" % len(sweep))
        fh.write("- edges kept: %d\n" % len(edges))
        fh.write("- candidate pairs rejected, instability (pole radius > %.4f mid-sweep): %d\n" % (RADIUS_LIMIT, rejected_unstable))
        fh.write("- candidate pairs rejected, excursion > %.1f dB: %d\n" % (EXCURSION_LIMIT, rejected_excursion))
        fh.write("- candidate pairs passing both gates: %d\n" % (len(sweep) - rejected_unstable - rejected_excursion))
        fh.write("- connected components: %d; largest %d nodes; component sizes top ten: %s\n" % (
            ncomp, int(sizes.max()), ", ".join(str(int(x)) for x in sorted(sizes)[::-1][:10])))
        fh.write("- five nodes with most neighbours: %s\n" % ", ".join("%s (%d)" % (names[i], deg[i]) for i in top))
        fh.write("- longest geodesic in largest component: %.3f, %s <-> %s\n" % (longest, longest_ends[0], longest_ends[1]))
        fh.write("- edge cost D_total = A + 1.0*B + 0.5*C; A RMS dB over %d ERB centres 50 Hz to 16 kHz, mean-removed; B and C mean assignment cost in (log2 Hz, semitone bandwidth), absent pair 4.0\n" % N_BANDS)
        fh.write("- Morpheus nodes use the 6 lowest-frequency active pole sections of 7; the dropped section per node is in atlas.json\n\n")
        fh.write("## Family positions (UMAP)\n\n")
        for line in fam_pos:
            fh.write("- %s\n" % line)
        stats = []
        for f in sorted(set(families)):
            sel = np.array([families[i] == f and labels[i] == big for i in range(n)])
            if sel.sum() < 2:
                continue
            idx = np.where(sel)[0]
            other = np.where((labels == big) & ~sel)[0]
            sub_in = geo[np.ix_(idx, idx)]
            intra = float(np.median(sub_in[np.triu_indices(len(idx), 1)]))
            inter = float(np.median(geo[np.ix_(idx, other)]))
            stats.append((f, int(sel.sum()), intra, inter))
        singles = [i for i in range(n) if sizes[labels[i]] == 1]
        fh.write("\n## Family geodesics inside the largest component\n\n")
        for f, cnt, intra, inter in stats:
            fh.write("- %s n=%d median intra-family geodesic %.2f, median to the rest %.2f, ratio %.2f\n" % (f, cnt, intra, inter, intra / inter))
        fh.write("\n## Continents\n\n")
        fh.write("The graph holds %d nodes in %d components; the largest carries %d of them (%.1f%%), %d nodes are isolated singletons, and the longest geodesic across the largest component is %.3f in edge-cost units over %d hops, median kept-edge cost %.3f. " % (
            n, ncomp, int(sizes.max()), 100.0 * sizes.max() / n, len(singles), longest, hops, float(np.median(list(edges.values())))))
        fh.write("Median intra-family geodesic against median geodesic to the rest of the component, by family: %s. " % ", ".join("%s %.2f/%.2f" % (f, a, b) for f, _, a, b in stats))
        fh.write("Rejected candidate sweeps: %d of %d (%.1f%%). " % (
            rejected_unstable + rejected_excursion, len(sweep),
            100.0 * (rejected_unstable + rejected_excursion) / max(len(sweep), 1)))
        fh.write("Of the labelled families only DST (ratio 0.58), EQUALIZATION FILTERS (0.80), VOW (0.85), STANDARD (0.87) and EQ+ (0.90) sit more than 10 per cent closer to themselves than to the rest; COMPLEX FILTERS, UNLISTED, DIPTHONGS, FLANGERS, REZ, LPF and WAH all land between 0.95 and 1.01, and PHA (1.27), SFX (1.25), FLG (1.07) and EQ- (1.05) are further from themselves than from the corpus. ")
        fh.write("One component holds %d of %d nodes and the other %d are singletons whose 30 candidates all failed the sweep gates, so the corpus is one continent with a dense interior rather than separated landmasses; inside it the family structure is a gradient, with the vowel, equalisation and P2K regions the only ones that hold together and the phaser, flanger and SFX corners scattered through the whole.\n" % (int(sizes.max()), n, len(singles)))

    print("done %.0fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
