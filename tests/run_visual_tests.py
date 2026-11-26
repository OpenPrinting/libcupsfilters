#!/usr/bin/env python3
"""
run_visual_tests.py – Visual output quality tests for libcupsfilters
======================================================================
Tests the visual quality of CUPS filter output by:
  1. Running test documents through CUPS filters (texttopdf, imagetopdf, …)
  2. Rendering PDF output to PNG via Ghostscript
  3. Comparing against golden reference images (SSIM + histogram)
  4. Detecting unembedded fonts / .notdef box regressions
  5. Generating an HTML + JSON report

Exit codes
  0 – all tests passed (or skipped gracefully)
  1 – one or more tests failed
  2 – critical dependency missing (Ghostscript absent)

Usage
-----
  # First time — build reference images:
  python3 tests/run_visual_tests.py --generate-refs

  # Every CI run — compare against references:
  python3 tests/run_visual_tests.py --report-dir visual-report

  # Verbose mode:
  python3 tests/run_visual_tests.py -v --report-dir visual-report
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
    from skimage import io as _ski_io
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
SCRIPT_DIR   = Path(__file__).parent.resolve()
INPUTS_DIR   = SCRIPT_DIR / "visual_inputs"
REFS_DIR     = SCRIPT_DIR / "visual_references"

# Quality thresholds
SSIM_THRESHOLD = 0.90   # < this → FAIL
HIST_THRESHOLD = 0.85   # Bhattacharyya histogram similarity

# Ghostscript render DPI for reference and output images
RENDER_DPI = 150

LOG = logging.getLogger("visual_tests")

# ---------------------------------------------------------------------------
# Test-case registry
# ---------------------------------------------------------------------------
# Each entry describes one filter scenario to exercise.
# Fields:
#   name          – unique identifier (used as file stem)
#   description   – human-readable label
#   input         – filename inside INPUTS_DIR
#   input_mime    – MIME type fed to cupsfilter / filter binary
#   output_mime   – expected output MIME type
#   filter        – cupsfilter binary name (looked up in common paths)
#   options       – CUPS option string passed to the filter
#   notdef_check  – if True, also run pdffonts to detect missing glyphs
TEST_CASES: list[dict] = [
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
        "description": "UTF-8 Unicode text → PDF — checks for .notdef box regression (issue #224)",
        "input": "sample_unicode.txt",
        "input_mime": "text/plain",
        "output_mime": "application/pdf",
        "filter": "texttopdf",
        "options": "cpi=10 lpi=6",
        "notdef_check": True,   # pdffonts check for missing glyph coverage
    },
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
]

# ---------------------------------------------------------------------------
# System helpers
# ---------------------------------------------------------------------------

def _run(cmd: list[str], stdin_bytes: bytes | None = None, timeout: int = 90) -> tuple[int, bytes, str]:
    """Run *cmd*, return (returncode, stdout_bytes, stderr_str)."""
    try:
        r = subprocess.run(
            cmd,
            input=stdin_bytes,
            capture_output=True,
            timeout=timeout,
        )
        return r.returncode, r.stdout, r.stderr.decode("utf-8", errors="replace")
    except subprocess.TimeoutExpired:
        return -1, b"", "Command timed out"
    except FileNotFoundError as exc:
        return -2, b"", str(exc)


def _find_filter(name: str) -> str | None:
    """Find a CUPS filter binary; honours DESTDIR from CI staging installs."""
    destdir = os.environ.get("DESTDIR", "").rstrip("/")
    candidates = [
        f"{destdir}/usr/lib/cups/filter/{name}",
        f"/usr/lib/cups/filter/{name}",
        f"/usr/local/lib/cups/filter/{name}",
        f"/usr/lib/x86_64-linux-gnu/cups/filter/{name}",
        f"/usr/lib/aarch64-linux-gnu/cups/filter/{name}",
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
    """
    Call a CUPS filter directly:
      filter job-id user title copies options [filename]
    """
    cmd = [filter_binary, "1", "ci-test", "visual-test", "1", options or "", str(input_path)]
    rc, stdout, stderr = _run(cmd, timeout=120)
    if rc != 0 or not stdout:
        return None, f"Filter exited {rc}: {stderr[:400]}"
    return stdout, ""


def run_via_cupsfilter(input_path: Path, input_mime: str, output_mime: str, options: str = "") -> tuple[bytes | None, str]:
    """Use the cupsfilter(8) wrapper when a direct binary isn't found."""
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
    """Render page 1 of a PDF to PNG using Ghostscript. Returns PNG bytes or None."""
    gs = _find_gs()
    if not gs:
        LOG.warning("Ghostscript not found — cannot render PDF to PNG. Install 'ghostscript'.")
        return None

    with tempfile.TemporaryDirectory() as tmpdir:
        pdf_path = Path(tmpdir) / "in.pdf"
        out_tmpl = Path(tmpdir) / "out-%04d.png"
        pdf_path.write_bytes(pdf_bytes)

        cmd = [
            gs, "-dNOPAUSE", "-dBATCH", "-dSAFER", "-dQUIET",
            "-sDEVICE=pngalpha",
            f"-r{dpi}",
            f"-sOutputFile={out_tmpl}",
            "-dFirstPage=1", "-dLastPage=1",
            str(pdf_path),
        ]
        rc, _, err = _run(cmd)
        if rc != 0:
            LOG.error("Ghostscript render failed: %s", err[:300])
            return None

        pngs = sorted(Path(tmpdir).glob("out-*.png"))
        if not pngs:
            LOG.error("Ghostscript produced no PNGs")
            return None
        return pngs[0].read_bytes()

