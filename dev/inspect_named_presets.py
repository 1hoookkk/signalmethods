import json

with open("ref/morpheus/cubes_decoded.json", "r") as f:
    d = json.load(f)

targets = ["AEParaVowel", "LPFlange.4", "Phaser", "Be-Ye.4", "Vocal Cube", "HiQ 4PoleLP", "TalkingHedz", "BrassyBlast", "Soup.4"]

print(f"Total cubes in file: {len(d['cubes'])}")
for c in d["cubes"]:
    name = c["name"].strip()
    for t in targets:
        if t.lower() in name.lower():
            active_corners = sum(1 for corner in c["corners"] if any(s["pole"]["r"] > 0 for s in corner["sections"]))
            print(f"\n=======================================================")
            print(f"Cube #{c['index']}: \"{name}\" (Active Corners: {active_corners}/8)")
            for ci, co in enumerate(c["corners"]):
                active_stages = [si for si, s in enumerate(co["sections"]) if s["pole"]["r"] > 0]
                if active_stages:
                    poles = [f"S{si+1}:{s['pole']['hz']:.0f}Hz(r={s['pole']['r']:.3f})" for si, s in enumerate(co["sections"]) if s["pole"]["r"] > 0]
                    zeros = [f"S{si+1}:{s['zero']['hz']:.0f}Hz(r={s['zero']['r']:.3f})" for si, s in enumerate(co["sections"]) if s["zero"]["r"] > 0]
                    print(f"  Corner C{ci} (Gain={co['gain']:.3f}): {len(active_stages)} live stages")
                    print(f"    Poles: {', '.join(poles[:4])}")
                    if zeros:
                        print(f"    Zeros: {', '.join(zeros[:4])}")
                else:
                    print(f"  Corner C{ci}: NULL / IDLE")
