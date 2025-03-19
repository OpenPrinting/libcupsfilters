# Process this file with autoconf to create configure.

AC_PREREQ([2.65])

# ====================
# Version informations
# ====================
AC_INIT([libcupsfilters], [2.1.1], [https://github.com/OpenPrinting/libcupsfilters/issues], [libcupsfilters], [https://github.com/OpenPrinting/libcupsfilters/])
libcupsfilters_version="AC_PACKAGE_VERSION"
libcupsfilters_version_major="`echo AC_PACKAGE_VERSION | awk -F. '{print $1}'`"
libcupsfilters_version_major="`echo AC_PACKAGE_VERSION | awk -F. '{printf("%d\n",$2);}'`"

# =============
# Automake init
# =============
AC_CONFIG_MACRO_DIR([m4])
m4_include([m4/ac_define_dir.m4])
m4_include([m4/ax_compare_version.m4])
m4_include([m4/basic-directories.m4])
AM_INIT_AUTOMAKE([1.11 gnu dist-xz dist-bzip2 subdir-objects foreign])
AM_SILENT_RULES([yes])
AC_LANG([C++])
AC_CONFIG_HEADERS([config.h])
# Extra defines for the config.h
AH_BOTTOM([
#ifdef HAVE_LONG_LONG
#  define CUPS_LLFMT   "%lld"
#  define CUPS_LLCAST  (long long)
#else
#  define CUPS_LLFMT   "%ld"
#  define CUPS_LLCAST  (long)
#endif /* HAVE_LONG_LONG */

#ifdef HAVE_ARC4RANDOM
#  define CUPS_RAND() arc4random()
#  define CUPS_SRAND(v) arc4random_stir()
#elif defined(HAVE_RANDOM)
#  define CUPS_RAND() random()
#  define CUPS_SRAND(v) srandom(v)
#elif defined(HAVE_LRAND48)
#  define CUPS_RAND() lrand48()
#  define CUPS_SRAND(v) srand48(v)
#else
#  define CUPS_RAND() rand()
#  define CUPS_SRAND(v) srand(v)
#endif /* HAVE_ARC4RANDOM */
])

# ===========================
# Find required base packages
# ===========================
AC_PROG_CC
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX([11],[noext],[mandatory])
AM_PROG_CC_C_O
AM_ICONV
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
LT_INIT

# ==========
# pkg-config
# ==========
PKG_PROG_PKG_CONFIG([0.20])
AC_PATH_TOOL([PKGCONFIG], [pkg-config])

AM_GNU_GETTEXT_VERSION([0.18.3])
AM_GNU_GETTEXT([external])

# ====
# CUPS
# ====
AC_MSG_CHECKING([for CUPS library v3.0 or higher])
AS_IF([$PKGCONFIG --exists cups3], [
    CUPS_VERSION="$($PKGCONFIG --modversion cups3)"
    AC_MSG_RESULT([yes, v$CUPS_VERSION])

    CUPS_CFLAGS="$($PKGCONFIG --cflags cups3)"
    CUPS_LIBS="$LIBS $($PKGCONFIG --libs cups3)"
    AC_SUBST(CUPS_CFLAGS)
    AC_SUBST(CUPS_LIBS)

    CUPS_DATADIR="$($PKGCONFIG --variable=prefix cups3)/share/cups"
    AC_DEFINE_UNQUOTED(CUPS_DATADIR, "$CUPS_DATADIR", [CUPS datadir])
    AC_SUBST(CUPS_DATADIR)

    AS_IF([test x$host_os_name = xdarwin], [
	CUPS_SERVERROOT="/private/etc/cups"
    ], [
	AS_IF([test "x$($PKGCONFIG --variable=prefix cups3)" = x/usr], [
	    CUPS_SERVERROOT="/etc/cups"
	], [
	    CUPS_SERVERROOT="$($PKGCONFIG --variable=prefix cups3)/etc/cups"
	])
    ])
    AC_DEFINE_UNQUOTED(CUPS_SERVERROOT, "$CUPS_SERVERROOT", [CUPS serverroot])
    AC_SUBST(CUPS_SERVERROOT)

    CUPS_FONTPATH="$CUPS_DATADIR/fonts"
    AC_DEFINE_UNQUOTED(CUPS_FONTPATH, "$CUPS_FONTPATH",
		       [Path to CUPS fonts dir])
    AC_SUBST(CUPS_FONTPATH)

    CUPS_SERVERBIN="$($PKGCONFIG --variable=prefix cups3)/lib/cups"
    AC_DEFINE_UNQUOTED(CUPS_SERVERBIN, "$CUPS_SERVERBIN",
		       [Path to CUPS binaries dir])
    AC_SUBST(CUPS_SERVERBIN)
], [
    AC_MSG_RESULT(no)
    AC_ARG_WITH([cups-config],
		[AS_HELP_STRING([--with-cups-config=path], [Specify path to cups-config executable.])],
		[with_cups_config="$withval"],
		[with_cups_config=system])
    AS_IF([test "x$with_cups_config" != "xsystem"], [
	CUPSCONFIG=$with_cups_config
    ], [
	AC_PATH_TOOL(CUPSCONFIG, [cups-config])
    ])
    AC_MSG_CHECKING([for CUPS library v2.x])
    AS_IF([test -n "$CUPSCONFIG"], [
	CUPS_VERSION=`$CUPSCONFIG --version`
	AC_MSG_RESULT([yes, v$CUPS_VERSION])

	AC_DEFINE([HAVE_LIBCUPS2], [1], [Use libcups2])

	CUPS_CFLAGS=`$CUPSCONFIG --cflags`
	CUPS_LIBS=`$CUPSCONFIG --image --libs`
	AC_SUBST(CUPS_CFLAGS)
	AC_SUBST(CUPS_LIBS)

	CUPS_DATADIR="`$CUPSCONFIG --datadir`"
	AC_DEFINE_UNQUOTED(CUPS_DATADIR, "$CUPS_DATADIR", [CUPS datadir])
	AC_SUBST(CUPS_DATADIR)

	CUPS_SERVERROOT="`$CUPSCONFIG --serverroot`"
	AC_DEFINE_UNQUOTED(CUPS_SERVERROOT, "$CUPS_SERVERROOT",
			   [CUPS serverroot])
	AC_SUBST(CUPS_SERVERROOT)

	CUPS_FONTPATH="$CUPS_DATADIR/fonts"
	AC_DEFINE_UNQUOTED(CUPS_FONTPATH, "$CUPS_FONTPATH",
			   [Path to CUPS fonts dir])
	AC_SUBST(CUPS_FONTPATH)

	CUPS_SERVERBIN="`$CUPSCONFIG --serverbin`"
	AC_DEFINE_UNQUOTED(CUPS_SERVERBIN, "$CUPS_SERVERBIN",
			   [Path to CUPS binaries dir])
	AC_SUBST(CUPS_SERVERBIN)
    ], [
	AC_MSG_RESULT(no)
	AC_MSG_ERROR([Required cups-config is missing. Please install CUPS developer packages.])
    ])
])

AC_DEFINE_UNQUOTED(CUPS_SERVERROOT, "$CUPS_SERVERROOT", [Location of CUPS config files])

# Transient run-time state dir of CUPS
CUPS_STATEDIR=""
AC_ARG_WITH(cups-rundir, [  --with-cups-rundir           set transient run-time state directory of CUPS],CUPS_STATEDIR="$withval",[
        case "$uname" in
                Darwin*)
                        # Darwin (OS X)
                        CUPS_STATEDIR="$CUPS_SERVERROOT"
                        ;;
                *)
                        # All others
                        CUPS_STATEDIR="$localstatedir/run/cups"
                        ;;
        esac])
