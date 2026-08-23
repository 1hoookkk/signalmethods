import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "out/build/windows-msvc-release/native/research"))
import trench_native_research as core

SR = 44100.0
OUT = ROOT / "ref" / "transfer"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    grid = list(core.erb_grid_hz())
    for path in sorted((ROOT / "ref" / "presets").glob("P2k_0*.bin")):
        words = struct.unpack("<120H", path.read_bytes())
        for corner in range(4):
            db = core.cascade_db(list(words[corner * 30:(corner + 1) * 30]), grid, SR)
            target = OUT / f"{path.stem}_c{corner}.txt"
            target.write_text("\n".join(f"{v:.6f}" for v in db) + "\n")
    print(len(list(OUT.glob("*.txt"))), "curves on", len(grid), "points")


if __name__ == "__main__":
    main()
