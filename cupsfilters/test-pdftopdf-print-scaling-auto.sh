#!/usr/bin/env bash
#
# Regression test: print-scaling=auto (the default) and auto-fit must only
# scale pages that do not fit on the paper (issues #339, #304, #300).
#
# The QPDF-based pdftopdf of libcupsfilters 2.1.x printed a document whose
# pages fit on the paper at its original size, centered on the sheet.  The
# PDFio-based pdftopdf treated "auto" like "fit", so a Letter page on Letter
# paper was shrunk into the printable area and a small label page was blown
# up to fill it.  PageSize=Letter makes pdftopdf use the printer's margins,
# like a PPD queue does.
#
# The page placement is read with the checker of
# test-pdftopdf-landscape-centered.sh.
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

WORKDIR="$(mktemp -d ./print-scaling-auto.XXXXXX)"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

CHECKER="${WORKDIR}/check"
"${CC}" -std=gnu11 -O0 ${PKG_CFLAGS} "${CHECKER_SRC}" ${PKG_LIBS} -lm -o "${CHECKER}"

# name, input width, input height, options, expected "x0 y0 x1 y1" on Letter
CASES_LIST=(
  "default|612|792|PageSize=Letter|0.000 0.000 612.000 792.000"
  "auto|612|792|PageSize=Letter print-scaling=auto|0.000 0.000 612.000 792.000"
  "auto-fit|612|792|PageSize=Letter print-scaling=auto-fit|0.000 0.000 612.000 792.000"
  "landscape|792|612|PageSize=Letter print-scaling=auto|0.000 0.000 612.000 792.000"
  "label|288|432|PageSize=Letter print-scaling=auto|162.000 180.000 450.000 612.000"
  "too-large|792|1224|PageSize=Letter print-scaling=auto|61.412 33.109 550.588 789.109"
  "fidelity|612|792|PageSize=Letter print-scaling=auto ipp-attribute-fidelity=true|18.000 38.403 594.000 783.814"
)

CASES="${WORKDIR}/cases.txt"
: > "${CASES}"
for c in "${CASES_LIST[@]}"; do
  IFS='|' read -r name w h opts expected <<< "${c}"
  "${CHECKER}" make "${WORKDIR}/${name}.pdf" "${w}" "${h}"
  printf '%s\tapplication/pdf\t%s\tapplication/pdf\tGeneric\tPDF Color 2\t1\t1\tapplication/pdf\t42\tprint-scaling-auto-user\t%s\t1\t%s\tpdftopdf\n' \
    "${WORKDIR}/${name}.pdf" "${WORKDIR}/${name}.out.pdf" "${name}" "${opts}" >> "${CASES}"
done

"${TESTFILTERS}" "${CASES}"

status=0
for c in "${CASES_LIST[@]}"; do
  IFS='|' read -r name w h opts expected <<< "${c}"
  if [[ ! -s "${WORKDIR}/${name}.out.pdf" ]]; then
    echo "${name}: pdftopdf produced no output" >&2
    status=1
    continue
  fi
  got="$("${CHECKER}" box "${WORKDIR}/${name}.out.pdf" "${w}" "${h}")"
  if [[ "${got}" != "${expected}" ]]; then
    echo "${name} (${opts}): page placed at ${got}, expected ${expected}" >&2
    status=1
  fi
done

exit "${status}"
