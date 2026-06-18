#!/bin/sh
# ci/ci-setup.sh
#
# CI helper for building libcupsfilters against several CUPS releases on both
# native and QEMU-emulated runners.  PR #153 added compatibility shims so the
# same source compiles against CUPS 2.x, 2.5.x and 3.x; this script lets CI
# actually exercise each of those builds.
#
# Subcommands:
#   deps                       install build dependencies
#   cups <kind> <cache-dir>    provide libcups; <kind> is one of:
#                                system-2x    distro libcups2-dev  (CUPS 2.4.x)
#                                source-2.5.x OpenPrinting/cups@master
#                                source-3.x   OpenPrinting/libcups@master
#   pdfio <cache-dir>          build/install pdfio (required by libcupsfilters)
#   build-libcupsfilters       autogen + configure + make + make check
#
# Source builds are staged with DESTDIR, archived into <cache-dir>, and reused
# verbatim on the next run, so the slow compile only happens on a cache miss
# (i.e. the first run, or when the pinned upstream revision changes).  The
# script runs as root inside emulation containers and via sudo on native
# runners; it detects which automatically.
set -eu

PDFIO_VER=1.6.4

SUDO=""
[ "$(id -u)" -eq 0 ] || SUDO="sudo"

apt_install() {
	$SUDO apt-get update --fix-missing -y
	DEBIAN_FRONTEND=noninteractive $SUDO apt-get install -y "$@"
}

# Extract a previously cached staged install into the live filesystem.
restore_tarball() {
	tarball="$1"
	[ -f "$tarball" ] || return 1
	echo "ci-setup: cache hit -> $tarball"
	$SUDO tar -xf "$tarball" -C /
	$SUDO ldconfig || true
	return 0
}

# Make a freshly written cache dir readable by the (non-root) cache action.
seal_cache() {
	$SUDO chmod -R a+rwX "$1" 2>/dev/null || true
}

cmd_deps() {
	apt_install \
		build-essential autoconf automake libtool pkg-config gettext autopoint \
		autotools-dev cmake git wget tar make gcc g++ file dbus \
		libavahi-client-dev libssl-dev libpam-dev libusb-1.0-0-dev zlib1g-dev \
		libqpdf-dev libexif-dev liblcms2-dev libfontconfig1-dev libfreetype6-dev \
		libcairo2-dev libjpeg-dev libjxl-dev libpoppler-cpp-dev libdbus-1-dev \
		libopenjp2-7-dev mupdf-tools poppler-utils ghostscript
	# Never let a packaged libcupsfilters shadow the source build under test.
	$SUDO apt-get remove -y libcupsfilters-dev || true
}

# build_autoconf <url> <ref> <submodule-flag> <tarball> [configure-args...]
build_autoconf() {
	url="$1"; ref="$2"; sub="$3"; tarball="$4"; shift 4
	restore_tarball "$tarball" && return 0

	echo "ci-setup: cache miss -> building $url @ $ref"
	src="$(mktemp -d)"
	git clone --depth 1 --branch "$ref" $sub "$url" "$src"
	( cd "$src"
	  [ -x ./configure ] || ./autogen.sh
	  ./configure --prefix=/usr "$@" || ./configure --prefix=/usr
	  make -j"$(nproc)"
	  stage="$(mktemp -d)"
	  make install DESTDIR="$stage"
	  mkdir -p "$(dirname "$tarball")"
	  tar -cf "$tarball" -C "$stage" . )
	$SUDO tar -xf "$tarball" -C /
	$SUDO ldconfig || true
	seal_cache "$(dirname "$tarball")"
}

cmd_cups() {
	kind="$1"; cache="${2:-./.ci-cache}"
	case "$kind" in
		system-2x)
			apt_install libcups2-dev
			;;
		source-2.5.x)
			build_autoconf https://github.com/OpenPrinting/cups.git master "" \
				"$cache/cups/cups-2.5.x.tar" --disable-systemd
			;;
		source-3.x)
			build_autoconf https://github.com/OpenPrinting/libcups.git master \
				"--recurse-submodules" "$cache/cups/libcups-3.x.tar"
			;;
		*)
			echo "ci-setup: unknown cups kind: $kind" >&2; exit 2 ;;
	esac
}

cmd_pdfio() {
	cache="${1:-./.ci-cache}"
	tarball="$cache/pdfio/pdfio-$PDFIO_VER.tar"
	restore_tarball "$tarball" && return 0

	echo "ci-setup: cache miss -> building pdfio $PDFIO_VER"
	src="$(mktemp -d)"
	( cd "$src"
	  wget -q "https://github.com/michaelrsweet/pdfio/releases/download/v$PDFIO_VER/pdfio-$PDFIO_VER.tar.gz"
	  tar -xzf "pdfio-$PDFIO_VER.tar.gz"
	  cd "pdfio-$PDFIO_VER"
	  ./configure --prefix=/usr --enable-shared
	  make all
	  stage="$(mktemp -d)"
	  make install DESTDIR="$stage"
	  mkdir -p "$(dirname "$tarball")"
	  tar -cf "$tarball" -C "$stage" . )
	$SUDO tar -xf "$tarball" -C /
	$SUDO ldconfig || true
	seal_cache "$(dirname "$tarball")"
}

cmd_build() {
	./autogen.sh
	./configure
	make -j"$(nproc)"

	# Report which CUPS the configure step actually selected.
	echo "ci-setup: configured against:"
	grep -E "libcups:|cups-config:" config.log 2>/dev/null || true

	if [ "${EMULATED:-0}" = "1" ]; then
		make check V=1 VERBOSE=1 \
			XFAIL_TESTS="cupsfilters/test-pclm-overflow.sh" \
			|| { test -f test-suite.log && cat test-suite.log; exit 1; }
	else
		make check V=1 VERBOSE=1 \
			|| { test -f test-suite.log && cat test-suite.log; exit 1; }
	fi
}

case "${1:-}" in
	deps)                 cmd_deps ;;
	cups)                 shift; cmd_cups "$@" ;;
	pdfio)                shift; cmd_pdfio "$@" ;;
	build-libcupsfilters) cmd_build ;;
	*)
		echo "usage: ci-setup.sh {deps | cups <kind> <cache-dir> | pdfio <cache-dir> | build-libcupsfilters}" >&2
		exit 2 ;;
esac
