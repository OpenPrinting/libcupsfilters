#!/usr/bin/env bash
#
# Regression test: link annotations with no appearance stream must not
# make pdftopdf fail.
#
# Chrome's print PDF gives every hyperlink an annotation and no /AP
# stream.  pdftopdf flattened those by registering a Form XObject under
# a heap-allocated name and then freeing the name.  pdfioDictSetObj()
# keeps that pointer, so the flattened file's /XObject dictionary was
# corrupt, reopening it failed, and the filter exited 1.  A page of
# ordinary content with several such links is enough to reproduce it
# (issue #246).
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
CHECKER_SRC="cupsfilters/test-pdftopdf-no-appearance.c"
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

WORKDIR="$(mktemp -d ./no-appearance.XXXXXX)"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

CASES="${WORKDIR}/cases.txt"
OUTPUT="${WORKDIR}/output.pdf"
CHECKER="${WORKDIR}/check"

printf '%s\tapplication/pdf\t%s\tapplication/pdf\tGeneric\tPDF Color 2\t1\t1\tapplication/pdf\t42\tno-appearance-user\tlink-annots\t1\tmedia-size=letter print-scaling=auto\tpdftopdf\n' \
  "${FIXTURE}" "${OUTPUT}" > "${CASES}"

"${TESTFILTERS}" "${CASES}"

if [[ ! -s "${OUTPUT}" ]]; then
  echo "pdftopdf produced no output" >&2
  exit 1
fi

"${CC}" -std=gnu11 -O0 ${PKG_CFLAGS} "${CHECKER_SRC}" ${PKG_LIBS} -lm -o "${CHECKER}"

"${CHECKER}" "${OUTPUT}"
