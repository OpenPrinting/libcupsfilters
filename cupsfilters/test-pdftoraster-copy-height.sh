#!/usr/bin/env bash
#
# Regression test for the pdftoraster copy_height off-by-one / out-of-bounds fix.
#
# The C harness (test-pdftoraster-copy-height.c) #includes the in-tree
# cupsfilters/pdftoraster.c so it drives the REAL copy_image_rows() helper that
# write_page_image() uses in production -- not a re-implementation.  It is built
# with AddressSanitizer and fed a colordata buffer sized to exactly copy_height
# rows while the page is one row taller, i.e. the "rendered image shorter than
# page" case where the off-by-one lived.  The buggy `h <= copy_height` then
# reads one row past the buffer and ASan aborts; the fixed `h < copy_height`
# stays in bounds.  Any future regression of that loop is therefore caught.
#
# Skips (Automake exit 77) when AddressSanitizer is unavailable.
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ROOT="$(cd "${ROOT}/.." && pwd)"
LIBTOOL="${BUILD_ROOT}/libtool"
CC="${CC:-cc}"
SAN_FLAGS="${SAN_FLAGS:--fsanitize=address -fno-omit-frame-pointer}"

# AddressSanitizer is what gives this test teeth.  It must be both linkable AND
# runnable here: without libasan the link fails, and under qemu-user emulation
# (the armhf/riscv64 legs) the ASan runtime aborts at init -- both are
# environment gaps, not libcupsfilters bugs.  Compile and RUN a trivial probe;
# skip (Automake exit 77) when ASan cannot actually run.
asan_probe="$(mktemp "${TMPDIR:-/tmp}/asan-probe.XXXXXX")"
if ! printf 'int main(void){return 0;}\n' \
       | "${CC}" ${SAN_FLAGS} -x c - -o "${asan_probe}" >/dev/null 2>&1 \
   || ! "${asan_probe}" >/dev/null 2>&1; then
  echo "AddressSanitizer not usable in this environment; skipping." >&2
  rm -f "${asan_probe}"
  exit 77
fi
rm -f "${asan_probe}"

if [[ ! -x "${LIBTOOL}" ]]; then
  echo "libtool helper not found at ${LIBTOOL}" >&2
  exit 99
fi

SRC="${ROOT}/test-pdftoraster-copy-height.c"
if [[ ! -f "${SRC}" ]]; then
  echo "test source not found: ${SRC}" >&2
  exit 99
fi

TMP_PARENT="${TMPDIR:-/tmp}"
WORKDIR="$(mktemp -d "${TMP_PARENT%/}/pdftoraster-copy-height.XXXXXX")"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

OBJ="${WORKDIR}/test-pdftoraster-copy-height.o"
BIN="${WORKDIR}/test-pdftoraster-copy-height"
RUN_LOG="${WORKDIR}/run.log"

# Flags to compile the harness (it pulls in pdftoraster.c -> needs config.h, the
# internal headers and pdftoraster.c's own dependencies).  Fall back to cups3.
PKG_CFLAGS="$(pkg-config --cflags lcms2 pdfio cups 2>/dev/null \
              || pkg-config --cflags lcms2 pdfio cups3 2>/dev/null || true)"
PKG_LIBS="$(pkg-config --libs lcms2 pdfio cups 2>/dev/null \
            || pkg-config --libs lcms2 pdfio cups3 2>/dev/null || true)"
INCLUDES="-I${BUILD_ROOT} -I${BUILD_ROOT}/cupsfilters"

# Compile the harness (which #includes the real pdftoraster.c) under ASan.
"${CC}" -std=gnu11 -O0 -D_GNU_SOURCE ${SAN_FLAGS} \
  ${INCLUDES} ${PKG_CFLAGS} \
  -c "${SRC}" -o "${OBJ}"

# Link against libcupsfilters.la for the symbols pdftoraster.c references.
# ${PKG_LIBS} carries the right CUPS library (-lcups or -lcups3); do not force
# -lcups, which is absent on the libcups3 (source-3.x) leg.
"${LIBTOOL}" --mode=link --tag=CC "${CC}" ${SAN_FLAGS} \
  "${OBJ}" "${BUILD_ROOT}/libcupsfilters.la" ${PKG_LIBS} -lm \
  -o "${BIN}" >/dev/null

: > "${RUN_LOG}"
ASAN_OPTS="${ASAN_OPTIONS:-detect_leaks=0,abort_on_error=1}"

set +e
"${LIBTOOL}" --mode=execute env ASAN_OPTIONS="${ASAN_OPTS}" \
  "${BIN}" >>"${RUN_LOG}" 2>&1
STATUS=$?
set -e

if grep -q "AddressSanitizer" "${RUN_LOG}"; then
  cat "${RUN_LOG}" >&2
  echo "AddressSanitizer reported a memory error in copy_image_rows()" >&2
  exit 1
fi

if [[ ${STATUS} -ne 0 ]]; then
  cat "${RUN_LOG}" >&2
  echo "harness exited with status ${STATUS}" >&2
  exit 1
fi

exit 0
