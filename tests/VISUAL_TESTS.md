# Visual Output Quality Tests — Developer Guide

## Overview

This directory adds **automated visual regression testing** to the
`libcupsfilters` CI pipeline. After every push or PR, GitHub Actions runs
print filters on synthetic test documents, renders the PDF output to PNG,
and compares against committed reference images using SSIM (Structural
Similarity Index).

```
Test input  ──►  CUPS filter  ──►  PDF output  ──►  Ghostscript  ──►  PNG
                 (texttopdf,                         (gs)              │
                  imagetopdf, …)                                        ▼
                                                               SSIM vs reference
                                                               ≥ 0.90 → PASS ✅
                                                               < 0.90 → FAIL ❌
```

## Test cases

| Test name | Input | Filter | Extra check |
|-----------|-------|--------|-------------|
| `ascii_text_to_pdf` | `sample_ascii.txt` | `texttopdf` | layout / SSIM |
| `unicode_text_to_pdf` | `sample_unicode.txt` | `texttopdf` | SSIM + **pdffonts** (issue #224 .notdef regression) |
| `image_png_to_pdf` | `sample_image.png` | `imagetopdf` | colour / resolution SSIM |

## Directory layout

```
tests/
  run_visual_tests.py        Main test runner (called by CI)
  generate_references.py     One-time reference image generator
  visual_inputs/             Test input documents (committed)
    sample_ascii.txt
    sample_unicode.txt
    sample_image.png
  visual_references/         Golden PNG snapshots (committed)
    ascii_text_to_pdf.png
    unicode_text_to_pdf.png
    image_png_to_pdf.png

ci/
  install-visual-deps.sh     Installs Ghostscript, Pillow, scikit-image, …

.github/workflows/
  visual-analysis.yml        GitHub Actions workflow definition
```

## Running locally (WSL / Ubuntu)

### Prerequisites

```bash
sudo apt-get install -y ghostscript poppler-utils \
    python3-numpy python3-pil python3-skimage cups-filters cups
```

### First time — build reference images

Run this once on a known-good build:

```bash
# 1. Build and install libcupsfilters (or use system package)
# 2. Generate reference images
python3 tests/generate_references.py

# 3. Commit the references
git add tests/visual_references/ tests/visual_inputs/
git commit -m "tests: add visual reference images for CI"
git push
```

### Every subsequent run — compare against references

```bash
python3 tests/run_visual_tests.py --report-dir visual-report --verbose
```

Open `visual-report/visual_test_report.html` in a browser to see
side-by-side reference vs. output thumbnails.

### Regenerating references after an intentional change

If you intentionally change filter output (e.g. a layout fix), regenerate:

```bash
python3 tests/generate_references.py
git add tests/visual_references/
git commit -m "tests: update visual references after <change description>"
```

## CI workflow

The workflow (`visual-analysis.yml`) runs automatically on every push and
PR. It:

1. Builds `libcupsfilters` using the same `ci/ci-setup.sh` as the main CI.
2. Installs visual test dependencies (`ci/install-visual-deps.sh`).
3. Runs `python3 tests/run_visual_tests.py`.
4. Uploads an HTML + JSON report as a **GitHub Actions artifact** (kept for
   30 days).

The job exits 1 if any test's SSIM score drops below **0.90**, or if
`pdffonts` detects unembedded fonts in the Unicode test (indicating a
.notdef regression).

## Thresholds

| Metric | Threshold | Meaning |
|--------|-----------|---------|
| SSIM | ≥ 0.90 | Structural similarity between output and reference PNG |
| pdffonts | all embedded | No unembedded fonts (no .notdef boxes) |

SSIM of 1.0 = pixel-identical. Scores ≥ 0.90 tolerate minor antialiasing
or DPI rounding differences between builds.

## Adding a new test case

1. Add an input file to `tests/visual_inputs/`.
2. Add an entry to the `TEST_CASES` list in `tests/run_visual_tests.py`.
3. Run `python3 tests/generate_references.py` to create the reference image.
4. `git add` the new reference and commit.

## Dependencies

All dependencies are standard Ubuntu packages — no pip or virtual
environment required in CI:

| Package | Purpose |
|---------|---------|
| `ghostscript` | Render PDF → PNG for visual comparison |
| `poppler-utils` | `pdffonts` for font embedding check |
| `python3-numpy` | Array operations |
| `python3-pil` | PNG loading (Pillow) |
| `python3-skimage` | SSIM metric |
| `cups-filters` | Provides `texttopdf`, `imagetopdf`, etc. |

## Relation to OpenPrinting-Image-Evaluation

The image comparison logic in `run_visual_tests.py` is a lightweight,
self-contained version of the metrics developed in the
[OpenPrinting-Image-Evaluation](https://github.com/Sanskary2303/OpenPrinting-Image-Evaluation)
GSoC 2025 project. This integration brings that work into the
`libcupsfilters` CI as Till Kamppeter outlined.
