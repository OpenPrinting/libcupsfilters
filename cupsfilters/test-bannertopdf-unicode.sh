#!/usr/bin/env bash
#
# Regression test: bannertopdf must embed a fallback font for Unicode job
# information while keeping and safely escaping the ASCII Courier path.
#
# This follows the same pattern as test-pdftopdf-inherited-mediabox.sh: run a
# focused case through the testfilters harness, then inspect the resulting PDF
# with a small PDFio checker.
#
set -euo pipefail

CC="${CC:-cc}"

if [[ ! -x ./testfilters ]]; then
  echo "testfilters harness not found at ./testfilters" >&2
  exit 99
fi

if [[ ! -f config.h ]] || ! grep -q '^#define HAVE_FONTCONFIG 1' config.h; then
  echo "Fontconfig support is disabled; skipping." >&2
  exit 77
fi

PKG_CFLAGS="$(pkg-config --cflags pdfio fontconfig 2>/dev/null || true)"
PKG_LIBS="$(pkg-config --libs pdfio fontconfig 2>/dev/null || true)"
if [[ -z "${PKG_LIBS}" ]]; then
  echo "pkg-config cannot find PDFio and Fontconfig; skipping." >&2
  exit 77
fi

if ! fc-match --format='%{file}\n' ':charset=00e9' 2>/dev/null | grep -q .; then
  echo "No font covering U+00E9 is available; skipping." >&2
  exit 77
fi

CHECKER_SRC="cupsfilters/test-bannertopdf-unicode.c"
if [[ ! -f "${CHECKER_SRC}" ]]; then
  echo "test file not found: ${CHECKER_SRC}" >&2
  exit 99
fi

WORKDIR="$(mktemp -d ./bannertopdf-unicode.XXXXXX)"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

BANNER="${WORKDIR}/unicode-banner"
CASES="${WORKDIR}/cases.txt"
OUTPUT="${WORKDIR}/output.pdf"
CHECKER="${WORKDIR}/check"

cat > "${BANNER}" <<'EOF'
#PDF-BANNER
Template default-testpage.pdf
Show printer-name job-name job-originating-user-name
EOF

printf '%s\tapplication/vnd.cups-pdf-banner\t%s\tapplication/pdf\tGeneric\tPDF Color 2\t1\t1\tapplication/pdf\t42\tunicode-user\tcafé\t1\t \tbannertopdf\n' \
  "${BANNER}" "${OUTPUT}" > "${CASES}"

PRINTER='Office (Floor 2) \ Queue' ./testfilters "${CASES}"

if [[ ! -s "${OUTPUT}" ]]; then
  echo "bannertopdf produced no output" >&2
  exit 1
fi

${CC} -std=gnu11 -O0 ${PKG_CFLAGS} "${CHECKER_SRC}" ${PKG_LIBS} -o "${CHECKER}"
"${CHECKER}" "${OUTPUT}"
