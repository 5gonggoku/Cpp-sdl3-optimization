import argparse
import csv
import os
import statistics
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
ALLOWED_DIRS = [os.path.join(ROOT, 'docs', 'perf'), os.path.join(ROOT, '.claude', 'scratch')]


def resolve_csv(path):
    full = os.path.abspath(path if os.path.isabs(path) else os.path.join(ROOT, path))
    for d in ALLOWED_DIRS:
        if full.lower().startswith((d + os.sep).lower()):
            return full
    sys.exit('csv path not allowed (must be under docs/perf or .claude/scratch): ' + path)


def median(rows, key):
    return statistics.median(float(r[key]) for r in rows)


def spread_percent(rows, key):
    values = [float(r[key]) for r in rows]
    low = min(values)
    return (max(values) - low) / low * 100.0 if low > 0 else 0.0


def main():
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    ap = argparse.ArgumentParser()
    ap.add_argument('--csv', default='docs/perf/bench_selected.csv')
    ap.add_argument('--enemies', type=int, default=10000)
    ap.add_argument('--label', default='')
    ap.add_argument('--chunk', type=int, default=0)
    args = ap.parse_args()

    path = resolve_csv(args.csv)
    with open(path, encoding='utf-8', newline='') as f:
        rows = list(csv.DictReader(f))

    groups = {}
    order = []
    for r in rows:
        if args.enemies and int(r['enemies']) != args.enemies:
            continue
        name = r.get('label') or r['commit']
        if args.label and not name.startswith(args.label):
            continue
        key = (name, r['enemies'], r.get('data_hash', ''))
        if key not in groups:
            groups[key] = []
            order.append(key)
        groups[key].append(r)

    print('csv: %s  rows: %d' % (path, len(rows)))
    print('%-26s %6s %-16s %3s %9s %9s %8s %8s %8s %7s  %-16s %s' %
          ('label', 'N', 'data_hash', 'n', 'avg_ms', 'collision', 'move', 'p99', 'checks', 'spread', 'checksum', 'lines'))
    for key in order:
        rs = groups[key]
        size = args.chunk if args.chunk > 0 else len(rs)
        for start in range(0, len(rs), size):
            chunk = rs[start:start + size]
            sums = sorted(set(r['checksum'] for r in chunk))
            first = chunk[0].get('source_line', '')
            last = chunk[-1].get('source_line', '')
            print('%-26s %6s %-16s %3d %9.4f %9.4f %8.4f %8.4f %8.1f %6.1f%%  %-16s %s' %
                  (key[0], key[1], key[2][:16] or '-', len(chunk), median(chunk, 'avg_ms'),
                   median(chunk, 'collision_ms'), median(chunk, 'move_ms'), median(chunk, 'p99_ms'),
                   median(chunk, 'checks_per_tick'), spread_percent(chunk, 'avg_ms'), ','.join(sums),
                   '%s-%s' % (first, last) if first else ''))
    if not order:
        print('no rows matched the filters')
    return 0


if __name__ == '__main__':
    sys.exit(main())
