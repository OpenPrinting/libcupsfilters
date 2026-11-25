#!/usr/bin/env bash
#
# Regression test: print-scaling=auto (the default) and auto-fit must not
# scale a document that fits the output media.
#
# libcupsfilters 2.2.x treated auto/auto-fit like fit, shrinking every job
# into the printable area (issues #304, #339).  Like the QPDF-based
# pdftopdf of 2.1.x, a document whose pages fit the media (2% tolerance)
# must stay at 100%, centered, cropped by the hardware margins; only a
# document too large for the media, or a job with ipp-attribute-fidelity,
# gets scaled to fit.
#
# The fixture is a single A4 page without annotations.  The synthesized
# "Generic PDF Color 2" printer has hardware margins (3.4mm left/right,
# 6.35mm top/bottom), so a wrong auto handling shows up as a shrink.
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

FIXTURE="cupsfilters/test_files/test_file_1pg.pdf"
CHECKER_SRC="cupsfilters/test-pdftopdf-print-scaling-auto.c"
GEN_SRC="cupsfilters/test-pdftopdf-print-scaling-auto-mixed.c"
for f in "${FIXTURE}" "${CHECKER_SRC}" "${GEN_SRC}"; do
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

WORKDIR="$(mktemp -d ./print-scaling-auto.XXXXXX)"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

CHECKER="${WORKDIR}/check"

"${CC}" -std=gnu11 -O0 ${PKG_CFLAGS} "${CHECKER_SRC}" ${PKG_LIBS} -lm -o "${CHECKER}"

# Generate the mixed-size fixture (page 1 A3, page 2 A4) for the
# page-ranges cases...
GEN="${WORKDIR}/gen-mixed"
MIXED="${WORKDIR}/mixed.pdf"
"${CC}" -std=gnu11 -O0 ${PKG_CFLAGS} "${GEN_SRC}" ${PKG_LIBS} -o "${GEN}"
"${GEN}" "${MIXED}"

FAILED_CASES=""

run_case() {
  local n="$1" desc="$2" mode="$3" opts="$4" fixture out

  fixture="${5:-${FIXTURE}}"

  echo "=== Test case ${n}: ${desc} ==="

  out="${WORKDIR}/out${n}.pdf"
  printf '%s\tapplication/pdf\t%s\tapplication/pdf\tGeneric\tPDF Color 2\t1\t1\tapplication/pdf\t42\tauto-user\tcase%s\t1\t%s\tpdftopdf\n' \
    "${fixture}" "${out}" "${n}" "${opts}" > "${WORKDIR}/case${n}.txt"

  if ! "${TESTFILTERS}" "${WORKDIR}/case${n}.txt"; then
    echo "ERROR: test case ${n}: testfilters failed" >&2
    FAILED_CASES="${FAILED_CASES} ${n}"
    return
  fi

  if [[ ! -s "${out}" ]]; then
    echo "ERROR: test case ${n}: pdftopdf produced no output" >&2
    FAILED_CASES="${FAILED_CASES} ${n}"
    return
  fi

  if ! "${CHECKER}" "${out}" "${mode}"; then
    echo "ERROR: test case ${n} failed (${mode}): ${out}" >&2
    FAILED_CASES="${FAILED_CASES} ${n}"
    return
  fi

  echo "PASS: test case ${n} (${mode})"
}

# case 1: A4 doc on A4, default scaling (auto) -> no scaling
# case 2: A4 doc on A4, print-scaling=fit -> scaled (unchanged behavior)
# case 3: A4 doc on Letter, default scaling (auto) -> doc too large, scaled
# case 4: A4 doc on A4, print-scaling=auto-fit -> no scaling (2.1.x semantics)
# case 5: A4 doc on A4, ipp-attribute-fidelity=true -> scaled
# case 6: A4 doc on Letter, print-scaling=auto-fit -> doc too large, scaled
# case 7: mixed-size doc (A3 p1, A4 p2) on A4, page-ranges=2 -> only the A4
#         page gets printed, only requested pages count for the too-large
#         decision -> no scaling
# case 8: mixed-size doc (A3 p1, A4 p2) on A4, page-ranges=1 -> A3 page
#         printed, too large -> scaled
run_case 1 "A4 document on A4 paper, default scaling (auto) - expect NO scaling" noscale "PageSize=A4"
run_case 2 "A4 document on A4 paper, print-scaling=fit - expect scaling" scaled "PageSize=A4 print-scaling=fit"
run_case 3 "A4 document on Letter paper, default scaling (auto) - document too large, expect scaling" scaled "PageSize=letter"
run_case 4 "A4 document on A4 paper, print-scaling=auto-fit - expect NO scaling (2.1.x semantics)" noscale "PageSize=A4 print-scaling=auto-fit"
run_case 5 "A4 document on A4 paper, ipp-attribute-fidelity=true - expect scaling" scaled "PageSize=A4 ipp-attribute-fidelity=true"
run_case 6 "A4 document on Letter paper, print-scaling=auto-fit - document too large, expect scaling" scaled "PageSize=letter print-scaling=auto-fit"
run_case 7 "Mixed-size document (A3 page 1, A4 page 2) on A4 paper, page-ranges=2 - only A4 page printed, expect NO scaling" noscale "PageSize=A4 page-ranges=2" "${MIXED}"
run_case 8 "Mixed-size document (A3 page 1, A4 page 2) on A4 paper, page-ranges=1 - A3 page printed, expect scaling" scaled "PageSize=A4 page-ranges=1" "${MIXED}"

if [[ -n "${FAILED_CASES}" ]]; then
  echo "ERROR: failed test cases:${FAILED_CASES}" >&2
  exit 1
fi

echo "All 8 test cases passed."
