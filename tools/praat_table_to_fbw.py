"""Praat Formant Table (Formant -> Down to Table, with bandwidths) to .fbw.

    python tools/praat_table_to_fbw.py track.Table [--at SECONDS | --median]
                                                   [--min-formants N] [--out PATH]

--at picks the frame nearest that time; --median (default) takes the median
F/B of every frame that carries at least --min-formants formants. Rows are
written F1..Fn as frequency_hz bandwidth_hz, the workstation's pole material.
"""
import argparse, csv, os, statistics


def read_table(path):
    with open(path, newline='') as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        raise SystemExit('empty table')
    count = 0
    while 'F%d(Hz)' % (count + 1) in rows[0]:
        count += 1
    if count == 0:
        raise SystemExit('no F1(Hz) column; export with bandwidths from Praat')
    return rows, count


def value(row, key):
    text = row.get(key, '')
    if text in ('', '--undefined--'):
        return None
    return float(text)


def frame_pairs(row, count):
    pairs = []
    for k in range(1, count + 1):
        f = value(row, 'F%d(Hz)' % k)
        b = value(row, 'B%d(Hz)' % k)
        if f is None or b is None:
            break
        pairs.append((f, b))
    return pairs


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('table')
    ap.add_argument('--at', type=float, default=None)
    ap.add_argument('--median', action='store_true')
    ap.add_argument('--min-formants', type=int, default=4)
    ap.add_argument('--out', default=None)
    args = ap.parse_args()
    rows, count = read_table(args.table)
    stem = os.path.splitext(os.path.basename(args.table))[0]

    if args.at is not None:
        row = min(rows, key=lambda r: abs(float(r['time(s)']) - args.at))
        pairs = frame_pairs(row, count)
        label = '%s at %.3f s' % (stem, float(row['time(s)']))
    else:
        kept = [frame_pairs(r, count) for r in rows]
        kept = [p for p in kept if len(p) >= args.min_formants]
        if not kept:
            raise SystemExit('no frame carries %d formants' % args.min_formants)
        depth = min(6, max(len(p) for p in kept))
        pairs = []
        for k in range(depth):
            column = [p[k] for p in kept if len(p) > k]
            pairs.append((statistics.median(f for f, _ in column), statistics.median(b for _, b in column)))
        label = '%s median of %d frames' % (stem, len(kept))

    pairs = pairs[:6]
    out = args.out or os.path.join(os.path.dirname(os.path.abspath(args.table)), stem + '.fbw')
    with open(out, 'w', newline='\n') as handle:
        handle.write('# %s\n' % label)
        for f, b in pairs:
            handle.write('%.4f %.4f\n' % (f, max(b, 1.0)))
    print(label)
    for k, (f, b) in enumerate(pairs, 1):
        print('S%d  %8.1f Hz  BW %7.1f Hz' % (k, f, b))
    print('Wrote %s' % out)


if __name__ == '__main__':
    main()
