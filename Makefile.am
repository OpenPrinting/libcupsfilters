ACLOCAL_AMFLAGS = -I m4

pkgconfdir = $(libdir)/pkgconfig
pkgconf_DATA = \
	libcupsfilters.pc

doc_DATA = \
	ABOUT-NLS \
	AUTHORS \
	COPYING \
	CHANGES.md \
	CHANGES-1.x.md \
	INSTALL.md \
	LICENSE \
	NOTICE \
	README.md \
	DEVELOPING.md \
	CONTRIBUTING.md

EXTRA_DIST = \
	$(doc_DATA) \
	autogen.sh \
	config.rpath \
	libcupsfilters.pc.in \
	pdf.utf-8.heavy.in \
	pdf.utf-8.simple.in

EXTRA_DIST += \
	cupsfilters/test-pclm-overflow.sh \
	data/makePDFfromPS.sh \
	data/classified.ps \
	data/confidential.ps \
	data/secret.ps \
	data/standard.ps \
	data/topsecret.ps \
	data/unclassified.ps \
	data/banner-instr-append.txt \
	data/form-instr-append.txt \
	data/testprint-instr-append.txt

# =========
# utilities
# =========

pkgutilsdir = $(bindir)
pkgutils_PROGRAMS =

# =======
# Banners
# =======
pkgbannerdir = $(CUPS_DATADIR)/banners
pkgbanner_DATA =

bannerfiles = \
	banners/classified \
	banners/confidential \
	banners/secret \
	banners/standard \
	banners/form \
	banners/topsecret \
	banners/unclassified

pkgbanner_DATA += $(bannerfiles)

EXTRA_DIST += $(bannerfiles)

# ========
# Charsets
# ========
pkgcharsetdir = $(CUPS_DATADIR)/charsets
pkgcharset_DATA = \
	charset/pdf.utf-8.heavy \
	charset/pdf.utf-8.simple

EXTRA_DIST += $(pkgcharset_DATA)

# ====================
# CUPS Filters library
# ====================
pkgfiltersincludedir = $(includedir)/cupsfilters
pkgfiltersinclude_DATA = \
	cupsfilters/bitmap.h \
	cupsfilters/catalog.h \
	cupsfilters/colord.h \
	cupsfilters/colormanager.h \
	cupsfilters/driver.h \
	cupsfilters/filter.h \
	cupsfilters/ieee1284.h \
	cupsfilters/image.h \
	cupsfilters/ipp.h \
	cupsfilters/log.h \
	cupsfilters/pdf.h \
	cupsfilters/raster.h

lib_LTLIBRARIES = libcupsfilters.la

check_SCRIPTS = \
	cupsfilters/testfilters.sh \
	cupsfilters/test-pclm-overflow.sh \
	cupsfilters/test-pdftoraster-copy-height.sh \
	cupsfilters/test-imagetoraster-overflow.sh

check_PROGRAMS = \
	testcmyk \
	testdither \
	testimage \
	testrgb \
	test1284 \
	testpdf1 \
	testpdf2 \
	test-analyze \
	test-pdf \
	test-ps \
	testimagetoraster \
	testfilters

TESTS = \
	testdither \
	testpdf1 \
	testpdf2 \
	test-analyze \
	test-pdf \
	test-ps \
	testimagetoraster \
	cupsfilters/testfilters.sh \
	cupsfilters/test-pclm-overflow.sh \
	cupsfilters/test-pdftoraster-copy-height.sh \
	cupsfilters/test-imagetoraster-overflow.sh

#	testcmyk # fails as it opens some image.ppm which is nowerhe to be found.
#	testimage # requires also some ppm file as argument
#	testrgb # same error
# FIXME: run old testdither
#	./testdither > test/0-255.pgm 2>test/0-255.log
#	./testdither 0 127 255 > test/0-127-255.pgm 2>test/0-127-255.log
#	./testdither 0 85 170 255 > test/0-85-170-255.pgm 2>test/0-85-170-255.log
#	./testdither 0 63 127 170 198 227 255 > test/0-63-127-170-198-227-255.pgm 2>test/0-63-127-170-198-227-255.log
#	./testdither 0 210 383 > test/0-210-383.pgm 2>test/0-210-383.log
#	./testdither 0 82 255 > test/0-82-255.pgm 2>test/0-82-255.log
#	./testdither 0 510 > test/0-510.pgm 2>test/0-510.log
#	./testdither 0 1020 > test/0-1020.pgm 2>test/0-1020.log

