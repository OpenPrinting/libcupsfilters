#!/usr/bin/env bash
#
# Regression test: pdftopdf must read "job-sheets" the way CUPS sends it.
#
# CUPS passes "job-sheets" as "start,end", so a queue without banners sends
# "none,none".  pdftopdf compared the whole value with "none" and added a
# banner at both ends of every job (issue #262).  A single value only asks
# for a start banner.  The banner must also show the job title and user
# that CUPS passes as filter arguments, not "Untitled" and "Guest".
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
CHECKER_SRC="cupsfilters/test-pdftopdf-job-sheets.c"
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

WORKDIR="$(mktemp -d ./job-sheets.XXXXXX)"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

CHECKER="${WORKDIR}/check"
"${CC}" -std=gnu11 -O0 ${PKG_CFLAGS} "${CHECKER_SRC}" ${PKG_LIBS} -lm -o "${CHECKER}"

status=0

check() {
  local job_sheets="$1" pages="$2" text="${3:-}"
  local cases="${WORKDIR}/cases.txt" output="${WORKDIR}/output.pdf"
  local options="PageSize=A4"

  if [[ -n "${job_sheets}" ]]; then
    options="job-sheets=${job_sheets} ${options}"
  fi

  rm -f "${output}"
  printf '%s\tapplication/pdf\t%s\tapplication/pdf\tGeneric\tPDF Color 2\t1\t1\tapplication/pdf\t42\tbanner-user\tbanner-title\t1\t%s\tpdftopdf\n' \
    "${FIXTURE}" "${output}" "${options}" > "${cases}"

  "${TESTFILTERS}" "${cases}" > /dev/null

  if "${CHECKER}" "${output}" "${pages}" ${text:+"${text}"}; then
    echo "PASS: job-sheets=${job_sheets:-(unset)}"
  else
    echo "FAIL: job-sheets=${job_sheets:-(unset)}" >&2
    status=1
  fi
}

check ""                1
check "none"            1
check "none,none"       1
check "None,NONE"       1
check "standard"        2 "Title: banner-title"
check "standard,none"   2 "User: banner-user"
check "none,standard"   2
check "standard,standard" 3

exit "${status}"
