#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ROOT="$(cd "${ROOT}/.." && pwd)"
LIBTOOL="${BUILD_ROOT}/libtool"
CC="${CC:-cc}"
SAN_FLAGS="${SAN_FLAGS:--fsanitize=address -fno-omit-frame-pointer}"
PKG_CFLAGS="$(pkg-config --cflags lcms2 pdfio 2>/dev/null || true)"
PKG_LIBS="$(pkg-config --libs lcms2 pdfio 2>/dev/null || true)"

if pkg-config --exists cups 2>/dev/null; then
  PKG_CFLAGS+=" $(pkg-config --cflags cups)"
  PKG_LIBS+=" $(pkg-config --libs cups)"
elif pkg-config --exists cups3 2>/dev/null; then
  PKG_CFLAGS+=" $(pkg-config --cflags cups3)"
  PKG_LIBS+=" $(pkg-config --libs cups3)"
elif command -v cups-config >/dev/null 2>&1; then
  PKG_CFLAGS+=" $(cups-config --cflags)"
  PKG_LIBS+=" $(cups-config --libs)"
fi

if ! printf 'int main(void){return 0;}\n' \
     | "${CC}" ${SAN_FLAGS} -x c - -o /dev/null >/dev/null 2>&1; then
  echo "AddressSanitizer not available; skipping." >&2
  exit 77
fi

if [[ ! -x "${LIBTOOL}" ]]; then
  echo "libtool helper not found at ${LIBTOOL}" >&2
  exit 99
fi

WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/pwgtopdf-bit-row.XXXXXX")"
cleanup() {
  rm -rf "${WORKDIR}"
}
trap cleanup EXIT

INPUT_PWG="${WORKDIR}/black-1bit.pwg"
OUTPUT_PDF="${WORKDIR}/output.pdf"
GENERATOR_SRC="${WORKDIR}/make-pwg.c"
GENERATOR_BIN="${WORKDIR}/make-pwg"
HARNESS_SRC="${WORKDIR}/run-filter.c"
HARNESS_OBJ="${WORKDIR}/run-filter.lo"
HARNESS_BIN="${WORKDIR}/run-filter"
RUN_LOG="${WORKDIR}/run.log"

cat > "${GENERATOR_SRC}" <<'EOF'
#include <cups/raster.h>
#include "cupsfilters/libcups2-private.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
  cups_page_header_t header;
  unsigned char row[16] = {0};
  cups_raster_t *ras;
  int fd;

  if (argc != 2)
    return 1;

  fd = open(argv[1], O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (fd < 0)
    return 1;

  ras = cupsRasterOpen(fd, CUPS_RASTER_WRITE);
  if (!ras) {
    close(fd);
    return 1;
  }

  memset(&header, 0, sizeof(header));
  header.HWResolution[0] = header.HWResolution[1] = 300;
  header.PageSize[0] = 612;
  header.PageSize[1] = 792;
  header.cupsPageSize[0] = 612.0f;
  header.cupsPageSize[1] = 792.0f;
  header.cupsImagingBBox[2] = 612.0f;
  header.cupsImagingBBox[3] = 792.0f;
  header.cupsWidth = 128;
  header.cupsHeight = 1;
  header.cupsBitsPerColor = 1;
  header.cupsBitsPerPixel = 1;
  header.cupsNumColors = 1;
  header.cupsBytesPerLine = sizeof(row);
  header.cupsColorOrder = CUPS_ORDER_CHUNKED;
  header.cupsColorSpace = CUPS_CSPACE_K;

  if (!cupsRasterWriteHeader(ras, &header) ||
      cupsRasterWritePixels(ras, row, sizeof(row)) != sizeof(row)) {
    cupsRasterClose(ras);
    close(fd);
    return 1;
  }

  cupsRasterClose(ras);
  close(fd);
  return 0;
}
EOF

cat > "${HARNESS_SRC}" <<'EOF'
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Compile the production implementation with ASan even when the surrounding
// project build is not sanitized.
#include "cupsfilters/pwgtopdf.c"

int main(int argc, char **argv) {
  cf_filter_data_t data;
  cf_filter_out_format_t outformat = CF_FILTER_OUT_FORMAT_PDF;
  int inputfd;
  int outputfd;
  int result;

  if (argc != 3)
    return 1;

  inputfd = open(argv[1], O_RDONLY);
  outputfd = open(argv[2], O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (inputfd < 0 || outputfd < 0)
    return 1;

  memset(&data, 0, sizeof(data));
  data.content_type = (char *)"image/pwg-raster";
  data.final_content_type = (char *)"application/pdf";

  result = cfFilterPWGToPDF(inputfd, outputfd, 1, &data, &outformat);
  close(inputfd);
  close(outputfd);
  return result;
}
EOF

"${CC}" -std=c11 -O0 ${SAN_FLAGS} -o "${GENERATOR_BIN}" \
  -I"${BUILD_ROOT}" ${PKG_CFLAGS} "${GENERATOR_SRC}" ${PKG_LIBS}
"${GENERATOR_BIN}" "${INPUT_PWG}"

"${LIBTOOL}" --mode=compile --tag=CC "${CC}" -std=gnu11 -O0 \
  -D_GNU_SOURCE ${SAN_FLAGS} \
  -I"${BUILD_ROOT}" -I"${BUILD_ROOT}/cupsfilters" ${PKG_CFLAGS} \
  -c "${HARNESS_SRC}" -o "${HARNESS_OBJ}" >/dev/null

"${LIBTOOL}" --mode=link --tag=CC "${CC}" ${SAN_FLAGS} "${HARNESS_OBJ}" \
  "${BUILD_ROOT}/libcupsfilters.la" ${PKG_LIBS} -lm \
  -o "${HARNESS_BIN}" >/dev/null

ASAN_OPTS="${ASAN_OPTIONS:-detect_leaks=0,abort_on_error=0}"
set +e
"${LIBTOOL}" --mode=execute env \
  DYLD_LIBRARY_PATH="${BUILD_ROOT}/.libs${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}" \
  ASAN_OPTIONS="${ASAN_OPTS}" \
  "${HARNESS_BIN}" "${INPUT_PWG}" "${OUTPUT_PDF}" >"${RUN_LOG}" 2>&1
STATUS=$?
set -e

if [[ ${STATUS} -ne 0 ]]; then
  cat "${RUN_LOG}" >&2
  echo "pwgtopdf exited with status ${STATUS}" >&2
  exit 1
fi

if grep -q "AddressSanitizer" "${RUN_LOG}"; then
  cat "${RUN_LOG}" >&2
  echo "AddressSanitizer reported a memory error" >&2
  exit 1
fi

if [[ ! -s "${OUTPUT_PDF}" ]]; then
  echo "No PDF output generated" >&2
  exit 1
fi

exit 0
