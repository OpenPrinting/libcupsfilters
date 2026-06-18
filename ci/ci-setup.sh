#!/bin/sh
# ci/ci-setup.sh
#
# CI helper for building libcupsfilters against several CUPS releases on both
# native and QEMU-emulated runners.  The same source compiles against CUPS
# 2.4.x, 2.5.x and 3.x; this script provides each of those CUPS builds and then
# builds and tests libcupsfilters against it.
#
# Subcommands:
#   deps                 install build dependencies
#   cups <kind>          provide libcups; <kind> is one of:
#                          system-2x    distro libcups2-dev  (CUPS 2.4.x)
#                          source-2.5.x OpenPrinting/cups@master    (CUPS 2.5.x)
#                          source-3.x   OpenPrinting/libcups@master (libcups3)
#   pdfio                build/install pdfio (required by libcupsfilters)
#   build-libcupsfilters autogen + configure + make + make check
#
# Environment knobs honoured by build-libcupsfilters:
#   CUPS_KIND   the <kind> above (controls test XFAILs for source CUPS)
#   EMULATED    "1" when running under QEMU emulation (controls test XFAILs)
#
# The script runs as root inside emulation containers and via sudo on native
# runners; it detects which automatically.
set -eu

PDFIO_VER=1.6.4

SUDO=""
[ "$(id -u)" -eq 0 ] || SUDO="sudo"

# Make apt completely non-interactive.  Native GitHub runners ship needrestart,
# whose service-restart prompt otherwise hangs the job forever; the emulated
# containers do not have it, which is why only the native legs stalled.
export DEBIAN_FRONTEND=noninteractive
export NEEDRESTART_MODE=a
export NEEDRESTART_SUSPEND=1

# Source-built CUPS installs its .pc file under $prefix/lib/pkgconfig; make sure
# pkg-config (and therefore libcupsfilters' configure) can find it.
ma=$(gcc -dumpmachine 2>/dev/null || echo "")
PKG_CONFIG_PATH="/usr/lib/pkgconfig${ma:+:/usr/lib/$ma/pkgconfig}:/usr/local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export PKG_CONFIG_PATH

apt_install() {
	$SUDO apt-get update --fix-missing -y
	$SUDO apt-get install -y "$@"
}

cmd_deps() {
	apt_install \
		build-essential autoconf automake libtool pkg-config gettext autopoint \
		autotools-dev cmake git wget tar make gcc g++ file dbus \
		libavahi-client-dev libssl-dev libpam-dev libusb-1.0-0-dev zlib1g-dev \
		libqpdf-dev libexif-dev liblcms2-dev libfontconfig1-dev libfreetype6-dev \
		libcairo2-dev libjpeg-dev libpng-dev libtiff-dev libjxl-dev \
		libpoppler-cpp-dev libdbus-1-dev libopenjp2-7-dev \
		mupdf-tools poppler-utils ghostscript
	# Never let a packaged libcupsfilters shadow the source build under test.
	$SUDO apt-get remove -y libcupsfilters-dev || true
}

# build_autoconf <url> <ref> <submodule-flag> [configure-args...]
build_autoconf() {
	url="$1"; ref="$2"; sub="$3"; shift 3
	echo "ci-setup: building $url @ $ref"
	src="$(mktemp -d)"
	git clone --depth 1 --branch "$ref" $sub "$url" "$src"
	( cd "$src"
	  [ -x ./configure ] || ./autogen.sh
	  ./configure --prefix=/usr "$@" || ./configure --prefix=/usr
	  make -j"$(nproc)"
	  $SUDO make install )
	$SUDO ldconfig || true
}

cmd_cups() {
	kind="$1"
	case "$kind" in
		system-2x)
			apt_install libcups2-dev
			;;
		source-2.5.x)
			# CUPS 2.5 (OpenPrinting/cups master) ships cups.pc and has dropped
			# cups-config; libcupsfilters' configure now detects it via
			# pkg-config, so no cups-config shim is needed.
			#
			# Force the multiarch libdir: CUPS's configure otherwise installs
			# libcups into /usr/lib64 on 64-bit hosts, which is not on the
			# default linker search path.  libcupsfilters now re-exports CUPS
			# string/option symbols, so a downstream consumer that links only
			# "-lcupsfilters" must be able to find libcups transitively at link
			# time - which only works if libcups sits in a default search dir.
			build_autoconf https://github.com/OpenPrinting/cups.git master "" \
				--disable-systemd ${ma:+--libdir=/usr/lib/$ma}
			;;
		source-3.x)
			build_autoconf https://github.com/OpenPrinting/libcups.git master \
				"--recurse-submodules"
			;;
		*)
			echo "ci-setup: unknown cups kind: $kind" >&2; exit 2 ;;
	esac
}

cmd_pdfio() {
	echo "ci-setup: building pdfio $PDFIO_VER"
	src="$(mktemp -d)"
	( cd "$src"
	  wget -q "https://github.com/michaelrsweet/pdfio/releases/download/v$PDFIO_VER/pdfio-$PDFIO_VER.tar.gz"
	  tar -xzf "pdfio-$PDFIO_VER.tar.gz"
	  cd "pdfio-$PDFIO_VER"
	  ./configure --prefix=/usr --enable-shared
	  make all
	  $SUDO make install )
	$SUDO ldconfig || true
}

cmd_build() {
	./autogen.sh
	./configure
	make -j"$(nproc)"

	# Report which CUPS the configure step actually selected.
	echo "ci-setup: configured against:"
	grep -E "libcups:|cups-config:" config.log 2>/dev/null || true

	# test-pclm-overflow.sh compiles a helper that #includes <cups/raster.h>
	# with a bare gcc.  That only works when CUPS headers sit in the default
	# path - true for the distro package, but not for a source-installed CUPS
	# (headers under /usr/include/cupsN) or under QEMU.  Mark it XFAIL in those
	# cases so the environment quirk does not fail the suite.
	xfail=""
	case "${CUPS_KIND:-}" in source-*) xfail="cupsfilters/test-pclm-overflow.sh" ;; esac
	[ "${EMULATED:-0}" = "1" ] && xfail="cupsfilters/test-pclm-overflow.sh"

	if [ -n "$xfail" ]; then
		make check V=1 VERBOSE=1 XFAIL_TESTS="$xfail" \
			|| { test -f test-suite.log && cat test-suite.log; exit 1; }
	else
		make check V=1 VERBOSE=1 \
			|| { test -f test-suite.log && cat test-suite.log; exit 1; }
	fi
}

case "${1:-}" in
	deps)                 cmd_deps ;;
	cups)                 shift; cmd_cups "$@" ;;
	pdfio)                cmd_pdfio ;;
	build-libcupsfilters) cmd_build ;;
	*)
		echo "usage: ci-setup.sh {deps | cups <kind> | pdfio | build-libcupsfilters}" >&2
		exit 2 ;;
esac