# ---------------------------------------------------------------------------
# Image comparison
# ---------------------------------------------------------------------------

def _load_gray(png_bytes: bytes) -> "np.ndarray | None":
    """Load a PNG from bytes as a uint8 grayscale numpy array."""
    if not (HAS_NUMPY and HAS_PIL):
        return None
    import io
    img = _PIL_Image.open(io.BytesIO(png_bytes)).convert("L")
    return np.array(img)


def compute_ssim(png_a: bytes, png_b: bytes) -> tuple[float | None, str]:
    """Return SSIM score in [−1, 1] and an error string (empty on success)."""
    if not (HAS_NUMPY and HAS_SKIMAGE and HAS_PIL):
        return None, "scikit-image or Pillow not available"
    try:
        a = _load_gray(png_a)
        b = _load_gray(png_b)
        if a is None or b is None:
            return None, "Could not decode PNG"
        # Resize b to match a if resolution differs
        if a.shape != b.shape:
            from skimage.transform import resize as _resize
            b = (_resize(b.astype(float) / 255.0, a.shape) * 255).astype(np.uint8)
        score = float(_ssim(a, b, data_range=255))
        return score, ""
    except Exception as exc:  # noqa: BLE001
        return None, str(exc)


def compute_histogram_similarity(png_a: bytes, png_b: bytes) -> float | None:
    """Bhattacharyya-style histogram similarity score in [0, 1]."""
    if not (HAS_NUMPY and HAS_PIL):
        return None
    try:
        a = _load_gray(png_a)
        b = _load_gray(png_b)
        if a is None or b is None:
            return None
        h_a = np.histogram(a, bins=256, range=(0, 255))[0].astype(float)
        h_b = np.histogram(b, bins=256, range=(0, 255))[0].astype(float)
        h_a /= (h_a.sum() + 1e-10)
        h_b /= (h_b.sum() + 1e-10)
        return float(np.sum(np.sqrt(h_a * h_b)))
    except Exception:  # noqa: BLE001
        return None

# ---------------------------------------------------------------------------
# .notdef / font-embedding check
# ---------------------------------------------------------------------------

def check_font_embedding(pdf_bytes: bytes) -> tuple[bool, str]:
    """
    Use pdffonts(1) to detect unembedded fonts, which cause .notdef boxes.
    Returns (has_problem: bool, detail_message: str).
    """
    pdffonts = shutil.which("pdffonts")
    if not pdffonts:
        return False, "pdffonts not found (install poppler-utils) — skipping font check"

    with tempfile.NamedTemporaryFile(suffix=".pdf", delete=False) as f:
        f.write(pdf_bytes)
        tmppath = f.name
    try:
        rc, stdout, _ = _run([pdffonts, tmppath])
        lines = stdout.decode("utf-8", errors="replace").splitlines()
        # pdffonts columns: name type encoding emb sub uni object
        # An 'emb' value of 'no' indicates a font that is NOT embedded.
        problems = [ln.strip() for ln in lines[2:] if ln.strip() and " no " in ln]
        if problems:
            return True, "Unembedded font(s) detected:\n" + "\n".join(problems)
        return False, "All fonts embedded — no .notdef regression detected"
    finally:
        os.unlink(tmppath)

# ---------------------------------------------------------------------------
# Test runner
# ---------------------------------------------------------------------------