# We need ieee1284 up and running.
# Leave it to the user to run if they have the bus.
#TESTS += test1284

libcupsfilters_la_SOURCES = \
	cupsfilters/bannertopdf.c \
	cupsfilters/bitmap.c \
	cupsfilters/catalog.c \
	cupsfilters/check.c \
	cupsfilters/cmyk.c \
	cupsfilters/colord.c \
	cupsfilters/colormanager.c \
	cupsfilters/testfilters.c \
	cupsfilters/debug.c \
	cupsfilters/debug-internal.h \
	cupsfilters/dither.c \
	cupsfilters/filter.c \
	cupsfilters/fontembed-private.h \
	cupsfilters/fontembed/aglfn13.c \
	cupsfilters/fontembed/dynstring.c \
	cupsfilters/fontembed/dynstring-private.h \
	cupsfilters/fontembed/embed.c \
	cupsfilters/fontembed/embed-sfnt.c \
	cupsfilters/fontembed/embed-sfnt-private.h \
	cupsfilters/fontembed/embed-pdf.c \
	cupsfilters/fontembed/embed-pdf-private.h \
	cupsfilters/fontembed/fontfile.c \
	cupsfilters/fontembed/frequent.c \
	cupsfilters/fontembed/frequent-private.h \
	cupsfilters/fontembed/macroman-private.h \
	cupsfilters/fontembed/sfnt.c \
	cupsfilters/fontembed/sfnt-private.h \
	cupsfilters/fontembed/sfnt-subset.c \
	cupsfilters/ghostscript.c \
	cupsfilters/ieee1284.c \
	cupsfilters/image.c \
	cupsfilters/image-colorspace.c \
	cupsfilters/image-jpeg.c \
	cupsfilters/image-png.c \
	cupsfilters/image-private.h \
	cupsfilters/image-tiff.c \
	cupsfilters/image-jpeg-xl.c \
	cupsfilters/image-zoom.c \
	cupsfilters/imagetopdf.c \
	cupsfilters/imagetoraster.c \
	cupsfilters/ipp.c \
	cupsfilters/libcups2.c \
	cupsfilters/libcups2-private.h \
	cupsfilters/lut.c \
	cupsfilters/mupdftopwg.c \
	cupsfilters/pack.c \
	cupsfilters/pclmtoraster.c \
	cupsfilters/pdf.c \
	cupsfilters/pdftopdf.c \
	cupsfilters/pdftopdf-private.h \
	cupsfilters/ipp-options.c \
	cupsfilters/ipp-options-private.h \
	cupsfilters/pdftoraster.c \
	cupsfilters/pdfutils.c \
	cupsfilters/pdfutils-private.h \
	cupsfilters/pwgtopdf.c \
	cupsfilters/pwgtoraster.c \
	cupsfilters/raster.c \
	cupsfilters/rastertopwg.c \
	cupsfilters/rgb.c \
	cupsfilters/srgb.c \
	cupsfilters/texttopdf.c \
	cupsfilters/texttotext.c \
	cupsfilters/universal.c \
	$(pkgfiltersinclude_DATA)
libcupsfilters_la_LIBADD = \
	$(FONTCONFIG_LIBS) \
	$(CUPS_LIBS) \
	$(LCMS_LIBS) \
	$(LIBPDFIO_LIBS) \
	$(LIBJPEG_LIBS) \
	$(LIBJXL_LIBS) \
	$(EXIF_LIBS) \
	$(LIBPNG_LIBS) \
	$(TIFF_LIBS) \
	-lm
if ENABLE_POPPLER
libcupsfilters_la_LIBADD += \
	$(POPPLER_LIBS)