AC_DEFINE_UNQUOTED(CUPS_STATEDIR, "$CUPS_STATEDIR", [Transient run-time state dir of CUPS])
AC_SUBST(CUPS_STATEDIR)

# ======================
# Check system functions
# ======================
AC_CHECK_FUNCS(sigaction)
AC_CHECK_FUNCS(waitpid wait3)
AC_CHECK_FUNCS(strtoll)
AC_CHECK_FUNCS(open_memstream)
AC_CHECK_FUNCS(getline,[],AC_SUBST([GETLINE],['bannertopdf-getline.$(OBJEXT)']))
AC_CHECK_FUNCS(strcasestr,[],AC_SUBST([STRCASESTR],['pdftops-strcasestr.$(OBJEXT)']))
AC_SEARCH_LIBS(pow, m)
dnl Checks for string functions.
AC_CHECK_FUNCS(strdup strlcat strlcpy)
if test "$host_os_name" = "hp-ux" -a "$host_os_version" = "1020"; then
        echo Forcing snprintf emulation for HP-UX.
else
        AC_CHECK_FUNCS(snprintf vsnprintf)
fi

# ========================
# Check for system headers
# ========================
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([sys/stat.h])
AC_CHECK_HEADERS([sys/types.h])
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([endian.h])
AC_CHECK_HEADERS([dirent.h])
AC_CHECK_HEADERS([sys/ioctl.h])
AC_CHECK_HEADER(string.h,AC_DEFINE(HAVE_STRING_H))
AC_CHECK_HEADER(strings.h,AC_DEFINE(HAVE_STRINGS_H))

