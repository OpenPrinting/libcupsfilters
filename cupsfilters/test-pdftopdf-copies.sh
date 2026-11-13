#!/usr/bin/env bash
#
# Regression test: pdftopdf must not multiply copies that a PDF printer
# or a downstream PostScript filter will generate.
#
set -euo pipefail

CC="${CC:-cc}"
TESTFILTERS="./testfilters"
FIXTURE="cupsfilters/test_files/test_file_1pg.pdf"
CHECKER_SRC="cupsfilters/test-pdftopdf-job-sheets.c"

if [[ ! -x "${TESTFILTERS}" ]]; then
  echo "testfilters harness not found at ${TESTFILTERS}" >&2
  exit 99
fi

for f in "${FIXTURE}" "${CHECKER_SRC}"; do
  if [[ ! -f "${f}" ]]; then
    echo "test file not found: ${f}" >&2
    exit 99
  fi
done

if ! PKG_CFLAGS="$(pkg-config --cflags pdfio)" ||
   ! PKG_LIBS="$(pkg-config --libs pdfio)"; then
  echo "pkg-config cannot find pdfio; skipping." >&2
  exit 77
fi

WORKDIR="$(mktemp -d ./pdftopdf-copies.XXXXXX)"
cleanup() {
  rm -f -- "${WORKDIR}/check" "${WORKDIR}/cases.txt" \
    "${WORKDIR}/output.pdf" "${WORKDIR}/filter.log"
  rmdir -- "${WORKDIR}"
}
trap cleanup EXIT

# Reuse the page-count checker; banner-text checking is optional.
CHECKER="${WORKDIR}/check"
"${CC}" -std=gnu11 -O0 ${PKG_CFLAGS} "${CHECKER_SRC}" \
  ${PKG_LIBS} -lm -o "${CHECKER}"

status=0

check() {
  local name="$1" final_type="$2" copies="$3" options="$4" pages="$5"
  local cases="${WORKDIR}/cases.txt" output="${WORKDIR}/output.pdf"
  local filter_log="${WORKDIR}/filter.log"

  rm -f -- "${output}"
  printf '%s\tapplication/pdf\t%s\t%s\tGeneric\tCopy Printer\t1\t0\tapplication/pdf,image/pwg-raster\t42\tcopy-user\tcopy-title\t%s\t%s\tpdftopdf\n' \
    "${FIXTURE}" "${output}" "${final_type:- }" "${copies}" \
    "${options:- }" > "${cases}"

  if ! "${TESTFILTERS}" "${cases}" > "${filter_log}" 2>&1; then
    cat "${filter_log}" >&2
    echo "FAIL: ${name}: filter failed" >&2
    status=1
  elif "${CHECKER}" "${output}" "${pages}"; then
    echo "PASS: ${name}"
  else
    cat "${filter_log}" >&2
    echo "FAIL: ${name}" >&2
    status=1
  fi
}

check "driverless PDF copies argument" application/pdf 2 "" 1
check "PPD PostScript hardware copies" application/vnd.cups-postscript 3 \
  "hardware-copies=yes" 1
check "PPD printer-specific final type" printer/legacy 3 \
  "hardware-copies=true" 1
check "explicit software copies for PDF" application/pdf 3 \
  "hardware-copies=off" 3
check "PDF copies option" application/vnd.cups-pdf 1 \
  "copies=3 hardware-copies=ON" 1
check "software copies option" application/pdf 1 \
  "copies=3 hardware-copies=false" 3
check "unknown final type defaults to software copies" "" 3 "" 3

# The emulated printer advertises copies-supported=1-999 even in these cases.
for final_type in application/vnd.cups-raster image/pwg-raster image/urf \
                  application/PCLm; do
  check "preserve software copies for ${final_type}" "${final_type}" 3 \
    "hardware-copies=true" 3
done

exit "${status}"