endif
libcupsfilters_la_CFLAGS = \
	-I$(srcdir)/cupsfilters/fontembed/ \
	$(FONTCONFIG_CFLAGS) \
	$(CUPS_CFLAGS) \
	$(LCMS_CFLAGS) \
	$(LIBPDFIO_CFLAGS) \
	$(LIBJPEG_CFLAGS) \
	$(LIBJXL_CFLAGS) \
	$(EXIF_CFLAGS) \
	$(LIBPNG_CFLAGS) \
	$(TIFF_CFLAGS)
libcupsfilters_la_LDFLAGS = \
	-no-undefined \
	-version-info 2 \
	$(LIBJXL_LIBS)
if BUILD_DBUS
libcupsfilters_la_CFLAGS += $(DBUS_CFLAGS) -DHAVE_DBUS
libcupsfilters_la_CXXFLAGS = -std=c++11 $(libcupsfilters_la_CFLAGS)   # -std=c++11
libcupsfilters_la_LIBADD += $(DBUS_LIBS)
endif

testcmyk_SOURCES = \
	cupsfilters/testcmyk.c \
	$(pkgfiltersinclude_DATA)
testcmyk_LDADD = \
	libcupsfilters.la \
	$(CUPS_LIBS) \
	-lm
testcmyk_CFLAGS = \
	$(CUPS_CFLAGS)

testdither_SOURCES = \
	cupsfilters/testdither.c \
	$(pkgfiltersinclude_DATA)
testdither_LDADD = \
	libcupsfilters.la \
	$(CUPS_LIBS) \
	-lm
testdither_CFLAGS = \
	$(CUPS_CFLAGS)

testimage_SOURCES = \
	cupsfilters/testimage.c \
	$(pkgfiltersinclude_DATA)
testimage_LDADD = \
	$(LIBJPEG_LIBS) \
	$(LIBPNG_LIBS) \
	$(TIFF_LIBS) \
	libcupsfilters.la \
	$(CUPS_LIBS) \
	-lm
testimage_CFLAGS = \
	$(LIBJPEG_CFLAGS) \
	$(LIBPNG_CFLAGS) \
	$(TIFF_CFLAGS) \
	$(CUPS_CFLAGS)

testrgb_SOURCES = \
	cupsfilters/testrgb.c \
	$(pkgfiltersinclude_DATA)
testrgb_LDADD = \
	libcupsfilters.la \
	$(CUPS_LIBS) \
	-lm
testrgb_CFLAGS = \
	$(CUPS_CFLAGS)

testimagetoraster_SOURCES = \
	cupsfilters/testimagetoraster.c \
	$(pkgfiltersinclude_DATA)
testimagetoraster_CFLAGS = \
	$(CUPS_CFLAGS)
testimagetoraster_LDADD = \
	libcupsfilters.la \
	$(CUPS_LIBS) \
	-lm \
	-ljpeg

test1284_SOURCES = \
	cupsfilters/test1284.c
test1284_LDADD = \
	libcupsfilters.la \
	$(CUPS_LIBS)
test1284_CFLAGS = \
	-I$(srcdir)/cupsfilters/ \
	$(CUPS_CFLAGS)

testpdf1_SOURCES = \
	cupsfilters/testpdf1.c \
	cupsfilters/fontembed-private.h
testpdf1_CFLAGS = \
	-I$(srcdir)/cupsfilters/fontembed/ \
	-I$(srcdir)/cupsfilters/ \
	$(CUPS_CFLAGS)
testpdf1_LDADD = \
	libcupsfilters.la \
	$(CUPS_LIBS)

testpdf2_SOURCES = \
	cupsfilters/testpdf2.c \
	cupsfilters/fontembed-private.h
testpdf2_CFLAGS = \
	-I$(srcdir)/cupsfilters/fontembed/ \
	-I$(srcdir)/cupsfilters/ \
	$(CUPS_CFLAGS)
testpdf2_LDADD = \
	libcupsfilters.la \
	$(CUPS_LIBS)

test_analyze_SOURCES = cupsfilters/fontembed/test-analyze.c
test_analyze_LDADD = libcupsfilters.la $(CUPS_LIBS)
test_analyze_CFLAGS = $(CUPS_CFLAGS)

test_pdf_SOURCES = cupsfilters/fontembed/test-pdf.c
test_pdf_LDADD = libcupsfilters.la $(CUPS_LIBS)
test_pdf_CFLAGS = $(CUPS_CFLAGS)

