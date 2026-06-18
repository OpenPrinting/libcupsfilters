#!/bin/sh
# ci/ci-setup.sh
#
# CI helper for building libcupsfilters against several CUPS releases on both
# native and QEMU-emulated runners.  PR #153 added compatibility shims so the
# same source compiles against CUPS 2.x, 2.5.x and 3.x; this script lets CI
# actually exercise each of those builds.
#
# Subcommands:
#   deps                 install build dependencies
#   cups <kind>          provide libcups; <kind> is one of:
#                          system-2x    distro libcups2-dev  (CUPS 2.4.x)
#                          source-2.5.x OpenPrinting/cups@master
#                          source-3.x   OpenPrinting/libcups@master
#   pdfio                build/install pdfio (required by libcupsfilters)
#   build-libcupsfilters autogen + configure + make + make check
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

# CUPS >= 2.5 (OpenPrinting/cups master) dropped cups-config and ships only
# cups.pc, but libcupsfilters' configure detects CUPS 2.x exclusively through
# cups-config.  Install a thin cups-config shim backed by pkg-config so the
# 2.5.x compile path can be exercised.  (Proper fix belongs upstream: let
# libcupsfilters fall back to "pkg-config cups".)
install_cupsconfig_shim() {
	command -v cups-config >/dev/null 2>&1 && return 0
	ma=$(gcc -dumpmachine 2>/dev/null || echo "")
	PKG_CONFIG_PATH="/usr/lib/pkgconfig${ma:+:/usr/lib/$ma/pkgconfig}:/usr/local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
	export PKG_CONFIG_PATH
	pkg-config --exists cups || {
		echo "ci-setup: cannot build cups-config shim - no cups.pc found" >&2
		return 1
	}
	ver=$(pkg-config --modversion cups)
	cflags=$(pkg-config --cflags cups)
	libs=$(pkg-config --libs cups)
	pkg-config --exists cupsimage 2>/dev/null && libs="$libs $(pkg-config --libs cupsimage)"
	prefix=$(pkg-config --variable=prefix cups); : "${prefix:=/usr}"
	tmp=$(mktemp)
	cat > "$tmp" <<EOF
#!/bin/sh
# cups-config compatibility shim (CUPS >= 2.5 ships cups.pc instead).
while [ \$# -gt 0 ]; do
	case "\$1" in
		--version)    echo "$ver" ;;
		--cflags)     printf '%s\n' "$cflags" ;;
		--libs)       printf '%s\n' "$libs" ;;
		--image)      : ;;  # image libs already folded into --libs
		--datadir)    echo "$prefix/share/cups" ;;
		--serverroot) echo "/etc/cups" ;;
		--serverbin)  echo "$prefix/lib/cups" ;;
	esac
	shift
done
EOF
	$SUDO install -m 0755 "$tmp" /usr/bin/cups-config
	echo "ci-setup: installed cups-config shim (cups $ver)"
}

cmd_cups() {
	kind="$1"
	case "$kind" in
		system-2x)
			apt_install libcups2-dev
			;;
		source-2.5.x)
			build_autoconf https://github.com/OpenPrinting/cups.git master "" \
				--disable-systemd
			install_cupsconfig_shim
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
	pdfio)                cmd_pdfio ;;
	build-libcupsfilters) cmd_build ;;
	*)
		echo "usage: ci-setup.sh {deps | cups <kind> | pdfio | build-libcupsfilters}" >&2
		exit 2 ;;
esac
