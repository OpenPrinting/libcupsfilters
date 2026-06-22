#!/usr/bin/env bash
#
# Regression test for the strcpy() buffer-overflow fix in imagetoraster.c.
#
# The vulnerable code copied header.cupsPageSizeName (64-byte field with no
# guaranteed null terminator when supplied via filter_data.header) into a
# 64-byte stack buffer with strcpy(), overflowing by an unbounded amount.
# The fix replaces strcpy() with snprintf(buf, sizeof(buf), ...).
#
# This script compiles a C harness with AddressSanitizer, runs it against a
# crafted cups_page_header_t whose cupsPageSizeName has no null terminator,
# and fails if ASAN reports any memory error.
#
# Pattern follows cupsfilters/test-pclm-overflow.sh.
#
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

# Read the CUPS compiler flags that configure already discovered and wrote
# into the generated Makefile.  This is more reliable than re-running
# pkg-config at test time, which may fail or return wrong paths depending
# on the distro packaging (e.g. Ubuntu's libcups2 puts headers under
# /usr/include/libcups2 which pkg-config --cflags-only-I does not expose).
CUPS_CFLAGS_VAL="$(grep '^CUPS_CFLAGS' "${BUILD_ROOT}/Makefile" \
                   | sed 's/^CUPS_CFLAGS[[:space:]]*=[[:space:]]*//' \
                   | head -1)"

TMP_PARENT="${TMPDIR:-/tmp}"
WORKDIR="$(mktemp -d "${TMP_PARENT%/}/imagetoraster-overflow.XXXXXX")"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

MAKE_JPEG_SRC="${WORKDIR}/make_jpeg.c"
MAKE_JPEG_BIN="${WORKDIR}/make_jpeg"
INPUT_JPG="${WORKDIR}/test.jpg"
HARNESS_SRC="${WORKDIR}/trigger.c"
HARNESS_OBJ="${WORKDIR}/trigger.lo"
HARNESS_BIN="${WORKDIR}/trigger"
RUN_LOG="${WORKDIR}/trigger.log"

cat > "${MAKE_JPEG_SRC}" <<'EOF_JPEG'
#include <jpeglib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <output.jpg>\n", argv[0]);
    return 1;
  }

  FILE *fp = fopen(argv[1], "wb");
  if (!fp) { perror("fopen"); return 1; }

  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  jpeg_stdio_dest(&cinfo, fp);

  cinfo.image_width = 8;
  cinfo.image_height = 8;
  cinfo.input_components = 1;
  cinfo.in_color_space = JCS_GRAYSCALE;
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, 75, TRUE);
  jpeg_start_compress(&cinfo, TRUE);

  JSAMPROW row[1];
  unsigned char rowdata[8];
  memset(rowdata, 255, 8);
  row[0] = rowdata;
  while (cinfo.next_scanline < cinfo.image_height)
    jpeg_write_scanlines(&cinfo, row, 1);

  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);
  fclose(fp);
  return 0;
}
EOF_JPEG

"${CC}" -std=c11 -O0 -o "${MAKE_JPEG_BIN}" "${MAKE_JPEG_SRC}" -ljpeg >/dev/null 2>&1
if [[ ! -x "${MAKE_JPEG_BIN}" ]]; then
  echo "Failed to compile make_jpeg" >&2
  exit 99
fi
"${MAKE_JPEG_BIN}" "${INPUT_JPG}"

cat > "${HARNESS_SRC}" <<'EOF'
/*
 * Overflow-trigger harness for the imagetoraster strcpy() regression test.
 *
 * Injects a cups_page_header_t with cupsPageSizeName entirely filled with
 * non-null bytes (no null terminator) via filter_data.header.  The old
 * strcpy(defSize, header.cupsPageSizeName) in imagetoraster.c reads past the
 * end of the 64-byte field and overflows the 64-byte stack buffer defSize[].
 * AddressSanitizer will catch this.  The snprintf() fix is immune.
 *
 * cupsPageSize is left at {0.0f, 0.0f} intentionally: cfRasterPrepareHeader()
 * in raster.c only overwrites cupsPageSizeName through pwgMediaForSize() when
 * cupsPageSize dimensions are positive.  With {0,0} the crafted name survives
 * all the way to the vulnerable copy.  PageSize is set to Letter so the filter
 * can compute page geometry.
 */
