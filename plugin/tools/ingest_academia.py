
import csv
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = os.path.join(ROOT, "recipes", "tables", "academia")
RAW = os.path.join(BASE, "raw")

FORMANT_ORDER = ["F1", "F2", "F3", "F4"]

def write_dataset(dataset, category, citation, source, method, objects, note=None):
    doc = {
        "schema": "acoustic-source-v1",
        "dataset": dataset,
        "category": category,
        "citation": citation,
        "source": source,
        "measurement_method": method,
    }
    if note:
        doc["note"] = note
    doc["objects"] = objects
    out = os.path.join(BASE, dataset + ".json")
    with open(out, "w", encoding="utf-8") as fh:
        json.dump(doc, fh, indent=1, ensure_ascii=False)
    print("%-30s %5d objects -> %s" % (dataset, len(objects), os.path.basename(out)))

def formant_list(pairs):
    pairs = sorted((p for p in pairs if p[0]), key=lambda p: p[0])[:4]
    return [{"mode_or_formant": FORMANT_ORDER[i],
             "frequency_hz": f,
             "bandwidth_hz": b}
            for i, (f, b) in enumerate(pairs)]

ETL_SPEAKERS = ["S0001", "S0003", "S0010", "S0015", "S0041"]
ETL_VOWELS = ["i", "e", "a", "o", "uu"]

