"""Makes the "Powered by Bleed Engine" splash art: an SVG wordmark drawn in
the engine's own angular stroke lettering (no font file involved), written as
src/platform/BleedSplashArt.h for the platform to rasterise at the display's
resolution (src/platform/Splash.cpp).

Usage: python port/tools/make_bleed_splash.py [--svg <preview.svg>]
"""
import argparse
import os

# Each letter: its width, its centre-line path on a 100-high cap grid (the
# strokes are 18 wide with square caps, so paths keep 9 in from the box) and
# any filled shape (a diagonal too steep for a mitred join).
STROKE = 18
GAP = 22
SPACE = 40
LETTERS = {
    "B": (60, "M9,91 V9 H39 L51,21 V38 L39,50 M9,50 H39 L51,62 V79 L39,91 H9"),
    "D": (60, "M9,9 H37 L51,23 V77 L37,91 H9 Z"),
    "E": (60, "M51,9 H9 V91 H51 M9,50 H41"),
    "G": (60, "M48,9 H21 L9,21 V79 L21,91 H39 L51,79 V52 H31"),
    "I": (24, "M12,9 V91"),
    "L": (60, "M9,9 V91 H51"),
    "N": (60, "M9,9 V91 M51,9 V91", "M0,0 H20 L60,100 H40 Z"),
    "O": (60, "M21,9 H39 L51,21 V79 L39,91 H21 L9,79 V21 Z"),
    "P": (60, "M9,91 V9 H39 L51,21 V40 L39,52 H9"),
    "R": (60, "M9,91 V9 H39 L51,21 V40 L39,52 H9 M33,52 L51,91"),
    "W": (60, "M9,9 V91 L30,64 L51,91 V9"),
    "Y": (60, "M9,9 V36 L21,50 H39 L51,36 V9 M30,50 V91"),
}

RED = "#c3001d"
DARK_RED = "#7a0012"
GREY = "#d9d9d9"


def word(text):
    """The letters' paths placed along a line, and the line's width."""
    strokes, fills, x, placed = [], [], 0, []
    for ch in text:
        if ch == " ":
            x += SPACE
            continue
        w, d, *fill = LETTERS[ch]
        strokes.append('<path transform="translate(%d,0)" d="%s"/>' % (x, d))
        if fill:
            fills.append('<path transform="translate(%d,0)" d="%s"/>' % (x, fill[0]))
        placed.append((ch, x, w))
        x += w + GAP
    return strokes, fills, x - GAP, placed


def lettering(x, y, scale, colour, text):
    """A line of the lettering at (x, y), `scale` times the 100-high grid."""
    strokes, fills, _, _ = word(text)
    head = '<g transform="translate(%g,%g) scale(%g)"' % (x, y, scale)
    return [head + ' fill="none" stroke="%s" stroke-width="%d" stroke-linecap="square" '
            'stroke-linejoin="miter" stroke-miterlimit="2">' % (colour, STROKE), *strokes, '</g>',
            head + ' fill="%s">' % colour, *fills, '</g>']


def drip(x, top, length, width):
    """A drop of blood running down from the letters' foot: wide where it
    leaves them, thinning, then swelling into a bead at its end."""
    neck = width * 0.45
    r = width * 0.62
    y = top + length  # the bead's centre
    return ('<path d="M%.1f,%.1f C%.1f,%.1f %.1f,%.1f %.1f,%.1f C%.1f,%.1f %.1f,%.1f %.1f,%.1f '
            'A%.1f,%.1f 0 1 0 %.1f,%.1f C%.1f,%.1f %.1f,%.1f %.1f,%.1f C%.1f,%.1f %.1f,%.1f %.1f,%.1f Z"/>' % (
                x - width, top,
                x - width * 0.45, top + 2, x - neck / 2, top + length * 0.25, x - neck / 2, top + length * 0.5,
                x - neck / 2, y - r * 1.4, x - r * 0.9, y - r * 1.1, x - r * 0.9, y - r * 0.2,
                r, r, x + r * 0.9, y - r * 0.2,
                x + r * 0.9, y - r * 1.1, x + neck / 2, y - r * 1.4, x + neck / 2, top + length * 0.5,
                x + neck / 2, top + length * 0.25, x + width * 0.45, top + 2, x + width, top))


def splash_svg():
    margin = 24
    small_scale = 0.3
    small_w = word("POWERED BY")[2]
    _, _, big_w, letters = word("BLEED ENGINE")
    width = big_w + 2 * margin
    small_h = 100 * small_scale
    big_top = margin + small_h + 30
    height = big_top + 100 + 70 + margin
    # Drips under BLEED: from the foot of B, the first E and D (lengths and
    # places chosen so they read as running blood, not a pattern).
    runs = [(0, 0.30, 34), (2, 0.55, 62), (2, 0.85, 20), (4, 0.40, 46)]
    drips = []
    for k, at, length in runs:
        ch, x, w = letters[k]
        drips.append(drip(x + w * at, big_top + 96, length, 12))
    out = [
        '<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">' % (
            width, height, width, height),
        # Small line: centred over the big one.
        *lettering(margin + (big_w - small_w * small_scale) / 2, margin, small_scale, GREY, "POWERED BY"),
        # The name: a dark red edge under the red, a little down and right.
        *lettering(margin + 4, big_top + 4, 1, DARK_RED, "BLEED ENGINE"),
        '<g transform="translate(%d,0)" fill="%s">' % (margin, RED),
        *drips,
        '</g>',
        *lettering(margin, big_top, 1, RED, "BLEED ENGINE"),
        '</svg>',
    ]
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--svg", help="also write the SVG here (a preview)")
    args = ap.parse_args()
    svg = splash_svg()
    here = os.path.dirname(os.path.abspath(__file__))
    header = os.path.join(here, "..", "src", "platform", "BleedSplashArt.h")
    with open(header, "w", encoding="utf-8", newline="\n") as f:
        f.write("// Generated by port/tools/make_bleed_splash.py: the \"Powered by Bleed\n"
                "// Engine\" wordmark (Splash.cpp). Do not edit; rerun the tool.\n"
                "#pragma once\n\n"
                "namespace platform {\n\n"
                "inline constexpr const char kBleedSplashSvg[] = R\"svg(%s)svg\";\n\n"
                "}  // namespace platform\n" % svg)
    if args.svg:
        with open(args.svg, "w", encoding="utf-8") as f:
            f.write(svg)
    print("wrote", os.path.normpath(header))


if __name__ == "__main__":
    main()
