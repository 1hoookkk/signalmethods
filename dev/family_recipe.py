"""Derive each documented filter family's actual construction from the bytes."""
import json, pathlib, struct, sys, collections
import numpy as np
sys.path.insert(0, r"C:\Users\hooki\trench-native\out\build\windows-msvc-release\native\research")
import trench_native_research as core
SR = core.kMorpheusDatumHz
SP = pathlib.Path(r"C:\WINDOWS\TEMP\claude\C--Users-hooki-trench-native\c1331e8e-ca6d-4249-8dd3-5d4540d80e20\scratchpad")
fam = {int(k): v for k, v in json.load(open(SP / "up_families.json")).items()}
bodies = pathlib.Path(r"C:\Users\hooki\trench-authoring\ref\morpheus\bodies")
by_num = {}
for p in bodies.glob("*.body"):
    try: by_num[int(p.name.split("_")[0])] = p
    except ValueError: pass

acc = collections.defaultdict(lambda: dict(bodies=0, poles=[], zeros=[], npole=[], nzero=[],
                                           pole_r=[], zero_r=[], notch=0, boost=0, pair=0))
for num, family in sorted(fam.items()):
    p = by_num.get(num)
    if p is None: continue
    b = p.read_bytes()
    if len(b) != 560: continue
    w = struct.unpack("<280H", b)
    a = acc[family]; a["bodies"] += 1
    for c in range(8):
        poles, zeros = [], []
        for s in range(7):
            g = core.geometry(list(w[c*35+s*5:c*35+s*5+5]), SR)
            if g["pole"]["kind"] == "conjugate": poles.append((g["pole"]["hz"], g["pole"]["radius"]))
            if g["zero"]["kind"] == "conjugate": zeros.append((g["zero"]["hz"], g["zero"]["radius"]))
        a["npole"].append(len(poles)); a["nzero"].append(len(zeros))
        a["pole_r"] += [r for _, r in poles]
        a["zero_r"] += [r for _, r in zeros]
        # nearest-frequency pole for each zero, regardless of row
        for zh, zr in zeros:
            if not poles: continue
            ph, pr = min(poles, key=lambda t: abs(np.log(max(t[0],1)/max(zh,1))))
            if abs(np.log(max(ph,1)/max(zh,1))) < np.log(1.35):
                a["pair"] += 1
                if zr > pr: a["notch"] += 1
                else: a["boost"] += 1

order = ["FLANGERS", "DIPTHONGS", "STANDARD", "EQUALIZATION FILTERS", "COMPLEX FILTERS"]
print(f'{"family":<22}{"bodies":>7}{"poles/cnr":>10}{"zeros/cnr":>10}{"pole r":>9}{"zero r":>9}'
      f'{"paired":>8}{"as cut":>8}{"as boost":>9}')
for f in order:
    a = acc.get(f)
    if not a: continue
    pr, zr = np.asarray(a["pole_r"]), np.asarray(a["zero_r"])
    tot = max(a["pair"], 1)
    print(f'{f:<22}{a["bodies"]:>7}{np.mean(a["npole"]):>10.2f}{np.mean(a["nzero"]):>10.2f}'
          f'{np.median(pr):>9.4f}{np.median(zr) if len(zr) else 0:>9.4f}'
          f'{a["pair"]:>8}{100*a["notch"]/tot:>7.0f}%{100*a["boost"]/tot:>8.0f}%')
print()
print("zero radius distribution per family (how close zeros sit to the unit circle):")
for f in order:
    a = acc.get(f)
    if not a or not a["zero_r"]: continue
    zr = np.asarray(a["zero_r"])
    print(f'  {f:<22} >.99 {100*(zr>.99).mean():5.1f}%   .9-.99 {100*((zr>.9)&(zr<=.99)).mean():5.1f}%'
          f'   <.9 {100*(zr<=.9).mean():5.1f}%')
