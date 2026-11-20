#!/usr/bin/env bash
#
# Regression test for Issue #178: texttopdf must not read or write past the
# end of the job title when it is invalid or truncated UTF-8.
#
# With pretty printing on, make_wide() converts the title for the page
# header.  It took a lead byte such as 0xC0 as the start of a multi-byte
# character and skipped over the terminating nul.  This is reported by
# AddressSanitizer (heap-buffer-overflow) when the library is built with
# -fsanitize=address; in a normal build texttopdf must still produce a PDF.
#
set -euo pipefail

if [[ ! -x ./testfilters ]]; then
  echo "testfilters harness not found at ./testfilters" >&2
  exit 99
fi

# texttopdf reads its charset file from CUPS_DATADIR
DATADIR="$(sed -n 's/^#define CUPS_DATADIR "\(.*\)"$/\1/p' config.h)"
if [[ ! -f "${DATADIR}/charsets/pdf.utf-8" ]]; then
  echo "No charset file ${DATADIR}/charsets/pdf.utf-8; skipping." >&2
  exit 77
fi

WORKDIR="$(mktemp -d ./texttopdf-invalid-utf8-title.XXXXXX)"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

INPUT="${WORKDIR}/input.txt"
CASES="${WORKDIR}/cases.txt"

printf 'Hello\n' > "${INPUT}"

# A lone lead byte and a truncated 3-byte character
for title in '\300' '\342\202'; do
  OUTPUT="${WORKDIR}/output.pdf"
  rm -f "${OUTPUT}"

  printf "%s\ttext/plain\t%s\tapplication/pdf\tGeneric\tPDF Color 2\t1\t1\tapplication/pdf\t178\ttitle-user\t${title}\t1\tprettyprint\ttexttopdf\n" \
    "${INPUT}" "${OUTPUT}" > "${CASES}"

  ./testfilters "${CASES}"

  if [[ ! -s "${OUTPUT}" ]]; then
    echo "texttopdf produced no output for title ${title}" >&2
    exit 1
  fi
done

echo "texttopdf handled invalid UTF-8 job titles"
