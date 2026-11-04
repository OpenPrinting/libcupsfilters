#!/usr/bin/env bash
#
# Regression test for the scanline handling of _cfImageReadTIFF() (issues
# #116 and #172).
#
# The C harness (test-image-tiff-scanline.c) writes small TIFF files with
# libtiff -- gray with 1, 2 and 4 samples per pixel, RGB and RGBA with 2, 4
# and 8 bits per sample -- opens each with cfImageOpen() and compares the
# decoded pixels.  Where AddressSanitizer can run, the harness is built with
# it, so libtiff writing a scanline into a buffer smaller than
# TIFFScanlineSize() aborts the test.
#
# Skips (Automake exit 77) when libcupsfilters or the system lacks libtiff.
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ROOT="$(cd "${ROOT}/.." && pwd)"
LIBTOOL="${BUILD_ROOT}/libtool"
CC="${CC:-cc}"
SAN_FLAGS="${SAN_FLAGS:--fsanitize=address -fno-omit-frame-pointer}"

# AddressSanitizer must be both linkable and runnable here (it is not under
# qemu-user emulation); without it the pixel checks still run.
asan_probe="$(mktemp "${TMPDIR:-/tmp}/asan-probe.XXXXXX")"
if ! printf 'int main(void){return 0;}\n' \
       | "${CC}" ${SAN_FLAGS} -x c - -o "${asan_probe}" >/dev/null 2>&1 \
   || ! "${asan_probe}" >/dev/null 2>&1; then
  echo "AddressSanitizer not usable in this environment; running without it." >&2
  SAN_FLAGS=""
fi
rm -f "${asan_probe}"

if [[ ! -x "${LIBTOOL}" ]]; then
  echo "libtool helper not found at ${LIBTOOL}" >&2
  exit 99
fi

if ! grep -q '^#define HAVE_LIBTIFF 1' "${BUILD_ROOT}/config.h" 2>/dev/null; then
  echo "libcupsfilters built without TIFF support; skipping." >&2
  exit 77
fi

if ! pkg-config --exists libtiff-4 2>/dev/null; then
  echo "libtiff-4 pkg-config file not found; skipping." >&2
  exit 77
fi

SRC="${ROOT}/test-image-tiff-scanline.c"
if [[ ! -f "${SRC}" ]]; then
  echo "test source not found: ${SRC}" >&2
  exit 99
fi

TMP_PARENT="${TMPDIR:-/tmp}"
WORKDIR="$(mktemp -d "${TMP_PARENT%/}/image-tiff-scanline.XXXXXX")"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

OBJ="${WORKDIR}/test-image-tiff-scanline.o"
BIN="${WORKDIR}/test-image-tiff-scanline"
RUN_LOG="${WORKDIR}/run.log"

# The harness writes its images with libtiff, and cupsfilters/image.h
# includes <cups/raster.h>.
PKG_CFLAGS="$(pkg-config --cflags libtiff-4)"
PKG_LIBS="$(pkg-config --libs libtiff-4)"

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

"${CC}" -std=gnu11 -O0 -D_GNU_SOURCE ${SAN_FLAGS} \
  -I"${BUILD_ROOT}" ${PKG_CFLAGS} \
  -c "${SRC}" -o "${OBJ}"

"${LIBTOOL}" --mode=link --tag=CC "${CC}" ${SAN_FLAGS} \
  "${OBJ}" "${BUILD_ROOT}/libcupsfilters.la" ${PKG_LIBS} -lm \
  -o "${BIN}" >/dev/null

: > "${RUN_LOG}"
ASAN_OPTS="${ASAN_OPTIONS:-detect_leaks=0,abort_on_error=1}"

set +e
"${LIBTOOL}" --mode=execute env ASAN_OPTIONS="${ASAN_OPTS}" \
  TMPDIR="${WORKDIR}" "${BIN}" >>"${RUN_LOG}" 2>&1
STATUS=$?
set -e

if grep -q "AddressSanitizer" "${RUN_LOG}"; then
  cat "${RUN_LOG}" >&2
  echo "AddressSanitizer reported a memory error in _cfImageReadTIFF()" >&2
  exit 1
fi

if [[ ${STATUS} -ne 0 ]]; then
  cat "${RUN_LOG}" >&2
  echo "harness exited with status ${STATUS}" >&2
  exit 1
fi

cat "${RUN_LOG}"
exit 0
