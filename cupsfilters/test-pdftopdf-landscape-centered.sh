#!/usr/bin/env bash
#
# Regression test: a landscape page that pdftopdf turns onto portrait paper
# must be centered in the printable area.
#
# The same US Letter page is made once in portrait (612x792, placed without
# turning) and once in landscape (792x612, turned by 90 degrees).  Both are
# scaled to fit the same cell on A4, so both end up the same size, and both
# must land on the same rectangle.  The turned page used to be moved by the
# whole vertical slack (the placement added half of the slack to the top of
# the cell instead of subtracting it).
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

CHECKER_SRC="cupsfilters/test-pdftopdf-landscape-centered.c"
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

WORKDIR="$(mktemp -d ./landscape-centered.XXXXXX)"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

CHECKER="${WORKDIR}/check"
"${CC}" -std=gnu11 -O0 ${PKG_CFLAGS} "${CHECKER_SRC}" ${PKG_LIBS} -lm -o "${CHECKER}"

"${CHECKER}" make "${WORKDIR}/portrait.pdf" 612 792
"${CHECKER}" make "${WORKDIR}/landscape.pdf" 792 612

CASES="${WORKDIR}/cases.txt"
: > "${CASES}"
for kind in portrait landscape; do
  printf '%s\tapplication/pdf\t%s\tapplication/pdf\tGeneric\tPDF Color 2\t1\t1\tapplication/pdf\t42\tlandscape-centered-user\t%s\t1\tPageSize=A4 print-scaling=fit\tpdftopdf\n' \
    "${WORKDIR}/${kind}.pdf" "${WORKDIR}/${kind}.out.pdf" "${kind}" >> "${CASES}"
done

"${TESTFILTERS}" "${CASES}"

for kind in portrait landscape; do
  if [[ ! -s "${WORKDIR}/${kind}.out.pdf" ]]; then
    echo "${kind}: pdftopdf produced no output" >&2
    exit 1
  fi
done

portrait="$("${CHECKER}" box "${WORKDIR}/portrait.out.pdf" 612 792)"
landscape="$("${CHECKER}" box "${WORKDIR}/landscape.out.pdf" 792 612)"

echo "portrait page placed on:  ${portrait}"
echo "landscape page placed on: ${landscape}"

# The two rectangles must match to within half a point.
awk -v p="${portrait}" -v l="${landscape}" 'BEGIN {
  split(p, a, " "); split(l, b, " ");
  for (i = 1; i <= 4; i ++) {
    d = a[i] - b[i]; if (d < 0) d = -d;
    if (d > 0.5) { print "landscape page is not placed like the portrait page" > "/dev/stderr"; exit 1 }
  }
}'
