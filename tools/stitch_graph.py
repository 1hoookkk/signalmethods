import glob
import json
import math
import os
import struct
import sys
from collections import defaultdict

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
P2K_DIR = os.path.join(ROOT, "plugin", "presets", "p2k")
XML_DIR = os.path.join(ROOT, "plugin", "presets", "bodies")
MORPHEUS_GLOB = os.path.join(ROOT, "evidence", "factory-data", "morpheus", "decoded", "*", "*.json")
FRAMES_JSON = os.path.join(ROOT, "native", "python", "workstation", "frames_3d.json")
OUT = os.path.join(ROOT, "native", "python", "workstation", "stitch.json")

FLOORS = ["P2K", "MORPHEUS", "X3", "VOWELS", "HEADS", "XL-1", "INSTRUMENTS"]
FLOOR_GAP = 0.45
NEAR_HZ = 0.03
NEAR_R = 0.01


def decode_word(word):
    u = word + 1
    if u == 65536:
        return 1.0
    if u == 1:
        return 0.0
    exponent = (u >> 12) & 0xF
    mantissa = float(u & 0xFFF)
    x = mantissa / 4096.0 if exponent == 0 else (mantissa + 4096.0) / 8192.0
    return math.ldexp(x, exponent - 15)


def pole_of(words, datum):
    d2, d3 = decode_word(words[2]), decode_word(words[3])
    q = 1.0 - d3
    p = 4.0 * d2 + d3 - 2.0
    disc = p * p - 4.0 * q
    if disc < 0.0 and q > 0.0:
        r = math.sqrt(q)
        c = max(-1.0, min(1.0, -p / (2.0 * r)))
        return (math.acos(c) / (2.0 * math.pi) * datum, r)
    return None


def title_of(filename):
    return " ".join(w.capitalize() for w in filename.replace(".body240", "").split("_"))


def read_body240(path):
    data = open(path, "rb").read()
    assert len(data) == 240, path
    words = struct.unpack("<120H", data)
    return [[list(words[(c * 6 + s) * 5:(c * 6 + s) * 5 + 5]) for s in range(6)] for c in range(4)]


def load_bodies():
    bodies = []
    for path in sorted(glob.glob(os.path.join(P2K_DIR, "*.body240"))):
        bodies.append({"name": title_of(os.path.basename(path)), "floor": "P2K", "datum": 44100.0, "corners": read_body240(path)})
    for path in sorted(glob.glob(os.path.join(XML_DIR, "*.body240"))):
        bodies.append({"name": os.path.basename(path).replace(".body240", ""), "floor": "X3", "datum": 44100.0, "corners": read_body240(path)})
    for path in sorted(glob.glob(MORPHEUS_GLOB)):
        d = json.load(open(path, encoding="utf-8"))
        corners = [[sec["words"] for sec in c["sections"]] for c in d["corner_data"]]
        bodies.append({"name": "MORPHEUS %03d %s" % (d["filter_number"], d["manual_name"] or ("filter %d" % d["filter_number"])), "floor": "MORPHEUS", "datum": d["datum_hz"], "corners": corners})
    return bodies


def group_of(name):
    if " · " in name and name.split(" · ")[1][:1] == "M":
        return "P2K"
    if name.startswith("MORPHEUS"):
        return "MORPHEUS"
    if name.startswith("X3") or name.startswith("LADDER"):
        return "X3"
    low = name.lower()
    if low.startswith("sung") or "hedz" in low or "vowel" in low:
        return "VOWELS"
    if "ear az" in low:
        return "HEADS"
    if name.startswith("Aud "):
        return "XL-1"
    return "INSTRUMENTS"


def load_loose_frames(bodies):
    known = set()
    for b in bodies:
        if b["floor"] == "P2K":
            known.add(b["name"])
    out = []
    for f in json.load(open(FRAMES_JSON, encoding="utf-8")):
        name = f["name"]
        if " · " in name and name.split(" · ")[0] in known:
            continue
        if name.startswith("MORPHEUS"):
            continue
        out.append({"name": name, "floor": group_of(name), "words": f["words"][:6], "datum": 44100.0})
    return out


def pole_key(corner):
    return tuple((row[2], row[3]) for row in corner)


def poles_of(corner, datum):
    return [pole_of(row, datum) for row in corner]


def near(pa, pb):
    if len(pa) != len(pb):
        return False
    for a, b in zip(pa, pb):
        if (a is None) != (b is None):
            return False
        if a is None:
            continue
        if abs(a[0] - b[0]) > NEAR_HZ * max(a[0], b[0], 1.0) or abs(a[1] - b[1]) > NEAR_R:
            return False
    return True


class Union:
    def __init__(self, n):
        self.p = list(range(n))

    def find(self, i):
        while self.p[i] != i:
            self.p[i] = self.p[self.p[i]]
            i = self.p[i]
        return i

    def join(self, a, b):
        a, b = self.find(a), self.find(b)
        if a != b:
            self.p[b] = a


