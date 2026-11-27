#!/usr/bin/env bash
#
# Regression test: with print-scaling=none and a requested page size, a
# landscape page must be turned onto the portrait media and centered at its
# original size, like the same page in portrait orientation (Issues #261,
# #274).
#
# pdftopdf used to copy pages with print-scaling=none unchanged, so the
# landscape page went out as a landscape sheet and the rasterizer, which
# works on the requested portrait media, shrank it into the top or bottom
# part of the sheet.
#
# The same US Letter page is made once in portrait (612x792) and once in
# landscape (792x612) and printed on A4 with print-scaling=none.  Both must
# land on the same rectangle, unscaled (612 points wide), on an A4 sheet.
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

# The checker of the landscape-centered test measures where the input page
# got placed, reuse it...
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

WORKDIR="$(mktemp -d ./print-scaling-none-landscape.XXXXXX)"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

CHECKER="${WORKDIR}/check"
"${CC}" -std=gnu11 -O0 ${PKG_CFLAGS} "${CHECKER_SRC}" ${PKG_LIBS} -lm -o "${CHECKER}"

"${CHECKER}" make "${WORKDIR}/portrait.pdf" 612 792
"${CHECKER}" make "${WORKDIR}/landscape.pdf" 792 612

CASES="${WORKDIR}/cases.txt"
: > "${CASES}"
for kind in portrait landscape; do
  printf '%s\tapplication/pdf\t%s\tapplication/pdf\tGeneric\tPDF Color 2\t1\t1\tapplication/pdf\t42\tprint-scaling-none-landscape-user\t%s\t1\tPageSize=A4 print-scaling=none\tpdftopdf\n' \
    "${WORKDIR}/${kind}.pdf" "${WORKDIR}/${kind}.out.pdf" "${kind}" >> "${CASES}"
done

"${TESTFILTERS}" "${CASES}"

for kind in portrait landscape; do
  if [[ ! -s "${WORKDIR}/${kind}.out.pdf" ]]; then
    echo "${kind}: pdftopdf produced no output" >&2
    exit 1
  fi
  if ! grep -aq '/MediaBox *\[ *0 0 595\.[0-9]* 841\.[0-9]*\]' "${WORKDIR}/${kind}.out.pdf"; then
    echo "${kind}: output page is not A4 portrait:" >&2
    grep -ao '/MediaBox *\[[^]]*\]' "${WORKDIR}/${kind}.out.pdf" >&2 || true
    exit 1
  fi
done

portrait="$("${CHECKER}" box "${WORKDIR}/portrait.out.pdf" 612 792)"
landscape="$("${CHECKER}" box "${WORKDIR}/landscape.out.pdf" 792 612)"
echo "portrait page placed on:  ${portrait}"
echo "landscape page placed on: ${landscape}"

# Both rectangles must be unscaled (612x792) and match to within half a
# point.
awk -v p="${portrait}" -v l="${landscape}" 'BEGIN {
  split(p, a, " "); split(l, b, " ");
  for (i = 1; i <= 4; i ++) {
    d = a[i] - b[i]; if (d < 0) d = -d;
    if (d > 0.5) { print "landscape page is not placed like the portrait page" > "/dev/stderr"; exit 1 }
  }
  w = b[3] - b[1]; h = b[4] - b[2];
  if (w < 611.5 || w > 612.5 || h < 791.5 || h > 792.5) {
    print "landscape page got scaled with print-scaling=none" > "/dev/stderr"; exit 1
  }
}'
