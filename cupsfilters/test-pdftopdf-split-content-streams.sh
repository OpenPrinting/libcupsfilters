#!/usr/bin/env bash
#
# Regression test: cfFilterPDFToPDF() must keep content streams separated
# when a page's /Contents array is merged into one stream (Issue #293).
#
# A page's /Contents may be an array of streams, and a stream need not end
# in whitespace.  pdftopdf concatenates them into one output stream;
# without a separator "Q" + "Q" became the invalid operator "QQ" and
# "q" + "0.5" became "q0.5", so the page rendered blank or the printer
# rejected the job.
#
# test-pdftopdf-split-content-streams.c writes the two inputs: a single
# page whose content is split as "q\nq" | "0.5 0 0 0.5 0 0 cm ... Q" | "Q",
# and the same page with an annotation, so that pdftopdf flattens it first
# (flatten_pdf()) before laying it out (copy_page()).
#
# Each output is then tokenized with PDFio by the same program: no fused
# tokens may appear and q/Q must stay balanced.
#
# Like testfilters.sh, this runs from the top-level build directory and
# passes file names relative to it: testfilters copies the case file names
# into 100-byte buffers, so long absolute paths must be avoided.
#
set -euo pipefail

CC="${CC:-cc}"

TESTFILTERS="./testfilters"
if [[ ! -x "${TESTFILTERS}" ]]; then
  echo "testfilters harness not found at ${TESTFILTERS}" >&2
  exit 99
fi

CHECKER_SRC="cupsfilters/test-pdftopdf-split-content-streams.c"
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

WORKDIR="$(mktemp -d ./split-content-streams.XXXXXX)"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

FIXTURE="${WORKDIR}/input.pdf"
FIXTURE_ANNOT="${WORKDIR}/input-annot.pdf"
CASES="${WORKDIR}/cases.txt"
OUTPUT="${WORKDIR}/output.pdf"
OUTPUT_ANNOT="${WORKDIR}/output-annot.pdf"
CHECKER="${WORKDIR}/check"

"${CC}" -std=gnu11 -O0 ${PKG_CFLAGS} "${CHECKER_SRC}" ${PKG_LIBS} -o "${CHECKER}"
"${CHECKER}" --write "${FIXTURE}"
"${CHECKER}" --write "${FIXTURE_ANNOT}" --annot

{
  printf '%s\tapplication/pdf\t%s\tapplication/pdf\tGeneric\tPDF Color 2\t1\t1\tapplication/pdf\t42\tsplit-user\tsplit-content-streams\t1\tmedia-size=letter\tpdftopdf\n' \
    "${FIXTURE}" "${OUTPUT}"
  printf '%s\tapplication/pdf\t%s\tapplication/pdf\tGeneric\tPDF Color 2\t1\t1\tapplication/pdf\t42\tsplit-user\tsplit-content-streams\t1\tmedia-size=letter\tpdftopdf\n' \
    "${FIXTURE_ANNOT}" "${OUTPUT_ANNOT}"
} > "${CASES}"

"${TESTFILTERS}" "${CASES}"

status=0
for out in "${OUTPUT}" "${OUTPUT_ANNOT}"; do
  echo "Checking ${out}..."
  if [[ ! -s "${out}" ]]; then
    echo "No output produced for ${out}" >&2
    status=1
    continue
  fi
  "${CHECKER}" "${out}" || status=1
done

exit "${status}"
