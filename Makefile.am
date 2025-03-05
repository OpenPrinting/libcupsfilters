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
	libcupsfilters.pc.in

EXTRA_DIST += \
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

check_SCRIPTS = cupsfilters/testfilters.sh

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
	testfilters

TESTS = \
	testdither \
	testpdf1 \
	testpdf2 \
	test-analyze \
	test-pdf \
	test-ps \
	cupsfilters/testfilters.sh

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
	cupsfilters/pclmtoraster.cxx \
	cupsfilters/pdf.cxx \
	cupsfilters/pdftopdf/pdftopdf.cxx \
	cupsfilters/pdftopdf/pdftopdf-private.h \
	cupsfilters/pdftopdf/pdftopdf-processor.cxx \
	cupsfilters/pdftopdf/pdftopdf-processor-private.h \
	cupsfilters/pdftopdf/qpdf-pdftopdf-processor.cxx \
	cupsfilters/pdftopdf/qpdf-pdftopdf-processor-private.h \
	cupsfilters/pdftopdf/pptypes.cxx \
	cupsfilters/pdftopdf/pptypes-private.h \
	cupsfilters/pdftopdf/nup.cxx \
	cupsfilters/pdftopdf/nup-private.h \
	cupsfilters/pdftopdf/intervalset.cxx \
	cupsfilters/pdftopdf/intervalset-private.h \
	cupsfilters/pdftopdf/qpdf-tools.cxx \
	cupsfilters/pdftopdf/qpdf-tools-private.h \
	cupsfilters/pdftopdf/qpdf-xobject.cxx \
	cupsfilters/pdftopdf/qpdf-xobject-private.h \
	cupsfilters/pdftopdf/qpdf-pdftopdf.cxx \
	cupsfilters/pdftopdf/qpdf-pdftopdf-private.h \
	cupsfilters/pdftopdf/qpdf-cm.cxx \
	cupsfilters/pdftopdf/qpdf-cm-private.h \
	cupsfilters/pdftoraster.cxx \
	cupsfilters/pdfutils.c \
	cupsfilters/pdfutils-private.h \
	cupsfilters/pwgtopdf.cxx \
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
	$(LIBQPDF_LIBS) \
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
	$(LIBQPDF_CFLAGS) \
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
	-lm

testdither_SOURCES = \
	cupsfilters/testdither.c \
	$(pkgfiltersinclude_DATA)
testdither_LDADD = \
	libcupsfilters.la \
	-lm

testimage_SOURCES = \
	cupsfilters/testimage.c \
	$(pkgfiltersinclude_DATA)
testimage_LDADD = \
	$(LIBJPEG_LIBS) \
	$(LIBPNG_LIBS) \
	$(TIFF_LIBS) \
	libcupsfilters.la \
	-lm
testimage_CFLAGS = \
	$(LIBJPEG_CFLAGS) \
	$(LIBPNG_CFLAGS) \
	$(TIFF_CFLAGS)

testrgb_SOURCES = \
	cupsfilters/testrgb.c \
	$(pkgfiltersinclude_DATA)
testrgb_LDADD = \
	libcupsfilters.la \
	-lm

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
	-I$(srcdir)/cupsfilters/
testpdf1_LDADD = \
	libcupsfilters.la

testpdf2_SOURCES = \
	cupsfilters/testpdf2.c \
	cupsfilters/fontembed-private.h
testpdf2_CFLAGS = \
	-I$(srcdir)/cupsfilters/fontembed/ \
	-I$(srcdir)/cupsfilters/
testpdf2_LDADD = \
	libcupsfilters.la

test_analyze_SOURCES = cupsfilters/fontembed/test-analyze.c
test_analyze_LDADD = libcupsfilters.la

test_pdf_SOURCES = cupsfilters/fontembed/test-pdf.c
test_pdf_LDADD = libcupsfilters.la

test_ps_SOURCES = cupsfilters/fontembed/test-ps.c
test_ps_LDADD = libcupsfilters.la

testfilters_SOURCES = \
	cupsfilters/testfilters.c \
	$(pkgfiltersinclude_DATA)

testfilters_LDADD = \
	libcupsfilters.la \
	-lm -ldl -lcups

testfilters_LDFLAGS = \
	-D_GNU_SOURCE \
	-L/usr/lib

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
	cupsfilters/test_files/test_file_4pg.pdf

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
