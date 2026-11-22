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
    "gothic28b": ("LiberationSans-Bold.ttf", 25, 0.84, 6),
    "bitham42l": ("LiberationSans-Regular.ttf", 40, 0.92, 8),
}

def col(a):
    return tuple(((a >> s) & 3) * 85 for s in (4, 2, 0))

def text_width(s, f):
    fn, size, sq, off = FONTS[f]
    font = ImageFont.truetype(LIB + fn, size * S)
    return font.getlength(s) * sq / S

def group_rows(log):
    """Left-aligned texts on one line form a row: the C side placed them with
    rough width guesses, so lay them out again with real widths, centred on
    the same middle and with the same gaps the code meant (5 px)."""
    out, i = [], 0
    while i < len(log):
        c = log[i]
        if c[0] == "text" and len(c) > 8 and c[8] and c[2] in FONTS:
            row = [c]
            while i + 1 < len(log) and log[i + 1][0] == "text" and len(log[i + 1]) > 8 \
                    and log[i + 1][8] and log[i + 1][4] == c[4]:
                i += 1
                row.append(log[i])
            mid = (row[0][3] + row[-1][3] + row[-1][5]) / 2
            ws = [text_width(r[1], r[2]) for r in row]
            total = sum(ws) + 5 * (len(row) - 1)
            x = mid - total / 2
            for r, wd in zip(row, ws):
                out.append(["text", r[1], r[2], x, r[4], wd + 1, r[6], r[7]])
                x += wd + 5
        else:
            out.append(c)
        i += 1
    return out

def render(log, w, h, round_, out):
    img = Image.new("RGB", (w * S, h * S), (0, 0, 0))
    d = ImageDraw.Draw(img)
    overflow = []
    log = group_rows(log)
    for c in log:
        k = c[0]
        if k == "rect":
            _, x, y, rw, rh, a = c
            d.rectangle([x * S, y * S, (x + rw) * S, (y + rh) * S], fill=col(a))
        elif k == "px":
            # A symbol pixel mixed over what is there, k/16 coverage, in
            # 2-bit steps per channel like the watch.
            _, x, y, a, cov = c
            under = img.getpixel((x * S + S // 2, y * S + S // 2))
            over = col(a)
            mixed = tuple(round(((u // 85) * (16 - cov) + (o // 85) * cov) / 16) * 85 for u, o in zip(under, over))
            d.rectangle([x * S, y * S, x * S + S - 1, y * S + S - 1], fill=mixed)
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
        elif k == "tilt_begin":
            # Set the text layer aside (background see-through) and clear.
            _, a = c
            bgc = col(a)
            layer = img.convert("RGBA")
            px = layer.load()
            for yy in range(layer.height):
                for xx in range(layer.width):
                    if px[xx, yy][:3] == bgc:
                        px[xx, yy] = (0, 0, 0, 0)
            img = Image.new("RGB", img.size, bgc)
            d = ImageDraw.Draw(img)
        elif k == "tilt_end":
            # Lay the text back on top, turned, with bilinear smoothing.
            _, deg = c
            turned = layer.rotate(-deg, resample=Image.BILINEAR, center=(w * S / 2, h * S / 2))
            img = Image.alpha_composite(img.convert("RGBA"), turned).convert("RGB")
            d = ImageDraw.Draw(img)
        elif k == "digits":
            _, s, cx, top, gap, a = c
            plat = "gabbro" if round_ else "emery"
            ims = [Image.open(os.path.join(HERE, "../../resources/images/digits", plat, ch + ".png")).convert("L") for ch in s]
            total = sum(i.width for i in ims) + gap * (len(ims) - 1)
            x = cx - total // 2
            for ch, im in zip(s, ims):
                # Palette indices 0-3 are ink levels (see-through, two edge shades, full).
                idx = Image.open(os.path.join(HERE, "../../resources/images/digits", plat, ch + ".png"))
                mask = idx.convert("L")  # palette greys 0/85/170/255 = levels
                mask = mask.resize((im.width * S, im.height * S), Image.NEAREST)
                img.paste(Image.new("RGB", mask.size, col(a)), (x * S, top * S), mask)
                x += im.width + gap
            d = ImageDraw.Draw(img)
        elif k == "text" and (c[2].startswith("clock") or c[2].startswith("small")):
            _, s, f, x, y, bw, bh, a = c[:8]
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
            _, s, f, x, y, bw, bh, a = c[:8]
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
