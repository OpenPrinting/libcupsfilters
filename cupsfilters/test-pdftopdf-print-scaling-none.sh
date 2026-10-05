#!/usr/bin/env bash
#
# Regression test: print-scaling=none must be honored for pages that have
# annotations.
#
# A page with no annotations is copied untouched when print-scaling=none.
# A page with a single link annotation takes the full layout path instead,
# and that path treated "none" like "fit", so the page was shrunk to the
# printable area (issue #258).
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

FIXTURE="cupsfilters/test_files/link-annots-no-appearance.pdf"
CHECKER_SRC="cupsfilters/test-pdftopdf-print-scaling-none.c"
for f in "${FIXTURE}" "${CHECKER_SRC}"; do
  if [[ ! -f "${f}" ]]; then
    echo "test file not found: ${f}" >&2
    exit 99
  fi
done

PKG_CFLAGS="$(pkg-config --cflags pdfio 2>/dev/null || true)"
PKG_LIBS="$(pkg-config --libs pdfio 2>/dev/null || true)"
if [[ -z "${PKG_LIBS}" ]]; then
  echo "pkg-config cannot find pdfio; skipping." >&2
  exit 77
fi

WORKDIR="$(mktemp -d ./print-scaling-none.XXXXXX)"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

CASES="${WORKDIR}/cases.txt"
OUTPUT="${WORKDIR}/output.pdf"
CHECKER="${WORKDIR}/check"

printf '%s\tapplication/pdf\t%s\tapplication/pdf\tGeneric\tPDF Color 2\t1\t1\tapplication/pdf\t42\tprint-scaling-user\tlink-annots\t1\tmedia-size=letter print-scaling=none\tpdftopdf\n' \
  "${FIXTURE}" "${OUTPUT}" > "${CASES}"

"${TESTFILTERS}" "${CASES}"

if [[ ! -s "${OUTPUT}" ]]; then
  echo "pdftopdf produced no output" >&2
  exit 1
fi

"${CC}" -std=gnu11 -O0 ${PKG_CFLAGS} "${CHECKER_SRC}" ${PKG_LIBS} -lm -o "${CHECKER}"

"${CHECKER}" "${OUTPUT}"
