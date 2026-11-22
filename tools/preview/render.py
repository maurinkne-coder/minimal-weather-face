"""Render the JSON draw log from the stub into a PNG (approximate fonts)."""
import json, math, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
from PIL import Image, ImageDraw, ImageFont

S = 3  # supersample
LIB = "/usr/share/fonts/truetype/liberation/"
FONTS = {  # key: (file, px size, horizontal squeeze, y offset)
    "gothic14": ("LiberationSans-Regular.ttf", 13, 0.86, 3),
    "gothic14b": ("LiberationSans-Bold.ttf", 13, 0.86, 3),
    "gothic18": ("LiberationSans-Regular.ttf", 16, 0.86, 4),
    "gothic18b": ("LiberationSans-Bold.ttf", 16, 0.84, 4),
    "gothic24b": ("LiberationSans-Bold.ttf", 21, 0.84, 5),
    "bitham42l": ("LiberationSans-Regular.ttf", 40, 0.92, 8),
}

def col(a):
    return tuple(((a >> s) & 3) * 85 for s in (4, 2, 0))

def render(log, w, h, round_, out):
    img = Image.new("RGB", (w * S, h * S), (0, 0, 0))
    d = ImageDraw.Draw(img)
    overflow = []
    for c in log:
        k = c[0]
        if k == "rect":
            _, x, y, rw, rh, a = c
            d.rectangle([x * S, y * S, (x + rw) * S, (y + rh) * S], fill=col(a))
        elif k == "circle":
            _, x, y, r, a = c
            d.ellipse([(x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S], fill=col(a))
        elif k == "line":
            _, x0, y0, x1, y1, sw, a = c
            sw = max(1, sw)
            d.line([x0 * S, y0 * S, x1 * S, y1 * S], fill=col(a), width=sw * S)
            for (x, y) in ((x0, y0), (x1, y1)):
                r = sw * S / 2
                d.ellipse([x * S - r, y * S - r, x * S + r, y * S + r], fill=col(a))
        elif k == "radial":
            _, x, y, rw, rh, t, a0, a1, a = c
            cx, cy, R = (x + rw / 2) * S, (y + rh / 2) * S, rw / 2 * S
            pts = []
            n = 8
            for i in range(n + 1):
                ang = (a0 + (a1 - a0) * i / n) / 65536 * 2 * math.pi
                pts.append((cx + math.sin(ang) * R, cy - math.cos(ang) * R))
            for i in range(n, -1, -1):
                ang = (a0 + (a1 - a0) * i / n) / 65536 * 2 * math.pi
                pts.append((cx + math.sin(ang) * (R - t * S), cy - math.cos(ang) * (R - t * S)))
            d.polygon(pts, fill=col(a))
        elif k == "poly":
            _, a, pts = c
            d.polygon([(pts[i] * S, pts[i + 1] * S) for i in range(0, len(pts), 2)], fill=col(a))
        elif k == "polyline":
            _, a, pts = c
            q = [(pts[i] * S, pts[i + 1] * S) for i in range(0, len(pts), 2)]
            d.line(q + [q[0]], fill=col(a), width=S)
        elif k == "rotate":
            _, deg, a = c
            img = img.rotate(-deg, resample=Image.NEAREST, center=(w * S / 2, h * S / 2), fillcolor=col(a))
            d = ImageDraw.Draw(img)
        elif k == "text" and (c[2].startswith("clock") or c[2].startswith("small")):
            _, s, f, x, y, bw, bh, a = c
            size = int(f[5:])
            track = -2 if f.startswith("clock") else 1
            font = ImageFont.truetype(os.path.join(HERE, "../../resources/fonts/ContourDigits-Black.ttf"), size * S)
            adv = [d.textlength(ch, font=font) + track * S for ch in s]
            tw = sum(adv) - track * S
            if tw > bw * S:
                overflow.append(s)
            px = x * S + (bw * S - tw) / 2
            for ch, a_ in zip(s, adv):
                d.text((px, y * S), ch, font=font, fill=col(a))
                px += a_
        elif k == "text":
            _, s, f, x, y, bw, bh, a = c
            fn, size, sq, off = FONTS[f]
            font = ImageFont.truetype(LIB + fn, size * S)
            tw = d.textlength(s, font=font) * sq
            if tw > bw * S:
                overflow.append(s)
            tmp = Image.new("RGBA", (int(d.textlength(s, font=font)) + 4, (size + 10) * S), (0, 0, 0, 0))
            ImageDraw.Draw(tmp).text((0, 0), s, font=font, fill=col(a) + (255,))
            tmp = tmp.resize((max(1, int(tmp.width * sq)), tmp.height))
            px = int(x * S + (bw * S - tmp.width) / 2)
            img.paste(tmp, (px, int((y + off) * S - size * S * 0.15)), tmp)
    if round_:
        mask = Image.new("L", img.size, 0)
        ImageDraw.Draw(mask).ellipse([0, 0, w * S, h * S], fill=255)
        bg = Image.new("RGB", img.size, (40, 40, 40))
        bg.paste(img, (0, 0), mask)
        img = bg
    img = img.resize((w * 2, h * 2), Image.LANCZOS)
    img.save(out)
    return overflow

if __name__ == "__main__":
    w, h, rnd, out = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3] == "1", sys.argv[4]
    log = [json.loads(l) for l in sys.stdin if l.strip()]
    ov = render(log, w, h, rnd, out)
    if ov:
        print("OVERFLOW:", ov)
