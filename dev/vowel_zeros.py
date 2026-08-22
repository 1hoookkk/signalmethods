import pathlib, struct, sys
sys.path.insert(0, r"C:\Users\hooki\trench-native\out\build\windows-msvc-release\native\research")
import trench_native_research as core

SR = core.kMorpheusDatumHz
bodies = pathlib.Path(r"C:\Users\hooki\trench-authoring\ref\morpheus\bodies")

def show(stem, corners):
    p = next(bodies.glob(stem + "*.body"))
    w = struct.unpack("<280H", p.read_bytes())
    print(f"\n===== {p.stem}   datum {SR} Hz =====")
    for c in corners:
        print(f"\n  corner {c}")
        print(f'  {"sec":>3}  {"POLE hz":>9} {"r":>8}   {"ZERO hz":>9} {"r":>8}   {"f0 ratio":>8} {"peak dB":>8}   words')
        for s in range(7):
            row = list(w[c * 35 + s * 5:c * 35 + s * 5 + 5])
            g = core.geometry(row, SR)
            pole, zero = g["pole"], g["zero"]
            def fmt(d):
                if d["kind"] == "conjugate":
                    return f'{d["hz"]:9.0f} {d["radius"]:8.5f}'
                if d["kind"] == "real":
                    return f'{"real":>9} {d["root_a"]:8.4f}'
                return f'{"degen":>9} {"":8}'
            ratio = ""
            gain = ""
            if pole["kind"] == "conjugate" and zero["kind"] == "conjugate":
                ratio = f'{zero["hz"] / max(pole["hz"], 1e-9):8.3f}'
                gain = f'{20.0 * __import__("math").log10(max(1 - zero["radius"], 1e-12) / max(1 - pole["radius"], 1e-12)):8.1f}'
            print(f'  {s+1:>3}  {fmt(pole)}   {fmt(zero)}   {ratio:>8} {gain:>8}   '
                  + " ".join(f"{v:04X}" for v in row))

show("022_AEParaVowel", [0, 1])
show("021_AEParLPVow", [0])