test_ps_SOURCES = cupsfilters/fontembed/test-ps.c
test_ps_LDADD = libcupsfilters.la $(CUPS_LIBS)
test_ps_CFLAGS = $(CUPS_CFLAGS)

testfilters_SOURCES = \
	cupsfilters/testfilters.c \
	$(pkgfiltersinclude_DATA)
testfilters_LDADD = \
	libcupsfilters.la \
	$(CUPS_LIBS) \
	-lm -ldl
testfilters_CFLAGS = \
	-I$(srcdir)/cupsfilters/ \
	$(CUPS_CFLAGS)
testfilters_LDFLAGS = \
	-D_GNU_SOURCE \
	-L/usr/lib

noinst_PROGRAMS = gen-lorem-text
gen_lorem_text_SOURCES = cupsfilters/gen-lorem-text.c
EXTRA_DIST += cupsfilters/gen-lorem-text.c
# Generated deterministic lorem text for texttopdf tests
BUILT_SOURCES = cupsfilters/test_files/test_text_lorem.txt
CLEANFILES   = cupsfilters/test_files/test_text_lorem.txt

cupsfilters/test_files/test_text_lorem.txt: gen-lorem-text
	$(AM_V_GEN)./gen-lorem-text > $@

EXTRA_DIST += \
	$(check_SCRIPTS) \
	$(pkgfiltersinclude_DATA) \
	cupsfilters/image.pgm \
	cupsfilters/image.ppm \
	cupsfilters/fontembed/README \
	cupsfilters/test-filter-cases.txt \
	cupsfilters/test_files/bashrc.urf \
	cupsfilters/test_files/test_file.pdf \
	cupsfilters/test_files/test_file.pwg \
	cupsfilters/test_files/test_file_1pg.pdf \
	cupsfilters/test_files/test_file_2pg.pdf \
	cupsfilters/test_files/test_file_4pg.pdf \
	cupsfilters/test-pdftoraster-copy-height.c \
	cupsfilters/test-pdftoraster-copy-height.sh

# =========
# CUPS Data
# =========
pkgcupsdatadir = $(CUPS_DATADIR)/data
pkgcupsdata_DATA = \
	data/default.pdf \
	data/form_russian.pdf \
	data/form_english.pdf \
	data/form_english_in.odt \
	data/form_russian_in.odt \
	data/default-testpage.pdf \
	data/testprint \
	data/classified.pdf \
	data/confidential.pdf \
	data/secret.pdf \
	data/standard.pdf \
	data/topsecret.pdf \
	data/unclassified.pdf

EXTRA_DIST += $(pkgcupsdata_DATA)

distclean-local:
	rm -rf *.cache *~

install-data-hook:
	$(LN_S) -f pdf.utf-8.simple \
		$(DESTDIR)$(pkgcharsetdir)/pdf.utf-8
uninstall-hook:
	$(RM) $(DESTDIR)$(pkgcharsetdir)/pdf.utf-8

SUBDIRS =

# =============================================================================
# Downstream Debian autopkgtest harness (DESTDIR staging)
#
# Builds, DESTDIR-installs into an ephemeral $(CIROOT) tree, tops up the
# test-only programs/data that `make install` does not ship, rewrites the
# staged libcupsfilters.pc so a consumer resolves THIS build, then runs the
# vendored downstream scripts against it.  No privilege, no bind mounts, no
# proot -> identical on native and QEMU-emulated architectures.
# =============================================================================
CIROOT = $(abs_top_builddir)/_ciroot

EXTRA_DIST += \
	ci/autopkgtest/run.sh \
	ci/autopkgtest/debian-tests/control \
	ci/autopkgtest/debian-tests/libcupsfilters-2-dev \
	ci/autopkgtest/debian-tests/libcupsfilters-2-functionality

# Fix A: these are check_PROGRAMS (build-only; never copied by `make install`).
# The functionality test executes them as installed binaries (/usr/bin/...),
# so build them and stage them into $(CIROOT)$(bindir).  libtool --mode=install
# relinks each cleanly against $(libdir) (LD_LIBRARY_PATH then forces the
# staged library at run time).
STAGED_TEST_BINARIES = \
	testdither \
	testpdf1 \
	testpdf2 \
	test-analyze \
	test-pdf \
	test-ps \
	testcmyk \
	testrgb

