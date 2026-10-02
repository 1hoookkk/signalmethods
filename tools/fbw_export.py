import argparse
import cmath
import json
import math
import sys

import numpy as np
from scipy import signal

FS = 44100.0
LOW_HZ = 20.0
MIN_BW_HZ = 0.03
MAX_BW_HZ = 20000.0
ROWS = 6


class Report:
    def __init__(self):
        self.real_roots = 0
        self.reflected = 0
        self.unplaced_zeros = 0
        self.dropped_poles = 0


def bandwidth_hz(radius, fs):
    return -fs * math.log(radius) / math.pi


def pairs_from_roots(roots, fs, report, kind):
    pairs = []
    for root in roots:
        root = complex(root)
        if abs(root.imag) <= 1e-9:
            report.real_roots += 1
            continue
        if root.imag < 0.0:
            continue
        radius = abs(root)
        if radius > 1.0:
            radius = 1.0 / radius
            report.reflected += 1
        hz = cmath.phase(root) / (2.0 * math.pi) * fs
        bw = MIN_BW_HZ if radius >= 1.0 else max(MIN_BW_HZ, bandwidth_hz(radius, fs))
        pairs.append((hz, bw))
    pairs.sort()
    return pairs


def rows_from_zpk(z, p, fs=FS, report=None):
    report = report if report is not None else Report()
    poles = pairs_from_roots(p, fs, report, "pole")
    zeros = pairs_from_roots(z, fs, report, "zero")
    if len(poles) > ROWS:
        report.dropped_poles = len(poles) - ROWS
        poles = poles[:ROWS]
    rows = []
    for index, pole in enumerate(poles):
        zero = zeros[index] if index < len(zeros) else None
        rows.append((pole, zero))
    report.unplaced_zeros = max(0, len(zeros) - len(poles))
    return rows, report


def rows_from_ba(b, a, fs=FS):
    z, p, _ = signal.tf2zpk(np.asarray(b, dtype=float), np.asarray(a, dtype=float))
    return rows_from_zpk(z, p, fs)


def rows_from_design(kind, band, order, hz, fs=FS, rp=1.0, rs=40.0):
    nyq = fs / 2.0
    wn = [h / nyq for h in hz] if len(hz) > 1 else hz[0] / nyq
    z, p, _ = signal.iirfilter(order, wn, rp=rp, rs=rs, btype=band, ftype=kind, output="zpk")
    return rows_from_zpk(z, p, fs)


def rows_from_peak(hz, q, fs=FS, notch=False):
    b, a = (signal.iirnotch if notch else signal.iirpeak)(hz, q, fs=fs)
    return rows_from_ba(b, a, fs)


def rows_from_comb(start, step, count, bw, zero_bw=None, zeros=True):
    count = min(count, ROWS)
    rows = []
    for index in range(count):
        pole = (start + index * step, bw)
        zero = (start + (index + 0.5) * step, bw if zero_bw is None else zero_bw) if zeros else None
        rows.append((pole, zero))
    return rows, Report()


def clamp_rows(rows, fs=FS):
    out = []
    for pole, zero in rows:
        pole = (min(max(pole[0], LOW_HZ), fs / 2.0), min(max(pole[1], MIN_BW_HZ), MAX_BW_HZ))
        if zero is not None:
            zero = (min(max(zero[0], LOW_HZ), fs / 2.0), min(max(zero[1], MIN_BW_HZ), MAX_BW_HZ))
        out.append((pole, zero))
    return out


def fbw_text(name, rows):
    lines = ["# " + name]
    for pole, zero in rows:
        line = "%.4f %.4f" % pole
        if zero is not None:
            line += " %.4f %.4f" % zero
        lines.append(line)
    return "\n".join(lines) + "\n"


def write_fbw(path, name, rows):
    with open(path, "w", encoding="utf-8", newline="\n") as stream:
        stream.write(fbw_text(name, rows))


