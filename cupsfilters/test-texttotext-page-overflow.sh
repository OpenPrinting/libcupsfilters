#!/usr/bin/env bash
#
# Regression test for the cfFilterTextToText() page_size overflow fix.
#
# The C harness (test-texttotext-page-overflow.c) #includes the in-tree
# cupsfilters/texttotext.c so it drives the REAL cfFilterTextToText() entry
# point that a job goes through in production, not a re-implementation. It
# is built with AddressSanitizer and fed a job whose "PageWidth"/"PageHeight"
# options describe a page wide enough to overflow the page_size computation
# a few lines below (page_size = ((num_columns + 2) * num_lines + 2) * 4,
# done in a plain int). The unfixed code hands calloc() the wrapped, far too
# small size while the formatting loop still writes according to the real
# column count, so ASan aborts on the very first line; with the overflow
# guarded, cfFilterTextToText() falls back to the default page size instead.
#
# Skips (Automake exit 77) when AddressSanitizer is unavailable.
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ROOT="$(cd "${ROOT}/.." && pwd)"
LIBTOOL="${BUILD_ROOT}/libtool"
CC="${CC:-cc}"
SAN_FLAGS="${SAN_FLAGS:--fsanitize=address -fno-omit-frame-pointer}"

# AddressSanitizer is what gives this test teeth. It must be both linkable AND
# runnable here: without libasan the link fails, and under qemu-user emulation
# (the armhf/riscv64 legs) the ASan runtime aborts at init -- both are
# environment gaps, not libcupsfilters bugs. Compile and RUN a trivial probe;
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

SRC="${ROOT}/test-texttotext-page-overflow.c"
if [[ ! -f "${SRC}" ]]; then
  echo "test source not found: ${SRC}" >&2
  exit 99
fi

TMP_PARENT="${TMPDIR:-/tmp}"
WORKDIR="$(mktemp -d "${TMP_PARENT%/}/texttotext-page-overflow.XXXXXX")"
cleanup() { rm -rf "${WORKDIR}"; }
trap cleanup EXIT

OBJ="${WORKDIR}/test-texttotext-page-overflow.o"
BIN="${WORKDIR}/test-texttotext-page-overflow"
RUN_LOG="${WORKDIR}/run.log"

# Flags to compile the harness (it pulls in texttotext.c -> needs config.h,
# the internal headers and texttotext.c's own dependencies). Fall back to
# cups3.
PKG_CFLAGS="$(pkg-config --cflags fontconfig pdfio cups 2>/dev/null \
              || pkg-config --cflags fontconfig pdfio cups3 2>/dev/null || true)"
PKG_LIBS="$(pkg-config --libs fontconfig pdfio cups 2>/dev/null \
            || pkg-config --libs fontconfig pdfio cups3 2>/dev/null || true)"
INCLUDES="-I${BUILD_ROOT} -I${BUILD_ROOT}/cupsfilters"

# Compile the harness (which #includes the real texttotext.c) under ASan.
"${CC}" -std=gnu11 -O0 -D_GNU_SOURCE ${SAN_FLAGS} \
  ${INCLUDES} ${PKG_CFLAGS} \
  -c "${SRC}" -o "${OBJ}"

# Link against libcupsfilters.la for the symbols texttotext.c references
# (cfGetPageDimensions). ${PKG_LIBS} carries the right CUPS library (-lcups
# or -lcups3); do not force -lcups, which is absent on the libcups3
# (source-3.x) leg.
"${LIBTOOL}" --mode=link --tag=CC "${CC}" ${SAN_FLAGS} \
  "${OBJ}" "${BUILD_ROOT}/libcupsfilters.la" ${PKG_LIBS} -liconv -lm \
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
  echo "AddressSanitizer reported a memory error in cfFilterTextToText()" >&2
  exit 1
fi

if [[ ${STATUS} -ne 0 ]]; then
  cat "${RUN_LOG}" >&2
  echo "harness exited with status ${STATUS}" >&2
  exit 1
fi

exit 0
