#!/usr/bin/env python3
#
# Generate symbol-only.ttf, a tiny TrueType font for
# test-texttopdf-font-coverage.sh.
#
# The font has a single real glyph (a full block at U+2588) and nothing
# else: no letters, no digits, no space.  It stands in for symbol-only
# fonts such as PowerlineSymbols, which some distributions alias to
# "monospace" (Issue #224).
#
# The glyph is drawn here from scratch, so the font carries no third-party
# license.  Requires fontTools:
#
#   python3 cupsfilters/test_files/gen-symbol-only-font.py \
#           cupsfilters/test_files/symbol-only.ttf
#

import sys

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

FAMILY = "CupsfiltersSymbolOnly"


def box(x0, y0, x1, y1):
    pen = TTGlyphPen(None)
    pen.moveTo((x0, y0))
    pen.lineTo((x0, y1))
    pen.lineTo((x1, y1))
    pen.lineTo((x1, y0))
    pen.closePath()
    return pen.glyph()


def main(path):
    fb = FontBuilder(1000, isTTF=True)
    fb.setupGlyphOrder([".notdef", "block"])
    fb.setupCharacterMap({0x2588: "block"})
    fb.setupGlyf({".notdef": box(50, 0, 550, 700),
                  "block": box(0, -200, 600, 800)})
    fb.setupHorizontalMetrics({".notdef": (600, 50), "block": (600, 0)})
    fb.setupHorizontalHeader(ascent=800, descent=-200)
    fb.setupNameTable({"familyName": FAMILY, "styleName": "Regular",
                       "psName": FAMILY + "-Regular"})
    # isFixedPitch so that fontconfig reports it as monospaced, like the
    # real-world fonts that trigger the bug
    fb.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800,
                usWinDescent=200)
    fb.setupPost(isFixedPitch=1)
    fb.save(path)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "symbol-only.ttf")