def build_nodes(bodies):
    exact = {}
    members = []
    corner_node = []
    for bi, b in enumerate(bodies):
        ids = []
        for ci, corner in enumerate(b["corners"]):
            key = (b["floor"], pole_key(corner))
            if key not in exact:
                exact[key] = len(members)
                members.append({"floor": b["floor"], "datum": b["datum"], "corners": [], "poles": poles_of(corner, b["datum"])})
            members[exact[key]]["corners"].append((bi, ci))
            ids.append(exact[key])
        corner_node.append(ids)
    exact_count = len(members)
    by_floor = defaultdict(list)
    for i, m in enumerate(members):
        by_floor[m["floor"]].append(i)
    uf = Union(len(members))
    for floor, idx in by_floor.items():
        for x in range(len(idx)):
            for y in range(x + 1, len(idx)):
                if uf.find(idx[x]) != uf.find(idx[y]) and near(members[idx[x]]["poles"], members[idx[y]]["poles"]):
                    uf.join(idx[x], idx[y])
    roots = {}
    nodes = []
    for i in range(len(members)):
        r = uf.find(i)
        if r not in roots:
            roots[r] = len(nodes)
            nodes.append({"floor": members[i]["floor"], "datum": members[i]["datum"], "corners": [], "exact": 0})
        nodes[roots[r]]["corners"].extend(members[i]["corners"])
        nodes[roots[r]]["exact"] += 1
    node_of = [[roots[uf.find(n)] for n in ids] for ids in corner_node]
    for n in nodes:
        rows = None
        for bi, ci in n["corners"]:
            corner = bodies[bi]["corners"][ci]
            if rows is None:
                rows = [[0.0] * 5 for _ in corner]
            for s, row in enumerate(corner):
                for k in range(5):
                    rows[s][k] += row[k]
        n["words"] = [[int(round(v / len(n["corners"]))) for v in row] for row in rows]
        n["members"] = ["%s c%d" % (bodies[bi]["name"], ci) for bi, ci in n["corners"]]
        n["poles"] = [(round(p[0], 1), round(p[1], 4)) if p else None for p in poles_of(n["words"], n["datum"])]
    return nodes, node_of, exact_count


def build_edges_faces(bodies, node_of):
    edges = []
    faces = []
    seen = set()

    def edge(a, b, bi, axis):
        if a == b:
            return
        key = (min(a, b), max(a, b), bi, axis)
        if key in seen:
            return
        seen.add(key)
        edges.append({"a": a, "b": b, "body": bi, "axis": axis, "floor": bodies[bi]["floor"]})

    for bi, b in enumerate(bodies):
        ids = node_of[bi]
        if len(ids) == 4:
            edge(ids[0], ids[1], bi, "MORPH")
            edge(ids[2], ids[3], bi, "MORPH")
            edge(ids[0], ids[2], bi, "Q")
            edge(ids[1], ids[3], bi, "Q")
        else:
            for z in (0, 4):
                edge(ids[z], ids[z + 1], bi, "MORPH")
                edge(ids[z + 2], ids[z + 3], bi, "MORPH")
                edge(ids[z], ids[z + 2], bi, "Q")
                edge(ids[z + 1], ids[z + 3], bi, "Q")
            for c in range(4):
                edge(ids[c], ids[c + 4], bi, "Z")
        faces.append({"body": bi, "name": b["name"], "floor": b["floor"], "nodes": ids, "datum": b["datum"]})
    return edges, faces


def spectral(count, links):
    if count == 1:
        return np.zeros((1, 3))
    if count == 2:
        return np.array([[-0.5, 0.0, 0.0], [0.5, 0.0, 0.0]])
    L = np.zeros((count, count))
    for a, b in links:
        L[a, b] -= 1.0
        L[b, a] -= 1.0
        L[a, a] += 1.0
        L[b, b] += 1.0
    vals, vecs = np.linalg.eigh(L)
    order = np.argsort(vals)
    cols = []
    for i in order:
        if vals[i] > 1e-9:
            cols.append(vecs[:, i])
        if len(cols) == 3:
            break
    while len(cols) < 3:
        cols.append(np.zeros(count))
    xyz = np.stack(cols, axis=1)
    for k in range(3):
        col = xyz[:, k]
        if col.max() - col.min() > 1e-12:
            xyz[:, k] = (col - col.min()) / (col.max() - col.min()) - 0.5
    return xyz


def components(count, links):
    uf = Union(count)
    for a, b in links:
        uf.join(a, b)
    groups = defaultdict(list)
    for i in range(count):
        groups[uf.find(i)].append(i)
    return sorted(groups.values(), key=lambda g: -len(g))


