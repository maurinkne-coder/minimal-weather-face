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
