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

# The 8 is drawn from scratch to Maurin's description: two very heavy, wide,
# rounded-rectangle loops fused into one broad block with flat top and bottom,
# heavily rounded outer corners, a small sharp V notch on each side at the
# middle, and two equal, very thin, wide slot counters with small chamfers.
from fontTools.pens.ttGlyphPen import TTGlyphPen

f = TTFont(dst)
X0, X1, Y0, Y1 = 57, 764, 0, 720       # same box as the other digits
RC = 150                               # outer corner radius
MID, VN, VD = 360, 34, 44              # notch: middle, half height, depth
VR = 16                                # rounding where the side turns into the notch
SH, SX0, SX1, CH = 110, 215, 606, 16   # slot height, slot x range, chamfer
WALL = (Y1 - Y0 - 2 * SH) // 3         # equal top, middle and bottom bars

pen = TTGlyphPen(None)

def soft(corner, prev, nxt):
    # Round the corner at `corner` (coming from `prev`, going to `nxt`).
    import math
    def toward(p, q):
        dx, dy = q[0] - p[0], q[1] - p[1]
        l = math.hypot(dx, dy)
        return (round(p[0] + dx / l * VR), round(p[1] + dy / l * VR))
    pen.lineTo(toward(corner, prev))
    pen.qCurveTo(corner, toward(corner, nxt))

# Outer shape, clockwise. The V tips stay sharp; the four corners where the
# sides turn into the notches are slightly rounded.
pen.moveTo((X1 - RC, Y0))
pen.lineTo((X0 + RC, Y0))
pen.qCurveTo((X0, Y0), (X0, Y0 + RC))
soft((X0, MID - VN), (X0, Y0 + RC), (X0 + VD, MID))
pen.lineTo((X0 + VD, MID))
soft((X0, MID + VN), (X0 + VD, MID), (X0, Y1 - RC))
pen.lineTo((X0, Y1 - RC))
pen.qCurveTo((X0, Y1), (X0 + RC, Y1))
pen.lineTo((X1 - RC, Y1))
pen.qCurveTo((X1, Y1), (X1, Y1 - RC))
soft((X1, MID + VN), (X1, Y1 - RC), (X1 - VD, MID))
pen.lineTo((X1 - VD, MID))
soft((X1, MID - VN), (X1 - VD, MID), (X1, Y0 + RC))
pen.lineTo((X1, Y0 + RC))
pen.qCurveTo((X1, Y0), (X1 - RC, Y0))
pen.closePath()
# Two slot counters, anticlockwise.
for yb in (Y0 + WALL, Y1 - WALL - SH):
    yt = yb + SH
    pen.moveTo((SX0 + CH, yb))
    pen.lineTo((SX1 - CH, yb))
    pen.lineTo((SX1, yb + CH))
    pen.lineTo((SX1, yt - CH))
    pen.lineTo((SX1 - CH, yt))
    pen.lineTo((SX0 + CH, yt))
    pen.lineTo((SX0, yt - CH))
    pen.lineTo((SX0, yb + CH))
    pen.closePath()
f["glyf"]["eight"] = pen.glyph()
f.save(dst)

# Rounded corners: every sharp corner between two straight edges of
# the digits (outside and inside the counters) gets a small curve.
import math
CORNER = 70  # radius in font units (capped at 45% of each edge)

f = TTFont(dst)
glyf = f["glyf"]
for name in ["zero", "one", "two", "three", "four", "five", "six", "seven", "nine"]:  # the 8 is drawn above
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
