#!/bin/sh
# ci/autopkgtest/run.sh
#
# Universal DESTDIR-staging wrapper for the downstream Debian autopkgtests.
# Points PATH / LD_LIBRARY_PATH / PKG_CONFIG_PATH at the staged install tree
# ($CIROOT) produced by `make stage-ciroot`, then runs the unmodified
# downstream scripts vendored under ci/autopkgtest/debian-tests/.
#
# The downstream scripts take environment overrides (e.g. LIBCUPSFILTERS_BINDIR,
# LIBCUPSFILTERS_TESTDIR) that default to the system /usr paths but can be
# redirected into the staging tree.  That keeps every test free of absolute-path
# assumptions, so the wrapper needs no privilege and no bind mounts and runs
# identically on native and QEMU-emulated architectures.
#
# Env in:
#   CIROOT             staging root        (default: $PWD/_ciroot)
#   CIPREFIX           configured prefix   (default: /usr)
#   TOP_BUILDDIR       build tree          (default: $PWD)
#   Any extra exported variables (e.g. LIBCUPSFILTERS_BINDIR,
#   LIBCUPSFILTERS_TESTDIR) are passed straight through to the scripts.
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TESTS_DIR="$SCRIPT_DIR/debian-tests"

: "${CIROOT:=$PWD/_ciroot}"
: "${CIPREFIX:=/usr}"
: "${TOP_BUILDDIR:=$PWD}"

if [ ! -d "$CIROOT" ]; then
    echo "run.sh: staging root not found: $CIROOT (run 'make stage-ciroot' first)" >&2
    exit 1
fi

ROOT="$CIROOT$CIPREFIX"
MULTIARCH=$(dpkg-architecture -qDEB_HOST_MULTIARCH 2>/dev/null \
            || gcc -dumpmachine 2>/dev/null || echo "")

PATH="$ROOT/bin:$ROOT/sbin:$TOP_BUILDDIR:$TOP_BUILDDIR/.libs:$PATH"
LD_LIBRARY_PATH="$ROOT/lib${MULTIARCH:+:$ROOT/lib/$MULTIARCH}:$TOP_BUILDDIR/.libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
PKG_CONFIG_PATH="$ROOT/lib/pkgconfig${MULTIARCH:+:$ROOT/lib/$MULTIARCH/pkgconfig}:$ROOT/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export PATH LD_LIBRARY_PATH PKG_CONFIG_PATH

if [ "$#" -eq 0 ]; then
    echo "run.sh: usage: run.sh <test-name> [test-name...]" >&2
    exit 2
fi

rc=0
total=0
n_pass=0
n_fail=0
results=""
for name in "$@"; do
    total=$((total + 1))
    script="$TESTS_DIR/$name"
    if [ ! -f "$script" ]; then
        echo "run.sh: no such test: $script" >&2
        n_fail=$((n_fail + 1))
        results="$results
FAIL: $name (not found)"
        rc=1
        continue
    fi
    chmod +x "$script" 2>/dev/null || true
    workdir=$(mktemp -d)
    echo "=== autopkgtest: $name (CIROOT=$CIROOT, prefix=$CIPREFIX) ==="
    if ( cd "$workdir" && "$script" ); then
        echo "=== PASS: $name ==="
        n_pass=$((n_pass + 1))
        results="$results
PASS: $name"
    else
        ec=$?
        echo "=== FAIL: $name (exit $ec) ===" >&2
        n_fail=$((n_fail + 1))
        results="$results
FAIL: $name (exit $ec)"
        rc=1
    fi
    rm -rf "$workdir"
done

echo "============================================================================"
echo "Downstream autopkgtest summary"
echo "============================================================================"
printf '# TOTAL: %d\n' "$total"
printf '# PASS:  %d\n' "$n_pass"
printf '# FAIL:  %d\n' "$n_fail"
echo "----------------------------------------------------------------------------"
printf '%s\n' "$results" | sed '/^$/d'
echo "============================================================================"

exit $rc
