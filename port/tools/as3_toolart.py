"""AS3's tools without their cards, and the card without a tool, made from
the HUD's own tool buttons (docs/as3.md, "The surgery's HUD on a big screen").

usage: python as3_toolart.py <as3 package dir>      (writes <dir>/toolart/)

Every tool button of surgery_common1_sheet is the same grey card with its
tool drawn on it, in three states: normal, selected (the same picture, the
card orange) and disabled (the tool greyed). AS3 has no other picture of its
tools (the upgrades' icons are tiles too), so:

- The tool is told from the card by the selected state: the card changes
  colour there, the tool does not. With the clean card of both colours
  (below), each pixel is solved for the tool's colour and opacity
  ("difference matting": normal = a*T + (1-a)*grey card, selected =
  a*T + (1-a)*orange card), so the tool keeps its soft edges, its shadow and
  its see-through parts (the flame, the syringes' plastic) without the card.
  The card's dark outline, the same in both, is dropped: parts lying wholly
  within its ring.
- The clean card of each state: at each pixel, the median of the cards
  where it is card (not tool), the cards aligned by their picture's top left.

Written (straight alpha PNGs):
  toolart/card-<state>.png         the clean card made LONGER pixels longer
                                   (the HUD's top bar's: rows put in above its
                                   bottom edge, its stripes going on along
                                   their angle), 236 wide, its top left the
                                   cards' picture's top left
  toolart/<tool>-<state>.png       the tool alone in the button's untrimmed
                                   box (the frame's sourceSize), where the
                                   button draws it
  toolart/tools.json               "card": "longer" (pixels), "inside" the
                                   long card's inside, within its edges (left,
                                   top, right, bottom); "tools": per tool,
                                   "card" the card picture's top left in its
                                   box, "tool" the tool's bounds there (x, y,
                                   w, h), "circle" about its solid part (x, y,
                                   radius); pixels, y down
<tool> is the button's sprite name (ingame_<tool>_button: pizzacutter, tongs,
lighter, gel, stapler, vacuum, chainsaw, injection, injection_out, battery,
clamps, metaldetector); <state> normal, selected or disabled.
"""
import json
import os
import re
import sys
import warnings
from collections import deque

import numpy as np
from PIL import Image, ImageFilter

TOOLS = ["pizzacutter", "tongs", "lighter", "gel", "stapler", "vacuum", "chainsaw", "injection", "injection_out",
         "battery", "clamps", "metaldetector"]
STATES = ["normal", "selected", "disabled"]
CARD_W, CARD_H = 236, 200  # the clean card's box (the pictures are 228-230 x 190-194)
LONGER = 48  # pixels the HUD's bar's cards are made longer (12 of the game's points)
OUT = "toolart"


def nums(s):
    return [int(float(v)) for v in re.findall(r"-?\d+(?:\.\d+)?", s)]


def erode(m, it):
    im = Image.fromarray((m * 255).astype(np.uint8))
    for _ in range(it):
        im = im.filter(ImageFilter.MinFilter(3))
    return np.asarray(im) > 127


def dilate(m, it):
    im = Image.fromarray((m * 255).astype(np.uint8))
    for _ in range(it):
        im = im.filter(ImageFilter.MaxFilter(3))
    return np.asarray(im) > 127


def components(m):
    """4-connected parts of a mask: labels and their count"""
    lab = np.zeros(m.shape, np.int32)
    n = 0
    h, w = m.shape
    for y, x in zip(*np.nonzero(m)):
        if lab[y, x]:
            continue
        n += 1
        lab[y, x] = n
        q = deque([(y, x)])
        while q:
            cy, cx = q.popleft()
            for ny, nx in ((cy + 1, cx), (cy - 1, cx), (cy, cx + 1), (cy, cx - 1)):
                if 0 <= ny < h and 0 <= nx < w and m[ny, nx] and not lab[ny, nx]:
                    lab[ny, nx] = n
                    q.append((ny, nx))
    return lab, n


class Sheet:
    def __init__(self, package):
        base = os.path.join(package, "4.0x", "surgery_common1_sheet")
        with open(base + ".json", encoding="utf-8") as f:
            self.frames = json.load(f)["frames"]
        self.image = Image.open(base + ".png").convert("RGBA")

    def untrimmed(self, tool, state):
        """the frame in its untrimmed box, as cocos lays it out, and where its
        picture's top left lies there"""
        f = self.frames["ingame_%s_button-%s.png" % (tool, state)]
        x, y, w, h = nums(f["frame"])
        im = self.image.crop((x, y, x + h, y + w)).transpose(Image.ROTATE_90) if f["rotated"] else \
            self.image.crop((x, y, x + w, y + h))
        sw, sh = nums(f["sourceSize"])
        cx, cy, _, _ = nums(f["sourceColorRect"])
        box = Image.new("RGBA", (sw, sh), (0, 0, 0, 0))
        box.paste(im, (cx, cy))
        return np.asarray(box).astype(np.float32), (cx, cy)


