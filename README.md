# OpenPrinting libcupsfilters v2.0.0 - 2023-09-22

Looking for compile instructions? Read the file "INSTALL"
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

For compiling and using this package see the INSTALL file.

Report bugs to

    https://github.com/OpenPrinting/libcupsfilters/issues

See the "COPYING", "LICENCE", and "NOTICE" files for legal
information. The license is the same as for CUPS, for a maximum of
compatibility.

## LINKS

### cups-filters

* [Short history](https://openprinting.github.io/achievements/#cups-filters)
* [cups-filters 2.x development](https://openprinting.github.io/current/#cups-filters-2x)

### The New Architecture of Printing and Scanning

* [The New Architecture - What is it?](https://openprinting.github.io/current/#the-new-architecture-for-printing-and-scanning)
* [Ubuntu Desktop Team Indaba on YouTube](https://www.youtube.com/watch?v=P22DOu_ahBo)

### Printer Applications

* [All free drivers in a PPD-less world - OR - All free drivers in Snaps](https://openprinting.github.io/achievements/#all-free-drivers-in-a-ppd-less-world---or---all-free-drivers-in-snaps)
* [Current activity on Printer Applications](https://openprinting.github.io/current/#printer-applications)
* [PostScript Printer Application](https://github.com/OpenPrinting/ps-printer-app) ([Snap Store](https://snapcraft.io/ps-printer-app)): Printer Application Snap for PostScript printers which are supported by the manufacturer's PPD files. User can add PPD files if the needed one is not included or outdated.
* [Ghostscript Printer Application](https://github.com/OpenPrinting/ghostscript-printer-app) ([Snap Store](https://snapcraft.io/ghostscript-printer-app)): Printer Application with Ghostscript and many other drivers, for practically all Linux-supported printers which are not PostScript and not supported by HPLIP or Gutenprint.
* [HPLIP Printer Application](https://github.com/OpenPrinting/hplip-printer-app) ([Snap Store](https://snapcraft.io/hplip-printer-app)): HPLIP in a Printer Application Snap. Supports nearly every HP printer ever made. Installing HP's proprietary plugin (needed for a few printers) into the Snap is supported and easily done with the web interface.
* [Gutenprint Printer Application](https://github.com/OpenPrinting/gutenprint-printer-app) ([Snap Store](https://snapcraft.io/gutenprint-printer-app)): High quality output and a lot of knobs to adjust, especially for Epson and Canon inkjets but also for many other printers, in a Printer Application Snap.
* [Legacy Printer Application](https://github.com/OpenPrinting/pappl-retrofit#legacy-printer-application) (not available as Snap): It is a part of the [pappl-retrofit](https://github.com/OpenPrinting/pappl-retrofit) package and it makes drivers classically installed for the system's classically installed CUPS available in a Printer Application and this way for the CUPS Snap. It is especially helpful for drivers which are not (yet) available as Printer Application.
* [PAPPL](https://github.com/michaelrsweet/pappl/): Base infrastructure for all the Printer Applications linked above.
* [PAPPL CUPS driver retro-fit library](https://github.com/OpenPrinting/pappl-retrofit): Retro-fit layer to integrate CUPS drivers consisting of PPD files, CUPS filters, and CUPS backends into Printer Applications.
* [Printer Applications 2020 (PDF)](https://ftp.pwg.org/pub/pwg/liaison/openprinting/presentations/printer-applications-may-2020.pdf)
* [Printer Applications 2021 (PDF)](https://ftp.pwg.org/pub/pwg/liaison/openprinting/presentations/printer-applications-may-2021.pdf)
* [CUPS 2018 (PDF, pages 28-29)](https://ftp.pwg.org/pub/pwg/liaison/openprinting/presentations/cups-plenary-may-18.pdf)
* [CUPS 2019 (PDF, pages 30-35)](https://ftp.pwg.org/pub/pwg/liaison/openprinting/presentations/cups-plenary-april-19.pdf)
* [cups-filters 2018 (PDF, page 11)](https://ftp.pwg.org/pub/pwg/liaison/openprinting/presentations/cups-filters-ippusbxd-2018.pdf)
* [cups-filters 2019 (PDF, pages 16-17)](https://ftp.pwg.org/pub/pwg/liaison/openprinting/presentations/cups-filters-ippusbxd-2019.pdf)
* [cups-filters 2020 (PDF)](https://ftp.pwg.org/pub/pwg/liaison/openprinting/presentations/cups-filters-ippusbxd-2020.pdf)
* [cups-filters 2021 (PDF)](https://ftp.pwg.org/pub/pwg/liaison/openprinting/presentations/cups-filters-cups-snap-ipp-usb-and-more-2021.pdf)
* [cups-filters 2022 (PDF)](https://ftp.pwg.org/pub/pwg/liaison/openprinting/presentations/cups-filters-cups-snap-ipp-usb-and-more-2022.pdf)
