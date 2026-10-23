#!/usr/bin/env bash
#
# Test for cfFilterGhostscriptOCR(): Turn a raster-only PDF into a
# searchable PDF and check the text layer, the error handling without
# language data, the fallback to raster-only PDF, and that
# cfFilterGhostscript() still produces plain raster PDFs.
#
# Skipped (exit 77) without Ghostscript's pdfocr8 device, the English
# Tesseract data, or poppler-utils.
#

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ROOT="$(cd "${ROOT}/.." && pwd)"
LIBTOOL="${BUILD_ROOT}/libtool"
CC="${CC:-cc}"
GS="${GS:-gs}"

skip() {
  echo "$*; skipping." >&2
  exit 77
}

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

for tool in "${GS}" pdftotext pdfinfo; do
  command -v "${tool}" >/dev/null 2>&1 || skip "${tool} not found"
done

"${GS}" -h 2>&1 | tr ' ' '\n' | grep -x pdfocr8 >/dev/null ||
  skip "Ghostscript has no pdfocr8 device (built without Tesseract)"

# Find the English Tesseract language data
TESSDATA=""
for dir in "${TESSDATA_PREFIX:-}" \
           /usr/share/tesseract-ocr/5/tessdata \
           /usr/share/tesseract-ocr/4.00/tessdata \
           /usr/share/tesseract/tessdata \
           /usr/share/tessdata \
           /usr/local/share/tesseract-ocr/5/tessdata \
           /usr/local/share/tessdata \
           /opt/homebrew/share/tessdata \
           /opt/local/share/tessdata; do
  if [[ -n "${dir}" && -r "${dir}/eng.traineddata" ]]; then
    TESSDATA="${dir}"
    break
  fi
done
[[ -n "${TESSDATA}" ]] || skip "Tesseract language data eng.traineddata not found"

[[ -x "${LIBTOOL}" ]] || { echo "libtool helper not found at ${LIBTOOL}" >&2; exit 99; }

WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/ghostscript-ocr.XXXXXX")"
cleanup() {
  rm -rf "${WORKDIR}"
}
trap cleanup EXIT

HARNESS_SRC="${ROOT}/test-ghostscript-ocr.c"
HARNESS_OBJ="${WORKDIR}/test-ghostscript-ocr.lo"
HARNESS_BIN="${WORKDIR}/test-ghostscript-ocr"

"${LIBTOOL}" --quiet --mode=compile --tag=CC "${CC}" -std=c11 -O0 -g \
  -I"${BUILD_ROOT}" -I"${BUILD_ROOT}/cupsfilters" $(pkg-config --cflags cups 2>/dev/null) \
  -c "${HARNESS_SRC}" -o "${HARNESS_OBJ}"
"${LIBTOOL}" --quiet --mode=link --tag=CC "${CC}" "${HARNESS_OBJ}" \
  "${BUILD_ROOT}/libcupsfilters.la" $(pkg-config --libs cups 2>/dev/null || echo -lcups) \
  -o "${HARNESS_BIN}"

run_harness() {
  "${LIBTOOL}" --quiet --mode=execute "${HARNESS_BIN}" "$@"
}

# Two-page "scan": text rendered into a raster-only PDF
INPUT="${WORKDIR}/scan.pdf"
cat > "${WORKDIR}/scan.ps" <<'EOF'
%!PS
/Helvetica-Bold findfont 36 scalefont setfont
72 650 moveto (OPENPRINTING MAKES) show
72 590 moveto (SCANS SEARCHABLE) show
showpage
/Helvetica-Bold findfont 36 scalefont setfont
72 650 moveto (SECOND PAGE WITH) show
72 590 moveto (TESSERACT TEXT LAYER) show
showpage
EOF
"${GS}" -q -dSAFER -dNOPAUSE -dBATCH -sDEVICE=pdfimage8 -r300 \
  -sOutputFile="${INPUT}" "${WORKDIR}/scan.ps"
[[ -s "${INPUT}" ]] || fail "could not create test input"
if pdftotext "${INPUT}" - | grep OPENPRINTING >/dev/null; then
  fail "test input unexpectedly contains extractable text"
fi

EMPTY_TESSDATA="${WORKDIR}/no-tessdata"
mkdir -p "${EMPTY_TESSDATA}"

check_pdf() {
  [[ -s "$1" ]] || fail "$1 is empty"
  local pages
  pages="$(pdfinfo "$1" 2>/dev/null | sed -n 's/^Pages: *//p')"
  [[ "${pages}" == "$2" ]] || fail "$1 has ${pages:-no} pages, expected $2"
}

check_text() {
  pdftotext -f 1 -l 1 "$1" - | grep "OPENPRINTING" >/dev/null ||
    fail "$1: text of page 1 not found"
  pdftotext -f 2 -l 2 "$1" - | grep "TESSERACT" >/dev/null ||
    fail "$1: text of page 2 not found"
}

check_no_text() {
  if pdftotext "$1" - | grep -E "OPENPRINTING|TESSERACT" >/dev/null; then
    fail "$1 unexpectedly contains a text layer"
  fi
}

echo "1. Availability check"
dir="$(run_harness available "${INPUT}" /dev/null "${TESSDATA}" 2>"${WORKDIR}/log1")" ||
  { cat "${WORKDIR}/log1" >&2; fail "cfFilterGhostscriptOCRAvailable() returned 0"; }
[[ "${dir}" == "${TESSDATA}" ]] || fail "unexpected tessdata directory \"${dir}\""
if run_harness available "${INPUT}" /dev/null "${EMPTY_TESSDATA}" 2>/dev/null; then
  fail "cfFilterGhostscriptOCRAvailable() returned 1 without language data"