def layout_floor(node_ids, edges):
    local = {n: i for i, n in enumerate(node_ids)}
    links = [(local[e["a"]], local[e["b"]]) for e in edges if e["a"] in local and e["b"] in local]
    xyz = np.zeros((len(node_ids), 3))
    comps = components(len(node_ids), links)
    if not comps:
        return xyz
    giant = comps[0]
    gl = {n: i for i, n in enumerate(giant)}
    glinks = [(gl[a], gl[b]) for a, b in links if a in gl and b in gl]
    gxyz = spectral(len(giant), glinks)
    scale = 0.7 if len(comps) > 1 else 1.0
    for i, n in enumerate(giant):
        xyz[n] = gxyz[i] * scale
    rest = comps[1:]
    for k, comp in enumerate(rest):
        cl = {n: i for i, n in enumerate(comp)}
        clinks = [(cl[a], cl[b]) for a, b in links if a in cl and b in cl]
        cxyz = spectral(len(comp), clinks) * (0.08 + 0.02 * min(len(comp), 8))
        ang = 2.0 * math.pi * k / max(1, len(rest))
        radius = 0.55 if len(comps) > 1 else 0.0
        for i, n in enumerate(comp):
            xyz[n] = cxyz[i] + np.array([radius * math.cos(ang), radius * math.sin(ang), 0.0])
    return xyz


def main():
    bodies = load_bodies()
    loose = load_loose_frames(bodies)
    nodes, node_of, exact_count = build_nodes(bodies)
    edges, faces = build_edges_faces(bodies, node_of)
    face_count = defaultdict(int)
    for f in faces:
        for n in set(f["nodes"]):
            face_count[n] += 1
    for fi, floor in enumerate(FLOORS):
        ids = [i for i, n in enumerate(nodes) if n["floor"] == floor]
        if not ids:
            continue
        xyz = layout_floor(ids, [e for e in edges if e["floor"] == floor])
        for i, n in enumerate(ids):
            nodes[n]["x"] = float(xyz[i, 0])
            nodes[n]["y"] = float(xyz[i, 1])
            nodes[n]["z"] = float(fi * FLOOR_GAP + 0.25 * xyz[i, 2])
    stubs = []
    for f in loose:
        poles = poles_of(f["words"], f["datum"])
        best, best_d = -1, 1e18
        for i, n in enumerate(nodes):
            if n["floor"] == "MORPHEUS":
                continue
            d = 0.0
            for a, b in zip(poles, n["poles"][:6]):
                if a is None or b is None:
                    d += 4.0
                    continue
                d += (math.log2(a[0] / b[0])) ** 2 + (25.0 * (a[1] - b[1])) ** 2
            if d < best_d:
                best, best_d = i, d
        fi = FLOORS.index(f["floor"])
        n = nodes[best]
        k = sum(1 for s in stubs if s["node"] == best)
        ang = 2.4 * k
        rad = 0.04 + 0.006 * k
        stubs.append({"name": f["name"], "floor": f["floor"], "words": f["words"], "node": best, "distance": round(best_d, 3),
                      "x": n["x"] + rad * math.cos(ang), "y": n["y"] + rad * math.sin(ang), "z": n["z"] + 0.05 + 0.003 * k})
    p2k_corner_total = sum(len(b["corners"]) for b in bodies if b["floor"] == "P2K")
    p2k_exact = {}
    for b in bodies:
        if b["floor"] == "P2K":
            for c in b["corners"]:
                p2k_exact.setdefault(pole_key(c), 0)
                p2k_exact[pole_key(c)] += 1
    census = {
        "bodies": len(bodies),
        "p2k_corners": p2k_corner_total,
        "p2k_exact_nodes": len(p2k_exact),
        "p2k_exact_groups_shared": sum(1 for v in p2k_exact.values() if v > 1),
        "p2k_nodes": sum(1 for n in nodes if n["floor"] == "P2K"),
        "morpheus_corners": sum(len(b["corners"]) for b in bodies if b["floor"] == "MORPHEUS"),
        "morpheus_nodes": sum(1 for n in nodes if n["floor"] == "MORPHEUS"),
        "x3_nodes": sum(1 for n in nodes if n["floor"] == "X3"),
        "exact_nodes_total": exact_count,
        "nodes": len(nodes),
        "edges": len(edges),
        "faces": len(faces),
        "stubs": len(stubs),
        "hubs": sorted([(face_count[i], [p[0] if p else 0 for p in nodes[i]["poles"][:3]]) for i in range(len(nodes)) if face_count[i] >= 4], reverse=True)[:5],
    }
    used = [f for f in FLOORS if any(n["floor"] == f for n in nodes)]
    out = {
        "floors": FLOORS,
        "used_floors": used,
        "floor_gap": FLOOR_GAP,
        "nodes": [{"floor": n["floor"], "datum": n["datum"], "words": n["words"], "members": n["members"], "faces": face_count[i],
                   "x": n["x"], "y": n["y"], "z": n["z"]} for i, n in enumerate(nodes)],
        "edges": edges,
        "faces": [{"name": f["name"], "floor": f["floor"], "datum": f["datum"], "nodes": f["nodes"]} for f in faces],
        "stubs": stubs,
        "census": census,
    }
    json.dump(out, open(OUT, "w", encoding="utf-8"), separators=(",", ":"))
    for k, v in census.items():
        print("%-26s %s" % (k, v))
    print("wrote", OUT, os.path.getsize(OUT) // 1024, "kB")


if __name__ == "__main__":
    main()
