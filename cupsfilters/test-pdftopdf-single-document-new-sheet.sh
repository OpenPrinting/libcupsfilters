#!/usr/bin/env bash
#
# Regression test: with multiple-document-handling=single-document-new-
# sheet and an odd page selection, the fast-copy path (print-scaling=none)
# must schedule the same padding sheet the normal layout path does.
#
# page-ranges=1-3 of a 10-page document selects 3 pages; the layout path
# already padded this to 4 output pages (3 real + 1 blank), but the fast
# path dropped the padding slot because it was never marked as a
# scheduled blank page, so it produced only 3.
#
# Like testfilters.sh, this runs from the top-level build directory.
#
set -euo pipefail

CC="${CC:-cc}"

TESTFILTERS="./testfilters"
if [[ ! -x "${TESTFILTERS}" ]]; then
  echo "testfilters harness not found at ${TESTFILTERS}" >&2
  exit 99
fi

CHECKER_SRC="cupsfilters/test-pdftopdf-single-document-new-sheet.c"
if [[ ! -f "${CHECKER_SRC}" ]]; then
  echo "test file not found: ${CHECKER_SRC}" >&2
  exit 99
fi

PKG_CFLAGS="$(pkg-config --cflags pdfio 2>/dev/null || true)"
PKG_LIBS="$(pkg-config --libs pdfio 2>/dev/null || true)"
if [[ -z "${PKG_LIBS}" ]]; then
  echo "pkg-config cannot find pdfio; skipping." >&2
  exit 77
fi

WORKDIR="$(mktemp -d ./single-document-new-sheet.XXXXXX)"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

CHECKER="${WORKDIR}/check"
"${CC}" -std=gnu11 -O0 ${PKG_CFLAGS} "${CHECKER_SRC}" ${PKG_LIBS} -lm -o "${CHECKER}"

"${CHECKER}" make "${WORKDIR}/input.pdf" 10

EXPECTED=4
CASES="${WORKDIR}/cases.txt"
: > "${CASES}"

for scaling in auto none; do
  printf '%s\tapplication/pdf\t%s\tapplication/pdf\tGeneric\tPDF Color 2\t1\t1\tapplication/pdf\t42\tsingle-document-new-sheet-user\tsingle-document-new-sheet-%s\t1\tmedia-size=letter page-ranges=1-3 print-scaling=%s multiple-document-handling=single-document-new-sheet\tpdftopdf\n' \
    "${WORKDIR}/input.pdf" "${WORKDIR}/${scaling}.out.pdf" "${scaling}" "${scaling}" >> "${CASES}"
done

"${TESTFILTERS}" "${CASES}"

status=0
for scaling in auto none; do
  output="${WORKDIR}/${scaling}.out.pdf"
  if [[ ! -s "${output}" ]]; then
    echo "print-scaling=${scaling}: pdftopdf produced no output" >&2
    status=1
  elif ! "${CHECKER}" check "${output}" "${EXPECTED}"; then
    echo "print-scaling=${scaling}: wrong sheet count" >&2
    status=1
  fi
done

exit "${status}"