fi
TESSDATA_PREFIX="${TESSDATA}" run_harness available "${INPUT}" /dev/null - >/dev/null 2>&1 ||
  fail "cfFilterGhostscriptOCRAvailable() did not honor TESSDATA_PREFIX"

echo "2. Searchable PDF from seekable input (explicit parameters)"
OUT="${WORKDIR}/ocr-seekable.pdf"
run_harness ocr "${INPUT}" "${OUT}" "${TESSDATA}" 1 1 2>"${WORKDIR}/log2" ||
  { cat "${WORKDIR}/log2" >&2; fail "cfFilterGhostscriptOCR() failed"; }
check_pdf "${OUT}" 2
check_text "${OUT}"

echo "3. Searchable PDF from non-seekable input (pipe)"
OUT="${WORKDIR}/ocr-pipe.pdf"
run_harness ocr "${INPUT}" "${OUT}" "${TESSDATA}" 1 0 2>"${WORKDIR}/log3" ||
  { cat "${WORKDIR}/log3" >&2; fail "cfFilterGhostscriptOCR() failed on pipe input"; }
check_pdf "${OUT}" 2
check_text "${OUT}"

echo "4. Settings via job options"
OUT="${WORKDIR}/ocr-options.pdf"
run_harness options "${INPUT}" "${OUT}" "${TESSDATA}" 1 1 2>"${WORKDIR}/log4" ||
  { cat "${WORKDIR}/log4" >&2; fail "cfFilterGhostscriptOCR() with job options failed"; }
check_pdf "${OUT}" 2
check_text "${OUT}"

echo "5. OCR output selected via final_content_type in cfFilterGhostscript()"
OUT="${WORKDIR}/ocr-content-type.pdf"
run_harness content-type "${INPUT}" "${OUT}" "${TESSDATA}" 1 1 2>"${WORKDIR}/log5" ||
  { cat "${WORKDIR}/log5" >&2; fail "cfFilterGhostscript() with OCR content type failed"; }
check_pdf "${OUT}" 2
check_text "${OUT}"

echo "6. Missing language data, fallback disabled: must fail cleanly"
OUT="${WORKDIR}/ocr-nofallback.pdf"
if run_harness ocr "${INPUT}" "${OUT}" "${EMPTY_TESSDATA}" 0 1 2>"${WORKDIR}/log6"; then
  fail "cfFilterGhostscriptOCR() succeeded without language data"
fi
grep -q "language data" "${WORKDIR}/log6" || fail "no clear error message about missing language data"
[[ ! -s "${OUT}" ]] || fail "output written although the job failed"

echo "7. Missing language data, fallback enabled: raster-only PDF"
OUT="${WORKDIR}/ocr-fallback.pdf"
run_harness ocr "${INPUT}" "${OUT}" "${EMPTY_TESSDATA}" 1 1 2>"${WORKDIR}/log7" ||
  { cat "${WORKDIR}/log7" >&2; fail "fallback to raster-only PDF failed"; }
grep -q "falling back" "${WORKDIR}/log7" || fail "fallback not logged"
check_pdf "${OUT}" 2
check_no_text "${OUT}"

echo "8. Regression: cfFilterGhostscript() raster-only PDF output"
OUT="${WORKDIR}/image.pdf"
run_harness image "${INPUT}" "${OUT}" - 1 1 2>"${WORKDIR}/log8" ||
  { cat "${WORKDIR}/log8" >&2; fail "cfFilterGhostscript() PDF_IMAGE failed"; }
check_pdf "${OUT}" 2
check_no_text "${OUT}"

echo "9. cfFilterUniversal(): application/pdf -> application/vnd.cups-pdf-ocr"
OUT="${WORKDIR}/universal.pdf"
run_harness universal "${INPUT}" "${OUT}" "${TESSDATA}" 1 1 2>"${WORKDIR}/log9" ||
  { cat "${WORKDIR}/log9" >&2; fail "cfFilterUniversal() to searchable PDF failed"; }
check_pdf "${OUT}" 2
check_text "${OUT}"

echo "10. Second language (German)"
if [[ -r "${TESSDATA}/deu.traineddata" ]]; then
  # Umlauts and sharp s are only recognized with the German data
  cat > "${WORKDIR}/deu.ps" <<'EOF'
%!PS
/Helvetica-Bold findfont dup length dict copy dup /Encoding ISOLatin1Encoding put
/HB exch definefont pop
/HB findfont 36 scalefont setfont
72 650 moveto (GR\334SSE AUS M\334NCHEN) show
72 590 moveto (Stra\337e und Gr\374\337e) show
showpage
EOF
  DEU_INPUT="${WORKDIR}/deu.pdf"
  "${GS}" -q -dSAFER -dNOPAUSE -dBATCH -sDEVICE=pdfimage8 -r300 \
    -sOutputFile="${DEU_INPUT}" "${WORKDIR}/deu.ps"
  OUT="${WORKDIR}/ocr-deu.pdf"
  OCR_LANGUAGE=deu run_harness options "${DEU_INPUT}" "${OUT}" "${TESSDATA}" 1 1 \
    2>"${WORKDIR}/log10" ||
    { cat "${WORKDIR}/log10" >&2; fail "cfFilterGhostscriptOCR() with ocr-language=deu failed"; }
  check_pdf "${OUT}" 1
  pdftotext "${OUT}" - | grep "Grüße" >/dev/null ||
    fail "${OUT}: German text with umlauts not found"
else
  echo "   deu.traineddata not installed, skipping this part"
fi

echo "PASS: cfFilterGhostscriptOCR()"
exit 0
