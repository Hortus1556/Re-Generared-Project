"""Checks how smoothly things move on screen, from an AS_FILM log.

usage: python tools/motion_check.py <film.csv> [--steps N] [--min-speed PX]

AS_FILM logs, for every display step between two logic passes, where each
draw landed. This follows each drawn thing (same drawing object, image and
place in its owner's drawing order) from step to step and reports, per thing
that moved:

  stutter  steps where it stood still while moving before and after it
           (move-stop-move: what reads as judder at a high refresh rate);
  jumps    steps that moved it much further than the steps around them
           (a teleport, or a pairing gone wrong between two frames);
  uneven   how much its speed varies from step to step beyond what steady
           acceleration explains (0 = perfectly even).

Keyframed art (Spritex pieces) is listed apart: its pieces are meant to
change at their keyframes.
"""
import argparse
import collections
import csv
import math
import statistics


def on_screen(x, y):
    return -256 <= x <= 1280 and -256 <= y <= 1024


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('csv')
    ap.add_argument('--min-speed', type=float, default=0.5, help='ignore things moving less than this per step')
    ap.add_argument('--top', type=int, default=25)
    a = ap.parse_args()

    # frame -> step -> list of rows
    steps = collections.OrderedDict()
    sub = 0
    for row in csv.reader(open(a.csv)):
        frame, k = int(row[0]), int(row[1])
        sub = max(sub, k + 1)
        steps.setdefault((frame, k), []).append(row)
    order = sorted(steps)

    # A thing: same context, owner, group and texture; among several such
    # draws, the one in the same place in drawing order while their number
    # stays the same (as the renderer pairs them), else the one nearest where
    # it was heading (its last position plus its last step).
    tracks = {}
    last = collections.defaultdict(list)  # base identity -> [(track id, x, y, dx, dy)]
    next_id = 0
    for idx, key in enumerate(order):
        now = collections.defaultdict(list)
        for row in steps[key]:
            now[tuple(row[2:6])].append((float(row[6]), float(row[7])))
        for base, pts in now.items():
            free = list(last.get(base, []))
            placed = []
            if len(free) == len(pts):  # the same set as before: in drawing order
                for (tid, lx, ly, _, _), (x, y) in zip(free, pts):
                    tracks.setdefault(base + (tid,), {})[idx] = (x, y)
                    placed.append((tid, x, y, x - lx, y - ly))
                last[base] = placed
                continue
            for x, y in pts:
                best = None
                for j, (tid, lx, ly, dx, dy) in enumerate(free):
                    d = math.hypot(x - lx - dx, y - ly - dy)
                    if d < 200 and (best is None or d < best[0]):
                        best = (d, j)
                if best is None:
                    tid, vx, vy = next_id, 0.0, 0.0
                    next_id += 1
                else:
                    tid, lx, ly, _, _ = free.pop(best[1])
                    vx, vy = x - lx, y - ly
                tracks.setdefault(base + (tid,), {})[idx] = (x, y)
                placed.append((tid, x, y, vx, vy))
            last[base] = placed
        for base in list(last):
            if base not in now:
                del last[base]

    results = []
    for ident, pts in tracks.items():
        idxs = sorted(pts)
        if len(idxs) < 4 * sub:  # on screen under 4 passes (200 ms): too brief to judge
            continue
        d = []
        for i0, i1 in zip(idxs, idxs[1:]):
            if i1 != i0 + 1:
                d.append(None)
                continue
            (x0, y0), (x1, y1) = pts[i0], pts[i1]
            if not (on_screen(x0, y0) and on_screen(x1, y1)):  # (partly) not seen
                d.append(None)
                continue
            d.append(math.hypot(x1 - x0, y1 - y0))
        moving = [v for v in d if v is not None and v > a.min_speed]
        if len(moving) < 3:
            continue
        stutter = jumps = 0
        uneven = []
        for i in range(1, len(d) - 1):
            p, c, n = d[i - 1], d[i], d[i + 1]
            if p is None or c is None or n is None:
                continue
            if c <= a.min_speed * 0.2 and p > a.min_speed and n > a.min_speed:
                stutter += 1
            around = max(p, n)
            if c > 30 and c > 4 * around:
                jumps += 1
            if p > a.min_speed and c > a.min_speed and n > a.min_speed:
                uneven.append(abs(c - (p + n) / 2) / max(c, 1e-3))
        group = ident[2] != '0'
        results.append({
            'ident': ident, 'group': group, 'stutter': stutter, 'jumps': jumps,
            'uneven': statistics.mean(uneven) if uneven else 0.0,
            'speed': statistics.mean(moving), 'steps': len(idxs),
            'first': order[idxs[0]], 'last': order[idxs[-1]],
        })

    def show(title, rows):
        print(f'== {title}: {len(rows)}')
        rows.sort(key=lambda r: (r['stutter'] + 3 * r['jumps'], r['uneven']), reverse=True)
        for r in rows[:a.top]:
            ctx, owner, group, tex, n = r['ident']
            print(f"  tex {tex:>4} owner {owner:>7} ctx {ctx:>7} #{n:<3} stutter {r['stutter']:>3} "
                  f"jumps {r['jumps']:>2} uneven {r['uneven']:.2f} speed {r['speed']:6.1f}px/step "
                  f"frames {r['first'][0]}..{r['last'][0]}")

    show('moving things', [r for r in results if not r['group']])
    show('keyframed art pieces', [r for r in results if r['group']])
    bad = [r for r in results if not r['group'] and (r['stutter'] or r['jumps'] or r['uneven'] > 0.35)]
    print(f'verdict: {len(bad)} moving things stutter, jump or move unevenly')


if __name__ == '__main__':
    main()
