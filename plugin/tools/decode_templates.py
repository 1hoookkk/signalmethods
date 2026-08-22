from __future__ import annotations
import json, math, os, sys
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from pyruntime.packed_interp import decode

TPL = r"C:\Users\hooki\OneDrive\Documents\Creative Professional\Emulator X Family\Templates\Filter"
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   "dossiers", "templates", "decoded")
FAM = 0
BASE = [18, 18, 4, 1]
SCALE = [220, 220, 200, 177]
SR = 44100.0
SHIFT = -32
PAD = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xE000)

def compile_endpoint(type_id: int, fbyte: int, gbyte: int):
    freq = ((SCALE[FAM] * fbyte) >> 7) + BASE[FAM]
    g = gbyte if gbyte < 128 else gbyte - 256
    gain = max(-32, min(31, ((g - 0x40) >> 1) + SHIFT))
    rad = ((freq * 0x7C) >> 8) + 0x76
    cl = lambda x: max(0, min(255, x))
    if type_id == 1:
        return (freq << 8, cl(rad + gain) << 8, freq << 8, cl(rad - gain) << 8, 0xE000)
    if type_id == 2:
        return (0xEC00, 0xFF00, freq << 8, cl(rad - gain) << 8,
                ((freq + 0xF5) << 8) & 0xFFFF)
    if type_id == 3:
        w0 = BASE[FAM] << 8
        w1 = (((BASE[FAM] * 0x7C) >> 8) + 0x96) << 8
        ef = freq
        if freq > 0xDB and gain < 0:
            ef = (((freq - 0xDC) * (gain + 0x20)) >> 5) + 0xDC
        return (w0, w1, ef << 8, cl(rad - gain) << 8,
                ((freq - 18) * -12 - 8192) & 0xFFFF)
    return None

def words_to_pz(w):
    d = [decode(x & 0xFFFF) for x in w]
    c0, c1, c2, c3, c4 = 4*d[0]+d[1], d[1], 4*d[2]+d[3], d[3], 4*d[4]
    b0, b1, b2 = c4, (c0-2.0)*c4, (1.0-c1)*c4
    a1, a2 = c2-2.0, 1.0-c3
    def root(b, c):
        disc = b*b - 4*c
        if disc >= 0:
            r = max(abs((-b+math.sqrt(disc))/2), abs((-b-math.sqrt(disc))/2))
            return {"hz": 0.0, "radius": round(r, 4), "real": True}
        th = math.atan2(math.sqrt(-disc)/2, -b/2)
        return {"hz": round(th/(2*math.pi)*SR, 1),
                "radius": round(math.sqrt(c), 4), "real": False}
    return {"pole": root(a1, a2),
            "zero": root(b1/b0, b2/b0) if b0 else None,
            "scale_db": round(20*math.log10(abs(b0)), 2) if b0 else None}

def main():
    os.makedirs(OUT, exist_ok=True)
    for fn in sorted(os.listdir(TPL)):
        if not fn.endswith(".xml"):
            continue
        name = fn[:-4]
        root = ET.parse(os.path.join(TPL, fn)).getroot()
        secs = []
        for s in root.iter("designer-section"):
            g = lambda t: int(s.find(t).text.strip())
            t = g("type")
            rec = {"index": int(s.get("index")), "type": t,
                   "bytes": {"lo_freq": g("low-freq"), "lo_gain": g("low-gain"),
                             "hi_freq": g("high-freq"), "hi_gain": g("high-gain")}}
            if t in (1, 2, 3):
                wA = compile_endpoint(t, g("low-freq"), g("low-gain"))
                wB = compile_endpoint(t, g("high-freq"), g("high-gain"))
                rec["M0"] = words_to_pz(wA)
                rec["M100"] = words_to_pz(wB)
                rec["words"] = {"M0": list(wA), "M100": list(wB)}
            secs.append(rec)
        doc = {"schema": "morph-designer-decoded-v1", "name": name,
               "sr_hz": SR, "family": FAM, "q_axis": "none (endpoints only)",
               "oracle": "ref/ghidra_extracts/morphdesigner_types.md",
               "sections": secs}
        json.dump(doc, open(os.path.join(OUT, name + ".json"), "w"), indent=1)
        act = [r for r in secs if "M0" in r]
        parts = [f"S{r['index']}t{r['type']} "
                 f"{r['M0']['pole']['hz']:.0f}->{r['M100']['pole']['hz']:.0f}Hz"
                 for r in act]
        print(f"{name:24s} {'; '.join(parts)}")

if __name__ == "__main__":
    main()