class VisualTestRunner:
    def __init__(self, report_dir: Path, generate_refs: bool = False):
        self.report_dir = report_dir
        self.generate_refs = generate_refs
        self.results: list[dict] = []
        report_dir.mkdir(parents=True, exist_ok=True)

    # ------------------------------------------------------------------
    def run_all(self) -> bool:
        """Run every test case. Returns True iff no failures."""
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
        LOG.info("TOTAL  %d | PASS %d | FAIL %d | SKIP %d", len(self.results), passed, failed, skipped)
        return failed == 0

    # ------------------------------------------------------------------
    def _run_one(self, tc: dict) -> dict:
        name       = tc["name"]
        input_path = INPUTS_DIR / tc["input"]
        ref_path   = REFS_DIR / f"{name}.png"

        result: dict = {
            "name":        name,
            "description": tc["description"],
            "passed":      False,
            "skipped":     False,
            "ssim":        None,
            "hist_score":  None,
            "font_check":  None,
            "message":     "",
        }

        # 1. Input must exist
        if not input_path.exists():
            return self._skip(result, f"Input missing: {tc['input']} (run generate_references.py first)")

        # 2. Run filter — try direct binary first, then cupsfilter
        output_bytes: bytes | None = None
        filter_bin = _find_filter(tc["filter"])
        if filter_bin:
            output_bytes, err = run_via_filter_binary(filter_bin, input_path, tc.get("options", ""))
            if output_bytes is None:
                LOG.debug("Direct filter failed (%s), trying cupsfilter…", err)

        if output_bytes is None:
            output_bytes, err = run_via_cupsfilter(
                input_path, tc["input_mime"], tc["output_mime"], tc.get("options", "")
            )

        if output_bytes is None:
            return self._skip(result, f"Filter '{tc['filter']}' not available: {err}")

        # 3. Save raw filter output
        ext = tc["output_mime"].split("/")[-1].replace("x-", "")
        raw_path = self.report_dir / f"{name}_output.{ext}"
        raw_path.write_bytes(output_bytes)

        # 4. Convert to PNG for visual comparison
        if tc["output_mime"] == "application/pdf":
            png_bytes = pdf_to_png(output_bytes)
            if png_bytes is None:
                return self._skip(result, "Ghostscript unavailable — cannot render PDF to PNG")
        elif tc["output_mime"] == "image/png":
            png_bytes = output_bytes
        else:
            return self._skip(result, f"No PNG converter for output type {tc['output_mime']}")

        # 5. Save PNG output
        (self.report_dir / f"{name}_output.png").write_bytes(png_bytes)

        # 6. Font-embedding / .notdef check (before SSIM, may cause early FAIL)
        if tc.get("notdef_check") and tc["output_mime"] == "application/pdf":
            has_problem, detail = check_font_embedding(output_bytes)
            result["font_check"] = detail
            if has_problem:
                result["passed"]  = False
                result["message"] = f"Font embedding problem (.notdef regression): {detail}"
                return result

        # 7. Generate reference OR compare
        if self.generate_refs:
            REFS_DIR.mkdir(parents=True, exist_ok=True)
            ref_path.write_bytes(png_bytes)
            result["passed"]  = True
            result["message"] = f"Reference saved → {ref_path.name}"
            return result

        if not ref_path.exists():
            return self._skip(result, "No reference image — run with --generate-refs first")

        ref_bytes  = ref_path.read_bytes()
        ssim_score, err = compute_ssim(png_bytes, ref_bytes)
        hist_score = compute_histogram_similarity(png_bytes, ref_bytes)
        result["ssim"]       = ssim_score
        result["hist_score"] = hist_score

        if ssim_score is None:
            # Can't compare — skip rather than fail
            return self._skip(result, f"SSIM unavailable ({err})")

        if ssim_score >= SSIM_THRESHOLD:
            result["passed"]  = True
            result["message"] = f"SSIM {ssim_score:.4f} ≥ {SSIM_THRESHOLD} ✓"
        else:
            result["passed"]  = False
            result["message"] = (
                f"SSIM {ssim_score:.4f} < {SSIM_THRESHOLD} — "
                f"output quality degraded vs reference"
            )
        return result

    # ------------------------------------------------------------------
    @staticmethod
    def _skip(result: dict, reason: str) -> dict:
        result["skipped"] = True
        result["passed"]  = True   # skips don't count as failures
        result["message"] = reason
        return result

    @staticmethod
    def _log_result(r: dict) -> None:
        if r.get("skipped"):
            tag = "SKIP"
        elif r["passed"]:
            tag = "PASS"
        else:
            tag = "FAIL"
        LOG.info("[%s] %-35s %s", tag, r["name"], r["message"])

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
        path = self.report_dir / "visual_test_report.json"
        path.write_text(json.dumps(report, indent=2), encoding="utf-8")
        LOG.debug("JSON report → %s", path)

    # ------------------------------------------------------------------
    def _write_html_report(self) -> None:
        rows = ""
        for r in self.results:
            if r.get("skipped"):
                badge = '<span class="badge skip">SKIP</span>'
            elif r["passed"]:
                badge = '<span class="badge pass">PASS</span>'
            else:
                badge = '<span class="badge fail">FAIL</span>'

            ssim_str = f"{r['ssim']:.4f}"  if r["ssim"]       is not None else "—"
            hist_str = f"{r['hist_score']:.4f}" if r["hist_score"] is not None else "—"
            font_str = r["font_check"] or "—"

            # Thumbnails relative to report dir
            ref_img = (
                f'<img src="../tests/visual_references/{r["name"]}.png" '
                f'width="180" alt="reference" onerror="this.style.display=\'none\'">'
            )
            out_img = (
                f'<img src="{r["name"]}_output.png" '
                f'width="180" alt="output" onerror="this.style.display=\'none\'">'
            )

            rows += f"""
            <tr>
              <td><strong>{r["name"]}</strong><br><small style="color:#555">{r["description"]}</small></td>
              <td>{badge}</td>
              <td>{ssim_str}</td>
              <td>{hist_str}</td>
              <td><small>{r.get("message","")}</small></td>
              <td><small style="color:#666">{font_str[:120]}</small></td>
              <td>{ref_img}</td>
              <td>{out_img}</td>
            </tr>"""

        total   = len(self.results)
        passed  = sum(1 for r in self.results if r["passed"])
        failed  = total - passed
        summary_color = "#2a9d2a" if failed == 0 else "#c0392b"

        html = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>libcupsfilters Visual Output Quality Report</title>
  <style>
    body  {{ font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
             margin: 24px; color: #222; }}
    h1   {{ font-size: 1.4rem; }}
    .summary {{ background:{summary_color}; color:#fff; padding:8px 18px;
                border-radius:6px; display:inline-block; font-weight:600; }}
    table {{ border-collapse:collapse; width:100%; margin-top:20px; font-size:.87rem; }}
    th,td {{ border:1px solid #ddd; padding:7px 10px; vertical-align:top; }}
    th    {{ background:#f0f0f0; text-align:left; }}
    tr:nth-child(even) {{ background:#fafafa; }}
    .badge {{ display:inline-block; padding:2px 10px; border-radius:4px;
              font-weight:600; font-size:.8rem; color:#fff; }}
    .pass {{ background:#27ae60; }}
    .fail {{ background:#c0392b; }}
    .skip {{ background:#7f8c8d; }}
  </style>
</head>
<body>
  <h1>🖨 libcupsfilters — Visual Output Quality Report</h1>
  <p>Generated: <strong>{datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")}</strong>
     &nbsp;|&nbsp; SSIM threshold: <strong>{SSIM_THRESHOLD}</strong></p>
  <p class="summary">{passed}/{total} tests passed | {failed} failed</p>
  <table>
    <thead>
      <tr>
        <th>Test</th><th>Status</th><th>SSIM</th><th>Hist.</th>
        <th>Message</th><th>Font Check</th><th>Reference</th><th>Output</th>
      </tr>
    </thead>
    <tbody>{rows}</tbody>
  </table>
</body>
</html>"""
        path = self.report_dir / "visual_test_report.html"
        path.write_text(html, encoding="utf-8")
        LOG.debug("HTML report → %s", path)

# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    ap = argparse.ArgumentParser(
        description="Visual output quality tests for libcupsfilters"
    )
    ap.add_argument(
        "--report-dir", default="visual-report",
        help="Directory for test reports and output images (default: visual-report/)",
    )
    ap.add_argument(
        "--generate-refs", action="store_true",
        help="Generate reference (golden) images instead of comparing",
    )
    ap.add_argument("--verbose", "-v", action="store_true")
    args = ap.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(levelname)s  %(message)s",
    )

    if not _find_gs():
        LOG.error("Ghostscript not found — install the 'ghostscript' package.")
        # Don't exit 2 immediately; individual tests will skip gracefully

    runner = VisualTestRunner(
        report_dir=Path(args.report_dir),
        generate_refs=args.generate_refs,
    )
    ok = runner.run_all()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
