#!/usr/bin/env bash
# A custom page whose raster line is larger than INT_MAX must be rejected.
# malloc(2 * cupsBytesPerLine) is a 32-bit multiply, so a line of 2^31 bytes
# allocates nothing and the filter used to return success anyway.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ROOT="$(cd "${ROOT}/.." && pwd)"
LIBTOOL="${BUILD_ROOT}/libtool"
CC="${CC:-cc}"
SAN_FLAGS="${SAN_FLAGS:--fsanitize=address,undefined -fno-omit-frame-pointer}"
# Source-built CUPS installs headers under /usr/include/libcups3 (or cupsN),
# not on the default include path. The distro package is the one case where
# a bare -lcups compile works.
if pkg-config --exists cups 2>/dev/null; then
  PKG_CFLAGS="$(pkg-config --cflags cups)"
  PKG_LIBS="$(pkg-config --libs cups)"
elif pkg-config --exists cups3 2>/dev/null; then
  PKG_CFLAGS="$(pkg-config --cflags cups3)"
  PKG_LIBS="$(pkg-config --libs cups3)"
elif command -v cups-config >/dev/null 2>&1; then
  PKG_CFLAGS="$(cups-config --cflags)"
  PKG_LIBS="$(cups-config --libs)"
else
  PKG_CFLAGS=""
  PKG_LIBS="-lcups"
fi

if ! printf 'int main(void){return 0;}\n' | "${CC}" ${SAN_FLAGS} -x c - -o /dev/null >/dev/null 2>&1; then
  echo "AddressSanitizer/UBSan not available; skipping." >&2
  exit 77
fi
[[ -x "${LIBTOOL}" ]] || { echo "libtool helper not found" >&2; exit 99; }

WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/imagetoraster-row.XXXXXX")"
trap 'rm -rf "${WORKDIR}"' EXIT

python3 - "${WORKDIR}/one.png" << 'PY'
import struct, sys, zlib
def chunk(t, d):
    return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
raw = b"\x00\xff\x00\x00"
png = (b"\x89PNG\r\n\x1a\n"
       + chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0))
       + chunk(b"IDAT", zlib.compress(raw))
       + chunk(b"IEND", b""))
open(sys.argv[1], "wb").write(png)
PY

cat > "${WORKDIR}/harness.c" << 'EOF'
#include <cups/cups.h>
#include <cupsfilters/filter.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void logfn(void *d, cf_loglevel_t level, const char *message, ...) {
  va_list ap;
  (void)d;
  (void)level;
  va_start(ap, message);
  vfprintf(stderr, message, ap);
  fputc('\n', stderr);
  va_end(ap);
}

int main(int argc, char **argv) {
  int n = 0;
  cups_option_t *options = NULL;
  cf_filter_data_t data;
  cf_filter_out_format_t fmt = CF_FILTER_OUT_FORMAT_CUPS_RASTER;
  int in, out, ret;

  if (argc != 4) return 2;
  n = cupsAddOption("media", argv[3], n, &options);
  n = cupsAddOption("print-color-mode", "monochrome", n, &options);
  n = cupsAddOption("Resolution", "72dpi", n, &options);
  in = open(argv[1], O_RDONLY);
  out = open(argv[2], O_CREAT | O_TRUNC | O_WRONLY, 0644);
  memset(&data, 0, sizeof(data));
  data.num_options = n;
  data.options = options;
  data.logfunc = logfn;
  data.content_type = (char *)"image/png";
  ret = cfFilterImageToRaster(in, out, 1, &data, &fmt);
  fprintf(stderr, "filter_ret=%d\n", ret);
  return ret == 0 ? 0 : 1;
}
EOF

if ! "${LIBTOOL}" --mode=link --tag=CC "${CC}" ${SAN_FLAGS} ${PKG_CFLAGS} \
  -I"${BUILD_ROOT}" -I"${BUILD_ROOT}/cupsfilters" \
  "${WORKDIR}/harness.c" "${BUILD_ROOT}/libcupsfilters.la" ${PKG_LIBS} \
  -o "${WORKDIR}/harness" >"${WORKDIR}/link.log" 2>&1; then
  cat "${WORKDIR}/link.log" >&2
  echo "could not link the imagetoraster harness; skipping." >&2
  exit 77
fi

run() {
  local name="$1" media="$2"
  set +e
  "${LIBTOOL}" --mode=execute env ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
    UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}" \
    "${WORKDIR}/harness" "${WORKDIR}/one.png" "${WORKDIR}/${name}.ras" "${media}" \
    >"${WORKDIR}/${name}.log" 2>&1
  local status=$?
  set -e
  if grep -q "AddressSanitizer\|runtime error" "${WORKDIR}/${name}.log"; then
    cat "${WORKDIR}/${name}.log" >&2
    echo "FAIL: ${name} reported a sanitizer error" >&2
    exit 1
  fi
  echo "${name}: exit=${status}"
  return "${status}"
}

echo "1. Custom page whose line does not fit in the row allocation"
if run wide "Custom.200000x200000"; then
  echo "FAIL: overflowing page was accepted" >&2
  exit 1
fi

echo "2. Letter page still converts"
if ! run letter "na_letter_8.5x11in"; then
  cat "${WORKDIR}/letter.log" >&2
  echo "FAIL: letter page was rejected" >&2
  exit 1
fi
[[ -s "${WORKDIR}/letter.ras" ]] || { echo "FAIL: letter output is empty" >&2; exit 1; }

echo "PASS: imagetoraster row allocation"
exit 0