# =============
# Image options
# =============
AC_ARG_ENABLE([imagefilters],
	[AS_HELP_STRING([--disable-imagefilters], [Build the image filters.])],
	[enable_imagefilters="$enableval"],
	[enable_imagefilters=yes]
)
AM_CONDITIONAL([ENABLE_IMAGEFILTERS], [test "x$enable_imagefilters" != "xno"])

# Libraries
AC_ARG_WITH([jpeg],
	[AS_HELP_STRING([--without-jpeg], [Disable jpeg support.])],
	[with_jpeg="$withval"],
	[with_jpeg=yes]
)
AS_IF([test x"$with_jpeg" != "xno"], [
	AC_DEFINE([HAVE_LIBJPEG], [1], [Defines if we provide jpeg library.])
	AC_CHECK_HEADERS([jpeglib.h])
	AC_SEARCH_LIBS([jpeg_destroy_decompress],
		[jpeg],
		LIBJPEG_LIBS="-ljpeg",
		AC_MSG_ERROR([jpeg libraries not found.])
	)
	AC_SUBST(LIBJPEG_LIBS)
])

AC_ARG_WITH([png],
	[AS_HELP_STRING([--without-png], [Disable png support.])],
	[with_png="$withval"],
	[with_png=yes]
)
AS_IF([test x"$with_png" != "xno"], [
	PKG_CHECK_MODULES([LIBPNG], [libpng])
	AC_DEFINE([HAVE_LIBPNG], [1], [Defines if we provide png library.])
])

AC_ARG_WITH([tiff],
	[AS_HELP_STRING([--without-tiff], [Disable tiff support.])],
	[with_tiff="$withval"],
	[with_tiff=yes]
)
AS_IF([test x"$with_tiff" != "xno"], [
	AC_DEFINE([HAVE_LIBTIFF], [1], [Defines if we provide tiff library.])
	AC_CHECK_HEADERS([tiff.h])
	AC_SEARCH_LIBS([TIFFReadScanline],
		[tiff],
		LIBJPEG_LIBS="-ltiff",
		AC_MSG_ERROR([tiff libraries not found.])
	)
	AC_SUBST(LIBTIFF_LIBS)
])

dnl including EXIF library
EXIF_LIBS=""
EXIF_CFLAGS=""

AC_ARG_ENABLE([exif],
	[AS_HELP_STRING([--disable-exif], [Disable exif metadata readout from images])],
	[enable_exif="$enableval"],
	[enable_exif=yes]
)
AM_CONDITIONAL([ENABLE_EXIF], [test "x$enable_exif" != "xno"])

