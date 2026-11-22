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

# The 8 gets a clear waist: a square notch on both sides at the middle bar.
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.pens.recordingPen import DecomposingRecordingPen

f = TTFont(dst)
gs = f.getGlyphSet()
rec = DecomposingRecordingPen(gs)
gs["eight"].draw(rec)
coords, ends, flags = f["glyf"]["eight"].getCoordinates(f["glyf"])
D = 80  # notch depth in font units
outer = []
for (x, y), fl in zip(list(coords)[:ends[0] + 1], list(flags)[:ends[0] + 1]):
    outer.append(((x, y), fl & 1))
left = [((57, 297), 1), ((57, 318), 1), ((57 + D, 343), 1), ((57 + D, 387), 1), ((57, 412), 1), ((57, 434), 1)]
right = [((764, 434), 1), ((764, 412), 1), ((764 - D, 387), 1), ((764 - D, 343), 1), ((764, 318), 1), ((764, 297), 1)]
new = []
skip = None
for pt in outer:
    if pt[0] == (57, 297):
        new += left; skip = (57, 434); continue
    if pt[0] == (764, 434):
        new += right; skip = (764, 297); continue
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
