from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fit_line as F


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("folder", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("--ir", action="store_true")
    parser.add_argument("--limit", type=int, default=0)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    wavs = sorted(args.folder.rglob("*.wav"))
    if args.limit:
        wavs = wavs[: args.limit]
    report = []
    for path in wavs:
        try:
            x = F.load_mono(path)
            f, line = F.reference_line(x, impulse=args.ir)
            params, max_err, rms_err = F.fit_body(f, line)
        except Exception as error:
            report.append((path.stem, None, None, str(error)))
            print(f"{path.stem}: failed ({error})")
            continue
        gain, fp, rp, fz, rz = F.unpack(params)
        order = np.argsort(fp)
        fp, rp, fz, rz = fp[order], rp[order], fz[order], rz[order]
        rows = np.column_stack([fp, F.bandwidth_hz(rp), fz, F.bandwidth_hz(rz),
                                np.full(F.NSEC, gain / F.NSEC)])
        target = args.out / f"{path.stem}.fbw"
        newline = chr(10)
        with open(target, "w", encoding="utf-8") as handle:
            handle.write(f"# {path.stem}" + newline)
            for row in rows:
                handle.write("%.4f %.4f %.4f %.4f %.2f" % tuple(row) + newline)
        report.append((path.stem, max_err, rms_err, ""))
        print(f"{path.stem}: max {max_err:.2f} dB  rms {rms_err:.2f} dB")
    with open(args.out / "report.csv", "w", encoding="utf-8") as handle:
        handle.write("name,max_db,rms_db,note" + chr(10))
        for name, max_err, rms_err, note in report:
            handle.write(f"{name},{'' if max_err is None else '%.2f' % max_err},"
                         f"{'' if rms_err is None else '%.2f' % rms_err},{note}" + chr(10))
    done = [r for r in report if r[1] is not None]
    if done:
        print(f"{len(done)} fitted · median max {np.median([r[1] for r in done]):.2f} dB · "
              f"median rms {np.median([r[2] for r in done]):.2f} dB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