#include <cupsfilters/filter.h>
#include <cups/raster.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
  if (argc != 2)
  {
    fprintf(stderr, "Usage: %s <image.jpg>\n", argv[0]);
    return 1;
  }

  signal(SIGPIPE, SIG_IGN);

  int inputfd = open(argv[1], O_RDONLY);
  if (inputfd < 0) { perror("open input"); return 1; }

  int outputfd = open("/dev/null", O_WRONLY);
  if (outputfd < 0) { perror("open /dev/null"); close(inputfd); return 1; }

  cups_page_header_t crafted;
  memset(&crafted, 0, sizeof(crafted));
  crafted.PageSize[0]            = 612;   /* Letter: 8.5" × 11" in points */
  crafted.PageSize[1]            = 792;
  crafted.ImagingBoundingBox[0]  = 0;
  crafted.ImagingBoundingBox[1]  = 0;
  crafted.ImagingBoundingBox[2]  = 612;
  crafted.ImagingBoundingBox[3]  = 792;
  crafted.HWResolution[0]        = 100;
  crafted.HWResolution[1]        = 100;
  crafted.cupsWidth              = 850;
  crafted.cupsHeight             = 1100;
  crafted.cupsBitsPerColor       = 8;
  crafted.cupsBitsPerPixel       = 8;
  crafted.cupsNumColors          = 1;
  crafted.cupsBytesPerLine       = 850;
  crafted.cupsColorOrder         = CUPS_ORDER_CHUNKED;
  crafted.cupsColorSpace         = CUPS_CSPACE_K;
  /* 64 'A' bytes, no null terminator — triggers strcpy overflow */
  memset(crafted.cupsPageSizeName, 'A', sizeof(crafted.cupsPageSizeName));

  int              job_canceled = 0;
  cf_filter_data_t data;
  memset(&data, 0, sizeof(data));
  data.printer            = "test-printer";
  data.job_id             = 1;
  data.job_user           = "test";
  data.job_title          = "imagetoraster strcpy overflow regression";
  data.copies             = 1;
  data.content_type       = "image/jpeg";
  data.final_content_type = "application/vnd.cups-raster";
  data.header             = &crafted;
  data.printer_attrs      = NULL;
  data.logfunc            = cfCUPSLogFunc;
  data.iscanceledfunc     = cfCUPSIsCanceledFunc;
  data.iscanceleddata     = &job_canceled;
  data.back_pipe[0] = data.back_pipe[1] = -1;
  data.side_pipe[0] = data.side_pipe[1] = -1;

  int ret = cfFilterImageToRaster(inputfd, outputfd, 1, &data, NULL);
  close(inputfd);
  close(outputfd);
  return ret;
}
EOF

"${LIBTOOL}" --mode=compile --tag=CC "${CC}" -std=c11 -O0 ${SAN_FLAGS} \
  -I"${BUILD_ROOT}" -I"${BUILD_ROOT}/cupsfilters" ${CUPS_CFLAGS_VAL} \
  -c "${HARNESS_SRC}" -o "${HARNESS_OBJ}" >/dev/null
if [[ ! -f "${HARNESS_OBJ}" && ! -f "${WORKDIR}/trigger.o" ]]; then
  echo "test-imagetoraster-overflow: failed to compile harness" >&2
  exit 99
fi

"${LIBTOOL}" --mode=link --tag=CC "${CC}" ${SAN_FLAGS} "${HARNESS_OBJ}" \
  "${BUILD_ROOT}/libcupsfilters.la" -lcups -o "${HARNESS_BIN}" >/dev/null
if [[ ! -x "${HARNESS_BIN}" ]]; then
  echo "test-imagetoraster-overflow: failed to link harness" >&2
  exit 99
fi

: > "${RUN_LOG}"
ASAN_OPTS="${ASAN_OPTIONS:-detect_leaks=0,abort_on_error=0}"

set +e
"${LIBTOOL}" --mode=execute \
  env ASAN_OPTIONS="${ASAN_OPTS}" \
  "${HARNESS_BIN}" "${INPUT_JPG}" \
  >>"${RUN_LOG}" 2>&1
STATUS=$?
set -e

if [[ ${STATUS} -ne 0 ]]; then
  cat "${RUN_LOG}" >&2
  echo "test-imagetoraster-overflow: harness exited with status ${STATUS}" >&2
  exit 1
fi

if grep -q "AddressSanitizer" "${RUN_LOG}"; then
  cat "${RUN_LOG}" >&2
  echo "test-imagetoraster-overflow: AddressSanitizer reported a memory error" >&2
  exit 1
fi

echo "test-imagetoraster-overflow: PASSED"
exit 0
