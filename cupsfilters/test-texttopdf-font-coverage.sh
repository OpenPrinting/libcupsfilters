#!/usr/bin/env bash
#
# Regression test for Issue #224: texttopdf must not embed a font which has
# no glyphs for the text it is used for.
#
# A symbol-only font (cupsfilters/test_files/symbol-only.ttf, a single glyph
# at U+2588) is made the preferred "monospace" font through a private
# fontconfig configuration, the same way the powerline-fonts package does
# with PowerlineSymbols.  Plain ASCII text is then run through texttopdf,
# and the resulting PDF must not use the symbol-only font.
#
set -euo pipefail

if [[ ! -x ./testfilters ]]; then
  echo "testfilters harness not found at ./testfilters" >&2
  exit 99
fi

if [[ ! -f config.h ]] || ! grep -q '^#define HAVE_FONTCONFIG 1' config.h; then
  echo "Fontconfig support is disabled; skipping." >&2
  exit 77
fi

srcdir="${srcdir:-.}"
FONT_DIR="$(cd "${srcdir}/cupsfilters/test_files" && pwd)"
FONT_FAMILY="CupsfiltersSymbolOnly"
# Generated into the build directory by "make check" (check_DATA)
INPUT="cupsfilters/test_files/test_text_lorem.txt"

if [[ ! -f "${FONT_DIR}/symbol-only.ttf" || ! -f "${INPUT}" ]]; then
  echo "test files not found in ${FONT_DIR}" >&2
  exit 99
fi

# texttopdf reads its charset file from CUPS_DATADIR
DATADIR="$(sed -n 's/^#define CUPS_DATADIR "\(.*\)"$/\1/p' config.h)"
if [[ ! -f "${DATADIR}/charsets/pdf.utf-8" ]]; then
  echo "No charset file ${DATADIR}/charsets/pdf.utf-8; skipping." >&2
  exit 77
fi

# A font which actually covers the text must exist for the test to mean
# anything
if ! fc-match --format='%{file}\n' 'monospace:charset=41' 2>/dev/null |
     grep -q .; then
  echo "No font covering U+0041 is available; skipping." >&2
  exit 77
fi

SYS_FONTS_CONF="$(pkg-config --variable=sysconfdir fontconfig 2>/dev/null ||
                  true)/fonts/fonts.conf"
[[ -f "${SYS_FONTS_CONF}" ]] || SYS_FONTS_CONF=/etc/fonts/fonts.conf

WORKDIR="$(mktemp -d ./texttopdf-font-coverage.XXXXXX)"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

CASES="${WORKDIR}/cases.txt"
OUTPUT="${WORKDIR}/output.pdf"

cat > "${WORKDIR}/fonts.conf" <<EOF
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig>
  <include ignore_missing="yes">${SYS_FONTS_CONF}</include>
  <dir>${FONT_DIR}</dir>
  <cachedir>$(cd "${WORKDIR}" && pwd)/fc-cache</cachedir>
  <match target="pattern">
    <test name="family"><string>monospace</string></test>
    <edit name="family" mode="prepend" binding="strong">
      <string>${FONT_FAMILY}</string>
    </edit>
  </match>
</fontconfig>
EOF
export FONTCONFIG_FILE="$(cd "${WORKDIR}" && pwd)/fonts.conf"

# Make sure the setup really reproduces the bug condition
if [[ "$(fc-match --format='%{family[0]}' monospace)" != "${FONT_FAMILY}" ]]
then
  echo "Could not make ${FONT_FAMILY} the preferred monospace font" >&2
  exit 99
fi

printf '%s\ttext/plain\t%s\tapplication/pdf\tGeneric\tPDF Color 2\t1\t1\tapplication/pdf\t224\tcoverage-user\tcoverage-test\t1\t \ttexttopdf\n' \
  "${INPUT}" "${OUTPUT}" > "${CASES}"

./testfilters "${CASES}"

if [[ ! -s "${OUTPUT}" ]]; then
  echo "texttopdf produced no output" >&2
  exit 1
fi

if grep -aq "${FONT_FAMILY}" "${OUTPUT}"; then
  echo "texttopdf embedded ${FONT_FAMILY}, which has no glyphs for the text" >&2
  exit 1
fi

if ! grep -aq "/FontFile" "${OUTPUT}"; then
  echo "texttopdf output has no embedded font" >&2
  exit 1
fi

echo "texttopdf skipped the symbol-only font"