def ingest_etl():
    src = os.path.join(RAW, "etl_formantdata.txt")
    rows = [ln.split() for ln in open(src, encoding="utf-8") if ln.strip()]
    assert len(rows) == 2750, "expected 2750 rows, got %d" % len(rows)

    objects = []
    for i, cols in enumerate(rows):
        assert len(cols) == 8, "row %d has %d cols" % (i, len(cols))
        spk = ETL_SPEAKERS[i // 550]
        vow = ETL_VOWELS[(i // 110) % 5]
        word = (i // 5) % 22 + 1
        frame = i % 5 + 1
        objects.append({
            "object_id": "ETL_%s_%s_w%02d_f%d" % (spk, vow, word, frame),
            "label": vow,
            "f0_hz": None,
            "formants": formant_list([(float(cols[k]), float(cols[k + 4]))
                                      for k in range(4)]),
        })
    write_dataset(
        "etl_mokhtari_tanaka_2000", "vowels",
        "Mokhtari & Tanaka 2000, ETL word database formant study",
        "https://isd.pu-toyama.ac.jp/~parham/documents/formantsETL/",
        "F1-F4 frequencies and bandwidths, 5 consecutive steady-state frames "
        "per vowel nucleus; Japanese isolated words (ETL-WD-I/II)",
        objects,
        note="The only vowel set here carrying measured bandwidths.")

PHONTOOLS = {
    "pb52": ("peterson_barney_1952", "PB52",
             "Peterson & Barney 1952, JASA 24(2):175-184",
             "phonTools data(pb52)",
             "F1-F3 frequencies, /hVd/ syllables; 76 speakers x 10 vowels x 2 reps"),
    "h95": ("hillenbrand_1995", "H95",
            "Hillenbrand, Getty, Clark & Wheeler 1995, JASA 97(5):3099-3111",
            "phonTools data(h95)",
            "LPC formant tracking, /hVd/ syllables; 139 speakers x 12 vowels"),
    "b95": ("bark_1995", "B95", "phonTools data(b95)", "phonTools data(b95)",
            "published mean formant table"),
    "f73": ("fant_1973", "F73", "Fant 1973, Speech Sounds and Features",
            "phonTools data(f73)", "published mean formant table"),
    "p73": ("pols_1973", "P73", "Pols et al. 1973", "phonTools data(p73)",
            "published mean formant table"),
    "y96": ("yang_1996", "Y96", "Yang 1996", "phonTools data(y96)",
            "published mean formant table"),
    "a96": ("aronson_1996", "A96", "Aronson et al. 1996", "phonTools data(a96)",
            "published mean formant table"),
    "f99": ("fourakis_1999", "F99", "Fourakis et al. 1999", "phonTools data(f99)",
            "published mean formant table"),
    "t07": ("thomson_2007", "T07", "Thomson 2007", "phonTools data(t07)",
            "published mean formant table"),
}

def _pick(header, *names):
    low = {h.lower(): h for h in header}
    for n in names:
        if n in low:
            return low[n]
    return None

def _num(v):
    v = (v or "").strip()
    if not v or v in ("NA", "0"):
        return None
    try:
        return float(v)
    except ValueError:
        return None

def ingest_phontools(csvdir):
    for stem, (dataset, prefix, citation, source, method) in PHONTOOLS.items():
        path = os.path.join(csvdir, stem + ".csv")
        if not os.path.exists(path):
            print("skip %s (not exported)" % stem)
            continue
        rd = csv.DictReader(open(path, encoding="utf-8"))
        header = rd.fieldnames or []
        c_spk = _pick(header, "speaker", "spkr", "subject")
        c_vow = _pick(header, "vowel", "vwl")
        c_f0 = _pick(header, "f0")
        c_sex = _pick(header, "sex", "gender")
        c_grp = _pick(header, "type", "age", "group")
        c_rep = _pick(header, "repetition", "rep")
        fcols = [c for c in (_pick(header, "f%d" % k) for k in (1, 2, 3, 4)) if c]

        objects = []
        for i, r in enumerate(rd):
            spk = (r.get(c_spk) or str(i)).strip()
            if spk.endswith(".0"):
                spk = spk[:-2]
            vow = (r.get(c_vow) or "x").strip()
            oid = "%s_spk%s_%s_%d" % (prefix, spk, vow, i)
            obj = {
                "object_id": oid,
                "label": vow,
                "f0_hz": _num(r.get(c_f0)) if c_f0 else None,
                "formants": formant_list([(_num(r.get(c)), None) for c in fcols]),
            }
            for key, col in (("sex", c_sex), ("group", c_grp), ("repetition", c_rep)):
                if col and (r.get(col) or "").strip():
                    obj[key] = r[col].strip()
            objects.append(obj)
        write_dataset(dataset, "vowels", citation, source, method, objects)

def ingest_kent(csvdir):
    path = os.path.join(csvdir, "Bandwidth_Measurements.csv")
    if not os.path.exists(path):
        print("skip kent (Bandwidth_Measurements.csv not found)")
        return
    rows = list(csv.DictReader(open(path, encoding="utf-8")))
    groups = {}
    order = []
    for r in rows:
        key = (r["study_id"], r.get("condition_type", ""), r.get("condition_value", ""))
        if key not in groups:
            groups[key] = []
            order.append(key)
        groups[key].append(r)

    objects = []
    for key in order:
        sid, ctype, cval = key
        rs = groups[key]
        oid = "KV18_%s" % sid
        if cval:
            oid += "_%s" % str(cval).replace(" ", "")
        elif ctype and ctype != "overall":
            oid += "_%s" % ctype.replace(" ", "")
        obj = {
            "object_id": oid,
            "label": rs[0].get("source_citation", ""),
            "f0_hz": None,
            "bandwidths": [],
        }
        if ctype:
            obj["condition_type"] = ctype
        if cval:
            obj["condition_value"] = cval
            obj["condition_unit"] = rs[0].get("condition_unit", "") or None
        for r in sorted(rs, key=lambda x: x.get("formant", "")):
            obj["bandwidths"].append({
                "mode_or_formant": r.get("formant", ""),
                "frequency_hz": None,
                "bandwidth_hz": _num(r.get("mean_bandwidth_Hz")),
                "range_min_hz": _num(r.get("range_min_Hz")),
                "range_max_hz": _num(r.get("range_max_Hz")),
                "sd_hz": _num(r.get("sd_Hz")),
                "value_status": r.get("value_status", ""),
            })
        objects.append(obj)

    seen = set()
    for o in objects:
        assert o["object_id"] not in seen, "dup id %s" % o["object_id"]
        seen.add(o["object_id"])

    write_dataset(
        "kent_vorperian_2018", "vowel_bandwidth_reference",
        "Kent & Vorperian 2018, 'Static measurements of vowel formant "
        "frequencies and bandwidths: A review', J. Commun. Disord.",
        "PMC6002811 (nihms971733.pdf), tables extracted to "
        "raw/vowel_formant_review_fundamental_tables.xlsx",
        "Literature review of reported formant bandwidths (incl. Dunn 1961, "
        "Bogert 1953, Fant 1962, Childers & Wu 1993); values as printed",
        objects,
        note="Bandwidths only -- this review reports B1-B4 without paired "
             "formant frequencies, so objects carry a `bandwidths` key and "
             "are a reference for Q, not selectable morph endpoints.")

def ingest_bells(freqdir):
    objects = []
    ids = sorted((int(f[:-4]) for f in os.listdir(freqdir) if f.endswith(".csv")))
    for n in ids:
        modes = []
        for i, row in enumerate(csv.reader(open(os.path.join(freqdir, "%d.csv" % n),
                                                encoding="utf-8"))):
            if len(row) < 4:
                continue
            modes.append({
                "mode_or_formant": "M%d" % (i + 1),
                "frequency_hz": float(row[0]),
                "bandwidth_hz": None,
                "amplitude": float(row[1]),
                "decay_raw": float(row[2]),
                "phase_rad": float(row[3]),
            })
        objects.append({
            "object_id": "LURIE_bell_%02d" % n,
            "label": "Lurie carillon bell %d" % n,
            "f0_hz": None,
            "modes": modes,
        })
    write_dataset(
        "lurie_carillon_bells", "instruments",
        "Canfield-Dafilou & Abel, modal analysis of the Robert and Ann Lurie "
        "Carillon (University of Michigan), recorded October 2016",
        "https://ccrma.stanford.edu/~kermit/website/bells.html",
        "Modal decomposition into decaying sinusoids; CSV columns "
        "FREQUENCY, AMPLITUDE, DECAY, PHASE (frequency-sorted)",
        objects,
        note="`decay_raw` is the source DECAY column verbatim. The page calls "
             "it an 'exponential decay rate' but the values fall with "
             "frequency like a time constant in seconds. Convention is "
             "unresolved, so no bandwidth or Q is derived from it.")

MCLACHLAN = {
    "flat_steel": ("Flat steel gong", [
        ("2,0", 252.0, None, None, 1.0),
        ("0,1", 422.0, None, None, 1.67),
        ("3,0", 498.0, None, None, 1.98),
        ("4,0", None, 622.0, 662.0, None),
        ("1,1", 738.0, None, None, 2.98),
        ("2,1", 984.0, None, None, 3.91),
        ("0,2", 1223.0, None, None, 4.85),
    ]),
    "steel_with_boss": ("Steel gong with boss", [
        (None, 370.0, None, None, 1.0),
        (None, 540.0, None, None, 1.46),
        (None, 723.0, None, None, 1.95),
        (None, None, 878.0, 925.0, None),
        (None, 1080.0, None, None, 2.92),
        (None, 1380.0, None, None, 3.73),
    ]),
    "bronze_with_boss": ("Bronze gong with boss", [
        ("2,0/0,1", 298.0, None, None, 1.0),
        ("1,1", 597.0, None, None, 2.00),
        ("3,0", 891.0, None, None, 2.99),
        ("4,0", 1110.0, None, None, 3.72),
        ("2,1", 1190.0, None, None, 3.99),
        ("0,2", 1404.0, None, None, 4.71),
        (None, 1699.0, None, None, 5.70),
    ]),
}

def ingest_gongs():
    objects = []
    conflicts = []
    for key, (label, rows) in MCLACHLAN.items():
        f0 = rows[0][1]
        modes = []
        for i, (mode, f, fmin, fmax, ratio) in enumerate(rows):
            conflict = None
            if ratio is not None and f is not None:
                got = f / f0
                if abs(got - ratio) >= 0.02:
                    conflict = ("printed frequency %g Hz gives f/f(1)=%.3f, but the "
                                "paper prints %.2f (which implies %.0f Hz)"
                                % (f, got, ratio, ratio * f0))
                    conflicts.append("%s M%d: %s" % (key, i + 1, conflict))
            entry = {
                "mode_or_formant": "M%d" % (i + 1),
                "frequency_hz": f,
                "bandwidth_hz": None,
                "mode_label": mode,
                "ratio_to_first": ratio,
            }
            if f is None:
                entry["frequency_min_hz"] = fmin
                entry["frequency_max_hz"] = fmax
                entry["split_mode"] = True
            if conflict:
                entry["transcription_conflict"] = conflict
            modes.append(entry)
        objects.append({
            "object_id": "MCL97_%s" % key,
            "label": label,
            "f0_hz": None,
            "modes": modes,
        })
    write_dataset(
        "mclachlan_1997_gongs", "instruments",
        "McLachlan 1997, 'Finite Element Analysis and Gong Acoustics', "
        "Acoustics Australia 25(3):103-107",
        "https://www.acoustics.asn.au/journal/1997/1997_25_3_McLachlan.pdf",
        "First six/seven major spectral peaks from acoustic spectra measured "
        "50-400 ms after excitation; modes assigned from FEA (nodal "
        "lines, nodal rings)",
        objects,
        note="Split modes are printed as a frequency range in the source and "
             "carry no ratio; stored with frequency_hz null plus "
             "frequency_min_hz/frequency_max_hz. Ratios are the paper's "
             "printed f/f(1) values, used here to verify the transcription; "
             "cells where the frequency and ratio columns disagree are kept "
             "as printed and marked with transcription_conflict.")
    for c in conflicts:
        print("  CONFLICT %s" % c)

ROSSING83_OPERA = [
    (1, 180.0, 212.0), (2, 176.0, 212.0), (3, 376.0, 404.0),
    (4, 472.0, 430.0), (5, 472.0, 390.0), (6, 700.0, 640.0),
]
ROSSING83_PAN = [
    ("0,1", 91.0, 180.0, 116.0),
    ("1,1", 189.0, 252.0, 190.0),
    ("2,1", 310.0, 356.0, 284.0),
    ("0,2", 354.0, 428.0, 358.0),
]

def ingest_rossing83():
    objects = []
    for n, soft, hard in ROSSING83_OPERA:
        for blow, f in (("soft", soft), ("hard", hard)):
            objects.append({
                "object_id": "RF83_opera_gong_%d_%s" % (n, blow),
                "label": "Chinese opera gong %d (%s blow)" % (n, blow),
                "f0_hz": None,
                "excitation": blow,
                "modes": [{"mode_or_formant": "M1", "frequency_hz": f,
                           "bandwidth_hz": None, "mode_label": "fundamental"}],
            })
    objects.append({
        "object_id": "RF83_gold_pan",
        "label": "Gold pan, dish modes (measured at atmospheric pressure)",
        "f0_hz": None,
        "modes": [{"mode_or_formant": "M%d" % (i + 1), "frequency_hz": obs,
                   "bandwidth_hz": None, "mode_label": mode,
                   "frequency_calculated_hz": calc,
                   "frequency_minimum_hz": fmin}
                  for i, (mode, calc, obs, fmin) in enumerate(ROSSING83_PAN)],
    })
    write_dataset(
        "rossing_fletcher_1983_plates", "instruments",
        "Rossing & Fletcher 1983, 'Nonlinear vibrations in plates and gongs', "
        "JASA 73(1):345-351",
        "https://www.phys.unsw.edu.au/music/people/publications/Rossingetal1983.pdf",
        "Table I: fundamental mode under soft vs hard blows. Table II: gold "
        "pan dish modes, observed at atmospheric pressure",
        objects,
        note="Opera-gong objects carry a single measured fundamental, not a "
             "full mode set -- the soft/hard pair shows amplitude-dependent "
             "pitch (hardening in small gongs, softening in large). "
             "frequency_hz for the gold pan is the OBSERVED value; the "
             "paper's calculated and minimum-pressure values are kept "
             "alongside for reference.")

if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "etl"
    if cmd == "etl":
        ingest_etl()
    elif cmd == "phontools":
        ingest_phontools(sys.argv[2])
    elif cmd == "kent":
        ingest_kent(sys.argv[2])
    elif cmd == "bells":
        ingest_bells(sys.argv[2])
    elif cmd == "gongs":
        ingest_gongs()
    elif cmd == "rossing83":
        ingest_rossing83()
    else:
        sys.exit("unknown command: %s" % cmd)
