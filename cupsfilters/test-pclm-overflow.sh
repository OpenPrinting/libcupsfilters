#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ROOT="$(cd "${ROOT}/.." && pwd)"
LIBTOOL="${BUILD_ROOT}/libtool"
CC="${CC:-cc}"
SAN_FLAGS="${SAN_FLAGS:--fsanitize=address -fno-omit-frame-pointer}"

if [[ ! -x "${LIBTOOL}" ]]; then
  echo "libtool helper not found at ${LIBTOOL}" >&2
  exit 99
fi

TMP_PARENT="${TMPDIR:-/tmp}"
WORKDIR="$(mktemp -d "${TMP_PARENT%/}/pclm-overflow.XXXXXX")"
cleanup() {
  rm -rf "${WORKDIR}"
}
trap cleanup EXIT

TOOLS_DIR="${WORKDIR}/tools"
mkdir -p "${TOOLS_DIR}"

INPUT_PWG="${WORKDIR}/malicious.pwg"
OUTPUT_PCLM="${WORKDIR}/out.pclm"
HARNESS_SRC="${TOOLS_DIR}/trigger_overflow.c"
HARNESS_OBJ="${TOOLS_DIR}/trigger_overflow.lo"
HARNESS_BIN="${TOOLS_DIR}/trigger_overflow"
PWG_SRC="${TOOLS_DIR}/make_pwg.c"
PWG_BIN="${TOOLS_DIR}/make_pwg"
RUN_LOG="${WORKDIR}/trigger.log"

cat > "${PWG_SRC}" <<'EOF'
#include <cups/raster.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc != 4) {
    fprintf(stderr, "Usage: %s <width> <height> <output.pwg>\n", argv[0]);
    return 1;
  }

  int width = atoi(argv[1]);
  int height = atoi(argv[2]);
  const char *outpath = argv[3];

  if (width <= 0 || height <= 0) {
    fprintf(stderr, "Width and height must be positive integers.\n");
    return 1;
  }

  int fd = open(outpath, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (fd < 0) {
    perror("open");
    return 1;
  }

  cups_raster_t *ras = cupsRasterOpen(fd, CUPS_RASTER_WRITE);
  if (!ras) {
    fprintf(stderr, "Failed to open raster stream.\n");
    close(fd);
    return 1;
  }

  cups_page_header2_t header;
  memset(&header, 0, sizeof(header));

  header.HWResolution[0] = header.HWResolution[1] = 300;
  header.PageSize[0] = 612;
  header.PageSize[1] = 792;
  header.cupsPageSize[0] = 612.0f;
  header.cupsPageSize[1] = 792.0f;
  header.cupsImagingBBox[0] = 0.0f;
  header.cupsImagingBBox[1] = 0.0f;
  header.cupsImagingBBox[2] = 612.0f;
  header.cupsImagingBBox[3] = 792.0f;
  header.cupsWidth = (unsigned)width;
  header.cupsHeight = (unsigned)height;
  header.cupsBitsPerColor = 8;
  header.cupsBitsPerPixel = 8;
  header.cupsNumColors = 1;
  header.cupsBytesPerLine = (unsigned)width;
  header.cupsColorOrder = CUPS_ORDER_CHUNKED;
  header.cupsColorSpace = CUPS_CSPACE_K;
  header.cupsCompression = 0; /* CUPS_COMPRESSION_NONE */

  if (!cupsRasterWriteHeader2(ras, &header)) {
    fprintf(stderr, "Failed to write PWG header.\n");
    cupsRasterClose(ras);
    close(fd);
    return 1;
  }

  unsigned char *line = (unsigned char *)calloc((size_t)width, sizeof(unsigned char));
  if (!line) {
    fprintf(stderr, "Failed to allocate raster line.\n");
    cupsRasterClose(ras);
    close(fd);
    return 1;
  }

  for (int y = 0; y < height; y++) {
    if (cupsRasterWritePixels(ras, line, (unsigned)width) < (unsigned)width) {
      fprintf(stderr, "Failed to write raster line %d.\n", y);
      free(line);
      cupsRasterClose(ras);
      close(fd);
      return 1;
    }
  }

  free(line);
  cupsRasterClose(ras);
  close(fd);
  return 0;
}
EOF

