
# libcupsfilters v2.1.1 Installation Guide


## Overview

This guide provides step-by-step instructions for compiling and installing OpenPrinting libcupsfilters from source code. For detailed information about libcupsfilters, refer to "README.md", and for a complete change log, see "CHANGES.md".

### Important Note
For non-PDF printers (excluding Mac OS X users), you must install Ghostscript with specific drivers ("cups", "pwgraster", "appleraster", "urf", "pclm", "pclm8", "pdfwrite", "pdfimage24", "pdfimage32", "pdfimage8") after installing CUPS and before installing libcupsfilters.

## Before You Begin

### Prerequisites for Compilation
- ANSI-compliant C and C++ compilers (mostly written in C, some components in C++)
- make program
- POSIX-compliant shell (/bin/sh)
- autoconf, autopoint, automake, libtool for ./autogen.sh
- CUPS devel files (version 2.2.2 or higher)
- Poppler (with --enable-poppler-cpp) devel files for pdftoraster
- fontconfig devel files for texttopdf (disable using --without-fontconfig)
- liblcms (liblcms2 recommended) devel files for color management
- QPDF (11.0 or higher, 11.4.0 recommended) devel files

### Additional Binaries for Non-PDF Printers
- Ghostscript 10.01.1 or higher (with specific output devices support)
or
- MuPDF (version 1.15 or higher)

### Optional Features
- libdbus, libjpeg, libpng, libtiff, libexif devel files for various format supports
- Dejavu Sans fonts for internal test suite

### Compiler and Make Tools
- Tested with GNU compiler tools and bash
- Compatible with GNU make and other versions (BSD users should use GNU make)

## Compiling the GIT Repository Code
- The GIT repository doesn't include a pre-built configure script.
- Use GNU autoconf (2.65 or higher) to create it:
  ```
  ./autogen.sh
  ```

## Configuration
- Use the "configure" script in the main directory:
  ```
  ./configure
  ```
- Default installation path is "/usr".
- For custom installation path, use the "--prefix" option:
  ```
  ./configure --prefix=/some/directory
  ```
- Use `./configure --help` for all configuration options.
- Set environment variables for libraries in non-default locations.

## Building the Software
- Run `make` (or `gmake` for BSD systems) to build the software.

## Installing the Software
- After building, install the software with:
  ```
  make install
  ```
- For BSD systems:
  ```
  gmake install
  ```

## Runtime Dependencies
- dBUS provider (dbus-broker/dbus-daemon) and colord for dBUS support
- Ghostscript or MuPDF based on configuration
- A monospace font (e.g., liberation-mono-fonts) for texttopdf

## Packaging for Operating System Distributions
- libcupsfilters is supported for CUPS from version 2.2.2.
- For earlier CUPS versions (1.5.x, required since 1.6.x), use cups-filters 1.x.

## Testing
- For CI testing and build checks, run with:
```
make check
```

## Installing Required Tools

### For Debian/Ubuntu-based Systems
- Install C and C++ compilers, make, autoconf, autopoint, automake, libtool:
  ```
  sudo apt-get install build-essential autoconf autopoint automake libtool
  ```
- Install Poppler, fontconfig, liblcms2, mupdf-tools, gettext, libcups2-dev, libqpdf-dev:
  ```
  sudo apt-get install libpoppler-cpp-dev poppler-utils libfontconfig1-dev liblcms2-dev mupdf-tools gettext libcups2-dev libqpdf-dev
  ```
- Install Ghostscript (for non-PDF printers):
  ```
  sudo apt-get install ghostscript
  ```
- Install optional libraries (libdbus, libjpeg, libpng, libtiff, libexif):
  ```
  sudo apt-get install libdbus-1-dev libjpeg-dev libpng-dev libtiff-dev libexif-dev
  ```
- Install Dejavu Sans fonts (for internal test suite):
  ```
  sudo apt-get install fonts-dejavu-core
  ```

### For Red Hat/Fedora-based Systems
- Install C and C++ compilers, make, autoconf, automake, libtool:
  ```
  sudo dnf install gcc gcc-c++ make autoconf automake libtool
  ```
- Install Poppler, fontconfig, liblcms2, mupdf-tools:
  ```
  sudo dnf install libpoppler-cpp-dev poppler-utils libfontconfig1-dev liblcms2-dev mupdf-tools
  ```
- Install Ghostscript (for non-PDF printers):
  ```
  sudo dnf install ghostscript
  ```
- Install optional libraries (libdbus, libjpeg, libpng, libtiff, libexif):
  ```
  sudo dnf install dbus-devel libjpeg-turbo-devel libpng-devel libtiff-devel libexif-devel
  ```
- Install Dejavu Sans fonts (for internal test suite):
  ```
  sudo dnf install dejavu-sans-fonts
  ```

Note: The above commands are for common Linux distributions. For other operating systems or distributions, please refer to the respective package management instructions.
