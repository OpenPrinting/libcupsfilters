#!/usr/bin/env bash
#
# Regression test: cfFilterPDFToPDF() must honor inheritable page attributes.
#
# /MediaBox, /CropBox and /Rotate may be defined on an ancestor /Pages node
# instead of on the /Page itself (ISO 32000-1, 7.7.3.4), and the value may
# be an indirect reference.  The PDFio-based pdftopdf used to read these
# keys only from the page dictionary, so a page with an inherited MediaBox
# was taken to be the output page size already: no scaling, no
# auto-rotation, and an oversized page came out cropped to its lower-left
# corner.
#
# test_files/inherited_mediabox.pdf is a single 2970x2100 pt page whose
# MediaBox is only defined, as an indirect array, on the /Pages node.  We
# run it through pdftopdf for Letter with print-scaling=auto via the
# testfilters harness and then read the output back with PDFio (see
# test-pdftopdf-inherited-mediabox.c): the page transform must scale by
# roughly 0.26 and rotate by 90 degrees.
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

FIXTURE="cupsfilters/test_files/inherited_mediabox.pdf"
CHECKER_SRC="cupsfilters/test-pdftopdf-inherited-mediabox.c"
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

WORKDIR="$(mktemp -d ./inherited-mediabox.XXXXXX)"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

CASES="${WORKDIR}/cases.txt"
OUTPUT="${WORKDIR}/output.pdf"
CHECKER="${WORKDIR}/check"

# One testfilters case: fixture -> pdftopdf -> Letter PDF.
printf '%s\tapplication/pdf\t%s\tapplication/pdf\tGeneric\tPDF Color 2\t1\t1\tapplication/pdf\t42\tinherited-user\tinherited-mediabox\t1\tmedia-size=letter print-scaling=auto\tpdftopdf\n' \
  "${FIXTURE}" "${OUTPUT}" > "${CASES}"

"${TESTFILTERS}" "${CASES}"

if [[ ! -s "${OUTPUT}" ]]; then
  echo "pdftopdf produced no output" >&2
  exit 1
fi

"${CC}" -std=gnu11 -O0 ${PKG_CFLAGS} "${CHECKER_SRC}" ${PKG_LIBS} -lm -o "${CHECKER}"

"${CHECKER}" "${OUTPUT}"