AC_ARG_WITH(exif-libs,
	[AS_HELP_STRING([--with-exif-libs], [Set directory for exif library.])],
	EXIF_LIBS="-L$withval $EXIF_LIBS",)
AC_ARG_WITH(exif-includes,
	[AS_HELP_STRING([--with-exif-includes], [Set directory for exif includes.])],
	EXIF_CFLAGS="-I$withval $EXIF_CFLAGS",)

if test "x$enable_exif" != xno; then
	PKG_CHECK_MODULES(EXIF, libexif,
		[AC_DEFINE(HAVE_EXIF, [1], [Define if you have the libexif library])])
fi

AC_SUBST(EXIF_LIBS)
AC_SUBST(EXIF_CFLAGS)

# ==========================================
# Check for various required modules for PDF
# ==========================================
PKG_CHECK_MODULES([LCMS], [lcms2], [lcms2=yes], [lcms2=no])
AS_IF([test x"$lcms2" = "xno"], [
	PKG_CHECK_MODULES([LCMS], [lcms])
	AC_DEFINE([USE_LCMS1], [1], [Defines if use lcms1])
])
AC_ARG_WITH([fontconfig],
	[AS_HELP_STRING([--without-fontconfig], [Disable fontconfig support.])],
	[with_fontconfig="$withval"],
	[with_fontconfig=yes]
)
# --disable-texttopdf will do the same thing as --without-fontconfig.
AC_ARG_ENABLE([texttopdf],
	[AS_HELP_STRING([--disable-texttopdf], [Disable the texttopdf filter.])],
	[enable_texttopdf="$enableval"],
	[enable_texttopdf=yes]
)
AS_IF([test x"$with_fontconfig" != "xno" && test "x$enable_texttopdf" != "xno"], [
	AC_DEFINE([HAVE_FONTCONFIG], [1], [Defines if we are using fontconfig.])
  PKG_CHECK_MODULES([FONTCONFIG], [fontconfig >= 2.0.0])
])
PKG_CHECK_MODULES([LIBQPDF], [libqpdf >= 11.0.0])

# =================
# Check for Poppler
# =================
AC_ARG_ENABLE(poppler, AS_HELP_STRING([--enable-poppler],[enable Poppler-based filters]),
	      enable_poppler=$enableval,enable_poppler=yes)
