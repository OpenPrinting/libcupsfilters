#!/usr/bin/env bash
#
# Regression test for Issue #175: flattening a PDF form with more than 16
# pages must not read past the page structure in pdftopdf.
#
# flatten_pdf() logged outpage->resmap[pg] with the page number, but
# resmap had only XFORM_MAX_LAYOUT (16) entries and was never set for the
# flattened page.  From page 17 on that read went past the array, from
# page 19 on past the allocated structure.  A normal build only logs a
# wrong pointer; with AddressSanitizer pdftopdf aborts on page 19.
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

CHECKER_SRC="cupsfilters/test-pdftopdf-form-many-pages.c"
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

WORKDIR="$(mktemp -d ./form-many-pages.XXXXXX)"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

CHECKER="${WORKDIR}/check"
"${CC}" -std=gnu11 -O0 ${PKG_CFLAGS} "${CHECKER_SRC}" ${PKG_LIBS} -lm -o "${CHECKER}"

PAGES=20
INPUT="${WORKDIR}/input.pdf"
OUTPUT="${WORKDIR}/output.pdf"
CASES="${WORKDIR}/cases.txt"

"${CHECKER}" make "${INPUT}" "${PAGES}"

printf '%s\tapplication/pdf\t%s\tapplication/pdf\tGeneric\tPDF Color 2\t1\t1\tapplication/pdf\t175\tform-many-pages-user\tform-many-pages\t1\tPageSize=A4\tpdftopdf\n' \
  "${INPUT}" "${OUTPUT}" > "${CASES}"

"${TESTFILTERS}" "${CASES}"

if [[ ! -s "${OUTPUT}" ]]; then
  echo "pdftopdf produced no output" >&2
  exit 1
fi

"${CHECKER}" check "${OUTPUT}" "${PAGES}"
