"""Renders the clock digits 0-9 as 4-level (anti-aliased) images for the watch.

Pebble fonts can't hold glyphs this large (the SDK stops at about 88 px), so the
big clock is drawn from images instead. Each image is cropped to the digit's
ink, all with the same height (the digit height), with a transparent
background, two edge shades and one ink colour; the watch swaps them for
the theme colours.

    python3 tools/font/make_digit_images.py

Writes resources/images/digits/<platform>/0.png ... 9.png and sizes.txt.
"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
FONT = os.path.join(ROOT, "resources", "fonts", "ContourDigits-Black.ttf")
SS = 4  # supersampling for the edges
SIZES = {"gabbro": 103, "emery": 108}  # px, as large as each screen allows

for platform, size in SIZES.items():
    # Each digit is drawn SS times larger on a roomy canvas, on a common
    # baseline. Its image is then cropped to exactly its own ink across, and to
    # the common top and bottom of all digits, then scaled down, so the edge
    # pixels get a true coverage value (cut to 4 levels for the watch).
    font = ImageFont.truetype(FONT, size * SS)
    C = size * SS * 2
    bigs = {}
    for d in "0123456789":
        im = Image.new("L", (C, C), 0)
        ImageDraw.Draw(im).text((C // 4, C * 3 // 4), d, font=font, fill=255, anchor="ls")
        bigs[d] = im
    boxes = {d: bigs[d].getbbox() for d in bigs}
    top = min(b[1] for b in boxes.values())
    bottom = max(b[3] for b in boxes.values())
    h = -(-(bottom - top) // SS)  # rows on the watch, rounded up
    out = os.path.join(ROOT, "resources", "images", "digits", platform)
    os.makedirs(out, exist_ok=True)
    lines = []
    for d in "0123456789":
        x0, _, x1, _ = boxes[d]
        w = -(-(x1 - x0) // SS)
        pad = w * SS - (x1 - x0)        # share the rounding evenly on both sides
        crop = bigs[d].crop((x0 - pad // 2, top, x0 - pad // 2 + w * SS, top + h * SS))
        img = crop.resize((w, h), Image.BOX)
        lv = img.point(lambda v: min(3, (v * 3 + 127) // 255))
        pal = Image.new("P", (w, h), 0)
        # 0 see-through, 1 and 2 edge shades, 3 full ink. The watch swaps the
        # shades for mixes of the clock and background colours.
        pal.putpalette([0, 0, 0, 85, 85, 85, 170, 170, 170, 255, 255, 255] + [0, 0, 0] * 252)
        pal.paste(lv, (0, 0))
        pal.save(os.path.join(out, d + ".png"), transparency=0, bits=2, optimize=False)
        lines.append("%d %d" % (w, h))
    with open(os.path.join(out, "sizes.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    print(platform, size, "px: digit height", h, "widths", [l.split()[0] for l in lines])
