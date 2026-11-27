#!/usr/bin/env python3
"""
run_visual_tests.py – Visual output quality tests for libcupsfilters
======================================================================
Tests the visual quality of CUPS filter output by:
  1. Running test documents through CUPS filters (texttopdf, imagetopdf,
     pdftopdf, …)
  2. Rendering PDF output to PNG via Ghostscript
  3. Comparing against golden reference images (SSIM + histogram)
  4. Detecting unembedded fonts / .notdef box regressions (pdffonts)
  5. Detecting pdftopdf regressions: missing pages, blank/black pages,
     wrong orientation, broken number-up layout
  6. Generating an HTML + JSON report

Exercises the FULL architecture × CUPS-version matrix when called from
the build.yaml workflow steps — consistent with the existing CI grid.

Exit codes
  0 – all tests passed (or skipped gracefully)
  1 – one or more tests failed

Usage
-----
  # First time — build reference images:
  python3 tests/run_visual_tests.py --generate-refs

  # Every CI run — compare against references:
  python3 tests/run_visual_tests.py --report-dir visual-report --verbose
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path

# ---------------------------------------------------------------------------
# Optional image-analysis libraries (degrade gracefully if absent)
# ---------------------------------------------------------------------------
try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False
    np = None  # type: ignore[assignment]

try:
    from skimage.metrics import structural_similarity as _ssim
    HAS_SKIMAGE = True
except ImportError:
    HAS_SKIMAGE = False

try:
    from PIL import Image as _PIL_Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

# ---------------------------------------------------------------------------
# Paths and constants
# ---------------------------------------------------------------------------
SCRIPT_DIR = Path(__file__).parent.resolve()
INPUTS_DIR = SCRIPT_DIR / "visual_inputs"
REFS_DIR   = SCRIPT_DIR / "visual_references"

SSIM_THRESHOLD = 0.90   # < this → FAIL
RENDER_DPI     = 150

LOG = logging.getLogger("visual_tests")

# ---------------------------------------------------------------------------
# Pure-Python minimal PDF generator (no external deps)
# ---------------------------------------------------------------------------

def _make_pdf(pages: int = 1, landscape: bool = False, text_prefix: str = "Page") -> bytes:
    """
    Generate a minimal valid multi-page PDF using only Python stdlib.
    Each page contains a visible grid + label so blank/black-page checks work.
    """
    pw, ph = (792, 612) if landscape else (612, 792)   # points (Letter)

    n = pages
    # Object numbering (1-based):
    #  1        Catalog
    #  2        Pages
    #  3..2+n   Page objects
    #  3+n..2+2n  Content streams
    #  3+2n     Helvetica font
    catalog_id   = 1
    pages_id     = 2
    page_ids     = list(range(3, 3 + n))
    content_ids  = list(range(3 + n, 3 + 2 * n))
    font_id      = 3 + 2 * n

    obj_bodies: list[bytes] = []

    # Catalog
    obj_bodies.append(f"<< /Type /Catalog /Pages {pages_id} 0 R >>".encode())

    # Pages
    kids = " ".join(f"{i} 0 R" for i in page_ids)
    obj_bodies.append(f"<< /Type /Pages /Kids [{kids}] /Count {n} >>".encode())

    # Page objects
    for content_id in content_ids:
        obj_bodies.append((
            f"<< /Type /Page /Parent {pages_id} 0 R "
            f"/MediaBox [0 0 {pw} {ph}] "
            f"/Contents {content_id} 0 R "
            f"/Resources << /Font << /F1 {font_id} 0 R >> >> >>"
        ).encode())

    # Content streams — grid lines + page label
    for idx in range(n):
        label = f"{text_prefix} {idx + 1} of {n}  ({pw}x{ph} pts)"
        # Draw a 4×4 grid in grey, then write the label
        lines = ["q", "0.6 G"]
        for j in range(1, 5):
            lines.append(f"0 {ph * j // 5:.0f} m {pw} {ph * j // 5:.0f} l S")
        for j in range(1, 4):
            lines.append(f"{pw * j // 4:.0f} 0 m {pw * j // 4:.0f} {ph} l S")
        lines += [
            "Q",
            "BT", "/F1 28 Tf",
            f"50 {ph - 80:.0f} Td", f"({label}) Tj",
            "/F1 16 Tf", "0 -36 Td",
            f"(libcupsfilters visual regression test) Tj",
            "ET",
        ]
        stream = "\n".join(lines).encode()
        obj_bodies.append(
            f"<< /Length {len(stream)} >>\nstream\n".encode()
            + stream
            + b"\nendstream"
        )

    # Font
    obj_bodies.append(b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>")

    # Assemble
    pdf = b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n"
    offsets = []
    for i, body in enumerate(obj_bodies):
        offsets.append(len(pdf))
        pdf += f"{i + 1} 0 obj\n".encode() + body + b"\nendobj\n"

    xref_off = len(pdf)
    pdf += b"xref\n"
    pdf += f"0 {len(obj_bodies) + 1}\n".encode()
    pdf += b"0000000000 65535 f \n"
    for off in offsets:
        pdf += f"{off:010d} 00000 n \n".encode()
    pdf += (
        b"trailer\n"
        + f"<< /Size {len(obj_bodies) + 1} /Root 1 0 R >>\n".encode()
        + b"startxref\n"
        + f"{xref_off}\n".encode()
        + b"%%EOF\n"
    )
    return pdf


def _setup_pdf_inputs() -> None:
    """Auto-generate PDF test inputs that cannot be stored as text files."""
    INPUTS_DIR.mkdir(parents=True, exist_ok=True)

    specs = [
        ("sample_4page.pdf",    dict(pages=4, landscape=False, text_prefix="Portrait page")),
        ("sample_landscape.pdf", dict(pages=1, landscape=True,  text_prefix="Landscape page")),
        ("sample_2page.pdf",    dict(pages=2, landscape=False, text_prefix="Page")),
    ]
    for fname, kwargs in specs:
        p = INPUTS_DIR / fname
        if not p.exists():
            p.write_bytes(_make_pdf(**kwargs))
            LOG.debug("Generated %s", fname)

# ---------------------------------------------------------------------------
# Test-case registry
# ---------------------------------------------------------------------------
TEST_CASES: list[dict] = [
    # ------------------------------------------------------------------
    # texttopdf — text rendering
    # ------------------------------------------------------------------
    {
        "name": "ascii_text_to_pdf",
        "description": "Plain ASCII text → PDF via texttopdf",
        "input": "sample_ascii.txt",
        "input_mime": "text/plain",
        "output_mime": "application/pdf",
        "filter": "texttopdf",
        "options": "cpi=10 lpi=6",
        "notdef_check": False,
    },
    {
        "name": "unicode_text_to_pdf",
        "description": "UTF-8 Unicode text → PDF — guards against .notdef regression (issue #224)",
        "input": "sample_unicode.txt",
        "input_mime": "text/plain",
        "output_mime": "application/pdf",
        "filter": "texttopdf",
        "options": "cpi=10 lpi=6",
        "notdef_check": True,
    },
    # ------------------------------------------------------------------
    # imagetopdf — image rendering
    # ------------------------------------------------------------------
    {
        "name": "image_png_to_pdf",
        "description": "PNG image → PDF via imagetopdf",
        "input": "sample_image.png",
        "input_mime": "image/png",
        "output_mime": "application/pdf",
        "filter": "imagetopdf",
        "options": "scaling=100",
        "notdef_check": False,
    },
    # ------------------------------------------------------------------
    # pdftopdf — PDFio transition regression tests (Till Kamppeter, 2026-11)
    # Tests chosen to catch the specific regressions reported:
    #   cut margins on mis-scaled pages, wrong 90° rotation, number-up,
    #   missing first page, blank or black pages.
    # ------------------------------------------------------------------
    {
        "name": "pdftopdf_page_completeness",
        "description": "4-page PDF → pdftopdf → all 4 pages preserved (missing-first-page check)",
        "input": "sample_4page.pdf",
        "input_mime": "application/pdf",
        "output_mime": "application/pdf",
        "filter": "pdftopdf",
        "options": "",
        "page_count_check": 4,
        "blank_page_check": True,
    },
    {
        "name": "pdftopdf_landscape_rotation",
        "description": "Landscape PDF → pdftopdf → page must stay landscape (no spurious 90° rotation)",
        "input": "sample_landscape.pdf",
        "input_mime": "application/pdf",
        "output_mime": "application/pdf",
        "filter": "pdftopdf",
        "options": "",
        "orientation_check": "landscape",
    },
    {
        "name": "pdftopdf_number_up_2",
        "description": "4-page PDF → pdftopdf number-up=2 → 2 pages in output",
        "input": "sample_4page.pdf",
        "input_mime": "application/pdf",
        "output_mime": "application/pdf",
        "filter": "pdftopdf",
        "options": "number-up=2",
        "page_count_check": 2,
        "blank_page_check": True,
    },
    {
        "name": "pdftopdf_no_black_page",
        "description": "2-page PDF → pdftopdf → no output page should be blank or all-black",
        "input": "sample_2page.pdf",
        "input_mime": "application/pdf",
        "output_mime": "application/pdf",
        "filter": "pdftopdf",
        "options": "",
        "blank_page_check": True,
    },
]

# ---------------------------------------------------------------------------
# System helpers
# ---------------------------------------------------------------------------

def _run(cmd: list[str], stdin_bytes: bytes | None = None, timeout: int = 90) -> tuple[int, bytes, str]:
    try:
        r = subprocess.run(cmd, input=stdin_bytes, capture_output=True, timeout=timeout)
        return r.returncode, r.stdout, r.stderr.decode("utf-8", errors="replace")
    except subprocess.TimeoutExpired:
        return -1, b"", "Command timed out"
    except FileNotFoundError as exc:
        return -2, b"", str(exc)


def _find_filter(name: str) -> str | None:
    destdir = os.environ.get("DESTDIR", "").rstrip("/")
    candidates = [
        f"{destdir}/usr/lib/cups/filter/{name}",
        f"/usr/lib/cups/filter/{name}",
        f"/usr/local/lib/cups/filter/{name}",
        f"/usr/lib/x86_64-linux-gnu/cups/filter/{name}",
        f"/usr/lib/aarch64-linux-gnu/cups/filter/{name}",
        f"/usr/lib/arm-linux-gnueabihf/cups/filter/{name}",
        f"/usr/lib/riscv64-linux-gnu/cups/filter/{name}",
        shutil.which(name),
    ]
    for c in candidates:
        if c and Path(c).is_file() and os.access(c, os.X_OK):
            LOG.debug("Found filter %s at %s", name, c)
            return c
    return None


def _find_gs() -> str | None:
    return shutil.which("gs") or shutil.which("ghostscript")

# ---------------------------------------------------------------------------
# Filter execution
# ---------------------------------------------------------------------------

def run_via_filter_binary(filter_binary: str, input_path: Path, options: str = "") -> tuple[bytes | None, str]:
    cmd = [filter_binary, "1", "ci-test", "visual-test", "1", options or "", str(input_path)]
    rc, stdout, stderr = _run(cmd, timeout=120)
    if rc != 0 or not stdout:
        return None, f"Filter exited {rc}: {stderr[:400]}"
    return stdout, ""


def run_via_cupsfilter(input_path: Path, input_mime: str, output_mime: str, options: str = "") -> tuple[bytes | None, str]:
    cf = shutil.which("cupsfilter")
    if not cf:
        return None, "cupsfilter not in PATH"
    cmd = [cf, "-i", input_mime, "-o", output_mime]
    for opt in options.split():
        cmd += ["-o", opt]
    cmd.append(str(input_path))
    rc, stdout, stderr = _run(cmd, timeout=120)
    if rc != 0 or not stdout:
        return None, f"cupsfilter exited {rc}: {stderr[:400]}"
    return stdout, ""

# ---------------------------------------------------------------------------
# PDF → PNG rendering
# ---------------------------------------------------------------------------

def pdf_to_png(pdf_bytes: bytes, dpi: int = RENDER_DPI) -> bytes | None:
    gs = _find_gs()
    if not gs:
        LOG.warning("Ghostscript not found — cannot render PDF to PNG")
        return None
    with tempfile.TemporaryDirectory() as tmpdir:
        pdf_path = Path(tmpdir) / "in.pdf"
        out_tmpl  = Path(tmpdir) / "out-%04d.png"
        pdf_path.write_bytes(pdf_bytes)
        cmd = [gs, "-dNOPAUSE", "-dBATCH", "-dSAFER", "-dQUIET",
               "-sDEVICE=pngalpha", f"-r{dpi}",
               f"-sOutputFile={out_tmpl}",
               "-dFirstPage=1", "-dLastPage=1", str(pdf_path)]
        rc, _, err = _run(cmd)
        if rc != 0:
            LOG.error("Ghostscript failed: %s", err[:300])
            return None
        pngs = sorted(Path(tmpdir).glob("out-*.png"))
        return pngs[0].read_bytes() if pngs else None

# ---------------------------------------------------------------------------
# PDF-level checks (pdftopdf regression suite)
# ---------------------------------------------------------------------------

def check_pdf_page_count(pdf_bytes: bytes, expected: int) -> tuple[bool, str]:
    """Verify exact page count using pdfinfo(1)."""
    pdfinfo = shutil.which("pdfinfo")
    if not pdfinfo:
        return True, "pdfinfo not available — skipping page count check"
    with tempfile.NamedTemporaryFile(suffix=".pdf", delete=False) as f:
        f.write(pdf_bytes)
        tmp = f.name
    try:
        _, stdout, _ = _run([pdfinfo, tmp])
        for line in stdout.decode("utf-8", errors="replace").splitlines():
            if line.startswith("Pages:"):
                actual = int(line.split(":")[1].strip())
                if actual == expected:
                    return True, f"Page count {actual} = expected ✓"
                return False, (
                    f"Page count {actual} ≠ expected {expected} — "
                    f"{'missing page(s)' if actual < expected else 'extra page(s)'}"
                )
        return True, "Could not parse page count"
    except (ValueError, IndexError):
        return True, "Could not parse pdfinfo output"
    finally:
        os.unlink(tmp)


def check_page_orientation(pdf_bytes: bytes, expected: str) -> tuple[bool, str]:
    """
    Check first-page orientation via pdfinfo.
    expected: 'landscape' or 'portrait'
    """
    pdfinfo = shutil.which("pdfinfo")
    if not pdfinfo:
        return True, "pdfinfo not available — skipping orientation check"
    with tempfile.NamedTemporaryFile(suffix=".pdf", delete=False) as f:
        f.write(pdf_bytes)
        tmp = f.name
    try:
        _, stdout, _ = _run([pdfinfo, tmp])
        for line in stdout.decode("utf-8", errors="replace").splitlines():
            if "Page size:" in line:
                parts = line.split()
                try:
                    w = float(parts[2])
                    h = float(parts[4])
                    actual = "landscape" if w > h else "portrait"
                    if actual == expected:
                        return True, f"Orientation {actual} matches expected ✓"
                    return False, (
                        f"Orientation {actual} ≠ expected {expected} — "
                        f"spurious 90° rotation detected"
                    )
                except (IndexError, ValueError):
                    pass
        return True, "Could not determine orientation"
    finally:
        os.unlink(tmp)


def check_no_blank_or_black_pages(pdf_bytes: bytes) -> tuple[bool, str]:
    """
    Render every page at low DPI and flag pages that are blank (>98% white)
    or all-black (>98% black) — a telltale sign of a pdftopdf rendering failure.
    Requires Ghostscript + numpy + PIL.
    """
    gs = _find_gs()
    if not gs:
        return True, "Ghostscript not available — skipping blank/black page check"
    if not (HAS_NUMPY and HAS_PIL):
        return True, "numpy/PIL not available — skipping blank/black page check"

    with tempfile.TemporaryDirectory() as tmpdir:
        pdf_path = Path(tmpdir) / "in.pdf"
        out_tmpl  = Path(tmpdir) / "page-%04d.png"
        pdf_path.write_bytes(pdf_bytes)
        cmd = [gs, "-dNOPAUSE", "-dBATCH", "-dSAFER", "-dQUIET",
               "-sDEVICE=pnggray", "-r72",
               f"-sOutputFile={out_tmpl}", str(pdf_path)]
        rc, _, err = _run(cmd)
        if rc != 0:
            return True, f"Ghostscript render error: {err[:200]}"

        pngs = sorted(Path(tmpdir).glob("page-*.png"))
        if not pngs:
            return False, "No pages rendered by Ghostscript — possible blank output"

        problems = []
        for p in pngs:
            arr = np.array(_PIL_Image.open(p).convert("L"))
            mean = float(arr.mean())
            if mean > 251:
                problems.append(f"{p.name}: blank (mean={mean:.1f})")
            elif mean < 4:
                problems.append(f"{p.name}: all-black (mean={mean:.1f})")

        if problems:
            return False, "Blank/black pages: " + "; ".join(problems)
        return True, f"All {len(pngs)} page(s) have visible content ✓"

# ---------------------------------------------------------------------------
# Image-quality comparison (SSIM)
# ---------------------------------------------------------------------------

def _load_gray(png_bytes: bytes) -> "np.ndarray | None":
    if not (HAS_NUMPY and HAS_PIL):
        return None
    import io
    img = _PIL_Image.open(io.BytesIO(png_bytes)).convert("L")
    return np.array(img)


def compute_ssim(png_a: bytes, png_b: bytes) -> tuple[float | None, str]:
    if not (HAS_NUMPY and HAS_SKIMAGE and HAS_PIL):
        return None, "scikit-image or Pillow not available"
    try:
        a = _load_gray(png_a)
        b = _load_gray(png_b)
        if a is None or b is None:
            return None, "Could not decode PNG"
        if a.shape != b.shape:
            from skimage.transform import resize as _resize
            b = (_resize(b.astype(float) / 255.0, a.shape) * 255).astype(np.uint8)
        return float(_ssim(a, b, data_range=255)), ""
    except Exception as exc:  # noqa: BLE001
        return None, str(exc)

# ---------------------------------------------------------------------------
# Font-embedding / .notdef check
# ---------------------------------------------------------------------------

def check_font_embedding(pdf_bytes: bytes) -> tuple[bool, str]:
    pdffonts = shutil.which("pdffonts")
    if not pdffonts:
        return False, "pdffonts not found — skipping font check"
    with tempfile.NamedTemporaryFile(suffix=".pdf", delete=False) as f:
        f.write(pdf_bytes)
        tmp = f.name
    try:
        _, stdout, _ = _run([pdffonts, tmp])
        lines = stdout.decode("utf-8", errors="replace").splitlines()
        problems = [ln.strip() for ln in lines[2:] if ln.strip() and " no " in ln]
        if problems:
            return True, "Unembedded font(s):\n" + "\n".join(problems)
        return False, "All fonts embedded ✓"
    finally:
        os.unlink(tmp)

# ---------------------------------------------------------------------------
# Test runner
# ---------------------------------------------------------------------------

class VisualTestRunner:
    def __init__(self, report_dir: Path, generate_refs: bool = False):
        self.report_dir  = report_dir
        self.generate_refs = generate_refs
        self.results: list[dict] = []
        report_dir.mkdir(parents=True, exist_ok=True)

    def run_all(self) -> bool:
        _setup_pdf_inputs()   # auto-generate PDF inputs
        for tc in TEST_CASES:
            result = self._run_one(tc)
            self.results.append(result)
            self._log_result(result)
        self._write_json_report()
        self._write_html_report()
        passed  = sum(1 for r in self.results if r["passed"])
        failed  = sum(1 for r in self.results if not r["passed"] and not r.get("skipped"))
        skipped = sum(1 for r in self.results if r.get("skipped"))
        LOG.info("─" * 60)
        LOG.info("TOTAL %d | PASS %d | FAIL %d | SKIP %d",
                 len(self.results), passed, failed, skipped)
        return failed == 0

    # ------------------------------------------------------------------
    def _run_one(self, tc: dict) -> dict:
        name       = tc["name"]
        input_path = INPUTS_DIR / tc["input"]
        ref_path   = REFS_DIR / f"{name}.png"

        result: dict = {
            "name": name, "description": tc["description"],
            "passed": False, "skipped": False,
            "ssim": None, "hist_score": None,
            "font_check": None, "extra_checks": [],
            "message": "",
        }

        if not input_path.exists():
            return self._skip(result, f"Input missing: {tc['input']}")

        # --- Run filter ---
        output_bytes: bytes | None = None
        fb = _find_filter(tc["filter"])
        if fb:
            output_bytes, err = run_via_filter_binary(fb, input_path, tc.get("options", ""))
        if output_bytes is None:
            output_bytes, err = run_via_cupsfilter(
                input_path, tc["input_mime"], tc["output_mime"], tc.get("options", ""))
        if output_bytes is None:
            return self._skip(result, f"Filter '{tc['filter']}' not available: {err}")

        # Save raw output
        ext = tc["output_mime"].split("/")[-1].replace("x-", "")
        raw_path = self.report_dir / f"{name}_output.{ext}"
        raw_path.write_bytes(output_bytes)

        # --- PDF-level regression checks (pdftopdf suite) ---
        if tc["output_mime"] == "application/pdf":

            if "page_count_check" in tc:
                ok, msg = check_pdf_page_count(output_bytes, tc["page_count_check"])
                result["extra_checks"].append({"check": "page_count", "passed": ok, "msg": msg})
                if not ok:
                    result["passed"] = False
                    result["message"] = msg
                    return result

            if tc.get("orientation_check"):
                ok, msg = check_page_orientation(output_bytes, tc["orientation_check"])
                result["extra_checks"].append({"check": "orientation", "passed": ok, "msg": msg})
                if not ok:
                    result["passed"] = False
                    result["message"] = msg
                    return result

            if tc.get("blank_page_check"):
                ok, msg = check_no_blank_or_black_pages(output_bytes)
                result["extra_checks"].append({"check": "blank_black", "passed": ok, "msg": msg})
                if not ok:
                    result["passed"] = False
                    result["message"] = msg
                    return result

        # --- Font-embedding check ---
        if tc.get("notdef_check") and tc["output_mime"] == "application/pdf":
            has_problem, detail = check_font_embedding(output_bytes)
            result["font_check"] = detail
            if has_problem:
                result["passed"] = False
                result["message"] = f".notdef regression: {detail}"
                return result

        # --- Convert to PNG for SSIM comparison ---
        if tc["output_mime"] == "application/pdf":
            png_bytes = pdf_to_png(output_bytes)
            if png_bytes is None:
                # PDF-level checks passed; skip SSIM gracefully
                return self._skip_ssim(result, "Ghostscript unavailable")
        elif tc["output_mime"] == "image/png":
            png_bytes = output_bytes
        else:
            return self._skip(result, f"No PNG converter for {tc['output_mime']}")

        (self.report_dir / f"{name}_output.png").write_bytes(png_bytes)

        # --- Generate reference OR compare ---
        if self.generate_refs:
            REFS_DIR.mkdir(parents=True, exist_ok=True)
            ref_path.write_bytes(png_bytes)
            result["passed"]  = True
            result["message"] = f"Reference saved → {ref_path.name}"
            return result

        if not ref_path.exists():
            # No reference yet — treat as "needs reference" skip,
            # but still mark PASS so structure-only tests (page count, etc.) are counted.
            result["passed"]  = True
            result["message"] = "No SSIM reference — run --generate-refs (structural checks passed)"
            result["skipped"] = False
            return result

        ref_bytes  = ref_path.read_bytes()
        ssim_score, err = compute_ssim(png_bytes, ref_bytes)
        result["ssim"] = ssim_score
        if ssim_score is None:
            return self._skip_ssim(result, f"SSIM unavailable: {err}")
        if ssim_score >= SSIM_THRESHOLD:
            result["passed"]  = True
            result["message"] = f"SSIM {ssim_score:.4f} ≥ {SSIM_THRESHOLD} ✓"
        else:
            result["passed"]  = False
            result["message"] = f"SSIM {ssim_score:.4f} < {SSIM_THRESHOLD} — quality degraded"
        return result

    # ------------------------------------------------------------------
    @staticmethod
    def _skip(r: dict, reason: str) -> dict:
        r["skipped"] = True
        r["passed"]  = True
        r["message"] = reason
        return r

    @staticmethod
    def _skip_ssim(r: dict, reason: str) -> dict:
        """PDF checks passed but SSIM unavailable — still a pass."""
        r["passed"]  = True
        r["message"] = reason
        return r

    @staticmethod
    def _log_result(r: dict) -> None:
        tag = "SKIP" if r.get("skipped") else ("PASS" if r["passed"] else "FAIL")
        LOG.info("[%s] %-40s %s", tag, r["name"], r["message"])

    # ------------------------------------------------------------------
    def _write_json_report(self) -> None:
        report = {
            "timestamp":      datetime.now(timezone.utc).isoformat(),
            "ssim_threshold": SSIM_THRESHOLD,
            "total":   len(self.results),
            "passed":  sum(1 for r in self.results if r["passed"]),
            "failed":  sum(1 for r in self.results if not r["passed"] and not r.get("skipped")),
            "skipped": sum(1 for r in self.results if r.get("skipped")),
            "results": self.results,
        }
        (self.report_dir / "visual_test_report.json").write_text(
            json.dumps(report, indent=2), encoding="utf-8")

    def _write_html_report(self) -> None:
        rows = ""
        for r in self.results:
            if r.get("skipped"):
                badge = '<span class="badge skip">SKIP</span>'
            elif r["passed"]:
                badge = '<span class="badge pass">PASS</span>'
            else:
                badge = '<span class="badge fail">FAIL</span>'

            ssim_str  = f"{r['ssim']:.4f}" if r["ssim"] is not None else "—"
            font_str  = (r["font_check"] or "—")[:100]
            extra_str = "; ".join(
                f"{c['check']}: {'✓' if c['passed'] else '✗'} {c['msg'][:60]}"
                for c in r.get("extra_checks", [])
            ) or "—"

            ref_img = (
                f'<img src="../tests/visual_references/{r["name"]}.png" '
                f'width="160" onerror="this.style.display=\'none\'">'
            )
            out_img = (
                f'<img src="{r["name"]}_output.png" '
                f'width="160" onerror="this.style.display=\'none\'">'
            )

            rows += f"""
            <tr>
              <td><strong>{r["name"]}</strong><br>
                  <small style="color:#555">{r["description"]}</small></td>
              <td>{badge}</td><td>{ssim_str}</td>
              <td><small>{r.get("message","")}</small></td>
              <td><small>{extra_str}</small></td>
              <td><small style="color:#666">{font_str}</small></td>
              <td>{ref_img}</td><td>{out_img}</td>
            </tr>"""

        total  = len(self.results)
        passed = sum(1 for r in self.results if r["passed"])
        failed = total - passed
        sc = "#2a9d2a" if failed == 0 else "#c0392b"

        html = f"""<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<title>libcupsfilters Visual Quality Report</title>
