"""The heart monitor's crossed-brain animation (Ringmaster Pt2, mission 30:
the old brain out, the heart running down), made after the game's own
crossed heart (shocked0028-0041) with the game's brain.

The dial's figures are white silhouettes, 54x50. As the shocked animation's
heart does, the brain swells out of the standing jogger (the game's own
frame, shocked0028) with its legs still showing, pops to its full size, then
two strokes go through it, behind it: the first from bottom left up to top
right, the second from a dot at bottom right up to top left, the brain
pulsing a little as the heart does. The brain is the top-down one
(S10_Brain2) as a silhouette with its halves and folds drawn plain. The
frames are at the heart's full size: the dial shrinks them as it does the
heart's (GameState::render).
Writes brainout0001.png.. into games/as2/images. Run after extract_ipa.py
(it clears that folder):  python port/tools/brain_dial.py
"""
import os

from PIL import Image, ImageChops, ImageDraw

import fetch_sources

# fetch_sources.ROOT (not __file__, which points inside a frozen exe's
# temp unpack dir when this is bundled rather than run as a script).
IMAGES = os.path.join(fetch_sources.ROOT, "port", "games", "as2", "images")
W, H = 54, 50  # the dial figures' picture size
SS = 8         # supersampling for smooth edges
# Measured off the crossed heart: its full size (shocked0033: 42 wide, 35
# high, about the middle (24, 26)) and its strokes, 4 wide.
CENTRE, FULL_H = (24, 26), 37
STROKE_W = 4
STROKE1 = ((3, 44), (50, 3))   # bottom left to top right
STROKE2 = ((46, 43), (1, 6))   # bottom right to top left
# Each frame: the brain's scale (0 none), how far each stroke has grown
# (a dot first, as the heart's second), and where it swells from.
FRAMES = [
    # the jogger, then the brain swelling out of its chest
    (0.0, 0, 0), (0.35, 0, 0), (0.55, 0, 0), (0.75, 0, 0), (0.92, 0, 0),
    # popped to its full size
    (1.04, 0, 0), (1.0, 0, 0),
    # the first stroke, from bottom left, the brain drawn in a little
    (0.93, 0.35, 0), (0.96, 0.7, 0), (1.0, 1.0, 0),
    # the second, a dot first
    (1.0, 1.0, 0.06), (1.03, 1.0, 0.4), (1.03, 1.0, 0.75), (1.0, 1.0, 1.0),
    # settled, pulsing once
    (0.97, 1.0, 1.0), (1.0, 1.0, 1.0),
]


def mask(name):
    return Image.open(os.path.join(IMAGES, name)).convert("RGBA").getchannel("A")


def brain():
    """The brain at full size (supersampled): its outline, and its look
    (the outline with the halves and folds cut through)."""
    src = Image.open(os.path.join(IMAGES, "S10_Brain2.png")).convert("RGBA")
    src = src.crop(src.getbbox())
    h = FULL_H * SS
    w = round(src.width * h / src.height)
    outline = src.resize((w, h), Image.LANCZOS).getchannel("A").point(lambda a: 255 if a > 128 else 0)
    look = outline.copy()
    d = ImageDraw.Draw(look)
    line = round(1.6 * SS)
    d.line([(w / 2, h * 0.04), (w / 2, h * 0.96)], fill=0, width=line)
    for side in (-1, 1):
        for fy, fr in ((0.22, 0.18), (0.48, 0.2), (0.74, 0.18)):
            cx, cy = w / 2 + side * w * 0.26, h * fy
            rx, ry = w * fr, h * 0.09
            start, end = (100, 260) if side > 0 else (-80, 80)
            d.arc([cx - rx, cy - ry, cx + rx, cy + ry], start, end, fill=0, width=line)
    return outline, look


def stroke(d, line, t):
    if t <= 0:
        return
    (ax, ay), (bx, by) = line
    p = (ax * SS, ay * SS)
    e = ((ax + (bx - ax) * t) * SS, (ay + (by - ay) * t) * SS)
    r = STROKE_W * SS / 2
    d.line([p, e], fill=255, width=STROKE_W * SS)
    for c in (p, e):
        d.ellipse([c[0] - r, c[1] - r, c[0] + r, c[1] + r], fill=255)


def frames():
    outline, look = brain()
    jogger = mask("shocked0028.png").resize((W * SS, H * SS), Image.LANCZOS)
    out = []
    for k, (scale, t1, t2) in enumerate(FRAMES):
        canvas = Image.new("L", (W * SS, H * SS), 0)
        d = ImageDraw.Draw(canvas)
        stroke(d, STROKE1, t1)
        stroke(d, STROKE2, t2)
        if k < 5:
            # Swelling: the jogger still there, under it.
            canvas = ImageChops.lighter(canvas, jogger)
        if scale > 0:
            size = (max(1, round(outline.width * scale)), max(1, round(outline.height * scale)))
            # (Out of the chest at first, settling to the middle.)
            cy = CENTRE[1] - (1 - min(1.0, scale)) * 8
            at = (round(CENTRE[0] * SS - size[0] / 2), round(cy * SS - size[1] / 2))
            # The strokes go behind it: hidden by the whole brain.
            hole = Image.new("L", canvas.size, 0)
            hole.paste(outline.resize(size, Image.LANCZOS), at)
            canvas = ImageChops.subtract(canvas, hole)
            body = Image.new("L", canvas.size, 0)
            body.paste(look.resize(size, Image.LANCZOS), at)
            canvas = ImageChops.lighter(canvas, body)
        alpha = canvas.resize((W, H), Image.LANCZOS)
        img = Image.new("RGBA", (W, H), (255, 255, 255, 0))
        img.putalpha(alpha)
        out.append(img)
    return out


def main():
    for name in os.listdir(IMAGES):
        if name.startswith("brainout"):
            os.remove(os.path.join(IMAGES, name))
    fs = frames()
    for k, img in enumerate(fs, 1):
        img.save(os.path.join(IMAGES, "brainout%04d.png" % k))
    print("brainout0001..%04d written" % len(fs))


if __name__ == "__main__":
    main()
