import math
import pathlib
import struct

ROOT = pathlib.Path(__file__).resolve().parents[1]
X3 = pathlib.Path(r"C:\Users\hooki\trench-x3-clean\ref")
MORPHEUS = pathlib.Path(r"C:\Users\hooki\trench-authoring\ref\morpheus\bodies")
OUT = ROOT / "ref" / "sections.tsv"

COLUMNS = ("source", "body", "family", "family_basis", "variant", "datum_hz", "datum_basis", "corner", "section",
           "w_zero_mag", "w_zero_rsq", "w_pole_mag", "w_pole_rsq", "w_gain",
           "pole_kind", "pole_hz", "pole_r", "pole_bw_oct", "pole_real_a", "pole_real_b",
           "zero_kind", "zero_hz", "zero_r", "zero_bw_oct", "zero_real_a", "zero_real_b",
           "zero_offset_oct", "scale", "section_dc_db")


def decode(w):
    u = w + 1
    if u == 65536:
        return 1.0
    if u == 1:
        return 0.0
    e = (u >> 12) & 15
    m = u & 4095
    x = m / 4096 if e == 0 else (m + 4096) / 8192
    return x * 2.0 ** (e - 15)


def pair(mag, rsq, sr):
    d0, d1 = decode(mag), decode(rsq)
    p, q = 4 * d0 + d1 - 2, 1 - d1
    if p == 0.0 and q == 0.0:
        return ("degenerate", "", "", "", "", ""), p, q
    disc = p * p - 4 * q
    if disc < 0:
        r = math.sqrt(q)
        hz = math.acos(max(-1.0, min(1.0, -p / (2 * r)))) / (2 * math.pi) * sr
        bw = 2 * math.asinh((-math.log(max(r, 1e-12)) * sr / math.pi) / (2 * max(hz, 1e-9))) / math.log(2)
        return ("conjugate", f"{hz:.3f}", f"{r:.6f}", f"{bw:.4f}", "", ""), p, q
    s = math.sqrt(disc)
    a, b = (-p + s) / 2, (-p - s) / 2
    return ("real", "", "", "", f"{a:.6f}", f"{b:.6f}"), p, q


P2K_KEYWORDS = {
    "bass": ("bass", "303", "tb_or", "boland", "bottom", "sub"),
    "vocal": ("vox", "orator", "hedz", "bouche", "talk", "mouth", "aah", "ooh", "vow"),
    "sweep": ("sweep", "rizer", "ravage", "weava", "tracer", "hertz", "peaks", "shifta"),
    "phaser": ("phaze", "pha", "flg", "flange", "blissbatz", "comb"),
}
P2K_SUFFIX = {"lpf": "lowpass", "hpf": "highpass", "bpf": "bandpass", "eq": "eq", "pha": "phaser",
              "flg": "flanger", "vow": "vocal"}
MORPHEUS_FAMILIES = ((0, 20, "flangers"), (21, 43, "dipthongs"), (44, 69, "standard"),
                     (70, 75, "equalization"), (76, 156, "complex"))


def p2k_family(body):
    tail = body.rsplit("_", 1)[-1]
    if tail in P2K_SUFFIX:
        return P2K_SUFFIX[tail], "name suffix (SysEx table)"
    for family, keys in P2K_KEYWORDS.items():
        if any(k in body for k in keys):
            return family, "name keyword"
    return "other", "name keyword"


def morpheus_family(body):
    index = int(body[:3])
    for lo, hi, name in MORPHEUS_FAMILIES:
        if lo <= index <= hi:
            return name, "manual family header"
    return "unlisted", "beyond the manual's 157 listed filters"


def rows_of(source, body, family, basis, datum_basis, variant, sr, words, corners, sections):
    for corner in range(corners):
        for section in range(sections):
            w = words[(corner * sections + section) * 5:(corner * sections + section) * 5 + 5]
            zero, zp, zq = pair(w[0], w[1], sr)
            pole, pp, pq = pair(w[2], w[3], sr)
            scale = 4 * decode(w[4])
            dc_num, dc_den = 1 + zp + zq, 1 + pp + pq
            linear = abs(scale * dc_num / dc_den) if dc_den != 0 else 0.0
            dc = 20 * math.log10(linear) if linear > 0 else ""
            offset = ""
            if zero[0] == "conjugate" and pole[0] == "conjugate":
                offset = f"{math.log2(float(zero[1]) / float(pole[1])):.4f}"
            yield (source, body, family, basis, variant, f"{sr:g}", datum_basis, corner, section,
                   f"0x{w[0]:04X}", f"0x{w[1]:04X}", f"0x{w[2]:04X}", f"0x{w[3]:04X}", f"0x{w[4]:04X}",
                   *pole, *zero, offset, f"{scale:.6f}", f"{dc:.3f}" if dc != "" else "")


def main():
    rows = []
    for skin in sorted(X3.glob("p2k_variants/P2k_*")):
        for path in sorted(skin.glob("variant_*.bin")):
            words = struct.unpack("<120H", path.read_bytes())
            variant = path.name.split("_")[1]
            family, basis = p2k_family(skin.name)
            index = int(skin.name[4:7])
            datum_basis = ("44.1 kHz: Talking Hedz corner captures null against the decode at this rate"
                           if index <= 32 else
                           "44.1 kHz assumed; skins 033-049 decode to unit-circle roots and zero scales, not bodies")
            rows.extend(rows_of("p2k", skin.name, family, basis, datum_basis, variant, 44100.0, words, 4, 6))
    for path in sorted(MORPHEUS.glob("*.body")):
        data = path.read_bytes()
        if len(data) != 560:
            continue
        words = struct.unpack("<280H", data)
        family, basis = morpheus_family(path.stem)
        rows.extend(rows_of("morpheus", path.stem, family, basis,
                            "39062.5 Hz: the Morpheus datum, established against the manual's stated frequencies",
                            "", 39062.5, words, 8, 7))
    with OUT.open("w", newline="\n") as f:
        f.write("\t".join(COLUMNS) + "\n")
        for row in rows:
            f.write("\t".join(str(v) for v in row) + "\n")
    p2k = sum(1 for r in rows if r[0] == "p2k")
    print(f"{OUT}: {len(rows)} rows ({p2k} p2k, {len(rows) - p2k} morpheus)")


if __name__ == "__main__":
    main()