AM_CONDITIONAL([ENABLE_POPPLER], test x$enable_poppler = xyes)
if test x$enable_poppler = xyes; then
	PKG_CHECK_MODULES([POPPLER], [poppler-cpp >= 0.19])
	AC_CHECK_HEADER([poppler/cpp/poppler-version.h], [AC_DEFINE([HAVE_CPP_POPPLER_VERSION_H],[1],[Define if you have Poppler's "cpp/poppler-version.h" header file.])], [])
	AC_DEFINE([HAVE_POPPLER], [1], [Define that we provide Poppler library])
fi

# ===============
# Check for D-Bus
# ===============
AC_ARG_ENABLE(dbus, AS_HELP_STRING([--enable-dbus],[enable DBus CMS code]),
	      enable_dbus=$enableval,enable_dbus=yes)
AM_CONDITIONAL(BUILD_DBUS, test x$enable_dbus = xyes)
if test x$enable_dbus = xyes; then
	PKG_CHECK_MODULES(DBUS, dbus-1)
fi

# ===================================
# Check for large files and long long
# ===================================
AC_SYS_LARGEFILE
LARGEFILE=""
AS_IF([test x"$enable_largefile" != "xno"], [
	LARGEFILE="-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE"
	AS_IF([test x"$ac_cv_sys_large_files" = "x1"], [LARGEFILE="$LARGEFILE -D_LARGE_FILES"])
	AS_IF([test x"$ac_cv_sys_file_offset_bits" = "x64"], [LARGEFILE="$LARGEFILE -D_FILE_OFFSET_BITS=64"])
])
AC_SUBST(LARGEFILE)

AC_CHECK_TYPE(long long, [long_long_found=yes], [long_long_found=no])
AS_IF([test x"$long_long_found" = "xyes"], [
	AC_DEFINE([HAVE_LONG_LONG], [1], [Platform supports long long type])
])

# =====================
# Check for Ghostscript
# =====================
AC_ARG_ENABLE([ghostscript],
	[AS_HELP_STRING([--disable-ghostscript], [Disable filters using Ghostscript.])],
	[enable_ghostscript="$enableval"],
	[enable_ghostscript=yes]
)
AC_ARG_WITH([gs-path],
	[AS_HELP_STRING([--with-gs-path=value], [Set path to ghostcript binary (default: system).])],
	[with_gs_path="$withval"],
	[with_gs_path=system]
)
AC_ARG_WITH([ippfind-path],
        [AS_HELP_STRING([--with-ippfind-path=value], [Set path to ippfind binary (default: system).])],
        [with_ippfind_path="$withval"],
        [with_ippfind_path=system]
)

CUPS_GHOSTSCRIPT=""
AS_IF([test "x$enable_ghostscript" != "xyes"], [
       with_gs_path=""
], [
	AS_IF([test "x$with_gs_path" != "xsystem"], [
		CUPS_GHOSTSCRIPT="$with_gs_path"
	], [
		AS_IF([test "x$cross_compiling" = "xyes"], [
			CUPS_GHOSTSCRIPT="gs"
		], [
			AC_CHECK_PROG(CUPS_GHOSTSCRIPT, gs, gs)
		])
	])
	AS_IF([test "x$CUPS_GHOSTSCRIPT" = "x"], [
		AC_MSG_ERROR([Required gs binary is missing. Please install ghostscript package.])
	])
	AC_DEFINE([HAVE_GHOSTSCRIPT], [1], [Define that we provide ghostscript binary])
])
AM_CONDITIONAL(ENABLE_GHOSTSCRIPT, test "x$enable_ghostscript" = xyes)
AC_SUBST(CUPS_GHOSTSCRIPT)

# ================
# Check for mutool
# ================
AC_ARG_ENABLE([mutool],
	[AS_HELP_STRING([--disable-mutool], [Disable filters using mutool.])],
	[enable_mutool="$enableval"],
	[enable_mutool=yes]
)

AC_ARG_WITH([mutool-path],
	[AS_HELP_STRING([--with-mutool-path=value], [Set path to mutool binary (default: system).])],
	[with_mutool_path="$withval"],
	[with_mutool_path=system]
)

CUPS_MUTOOL=""
AS_IF([test "x$enable_mutool" != "xyes"], [
	with_mutool_path=""
], [
	AS_IF([test "x$with_mutool_path" != "xsystem"], [
		CUPS_MUTOOL="$with_mutool_path"
	], [
		AS_IF([test "x$cross_compiling" = "xyes"], [
			CUPS_MUTOOL="mutool"
		], [
			AC_CHECK_PROG(CUPS_MUTOOL, mutool, mutool)
		])
	])
	AS_IF([test "x$CUPS_MUTOOL" = "x"], [
		AC_MSG_ERROR([Required mutool binary is missing. Please install mutool.])
	])
	AC_DEFINE([HAVE_MUTOOL], [1], [Define that we provide mutool binary])
	AS_IF([test x"$with_pdftops" = xmupdf -o x"$CUPS_PDFTOPS_RENDERER" = x], [CUPS_PDFTOPS_RENDERER=MUPDF])
])
AM_CONDITIONAL(ENABLE_MUTOOL, test "x$enable_mutool" = xyes)
AC_SUBST(CUPS_MUTOOL)

AC_DEFINE_UNQUOTED([CUPS_GHOSTSCRIPT], "$CUPS_GHOSTSCRIPT", [gs binary to use])
AC_DEFINE_UNQUOTED([CUPS_MUTOOL],"$CUPS_MUTOOL",[mutool binary to use])

# =================
# Check for ippfind
# =================
AS_IF([test "x$with_ippfind_path" != "xsystem"], [
	CUPS_IPPFIND="$with_ippfind_path"
], [
	CUPS_IPPFIND="ippfind"
])
AC_DEFINE_UNQUOTED([CUPS_IPPFIND], "$CUPS_IPPFIND", [ippfind binary to use.])

# ======================================
# Further needs for the filter functions
# ======================================
AC_DEFINE(PDFTOPDF, [1], [Needed for pdftopdf filter compilation])
AC_DEFINE_DIR(BANNERTOPDF_DATADIR, "{CUPS_DATADIR}/data", [Directory where bannertopdf finds its data files (PDF templates)])

# =========
# Test Font
# =========
AC_ARG_WITH([test-font-path],
	[AS_HELP_STRING([--with-test-font-path=value], [Set path to font used for tests (default: /usr/share/fonts/dejavu/DejaVuSans.ttf).])],
	[with_test_font_path="$withval"],
	[with_test_font_path=`( find /usr/share/fonts -name DejaVuSans.ttf; echo /usr/share/fonts/dejavu/DejaVuSans.ttf ) | head -1`]
)

AS_IF([test "x$cross_compiling" != "xyes" && ! test -f "$with_test_font_path"],
	[AC_MSG_WARN(DejaVuSans.ttf font file is missing. Please install a package providing it.) && [with_test_font_path=no]]
)
AC_DEFINE_UNQUOTED([TESTFONT], ["$with_test_font_path"], [Path to font used in tests])

# ================
# Check for CFLAGS
# ================
AC_ARG_ENABLE([werror],
	[AS_HELP_STRING([--enable-werror], [Treat all warnings as errors, useful for development.])],
	[enable_werror="$enableval"],
	[enable_werror=no]
)
AS_IF([test x"$enable_werror" = "xyes"], [
	CFLAGS="$CFLAGS -Werror"
])
AS_IF([test x"$GCC" = "xyes"], [
	# Be tough with warnings and produce less careless code
	CFLAGS="$CFLAGS -Wall -std=gnu11"
	CXXFLAGS="$CXXFLAGS -Wall " # -Weffc++" # TODO: enable when it does not print 1MB of warnings
])
CFLAGS="$CFLAGS -D_GNU_SOURCE"
CXXFLAGS="$CXXFLAGS -D_GNU_SOURCE -DPOINTERHOLDER_TRANSITION=0"
#                                   ^^ Silence deprecation warnings of QPDF 11
#                                      See /usr/include/qpdf/PointerHolder.hh
CXXFLAGS="$CXXFLAGS -std=c++17"

# =====================
# Prepare all .in files
# =====================
AC_CONFIG_FILES([
	libcupsfilters.pc
	Makefile
])
AC_OUTPUT

# =============================================
# Display final information about configuration
# =============================================
AC_MSG_NOTICE([
==============================================================================
Environment settings:
	CFLAGS:                                    ${CFLAGS}
	CXXFLAGS:                                  ${CXXFLAGS}
	LDFLAGS:                                   ${LDFLAGS}
Build configuration:
        libcups:         ${CUPS_VERSION}
	cups-config:     ${with_cups_config}
	poppler:         ${enable_poppler}
	ghostscript:     ${enable_ghostscript}
	gs-path:         ${with_gs_path}
	mutool:          ${enable_mutool}
	mutool-path:     ${with_mutool_path}
	ippfind-path:    ${with_ippfind_path}
	imagefilters:    ${enable_imagefilters}
	jpeg:            ${with_jpeg}
	exif:            ${enable_exif}
	png:             ${with_png}
	tiff:            ${with_tiff}
	fontconfig:      ${with_fontconfig}
	texttopdf:       ${enable_texttopdf}
	dbus:            ${enable_dbus}
	werror:          ${enable_werror}
	test-font:       ${with_test_font_path}
==============================================================================
])
