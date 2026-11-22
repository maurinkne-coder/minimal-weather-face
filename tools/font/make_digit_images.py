"""Renders the clock digits 0-9 as 1-bit images for the watch.

Pebble fonts can't hold glyphs this large (the SDK stops at about 88 px), so the
big clock is drawn from images instead. Each image is cropped to the digit's
ink, all with the same height (the digit height), with a transparent
background and one ink colour that the watch swaps for the theme colour.

    python3 tools/font/make_digit_images.py

Writes resources/images/digits/<platform>/0.png ... 9.png and sizes.txt.
"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
FONT = os.path.join(ROOT, "resources", "fonts", "ContourDigits-Black.ttf")
SIZES = {"gabbro": 103, "emery": 108}  # px, as large as each screen allows

for platform, size in SIZES.items():
    font = ImageFont.truetype(FONT, size)
    # Common vertical box: top of the tallest digit to the baseline.
    top = min(font.getbbox(d)[1] for d in "0123456789")
    bottom = max(font.getbbox(d)[3] for d in "0123456789")
    out = os.path.join(ROOT, "resources", "images", "digits", platform)
    os.makedirs(out, exist_ok=True)
    lines = []
    for d in "0123456789":
        x0, _, x1, _ = font.getbbox(d)
        w, h = x1 - x0, bottom - top
        img = Image.new("L", (w, h), 0)
        ImageDraw.Draw(img).text((-x0, -top), d, font=font, fill=255)
        bw = img.point(lambda v: 1 if v >= 128 else 0)
        pal = Image.new("P", (w, h), 0)
        pal.putpalette([0, 0, 0, 255, 255, 255] + [0, 0, 0] * 254)
        pal.paste(bw, (0, 0))
        pal.info["transparency"] = 0          # index 0 is see-through
        pal.save(os.path.join(out, d + ".png"), transparency=0, bits=1, optimize=True)
        lines.append("%d %d" % (w, h))
    with open(os.path.join(out, "sizes.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    print(platform, size, "px: digit height", bottom - top, "widths", [l.split()[0] for l in lines])