def main(argv=None):
    parser = argparse.ArgumentParser(description="Write TRENCH .fbw rows (pole hz bw [zero hz bw]) from filter material.")
    parser.add_argument("--fs", type=float, default=FS)
    parser.add_argument("--out")
    parser.add_argument("--name")
    sub = parser.add_subparsers(dest="command", required=True)

    design = sub.add_parser("design", help="scipy iirfilter design")
    design.add_argument("--type", default="butter", choices=["butter", "cheby1", "cheby2", "ellip", "bessel"])
    design.add_argument("--band", default="lowpass", choices=["lowpass", "highpass", "bandpass", "bandstop"])
    design.add_argument("--order", type=int, required=True)
    design.add_argument("--hz", type=float, nargs="+", required=True)
    design.add_argument("--rp", type=float, default=1.0)
    design.add_argument("--rs", type=float, default=40.0)

    peak = sub.add_parser("peak", help="one resonant peak")
    peak.add_argument("--hz", type=float, required=True)
    peak.add_argument("--q", type=float, required=True)

    notch = sub.add_parser("notch", help="one notch")
    notch.add_argument("--hz", type=float, required=True)
    notch.add_argument("--q", type=float, required=True)

    comb = sub.add_parser("comb", help="arithmetic ladder: a pole every step, a zero half a step above each")
    comb.add_argument("--start", type=float, required=True)
    comb.add_argument("--step", type=float, required=True)
    comb.add_argument("--count", type=int, default=ROWS)
    comb.add_argument("--bw", type=float, required=True)
    comb.add_argument("--zero-bw", type=float)
    comb.add_argument("--no-zeros", action="store_true")

    coef = sub.add_parser("coef", help="transfer function b / a coefficients (MATLAB order, z^-1 powers)")
    coef.add_argument("--b", type=float, nargs="+", required=True)
    coef.add_argument("--a", type=float, nargs="+", required=True)

    zpk = sub.add_parser("zpk", help='JSON {"z": [[re, im], ...], "p": [[re, im], ...]}')
    zpk.add_argument("path")

    args = parser.parse_args(argv)
    fs = args.fs
    if args.command == "design":
        rows, report = rows_from_design(args.type, args.band, args.order, args.hz, fs, args.rp, args.rs)
        name = "%s %s %d %s" % (args.type, args.band, args.order, " ".join("%g" % h for h in args.hz))
    elif args.command in ("peak", "notch"):
        rows, report = rows_from_peak(args.hz, args.q, fs, notch=args.command == "notch")
        name = "%s %g q %g" % (args.command, args.hz, args.q)
    elif args.command == "comb":
        rows, report = rows_from_comb(args.start, args.step, args.count, args.bw, args.zero_bw, not args.no_zeros)
        name = "comb %g step %g" % (args.start, args.step)
    elif args.command == "coef":
        rows, report = rows_from_ba(args.b, args.a, fs)
        name = "coef"
    else:
        with open(args.path, encoding="utf-8") as stream:
            data = json.load(stream)
        z = [complex(*pair) for pair in data.get("z", [])]
        p = [complex(*pair) for pair in data.get("p", [])]
        rows, report = rows_from_zpk(z, p, fs)
        name = "zpk"
    rows = clamp_rows(rows, fs)
    if not rows:
        print("no conjugate poles to write", file=sys.stderr)
        return 1
    name = args.name or name
    text = fbw_text(name, rows)
    if args.out:
        write_fbw(args.out, name, rows)
        print("wrote %s (%d rows)" % (args.out, len(rows)))
    else:
        sys.stdout.write(text)
    notes = []
    if report.real_roots:
        notes.append("%d real roots not carried" % report.real_roots)
    if report.reflected:
        notes.append("%d roots reflected inside the circle" % report.reflected)
    if report.unplaced_zeros:
        notes.append("%d zeros had no pole row" % report.unplaced_zeros)
    if report.dropped_poles:
        notes.append("%d poles beyond six rows dropped" % report.dropped_poles)
    if notes:
        print("; ".join(notes), file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