"${CC}" -std=c11 -O0 ${SAN_FLAGS} -o "${PWG_BIN}" "${PWG_SRC}" -lcups
"${PWG_BIN}" 1024 8000 "${INPUT_PWG}" >/dev/null

cat > "${HARNESS_SRC}" <<'EOF'
#define _GNU_SOURCE
#include <cupsfilters/filter.h>
#include <cups/ipp.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(const char *prog) {
  fprintf(stderr, "Usage: %s <input.pwg> <output.pclm>\n", prog);
}

int main(int argc, char **argv) {
  if (argc != 3) {
    usage(argv[0]);
    return 1;
  }

  const char *input_path = argv[1];
  const char *output_path = argv[2];

  int in_fd = open(input_path, O_RDONLY);
  if (in_fd < 0) {
    perror("open input");
    return 1;
  }

  int out_fd = open(output_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (out_fd < 0) {
    perror("open output");
    close(in_fd);
    return 1;
  }

  ipp_t *attrs = ippNew();
  if (!attrs) {
    fprintf(stderr, "Failed to allocate IPP attributes.\n");
    close(in_fd);
    close(out_fd);
    return 1;
  }

  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "pclm-strip-height-preferred", 1);

  int strip_heights[2] = {1, 16};
  ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                 "pclm-strip-height-supported", 2, strip_heights);

  ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
               "pclm-source-resolution-default", NULL, "300dpi");
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
               "pclm-source-resolution-supported", NULL, "300dpi");
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
               "pclm-compression-method-preferred", NULL, "flate");

  cf_filter_data_t data;
  memset(&data, 0, sizeof(data));
  data.printer_attrs = attrs;
  data.content_type = (char *)"image/pwg-raster";
  data.final_content_type = (char *)"image/pclm";

  cf_filter_out_format_t outformat = CF_FILTER_OUT_FORMAT_PCLM;

  int ret = cfFilterPWGToPDF(in_fd, out_fd, 1, &data, &outformat);

  ippDelete(attrs);
  close(in_fd);
  close(out_fd);

  if (ret != 0) {
    fprintf(stderr, "cfFilterPWGToPDF failed with code %d\n", ret);
  }

  return ret;
}
EOF

"${LIBTOOL}" --mode=compile --tag=CC "${CC}" -std=c11 -O0 ${SAN_FLAGS} \
  -I"${BUILD_ROOT}" -I"${BUILD_ROOT}/cupsfilters" \
  -c "${HARNESS_SRC}" -o "${HARNESS_OBJ}" >/dev/null

"${LIBTOOL}" --mode=link --tag=CC "${CC}" ${SAN_FLAGS} "${HARNESS_OBJ}" \
  "${BUILD_ROOT}/libcupsfilters.la" -lcups -o "${HARNESS_BIN}" >/dev/null

: > "${RUN_LOG}"
ASAN_OPTS="${ASAN_OPTIONS:-detect_leaks=0,abort_on_error=0}"

set +e
"${LIBTOOL}" --mode=execute env ASAN_OPTIONS="${ASAN_OPTS}" \
  "${HARNESS_BIN}" "${INPUT_PWG}" "${OUTPUT_PCLM}" \
  >>"${RUN_LOG}" 2>&1
STATUS=$?
set -e

if [[ ${STATUS} -ne 0 ]]; then
  cat "${RUN_LOG}" >&2
  echo "trigger_overflow exited with status ${STATUS}" >&2
  exit 1
fi

if grep -q "AddressSanitizer" "${RUN_LOG}"; then
  cat "${RUN_LOG}" >&2
  echo "AddressSanitizer reported a memory error" >&2
  exit 1
fi

if [[ ! -s "${OUTPUT_PCLM}" ]]; then
  echo "No PCLm output generated" >&2
  exit 1
fi

exit 0
