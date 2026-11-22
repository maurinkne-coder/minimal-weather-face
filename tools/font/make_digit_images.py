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
        # Drawn 4x larger and scaled down, so the edge pixels get a true
        # coverage value; that is then cut to 4 levels for the watch.
        big = Image.new("L", (w * SS, h * SS), 0)
        bigfont = ImageFont.truetype(FONT, size * SS)
        bx0 = bigfont.getbbox(d)[0]
        btop = min(bigfont.getbbox(c)[1] for c in "0123456789")
        ImageDraw.Draw(big).text((-bx0, -btop), d, font=bigfont, fill=255)
        img = big.resize((w, h), Image.BOX)
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
    print(platform, size, "px: digit height", bottom - top, "widths", [l.split()[0] for l in lines])
