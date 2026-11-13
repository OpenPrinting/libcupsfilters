#!/usr/bin/env bash
#
# Regression test: pdftopdf must only generate copies in software when the
# printer does not make them itself, and it must tell the following filters
# about the copies left to the printer.
#
# Until 2.1.1 pdftopdf decided between hardware and software copies from
# the "hardware-copies"/"hardware-collate" options (set by libppd from the
# PPD) or, without them, from the output format, and wrote the result into
# the "%%PDFTOPDFNumCopies"/"%%PDFTOPDFCollate" comments of its output.
# The PDFio-based rewrite always generated the copies itself and dropped
# the comments, so printers doing hardware copies printed N x N sheets
# (issue #271) and libppd re-applied page options already applied by
# pdftopdf (issue #233).
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
CHECKER_SRC="cupsfilters/test-pdftopdf-copies.c"
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

WORKDIR="$(mktemp -d ./copies.XXXXXX)"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

CHECKER="${WORKDIR}/check"
"${CC}" -std=gnu11 -O0 ${PKG_CFLAGS} "${CHECKER_SRC}" ${PKG_LIBS} -lm -o "${CHECKER}"

status=0

# check COPIES OUTPUT-TYPE OPTIONS PAGES DEVICE-COPIES DEVICE-COLLATE
check() {
  local copies="$1" type="$2" options="$3" pages="$4" device_copies="$5" \
        device_collate="$6"
  local cases="${WORKDIR}/cases.txt" output="${WORKDIR}/output.pdf"

  rm -f "${output}"
  printf '%s\tapplication/pdf\t%s\t%s\tGeneric\tPDF Color 2\t1\t1\tapplication/pdf\t42\tcopies-user\tcopies-title\t%s\tPageSize=A4 %s\tpdftopdf\n' \
    "${FIXTURE}" "${output}" "${type}" "${copies}" "${options}" > "${cases}"

  "${TESTFILTERS}" "${cases}" > /dev/null

  if "${CHECKER}" "${output}" "${pages}" "${device_copies}" "${device_collate}"; then
    echo "PASS: ${copies} copies, ${type}, ${options:-(no options)}"
  else
    echo "FAIL: ${copies} copies, ${type}, ${options:-(no options)}" >&2
    status=1
  fi
}

# One copy: nothing to do
check 1 application/pdf "" 4 1 false

# PDF printers make the copies themselves, raster drivers get software
# copies, unless libppd says otherwise
check 3 application/pdf "" 4 3 false
check 3 application/pdf "Collate=True" 4 3 true
check 3 application/vnd.cups-raster "" 12 1 false
check 3 application/pdf "hardware-copies=false" 12 1 false
check 3 application/vnd.cups-raster "hardware-copies=true hardware-collate=false" 4 3 false

# A printer which makes copies but does not collate gets collated copies
# in software
check 3 application/vnd.cups-raster "hardware-copies=true hardware-collate=false Collate=True" 12 1 false
check 3 application/vnd.cups-raster "hardware-copies=true hardware-collate=false sheet-collate=collated" 12 1 false
check 3 application/vnd.cups-raster "hardware-copies=true hardware-collate=false sheet-collate=uncollated" 4 3 false
check 3 image/pwg-raster "hardware-copies=true Collate=True" 4 3 true

exit "${status}"
