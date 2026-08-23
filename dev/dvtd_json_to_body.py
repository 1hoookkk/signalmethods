import json
import pathlib
import struct
import sys

IDENTITY = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF)


def body_of(words):
    flat = [int(w) for w in words]
    rows = [tuple(flat[i:i + 5]) for i in range(0, len(flat), 5)]
    corners, width = (4, 6) if len(rows) <= 6 else (8, 7)
    while len(rows) < width:
        rows.append(IDENTITY)
    out = bytearray()
    for _corner in range(corners):
        for row in rows[:width]:
            out += struct.pack("<5H", *row)
    return bytes(out)


def main():
    source = pathlib.Path(sys.argv[1])
    out_dir = pathlib.Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)
    for result in json.loads(source.read_text()):
        suffix = ".bin" if len(result["words"]) <= 30 else ".body"
        path = out_dir / f"dvtd_{result['mouth']}{suffix}"
        path.write_bytes(body_of(result["words"]))
        print(path, f"{result['polished_rms_db']:.2f} dB rms")


if __name__ == "__main__":
    main()
