"""Checks the display's pace from an AS_DISPLAY_LOG file.

usage: python tools/display_pace.py <display log> [--pass SECONDS] [--from SECONDS] [--to SECONDS]

The display thread logs, per display frame: when it began, the frame it
showed, how far that frame was eased in from the one before (t), how long
drawing it took and how long the swap took (ms), and how many draws the two
frames held. Shown frame n at t is the logic's time n - 1 + t (in passes):
on a steady display it rises by the frame interval over the pass length
every display frame. This reports where it did not:

  held     display frames where it stood still (the next frame was late:
           its pass took too long, or its textures were not in yet), or
           went back;
  jumped   display frames that moved it more than twice the steady step
           (catching up after a hold);
  slow     display frames that took longer than a refresh to draw (the
           display thread itself missed a refresh).

Before the first eased frame (the splash, loops outside the main one) and
under a test driver's pauses between commands, frames are shown as they
are: judge real-time stretches only (--from/--to).
"""
import argparse
import statistics


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('log')
    ap.add_argument('--pass', dest='pass_s', type=float, default=0.04, help='the logic pass length (AS1: 0.04)')
    ap.add_argument('--from', dest='start', type=float, default=0, help='seconds from the first display frame')
    ap.add_argument('--to', dest='end', type=float, default=1e9)
    ap.add_argument('--top', type=int, default=20)
    a = ap.parse_args()
    rows = []
    for line in open(a.log):
        p = line.split()
        if len(p) >= 5:
            rows.append((float(p[0]), int(p[1]), float(p[2]), float(p[3]), float(p[4])))
    if not rows:
        print('no display frames')
        return
    t0 = rows[0][0]
    eased = next((r[0] for r in rows if r[2] < 1), None)
    rows = [r for r in rows if a.start <= r[0] - t0 <= a.end and (eased is None or r[0] >= eased)]
    if len(rows) < 2:
        print('no display frames in range')
        return
    refresh = statistics.median(b[0] - a_[0] for a_, b in zip(rows, rows[1:]))
    held, jumped, slow = [], [], []
    for (ta, na, ea, da, _), (tb, nb, eb, _, _) in zip(rows, rows[1:]):
        ideal = (tb - ta) / a.pass_s
        step = (nb + eb) - (na + ea)
        if step <= ideal * 0.25:
            held.append((tb - t0, nb))
        elif step > ideal * 2 and step > 0.2:
            jumped.append((tb - t0, nb))
        if da / 1000 > refresh * 0.85:
            slow.append((ta - t0, na, da))
    print(f'{len(rows)} display frames over {rows[-1][0] - rows[0][0]:.1f} s, refresh {refresh * 1000:.2f} ms; '
          f'held {len(held)}, jumped {len(jumped)}, slow {len(slow)}')
    for name, xs in (('held', held), ('jumped', jumped)):
        for t, n in xs[:a.top]:
            print(f'  {name} at {t:7.3f} s, frame {n}')
    for t, n, ms in slow[:a.top]:
        print(f'  slow at {t:7.3f} s, frame {n}: drawn in {ms:.1f} ms')


if __name__ == '__main__':
    main()