install-test-programs: $(STAGED_TEST_BINARIES)
	$(MKDIR_P) "$(CIROOT)$(bindir)"
	@for p in $(STAGED_TEST_BINARIES); do \
	  echo "  STAGE   $$p -> $(CIROOT)$(bindir)/$$p"; \
	  $(LIBTOOL) --mode=install $(INSTALL_PROGRAM) "$$p" "$(CIROOT)$(bindir)/$$p" || exit 1; \
	done

# Fix C (data seeding): the functionality test copies its input images from
# $(datadir)/libcupsfilters-tests, which upstream `make install` does not ship.
# Seed image.pgm and image.ppm (both are cupsfilters/image.pgm, matching the
# downstream packaging) so the unmodified script finds them.
install-test-data:
	$(MKDIR_P) "$(CIROOT)$(datadir)/libcupsfilters-tests"
	@echo "  STAGE   cupsfilters/image.pgm -> $(CIROOT)$(datadir)/libcupsfilters-tests/image.pgm"
	$(INSTALL_DATA) "$(srcdir)/cupsfilters/image.pgm" "$(CIROOT)$(datadir)/libcupsfilters-tests/image.pgm"
	@echo "  STAGE   cupsfilters/image.pgm -> $(CIROOT)$(datadir)/libcupsfilters-tests/image.ppm"
	$(INSTALL_DATA) "$(srcdir)/cupsfilters/image.pgm" "$(CIROOT)$(datadir)/libcupsfilters-tests/image.ppm"

stage-ciroot: all
	rm -rf "$(CIROOT)"
	$(MAKE) $(AM_MAKEFLAGS) install DESTDIR="$(CIROOT)"
	$(MAKE) $(AM_MAKEFLAGS) install-test-programs install-test-data
	@pc="$(CIROOT)$(libdir)/pkgconfig/libcupsfilters.pc"; \
	if [ -f "$$pc" ]; then \
	  sed -i.bak -E "s#^(prefix|exec_prefix|libdir|includedir)=(/.*)#\1=$(CIROOT)\2#" "$$pc"; \
	  rm -f "$$pc.bak"; \
	  echo "stage-ciroot: rewrote staged-tree paths in $$pc"; \
	fi

# Common environment for the wrapper.  LIBCUPSFILTERS_BINDIR / LIBCUPSFILTERS_TESTDIR
# point the functionality test at the staged binaries and data instead of the
# system /usr paths, so it needs no absolute-path redirection (no root, no proot)
# and runs identically on native and QEMU-emulated architectures.
CIROOT_ENV = \
	CIROOT="$(CIROOT)" \
	CIPREFIX="$(exec_prefix)" \
	TOP_BUILDDIR="$(abs_top_builddir)" \
	LIBCUPSFILTERS_BINDIR="$(CIROOT)$(bindir)" \
	LIBCUPSFILTERS_TESTDIR="$(CIROOT)$(datadir)/libcupsfilters-tests"

# Full downstream suite in a single wrapper invocation -> one aggregate
# PASS/FAIL summary across both tests.  Identical on every architecture.
test-autopkgtest: stage-ciroot
	$(CIROOT_ENV) \
	    $(SHELL) $(srcdir)/ci/autopkgtest/run.sh libcupsfilters-2-dev libcupsfilters-2-functionality

# Single-test convenience targets (handy for debugging one case in isolation).
test-autopkgtest-dev: stage-ciroot
	$(CIROOT_ENV) \
	    $(SHELL) $(srcdir)/ci/autopkgtest/run.sh libcupsfilters-2-dev

test-autopkgtest-func: stage-ciroot
	$(CIROOT_ENV) \
	    $(SHELL) $(srcdir)/ci/autopkgtest/run.sh libcupsfilters-2-functionality

.PHONY: stage-ciroot install-test-programs install-test-data \
	test-autopkgtest test-autopkgtest-dev test-autopkgtest-func
