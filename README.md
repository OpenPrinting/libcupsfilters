# OpenPrinting libcupsfilters v2.0b1 - 2022-11-17

Looking for compile instructions?  Read the file "INSTALL"
instead...


## INTRODUCTION

CUPS is a standards-based, open-source printing system used by Apple's
Mac OS® and other UNIX®-like operating systems, especially also
Linux. CUPS uses the Internet Printing Protocol ("IPP") and provides
System V and Berkeley command-line interfaces, a web interface, and a
C API to manage printers and print jobs.

CUPS 1.0 was released in early 2000 and since then and until CUPS 2.x
(at least) conversion of the data format of incoming print jobs to the
format the printer needs was done by external filter executables, each
taking an input format on stdin and producing an output format on
stdout. Depending on conversion needs one or more of them were run in
a chain.

The filters for common formats were part of CUPS and later on, when
Apple was maintaining CUPS and using their own, proprietary filters
for Mac OS, transferred to OpenPrinting as the cups-filters package.

In the New Architecture for printing we switch to an all-IPP workflow
with PPD files and printer driver executables being abolished and
classic CUPS printer drivers replaced by Printer Applications
(software emulation of driverless IPP printers).

To conserve the functionality of the CUPS filters which got developed
over the last 20+ years into a PPD-less, IPP-driven world without
having to maintain and include the legacy PPD support in OS
distributions and other system environments, the original cups-filters
package got split into 5 separate packages: libcupsfilters, libppd,
cups-filters, braille-printer-app, and cups-browsed, with
libcupsfilters and braille-printer-app not containing PPD file support
code any more and cups-browsed being planned to drop explicit use of
PPD files.

This package provides the libcupsfilters library, which in its 2.x
version contains all the code of the filters of the former
cups-filters package as library functions, the so-called filter
functions.

The call scheme of the filter functions is similar to the one of the
CUPS filter executables (see `cupsfilters/filter.h`), but
generalized. In addition, it accepts printer and job IPP attributes
but not PPD files any more. The PPD file interfacing for retro-fitting
got moved to libppd.

The filter functions are principally intended to be used for the data
format conversion tasks needed in Printer Applications. They are
already in use (together with libppd and pappl-retrofit) by the
CUPS-driver retro-fitting Printer Applications from OpenPrinting.

In addition to the filter functions libcupsfilters also contains
several API functions useful for developing printer drivers/Printer
Applications, like image and raster graphics handling,
make/model/device ID matching, ...

For compiling and using this package CUPS (2.2.2 or newer),
libqpdf (10.3.2 or newer), libjpeg, libpng, libtiff, freetype,
fontconfig, liblcms (liblcms2 recommended), libavahi-common,
libavahi-client, libdbus, and glib are needed. It is highly
recommended, especially if non-PDF printers are used, to have at
least one of Ghostscript (preferred), Poppler, or MuPDF.

It also needs gcc (C compiler), automake, autoconf, autopoint, and
libtool. On Debian, Ubuntu, and distributions derived from them
you could also install the "build-essential" package to
auto-install most of these packages.

If Ghostscript is used (via the `cfFilterGhostscript()` or the
`cfFilterUniversal()` filter functions), Ghostscript 10.00.0 is
required (10.01.0 is highly recommended) and it has to be built at
least with the "pdfwrite", "ps2write", "cups", "pwgraster",
"appleraster", "pclm", "pclm8", "pdfimage24", "pdfimage8", "pxlcolor",
and "pxlmono" output devices. libcups of CUPS 2.2.2 or newer is
required to build Ghostscript this way.

The Poppler-based pdftoraster filter needs a C++ compiler which
supports C++11 and Poppler being built with the "./configure"
option "-DENABLE_CPP=ON" for building the C++ support library
libpoppler-cpp. This is the case for most modern Linux
distributions.

If you use MuPDF as PDF renderer make sure to use at least version
1.15, as the older versions have bugs and so some files get not
printed correctly.

Report bugs to

    https://github.com/OpenPrinting/libcupsfilters/issues

See the "COPYING", "LICENCE", and "NOTICE" files for legal
information. The license is the same as for CUPS, for a maximum of
compatibility.
