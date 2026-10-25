#!/usr/bin/env python3
"""Screenshots of Amateur Surgeon 3's screens at each shape of window.

AS3's screen follows the window's shape (docs/as3.md, "The screen's shape"):
512x384 canvas points at 4:3 up to 640x320 at 2:1, letterboxed beyond. This
takes every kind of screen (the menus by AS_SCREEN, a surgery by AS_SURGERY
with the tool bar and with the side columns, the pause by AS_PAUSE, the
results by AS_ENDSCREEN) at each shape, at the window's own pixels
(AS_SHOT_NATIVE), and puts each screen's shapes side by side in one picture
(<out>/sheet_<screen>.png) to look at: no art edge showing, nothing cut
off, the HUD on the screen's edges.

Every run is hidden, on a scratch save seeded as a player who has reached
the Pocket Gods' whale (every area open), in debug mode.

Usage: as3_shapes.py [--build build-dev] [--out dir] [--shapes 4:3,16:9,...] [--jobs n] [screen...]
       screens: the names below (default all)
"""
import os
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import surgeon3_playtest as play  # noqa: E402  (its save seeding)

PORT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The shapes and the window each is shot in (720 pixels high; 4:3 768).
SHAPES = {"4:3": (1024, 768), "3:2": (1080, 720), "16:10": (1152, 720), "5:3": (1200, 720), "16:9": (1280, 720),
          "2:1": (1440, 720), "20:9": (1600, 720), "21:9": (1680, 720), "1:1": (900, 900)}
DEFAULT_SHAPES = ["4:3", "16:10", "16:9", "2:1", "20:9", "21:9"]

SURGERY = "surgery_bunker_prxy4"
# name: (environment, frames shot)
SCREENS = {
    "title": ({"AS_SCREEN": "title"}, [300]),
    "saveslots": ({"AS_SCREEN": "saveslots"}, [300]),
    "world": ({"AS_SCREEN": "world"}, [300, 420]),
    "area": ({"AS_SCREEN": "area:" + SURGERY}, [300]),
    "info": ({"AS_SCREEN": "info:" + SURGERY}, [300]),
    "dialogue": ({"AS_SCREEN": "dialogue:" + SURGERY}, [300]),
    "preop": ({"AS_SCREEN": "preop:" + SURGERY}, [300]),
    "partners": ({"AS_SCREEN": "partners"}, [300]),
    "upgrades": ({"AS_SCREEN": "upgrades"}, [300]),
    "achievements": ({"AS_SCREEN": "achievements"}, [300]),
    "credits": ({"AS_SCREEN": "credits"}, [300]),
    "editor": ({"AS_SCREEN": "editor"}, [300]),
    "editor-new": ({"AS_SCREEN": "editor-new"}, [300]),
    "surgery-top": ({"AS_SURGERY": SURGERY, "AS_HUD_LAYOUT": "top"}, [400]),
    "surgery-sides": ({"AS_SURGERY": SURGERY, "AS_HUD_LAYOUT": "sides"}, [400]),
    "surgery-hospital": ({"AS_SURGERY": "surgery_hospital_emoshun", "AS_HUD_LAYOUT": "sides"}, [400]),
    "pause": ({"AS_PAUSE": SURGERY + ",4"}, [520]),
    "won": ({"AS_ENDSCREEN": SURGERY + ",0,1,3,3,1"}, [600]),
    "lost": ({"AS_ENDSCREEN": SURGERY + ",0,0,0,0,0"}, [600]),
}


def shoot(exe, seeded, out, screen, shape):
    env_extra, frames = SCREENS[screen]
    w, h = SHAPES[shape]
    save = tempfile.mkdtemp(prefix="as3shape_")
    try:
        shutil.copytree(seeded, save, dirs_exist_ok=True)
        tag = "%s_%s" % (screen, shape.replace(":", "x"))
        env = dict(os.environ, AS_SAVE_DIR=save, AS_HIDDEN="1", AS_AUDIO_AVOID="A50", AS_SEED="7",
                   AS_WINDOW="%dx%d" % (w, h), AS_SHOT_NATIVE="1",
                   AS_SHOT="%s:%s" % (os.path.join(out, tag), ",".join(str(f) for f in frames)))
        env.update(env_extra)
        subprocess.run([exe, "-d"], env=env, cwd=os.path.dirname(exe), stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, timeout=180)
        return [os.path.join(out, "%s_%d.png" % (tag, f)) for f in frames]
    finally:
        shutil.rmtree(save, ignore_errors=True)


def sheet(out, screen, shapes):
    """The screen's shots side by side, each 300 pixels high, its shape under it."""
    from PIL import Image, ImageDraw
    frames = SCREENS[screen][1]
    tiles = []
    for f in frames:
        for shape in shapes:
            p = os.path.join(out, "%s_%s_%d.png" % (screen, shape.replace(":", "x"), f))
            if not os.path.exists(p):
                continue
            im = Image.open(p).convert("RGB")
            im = im.resize((int(im.width * 300 / im.height), 300))
            tiles.append((im, "%s (%d)" % (shape, f)))
    if not tiles:
        return None
    # (two to a row, grey between)
    cell = max(t[0].width for t in tiles) + 10
    rows = (len(tiles) + 1) // 2
    s = Image.new("RGB", (2 * cell + 10, rows * 330 + 10), (90, 90, 90))
    d = ImageDraw.Draw(s)
    for k, (im, label) in enumerate(tiles):
        x, y = 10 + (k % 2) * cell, 10 + (k // 2) * 330
        s.paste(im, (x, y))
        d.text((x, y + 304), label, fill=(255, 255, 255))
    path = os.path.join(out, "sheet_%s.png" % screen)
    s.save(path)
    return path


def main():
    args = sys.argv[1:]
    build, out, jobs, shapes = "build-dev", None, 4, DEFAULT_SHAPES
    for flag in ("--build", "--out", "--jobs", "--shapes"):
        if flag in args:
            i = args.index(flag)
            v = args[i + 1]
            del args[i:i + 2]
            if flag == "--build":
                build = v
            elif flag == "--out":
                out = v
            elif flag == "--jobs":
                jobs = int(v)
            else:
                shapes = v.split(",")
    screens = args or list(SCREENS)
    exe = os.path.join(PORT, build, "AmateurSurgeon3.exe")
    out = os.path.abspath(out or tempfile.mkdtemp(prefix="as3shapes_"))
    os.makedirs(out, exist_ok=True)
    seeded = tempfile.mkdtemp(prefix="as3shape_seed_")
    play.EXE_DIR = os.path.dirname(exe)
    play.seed_save(seeded, "surgery_pocketgods_whale")
    try:
        with ThreadPoolExecutor(jobs) as pool:
            list(pool.map(lambda job: shoot(exe, seeded, out, *job), [(s, sh) for s in screens for sh in shapes]))
    finally:
        shutil.rmtree(seeded, ignore_errors=True)
    for s in screens:
        p = sheet(out, s, shapes)
        print(p or "%s: no shots" % s)


if __name__ == "__main__":
    main()