def card_box(img, at):
    """the card's box (CARD_W x CARD_H) from its picture's top left"""
    x, y = at
    out = np.zeros((CARD_H, CARD_W, img.shape[2]), np.float32)
    part = img[y:y + CARD_H, x:x + CARD_W]
    out[:part.shape[0], :part.shape[1]] = part
    return out


def rough_tool(normal, selected):
    """where the tool is (1) and the card (0), before matting: the pixels the
    selected state leaves as they were"""
    diff = np.abs(normal[..., :3] - selected[..., :3]).max(axis=2)
    a = 1 - np.clip((diff - 10) / 60, 0, 1)
    return a * (normal[..., 3] / 255) * (selected[..., 3] / 255)


def outline_free(alpha, normal):
    """the tool without the card's outline: in the outline's ring only what
    lies within a few pixels of the tool inside it (where the tool reaches
    the edge), and nothing of parts wholly in the ring"""
    inside = erode(normal[..., 3] > 128, 7)
    lab, n = components(alpha > 0.5)
    if n == 0:
        return alpha
    sizes = np.bincount(lab[inside & (lab > 0)], minlength=n + 1)
    core = (sizes > 30)[lab] & (lab > 0) & inside
    return alpha * dilate(core, 4)


def stripe_angle(img, known):
    """the card's stripes' direction (radians, y down): across the bare
    pixels' edges (the structure tensor of the brightness's gradient), away
    from the card's border, whose edges are no stripes"""
    lum = img[..., :3].mean(axis=2)
    gx = np.zeros_like(lum)
    gy = np.zeros_like(lum)
    gx[:, 1:-1] = (lum[:, 2:] - lum[:, :-2]) / 2
    gy[1:-1, :] = (lum[2:, :] - lum[:-2, :]) / 2
    m = erode(known, 2) & erode(img[..., 3] > 128, 14)
    jxx, jyy, jxy = (gx * gx)[m].sum(), (gy * gy)[m].sum(), (gx * gy)[m].sum()
    return 0.5 * np.arctan2(2 * jxy, jxx - jyy) + np.pi / 2


def fill_along_stripes(img, known, angle):
    """the pixels no card shows bare (the middle, under every tool), filled
    along the card's diagonal stripes: each missing pixel the blend of the
    nearest bare ones either way along them"""
    h, w = known.shape
    dy, dx = np.sin(angle), np.cos(angle)
    out = img.copy()
    for y, x in zip(*np.nonzero(~known)):
        ends = []
        for sgn in (1, -1):
            for k in range(1, max(h, w)):
                py, px = int(round(y + sgn * k * dy)), int(round(x + sgn * k * dx))
                if not (0 <= py < h and 0 <= px < w):
                    break
                if known[py, px]:
                    ends.append((k, img[py, px]))
                    break
        if len(ends) == 2:
            (k1, c1), (k2, c2) = ends
            out[y, x] = (c1 * k2 + c2 * k1) / (k1 + k2)
        elif ends:
            out[y, x] = ends[0][1]
    return out


