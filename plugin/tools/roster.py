from __future__ import annotations

import argparse
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BODIES = ROOT / "plugin" / "presets" / "bodies"
INC = ROOT / "plugin" / "presets" / "PresetRoster.inc"
sys.path.insert(0, str(ROOT / "pyruntime"))

LINE = re.compile(r'^TRENCH_PRESET\("(?P<name>[^"]*)",\s*"(?P<stem>[^"]*)",\s*"(?P<cat>[^"]*)"\)\s*$')


def entries():
    out = []
    for line in INC.read_text(encoding="utf-8").splitlines():
        m = LINE.match(line.strip())
        if m:
            out.append((m["name"], m["stem"], m["cat"]))
    return out


def legal(body: bytes) -> bool:
    from ffi import probe
    return all(probe(body, m, q, 44100.0) is not None for m in (0.0, 1.0) for q in (0.0, 1.0))


def add(src: Path, name: str, category: str, stem: str | None) -> int:
    body = src.read_bytes()
    if len(body) != 240:
        print(f"refused: {src} is {len(body)} bytes, not 240")
        return 1
    if not legal(body):
        print(f"refused: {src} does not decode at all four corners")
        return 1
    stem = stem or src.stem
    if not re.fullmatch(r"[A-Za-z0-9_]+", stem):
        print(f"refused: stem '{stem}' must be [A-Za-z0-9_]")
        return 1
    if any(s == stem or n == name for n, s, _ in entries()):
        print(f"refused: '{name}' / '{stem}' already in the roster")
        return 1
    dst = BODIES / f"{stem}.body240"
    if dst.exists() and dst.read_bytes() != body:
        print(f"refused: {dst} exists with different bytes")
        return 1
    shutil.copyfile(src, dst)
    text = INC.read_text(encoding="utf-8")
    if not text.endswith("\n"):
        text += "\n"
    text += f'TRENCH_PRESET("{name}", "{stem}", "{category}")\n'
    INC.write_text(text, encoding="utf-8")
    print(f"added {name} <- {dst.relative_to(ROOT)} [{category}]")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("add")
    a.add_argument("body")
    a.add_argument("name")
    a.add_argument("category")
    a.add_argument("--stem")
    sub.add_parser("list")
    args = ap.parse_args()
    if args.cmd == "add":
        return add(Path(args.body).resolve(), args.name, args.category, args.stem)
    for name, stem, cat in entries():
        baked = (BODIES / f"{stem}.body240").exists() or (BODIES / f"{stem}.json").exists()
        print(f"{cat:10s} {name:32s} {stem:36s} {'baked' if baked else 'MISSING'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
