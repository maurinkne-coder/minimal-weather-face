"""Builds resources/fonts/ContourDigits-Black.ttf from Orbitron Black.

The zero gets the plain rounded-square shape of Orbitron's letter O instead
of the slashed zero, so the clock reads cleaner. Orbitron has the Reserved
Font Name "Orbitron" under the OFL, so the changed font gets a new name.

    pip install fonttools
    python3 tools/font/make_digits.py Orbitron_900Black.ttf resources/fonts/ContourDigits-Black.ttf
"""
import sys
from fontTools.ttLib import TTFont

src, dst = sys.argv[1], sys.argv[2]
f = TTFont(src)
glyf, hmtx = f["glyf"], f["hmtx"]
glyf["zero"] = glyf["O"]
hmtx["zero"] = hmtx["O"]

names = {1: "Contour Digits Black", 3: "ContourDigits-Black", 4: "Contour Digits Black",
         6: "ContourDigits-Black", 16: "Contour Digits", 17: "Black"}
for r in f["name"].names:
    if r.nameID in names:
        r.string = names[r.nameID]
    elif r.nameID == 0:
        r.string = r.toUnicode() + " Modified for Contour (plain zero)."
f.save(dst)

# The 8 gets a clear waist: a round notch on both sides at the middle bar.
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.pens.recordingPen import DecomposingRecordingPen

f = TTFont(dst)
gs = f.getGlyphSet()
rec = DecomposingRecordingPen(gs)
gs["eight"].draw(rec)
coords, ends, flags = f["glyf"]["eight"].getCoordinates(f["glyf"])
# A smooth dip: the side runs straight into the notch and out again with no
# corner (TrueType quadratic curves, tangent stays vertical at both ends).
M, W, D = 365, 100, 42  # middle of the bar, half height and depth of the dip
from fontTools.cu2qu import curve_to_quadratic
def dip(x, k):  # k = +1 on the left side, -1 on the right side
    # Two smooth eased S-curves (cubic, converted to TrueType quadratics):
    # vertical tangent where it leaves the side and at the deepest point.
    def half(p0, p3):
        my = (p0[1] + p3[1]) / 2
        q = curve_to_quadratic([p0, (p0[0], my), (p3[0], my), p3], 1.0)
        return [(q[0], 1)] + [(pt, 0) for pt in q[1:-1]] + [(q[-1], 1)]
    a = half((x, M - W), (x + k * D, M))
    b = half((x + k * D, M), (x, M + W))
    pts = a + b[1:]
    return [((round(px), round(py)), on) for (px, py), on in pts]
outer = [((x, y), fl & 1) for (x, y), fl in zip(list(coords)[:ends[0] + 1], list(flags)[:ends[0] + 1])]
left = [((57, 145), 1)] + dip(57, 1) + [((57, 575), 1)]
right = [((764, 601), 1)] + list(reversed(dip(764, -1))) + [((764, 145), 1)]
new = []
skip = None
for pt in outer:
    if pt[0] == (57, 145):
        new += left; skip = (57, 575); continue
    if pt[0] == (764, 601):
        new += right; skip = (764, 145); continue
    if skip:
        if pt[0] == skip: skip = None
        continue
    new.append(pt)
pen = TTGlyphPen(gs)
def contour(pts):
    # TrueType quadratic contour from on/off points
    start = pts[0][0]
    pen.moveTo(start)
    offs = []
    for p, on in pts[1:]:
        if on:
            if offs: pen.qCurveTo(*offs, p); offs = []
            else: pen.lineTo(p)
        else:
            offs.append(p)
    if offs: pen.qCurveTo(*offs, start)
    pen.closePath()
contour(new)
s = ends[0] + 1
for e in ends[1:]:
    contour([((x, y), fl & 1) for (x, y), fl in zip(list(coords)[s:e + 1], list(flags)[s:e + 1])])
    s = e + 1
f["glyf"]["eight"] = pen.glyph()
f.save(dst)

# Rounded corners: every sharp corner between two straight edges of
# the digits (outside and inside the counters) gets a small curve.
import math
CORNER = 70  # radius in font units (capped at 45% of each edge)

f = TTFont(dst)
glyf = f["glyf"]
for name in ["zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine"]:
    g = glyf[name]
    if g.isComposite():
        continue
    coords, ends, flags = g.getCoordinates(glyf)
    pen = TTGlyphPen(None)
    s = 0
    for e in ends:
        pts = []
        for (x, y), fl in zip(list(coords)[s:e + 1], list(flags)[s:e + 1]):
            p = ((x, y), fl & 1)
            if pts and pts[-1][0] == p[0]:
                if p[1]:
                    pts[-1] = p  # keep the on-curve copy of a duplicate point
                continue
            pts.append(p)
        while len(pts) > 1 and pts[0][0] == pts[-1][0]:
            pts.pop()
        s = e + 1
        n = len(pts)
        out = []
        for i, (p, on) in enumerate(pts):
            a, b = pts[i - 1], pts[(i + 1) % n]
            if not (on and a[1] and b[1]):
                out.append((p, on))
                continue
            ax, ay = a[0][0] - p[0], a[0][1] - p[1]
            bx, by = b[0][0] - p[0], b[0][1] - p[1]
            la, lb = math.hypot(ax, ay), math.hypot(bx, by)
            if la == 0 or lb == 0:
                out.append((p, on))
                continue
            cosang = (ax * bx + ay * by) / (la * lb)
            if cosang < -0.95:  # straight through, no corner
                out.append((p, on))
                continue
            r = min(CORNER, la * 0.45, lb * 0.45)
            out.append(((round(p[0] + ax / la * r), round(p[1] + ay / la * r)), 1))
            out.append((p, 0))
            out.append(((round(p[0] + bx / lb * r), round(p[1] + by / lb * r)), 1))
        start = out[0][0] if out[0][1] else None
        if start is None:
            out = out[1:] + out[:1]
            start = out[0][0]
        pen.moveTo(start)
        offs = []
        for p, on in out[1:]:
            if on:
                if offs:
                    pen.qCurveTo(*offs, p)
                    offs = []
                else:
                    pen.lineTo(p)
            else:
                offs.append(p)
        if offs:
            pen.qCurveTo(*offs, start)
        pen.closePath()
    glyf[name] = pen.glyph()
f.save(dst)
