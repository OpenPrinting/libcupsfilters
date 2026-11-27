#!/usr/bin/env python3
"""
generate_references.py – One-time golden reference image generator
====================================================================
Run this script ONCE on a known-good build to create the reference PNG
images that run_visual_tests.py compares against in CI.

Also creates synthetic test input files (sample_ascii.txt,
sample_unicode.txt, sample_image.png) if they don't exist yet.

Usage
-----
  cd /path/to/libcupsfilters
  python3 tests/generate_references.py

The generated files are committed to git so CI can compare against them
without re-generating on every run.
"""

from __future__ import annotations

import logging
import os
import subprocess
import sys
from pathlib import Path

LOG = logging.getLogger("gen_refs")
logging.basicConfig(level=logging.INFO, format="%(levelname)s  %(message)s")

SCRIPT_DIR  = Path(__file__).parent.resolve()
INPUTS_DIR  = SCRIPT_DIR / "visual_inputs"
REFS_DIR    = SCRIPT_DIR / "visual_references"

# ---------------------------------------------------------------------------
# Generate synthetic test inputs
# ---------------------------------------------------------------------------

ASCII_DOC = """\
LibcupsFilters Visual Test — ASCII Sample
==========================================

This document is used to verify that the texttopdf filter correctly
converts plain text to PDF without layout regressions.

Alphabet : ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz
Digits   : 0 1 2 3 4 5 6 7 8 9
Symbols  : ! @ # $ % ^ & * ( ) _ + - = [ ] { } | ; : ' " , . < > / ? \\
Long line: The quick brown fox jumps over the lazy dog. Pack my box with five dozen liquor jugs.

Filter options under test: cpi=10 lpi=6
"""

UNICODE_DOC = """\
LibcupsFilters Visual Test — Unicode Sample
============================================

This document tests multi-script Unicode support in texttopdf.
A regression causes characters outside the base font's charset
to be printed as empty .notdef boxes (see issue #224).

Latin extended : Ä Ö Ü ä ö ü ß é è ê ë à â ç ñ
Greek          : Α Β Γ Δ Ε Ζ Η Θ Ι Κ Λ Μ Ν Ξ Ο Π Ρ Σ Τ Υ Φ Χ Ψ Ω
Cyrillic       : А Б В Г Д Е Ё Ж З И К Л М Н О П Р С Т У Ф Х Ц Ч
Symbols        : © ® ™ € £ ¥ ± × ÷ ≤ ≥ ≠ ∞ → ← ↑ ↓ ✓ ✗
CJK sample     : 日本語 中文 한국어

If this document prints correctly, all characters above should be
visible — not empty boxes.
"""


def _make_sample_png() -> None:
    """Create a simple synthetic PNG for imagetopdf testing."""
    png_path = INPUTS_DIR / "sample_image.png"
    if png_path.exists():
        LOG.info("sample_image.png already exists — skipping")
        return

    try:
        import numpy as np
        from PIL import Image, ImageDraw, ImageFont
    except ImportError:
        LOG.warning("numpy/Pillow not available — sample_image.png not generated.")
        LOG.warning("Install them or provide your own tests/visual_inputs/sample_image.png")
        return

    W, H = 600, 400
    img = Image.new("RGB", (W, H), color=(255, 255, 255))
    draw = ImageDraw.Draw(img)

    # Gradient background
    for y in range(H):
        r = int(220 * (1 - y / H)) + 30
        g = int(100 * (y / H)) + 80
        b = 180
        draw.line([(0, y), (W, y)], fill=(r, g, b))

    # Colour bars
    for i, colour in enumerate([(255, 0, 0), (0, 200, 0), (0, 0, 255), (255, 220, 0)]):
        x0 = i * (W // 4)
        x1 = x0 + W // 4
        draw.rectangle([x0, 20, x1, 80], fill=colour)

    # Text labels
    draw.text((20, 100), "libcupsfilters imagetopdf test", fill=(0, 0, 0))
    draw.text((20, 130), "Colour fidelity and resolution check", fill=(40, 40, 40))

    # Grid lines
    for x in range(0, W, 60):
        draw.line([(x, 160), (x, H - 20)], fill=(180, 180, 180), width=1)
    for y in range(160, H - 20, 40):
        draw.line([(0, y), (W, y)], fill=(180, 180, 180), width=1)

    img.save(str(png_path), "PNG")
    LOG.info("Generated %s (%d×%d)", png_path.name, W, H)


def _create_inputs() -> None:
    INPUTS_DIR.mkdir(parents=True, exist_ok=True)

    ascii_path = INPUTS_DIR / "sample_ascii.txt"
    if not ascii_path.exists():
        ascii_path.write_text(ASCII_DOC, encoding="utf-8")
        LOG.info("Created %s", ascii_path.name)

    uni_path = INPUTS_DIR / "sample_unicode.txt"
    if not uni_path.exists():
        uni_path.write_text(UNICODE_DOC, encoding="utf-8")
        LOG.info("Created %s", uni_path.name)

    _make_sample_png()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    LOG.info("Step 1/2 — Creating test input files…")
    _create_inputs()

    LOG.info("Step 2/2 — Running filters and saving reference images…")
    # Delegate to run_visual_tests.py with --generate-refs
    script = SCRIPT_DIR / "run_visual_tests.py"
    cmd = [sys.executable, str(script), "--generate-refs", "--verbose"]
    LOG.info("Running: %s", " ".join(cmd))
    result = subprocess.run(cmd, cwd=SCRIPT_DIR.parent)
    if result.returncode != 0:
        LOG.error("Reference generation failed (exit %d).", result.returncode)
        LOG.error(
            "Make sure libcupsfilters is built and installed, and that "
            "'ghostscript' is available on PATH."
        )
        sys.exit(result.returncode)

    # List what was created
    refs = sorted(REFS_DIR.glob("*.png"))
    if refs:
        LOG.info("Reference images saved:")
        for p in refs:
            LOG.info("  %s", p)
        LOG.info(
            "\nNext steps:\n"
            "  git add tests/visual_references/*.png tests/visual_inputs/\n"
            "  git commit -m 'tests: add visual reference images for CI'\n"
            "  git push"
        )
    else:
        LOG.warning("No reference images were generated. Check filter availability.")


if __name__ == "__main__":
    main()