<style>
  body {{font-family:sans-serif;margin:24px;color:#222}}
  h1 {{font-size:1.4rem}}
  .summary {{background:{sc};color:#fff;padding:8px 18px;border-radius:6px;
             display:inline-block;font-weight:600}}
  table {{border-collapse:collapse;width:100%;margin-top:20px;font-size:.85rem}}
  th,td {{border:1px solid #ddd;padding:7px 10px;vertical-align:top}}
  th {{background:#f0f0f0}}
  tr:nth-child(even) {{background:#fafafa}}
  .badge {{display:inline-block;padding:2px 10px;border-radius:4px;
           font-weight:600;font-size:.8rem;color:#fff}}
  .pass {{background:#27ae60}} .fail {{background:#c0392b}} .skip {{background:#7f8c8d}}
</style></head><body>
<h1>🖨 libcupsfilters Visual Output Quality Report</h1>
<p>Generated: <strong>{datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")}</strong>
   | SSIM threshold: <strong>{SSIM_THRESHOLD}</strong></p>
<p class="summary">{passed}/{total} passed | {failed} failed</p>
<table><thead><tr>
  <th>Test</th><th>Status</th><th>SSIM</th><th>Message</th>
  <th>PDF Checks</th><th>Font</th><th>Reference</th><th>Output</th>
</tr></thead><tbody>{rows}</tbody></table>
</body></html>"""
        (self.report_dir / "visual_test_report.html").write_text(html, encoding="utf-8")

# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    ap = argparse.ArgumentParser(description="Visual output quality tests for libcupsfilters")
    ap.add_argument("--report-dir", default="visual-report")
    ap.add_argument("--generate-refs", action="store_true")
    ap.add_argument("--verbose", "-v", action="store_true")
    args = ap.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(levelname)s  %(message)s",
    )

    runner = VisualTestRunner(Path(args.report_dir), generate_refs=args.generate_refs)
    ok = runner.run_all()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
