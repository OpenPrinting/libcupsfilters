#!/usr/bin/env bash
#
# Regression test: pdftopdf must not flip or rotate the back sides of
# duplex sheets.
#
# Back side orientation (cfGetBackSideOrientation()) is applied by the
# rasterizing filters, and only for raster output, as in the former
# QPDF-based pdftopdf.  pdftopdf used to transform even output pages
# itself, and pages with annotations got the transform twice (once while
# flattening the annotations, once when composing the page), so documents
# with and without annotations came out differently.
#
# The input documents are generated here: four Letter pages, with and
# without a link annotation on every page.  Each is run through pdftopdf
# for long-edge and short-edge duplex, and the checker fails if any output
# page applies a "cm" with a negative horizontal or vertical scale.
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

CHECKER_SRC="cupsfilters/test-pdftopdf-duplex-backside.c"
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

WORKDIR="$(mktemp -d ./duplex-backside.XXXXXX)"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

CHECKER="${WORKDIR}/check"
"${CC}" -std=gnu11 -O0 ${PKG_CFLAGS} "${CHECKER_SRC}" ${PKG_LIBS} -lm -o "${CHECKER}"

PAGES=4
CASES="${WORKDIR}/cases.txt"
: > "${CASES}"

for kind in links plain; do
  "${CHECKER}" make "${WORKDIR}/${kind}.pdf" "${PAGES}" "${kind}"
  for sides in two-sided-long-edge two-sided-short-edge; do
    printf '%s\tapplication/pdf\t%s\tapplication/pdf\tGeneric\tPDF Color 2\t1\t1\tapplication/pdf\t42\tduplex-backside-user\t%s-%s\t1\tmedia-size=letter print-scaling=auto sides=%s\tpdftopdf\n' \
      "${WORKDIR}/${kind}.pdf" "${WORKDIR}/${kind}-${sides}.out.pdf" \
      "${kind}" "${sides}" "${sides}" >> "${CASES}"
  done
done

"${TESTFILTERS}" "${CASES}"

status=0
for kind in links plain; do
  for sides in two-sided-long-edge two-sided-short-edge; do
    output="${WORKDIR}/${kind}-${sides}.out.pdf"
    if [[ ! -s "${output}" ]]; then
      echo "${kind}, ${sides}: pdftopdf produced no output" >&2
      status=1
    elif ! "${CHECKER}" check "${output}" "${PAGES}"; then
      echo "${kind}, ${sides}: back side was transformed" >&2
      status=1
    fi
  done
done

exit "${status}"
