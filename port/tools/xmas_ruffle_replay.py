#!/usr/bin/env python3
"""Replays a live play-test's input on the Flash original, in Ruffle.

flash_playtest.py's live scenarios write the input they gave (actions.txt:
clicks, drags and screenshots, each at its game frame) next to their trace.
This plays the same input on the original SWF through ruffle_ref.py, at the
movie's 25 frames a second from the operation's start, and saves the same
screenshots: the reference to compare the native ones with.

Usage: xmas_ruffle_replay.py <play-test folder> <out folder>
"""
import os
import subprocess
import sys

PORT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORKSPACE = os.path.dirname(PORT)
FPS = 25.0
SAVE_NAME = "user_9848945498456554546545454"
SWF = os.path.join(WORKSPACE, "re", "flash", "xmas", "games.adultswim.com", "AmateurSurgeonChristmas",
                   "Amateur Surgeon Christmas.swf")
# The levels screen's patient cards (missions 2-5).
PATIENT = {2: (300, 170), 3: (430, 170), 4: (215, 320), 5: (400, 320)}


def prologue(mission):
    """To the operation: a new game for mission 1, else Continue and the patient."""
    if mission == 1:
        steps = ["wait 4", "click 190 420", "wait 5", "click 572 432", "wait 3", "click 540 432", "wait 5"]
    else:
        x, y = PATIENT[mission]
        steps = ["wait 4", "click 390 420", "wait 3", "move %d %d" % (x, y), "wait 0.5", "click %d %d" % (x, y),
                 "wait 3", "click 570 445", "wait 4"]
    # START, then the fade into the operation.
    return steps + ["click 530 425", "wait 1.5", "mark"]


def script(actions_file):
    lines = open(actions_file).read().split("\n")
    mission = int(lines[0].split()[1])
    op_start = int(lines[1].split()[1])
    out = prologue(mission)
    for line in lines[2:]:
        if not line:
            continue
        frame, cmd, arg = line.split(" ", 2)
        out.append("at %.2f" % ((int(frame) - op_start) / FPS))
        if cmd == "click":
            _, x, y = arg.split(":")[:3]
            out.append("click %s %s" % (x, y))
        elif cmd in ("down", "move", "up"):
            out.append("%s %s" % (cmd, arg))
        elif cmd == "shot":
            out.append("shot " + os.path.splitext(os.path.basename(arg))[0])
        elif cmd == "drag":
            v = [float(s) for s in arg.split(":")]
            x, y = v[1], v[2]
            out += ["move %g %g" % (x, y), "down %g %g" % (x, y)]
            for k in range(3, len(v), 3):
                out.append("glide %g %g %g %g %.3f" % (x, y, v[k], v[k + 1], v[k + 2] / FPS))
                x, y = v[k], v[k + 1]
            out.append("up %g %g" % (x, y))
    return mission, out


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    folder, outdir = sys.argv[1:]
    mission, lines = script(os.path.join(folder, "actions.txt"))
    os.makedirs(outdir, exist_ok=True)
    path = os.path.join(outdir, "replay.txt")
    open(path, "w").write("\n".join(lines) + "\n")
    # The missions before this one done.
    fields = ["balls=0", "timer=0"] + ["level%d=1,score%d=100,timer%d=100" % (k, k, k) for k in range(1, mission)]
    args = [sys.executable, os.path.join(PORT, "tools", "ruffle_ref.py"),
            SWF, outdir, path,
            "--files", os.path.join(PORT, "games", "christmas", "data")]
    if mission > 1:
        args += ["--so", SAVE_NAME, ",".join(fields)]
    sys.exit(subprocess.call(args))


if __name__ == "__main__":
    main()