def lengthen(card, angle, rows):
    """the clean card made `rows` pixels longer: rows put in just above its
    bottom edge, its stripes going on straight down along their angle (its
    sides' few pixels straight down), so nothing is stretched; and the
    card's inside (left, top, right, bottom: within its edges) in it"""
    h, w = card.shape[:2]
    lum = card[..., :3].mean(axis=2)
    alpha = card[..., 3] > 128
    cols = np.nonzero(alpha[h // 2])[0]
    left, right = cols.min(), cols.max()
    mid = slice(left + 30, right - 30)
    face = lum[h // 2:h * 3 // 4, mid][alpha[h // 2:h * 3 // 4, mid]].mean()
    # the bottom edge: the first row from the middle down much darker than the face
    edge = next(y for y in range(h // 2, h) if lum[y, mid][alpha[y, mid]].mean() < 0.8 * face
                or alpha[y, mid].mean() < 0.5)
    at = edge - 3
    src = card[at - 1]
    slope = np.cos(angle) / np.sin(angle)  # x a row down along a stripe
    xs = np.arange(w, dtype=np.float32)
    new = np.zeros((rows, w, 4), np.float32)
    row_alpha = card[at - 1, :, 3]
    inside = np.nonzero(row_alpha > 128)[0]
    l, r = inside.min(), inside.max()
    side = np.clip(np.minimum(xs - l, r - xs) / 10, 0, 1)[:, None]  # (0 at the sides: straight down)
    for k in range(1, rows + 1):
        sx = np.clip(xs - k * slope, l + 10, r - 10)  # (the face's, never the edges')
        x0 = np.floor(sx).astype(int)
        x1 = np.minimum(x0 + 1, w - 1)
        f = (sx - x0)[:, None]
        along = src[x0] * (1 - f) + src[x1] * f
        new[k - 1] = along * side + src * (1 - side)
        new[k - 1, :, 3] = row_alpha
    out = np.concatenate([card[:at], new, card[at:]], axis=0)
    bottom = np.nonzero(out[:, (left + right) // 2, 3] > 128)[0].max()
    return out, [int(left + 9), 9, int(right - 9), int(bottom - 12)]


def main(package):
    sheet = Sheet(package)
    cards = {t: {s: sheet.untrimmed(t, s) for s in STATES} for t in TOOLS}
    rough = {t: rough_tool(cards[t]["normal"][0], cards[t]["selected"][0]) for t in TOOLS}

    # the clean cards
    clean, angles = {}, {}
    for s in STATES:
        stack, card = [], []
        for t in TOOLS:
            img, at = cards[t][s]
            stack.append(card_box(img, at))
            # (bare: away from the tool, its soft edge and shadow, by 6 pixels)
            tool = card_box(rough[t][..., None], cards[t]["normal"][1])[..., 0] > 0.05
            card.append(~dilate(tool, 6))
        stack, card = np.stack(stack), np.stack(card)
        out = np.zeros((CARD_H, CARD_W, 4), np.float32)
        with np.errstate(all="ignore"), warnings.catch_warnings():
            warnings.simplefilter("ignore", RuntimeWarning)  # (pixels no card shows bare: filled below)
            for ch in range(4):
                out[..., ch] = np.nan_to_num(np.nanmedian(np.where(card, stack[..., ch], np.nan), axis=0))
        # (bare in at least 3 cards, inside the card: else filled)
        shape = np.median(stack[..., 3], axis=0) > 128
        known = (card.sum(axis=0) >= 3) | ~shape
        angle = stripe_angle(out, known)
        print("  the card's stripes run at %.0f degrees" % np.degrees(angle))
        out = fill_along_stripes(out, known, angle)
        out[..., 3] = np.where(shape, np.maximum(out[..., 3], 255 * shape), out[..., 3])
        clean[s] = out
        angles[s] = angle

    os.makedirs(os.path.join(package, OUT), exist_ok=True)
    written = []
    for s in STATES:
        long_card, inside = lengthen(clean[s], angles[s], LONGER)
        p = "%s/card-%s.png" % (OUT, s)
        Image.fromarray(long_card.clip(0, 255).astype(np.uint8)).save(os.path.join(package, p))
        written.append(p)

    boxes = {}
    for t in TOOLS:
        N, at = cards[t]["normal"]
        S, _ = cards[t]["selected"]
        D, _ = cards[t]["disabled"]
        # the clean cards where this button's picture lies
        def placed(c):
            box = np.zeros_like(N)
            x, y = at
            h, w = min(CARD_H, box.shape[0] - y), min(CARD_W, box.shape[1] - x)
            box[y:y + h, x:x + w] = c[:h, :w]
            return box
        Bn, Bs, Bd = placed(clean["normal"]), placed(clean["selected"]), placed(clean["disabled"])
        # the matting: a from how far the pixel moved against how far the card did
        dB = Bn[..., :3] - Bs[..., :3]
        dP = N[..., :3] - S[..., :3]
        with np.errstate(all="ignore"):
            a = 1 - np.nan_to_num(np.sum(dP * dB, axis=2) / np.maximum(np.sum(dB * dB, axis=2), 1e-3))
        a = np.clip(a, 0, 1) * (N[..., 3] / 255)
        a = outline_free(a, N)
        ys, xs = np.nonzero(a > 0.15)
        # (the circle about the tool's solid part, its shadow left out: the
        # middle of its bounds, the farthest solid pixel)
        sy, sx = np.nonzero(a > 0.5)
        cx, cy = (sx.min() + sx.max()) / 2, (sy.min() + sy.max()) / 2
        radius = float(np.sqrt((sx - cx) ** 2 + (sy - cy) ** 2).max())
        boxes[t] = {"card": [int(at[0]), int(at[1])],
                    "tool": [int(xs.min()), int(ys.min()), int(xs.max() - xs.min() + 1), int(ys.max() - ys.min() + 1)],
                    "circle": [round(float(cx), 1), round(float(cy), 1), round(radius, 1)]}
        safe = np.maximum(a, 1e-3)[..., None]
        for s, img, bg in (("normal", N, Bn), ("selected", S, Bs), ("disabled", D, Bd)):
            colour = np.clip((img[..., :3] - (1 - a[..., None]) * bg[..., :3]) / safe, 0, 255)
            out = np.concatenate([colour, (a * 255)[..., None]], axis=2)
            out[a < 0.02] = 0
            p = "%s/%s-%s.png" % (OUT, t, s)
            Image.fromarray(out.astype(np.uint8)).save(os.path.join(package, p))
            written.append(p)
    p = "%s/tools.json" % OUT
    with open(os.path.join(package, p), "w", encoding="utf-8") as f:
        json.dump({"card": {"longer": LONGER, "inside": inside}, "tools": boxes}, f, indent=1)
    written.append(p)
    print("%d tool pictures -> %s" % (len(written), os.path.join(package, OUT)))
    return written


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
