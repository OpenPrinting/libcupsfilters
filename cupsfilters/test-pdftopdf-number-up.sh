#!/usr/bin/env bash
#
# Regression test for Issue #176: pdftopdf must not write past its fixed
# layout array when "number-up" is larger than XFORM_MAX_LAYOUT (16).
#
# prepare_number_up() took any number-up value as the number of layout
# cells and filled that many entries of layout[16], so number-up=17 or
# more overflowed a structure on the stack.  Such values are now ignored
# and the job is printed 1-up, as is already done for booklets.
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

FIXTURE="cupsfilters/test_files/test_file_4pg.pdf"
CHECKER_SRC="cupsfilters/test-pdftopdf-number-up.c"
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

WORKDIR="$(mktemp -d ./number-up.XXXXXX)"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

CHECKER="${WORKDIR}/check"
"${CC}" -std=gnu11 -O0 ${PKG_CFLAGS} "${CHECKER_SRC}" ${PKG_LIBS} -lm -o "${CHECKER}"

status=0

# check NUMBER-UP PAGES
check() {
  local number_up="$1" pages="$2"
  local cases="${WORKDIR}/cases.txt" output="${WORKDIR}/output.pdf"

  rm -f "${output}"
  printf '%s\tapplication/pdf\t%s\tapplication/pdf\tGeneric\tPDF Color 2\t1\t1\tapplication/pdf\t176\tnumber-up-user\tnumber-up-title\t1\tPageSize=A4 number-up=%s\tpdftopdf\n' \
    "${FIXTURE}" "${output}" "${number_up}" > "${cases}"

  "${TESTFILTERS}" "${cases}" > /dev/null

  if [[ -s "${output}" ]] && "${CHECKER}" "${output}" "${pages}"; then
    echo "PASS: number-up=${number_up}"
  else
    echo "FAIL: number-up=${number_up}" >&2
    status=1
  fi
}

# 4 pages fit on one 16-up sheet
check 16 1

# Too many cells, printed 1-up
check 17 4
check 32 4

exit "${status}"
