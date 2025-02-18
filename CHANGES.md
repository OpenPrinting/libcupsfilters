# CHANGES - OpenPrinting libcupsfilters v2.1.1 - 2025-02-18

## CHANGES IN V2.1.1 (18th February 2025)

- Correct adding blank page when printing even/odd pages for manual duplex
  The "page-set" option with choices "even" and "odd" is designed for
  doing manual duplex, by printing the odd pages first, turning the
  printed pages over and put them back into the input tray and then
  print the even pages. If the total number of pages is odd, a blank
  page needs to get added. This chnage does corrections to make this
  work correctly (cups-filters issue #541).

- Do not default to input page size Letter when job format defines page sizes
  For input formats which define the absolute size dimensions for each
  page (PostScript, PDF, CUPS/PWG/Apple Raster) we do not default to
  US Letter if no input page size is given (Issue #68, pull request
  #69).

- Fix transferring error exit status from children in filter functions
  The Ghostcript and MuPDF filter functions did not transfer error
  exit codes from the called command line utilities and returned the
  successful exit code from another called utility instead (Issue #76,
  pull request #77).

- bannertopdf.c: Fix segfault when printing banners/test page
  (Pull request #80)

- Fix issues reported by OpenScanHub
  Open source static analyzer OpenScanHub found several issues
  regarding resource leaks, security, buffer overflows etc. which are
  fixed now. libcupsfilters passes sanity testing with the changes
  (Pull request #79).

- Update testfilters.c to resolve issues with different build directories
  (Issue #66, pull request #74)


## CHANGES IN V2.1.0 (17th October 2024)

- `cfGetPrinterAttributes5()`: Validate response attributes before return
  The IPP print destination which we are querying can be corrupted or
  forged, so validate the response to strenghten security. Fixes
  CVE-2024-47076.

- Include `cupsfilters/testfilters.sh` in release tarball
  `check_SCRIPTS` is not automatically included, has to be added to
  EXTRA_DIST.


## CHANGES IN V2.1b1 (14th August 2024)

- Added support for libcups3 (libcups of CUPS 3.x)
  With these changes libcupsfilters can be built either with libcups2
  (libcups of CUPS 2.x) or libcups3 (libcups of CUPS 3.x).

- Fix obsolete constant name so that build with libcups of CUPS 2.5.x works
  Replaced `HTTP_URI_OK` by `HTTP_URI_STATUS_OK` (Issue #36)

- Always use sRGB/sGray if driver is PWG/URF and RGB/Gray is requested
  PWG Raster and Apple Raster (URF) are for driverless printing abd
  require the sRGB and sGRay standard color profiles (Pull request
  #51).

- `raster_base_header()`: Several fixes on color space selection
  Internal (static) function to create a CUPS or PWG Raster header
  from scratch, without using data of a PPD file, only IPP attributes
  and command line options. Fixes are done to make sure a monochrome
  color space is used for monochrome printers, the actual color space
  seletions (if supported) is used for PWG Raster, not always sRGB
  8-bit, and DeviceN can be selected also without specifying the depth
  (Issue #38).

- Fix content data stream concatenation mangling output in `cfFilterPDFToPDF()`
  In `cfFilterPDFToPDF()` add newline after each content stream in
  `::provideStreamData`. When concatenating the data streams for the
  page's contents, add a new line at the end of each data stream to
  avoid cases where the concatenation might result in a corruption
  (Pull request #56).

- `cfCatalogLoad()`: Fix incorrect `strncpy()` limit calculation
  Preserve the new allocated size in a variable and use it instead of
  `sizeof()` (Pull request #49).

- Remove redundant data type definition in `libcups2-private.h`
  (Pull request #44)

- `pdf-cm.cxx`: Fix possible integer overflow
  (Issue #42, Pull request #43)

- `cfImageCMYKToCMY()`: Fixed copy-and-paste error
  (Issue #41)

- Fix compilation with clang and libc++
  Define `_GLIBCXX_THROW` to empty for C++ 11 and newer.
  (Issue #35, Pull request #47)

- Fix warnings reported by clang 17
  (Pull request #48)

- Added test programm for the filter functions to the build tests
  In the GSoC 2023 Pratyush Ranjan created a program for CI/build/unit
  testing which runs filter tasks defined in a table, by input files,
  input and output file formats, and options settings and checks
  whether they got correctly executed. This allows easily adding test
  cases from bug reports to avoid regressions (similar to
  Ghostscript's CI tests). This program is now incorporated and added
  to the tests run by `make check`, so that it gets commonly used as
  CI test, especially also when distros package libcupsfilters (Pull
  request #58).

- pkgconfig: Add '-I${includedir}' to Cflags
  This way, includes prefixed with the "cupsfilters/" directory path
  also work. Especially libppd build also without problem when
  libcupsfilters is installed in a non-default path (Pull request #57).

- `.gitignore`: Ignore temporary files created by the build tests

- Convert `INSTALL` to `INSTALL.md`
  (Pull request #45)

- `INSTALL.md`: Updated dependencies
  We also need: gettext, libcups2-dev, libqpdf-dev (Pull request #59)

- `Install.md`: Tell that we have `make check` for build testing
  (Pull request #59)

- Added GitHub workflow for Canonical Open Documentation Academy
  OpenPrinting is participating in Canonical's [Open Documentation
  Academy](https://github.com/canonical/open-documentation-academy/),
  as an organization in need of documentation. The workflow is still
  experimental and servs for auto-forwarding documentation-related
  issues.


## CHANGES IN V2.0.0 (22th September 2023)

- `cfFilterUniversal()`: Support `application/vnd.cups-postscript`
  Some filters (like `hpps` from HPLIP) produce this MIME type, so if
  the client uses a classic driver/Printer Application and the server
  IPP Everywhere, jobs fail because the library is not able to find a
  suitable conversion (Pull request #31).

- `CHANGES.md`: Added reference to Chromium bug report.

- `INSTALL`: We need Ghostscript 10.01.1 to get all changes for Raster
  output


## CHANGES IN V2.0rc2 (20th June 2023)

- Ignore unsupported resolution values when preparing a Raster header
  via `cfRasterPrepareHeader()` function, to avoid rasterization with
  wrong resolution (Issue #29, Ubuntu bug: #2022929, Chromium issue
  #1448735).

- `cfRasterPrepareHeader()`: When taking default resolution from
  `urf-supported` printer IPP attribute, use first value (lowest) of
  the list, to match the `ppdLoadAttributes()` function of libppd.

- `cfIPPAttrResolutionForPrinter()`: List of resolutions is not
  `IPP_TAG_RANGE`, corrected the search to use `IPP_TAG_ZERO`.

- `cfIEEE1284NormalizeMakeModel()`: Do not consider "XPrinter" as made
  by Xerox, only "XPrint" is (OpenPrinting CUPS pull request #506).

- `INSTALL`: Recommend QPDF 11.4.0 as it fixes loss of content filled
  into interactive forms as (Issue #28).

- `INSTALL`: Fixed some typos.


## CHANGES IN V2.0rc1 (11th April 2023)

- Many fixes for control of page orientation, printing text in
  landscape, image scaling fixes

- Make orientation-requested work correctly In the filter functions
  cfFilterImageToRaster(), cfFilterImageToPDF(), cfFilterTextToPDF(),
  and cfFilterPDFToPDF() supplying the attributes
  orientation-requested or (no)landscape do the expected, rotating
  content 0/90/180/270 degrees

- Auto-rotation is always done in the printer's default direction, +90
  or -90 degrees as described by the
  landscape-orientation-requested-preferred IPP attribute or by the
  "*LandscapeOrientation: ..." PPD keyword (via libppd).

- The ppi attribute works correctly now,no cropping and using more
  than one sheet if the image gets too large.

- If ore than one image-scaling-related attribute is supplied, only
  the one with the highest priority is used, no mixing with unexpected
  results.

- Default is always print-scaling=auto.

- cfFilterTextToPDF() is now capable of printing text in landscape
  layout, controlled by the orientation-requested and landscape
  attributes.

- The prettyprint attribute of the cfFilterTextToPDF() now uses wider
  margins for binding/stitching/punching as originally designed. If
  the printer has even wider margins this is taken into account.

- cfFilterImageToRaster(), cfFilterImageToPDF(), cfFilterTextToPDF(),
  and cfFilterPDFToPDF() all work now correctly also with no printer
  attributes/capabilities supplied a all, and also with and without
  supplying a page size. With PDF input the input page sizes are used
  if applicable. As last mean we resort to US Letter page size (Issue
  #26).

- The cfFilterUniversal() filter funtion now considers the output of
  cfFilterImageToPDF() correctly as application/vnd.cups-pdf and not
  as application/pdf, avoiding duplicate application of margins or
  rotation.

- cfFilterImageToRaster() now produces 16-bit-per-color output if
  requested by the job/printer, and does not output a blank page any
  more (Issue #25, pull request #22).

- On 1-bit dithered output white got a grid of dots
  An off-by-one bug in cfOneBitLine() made white areas appear with a
  grid of black dots (1 per 16x16 square). This corrected now (Issue
  #20).

- DNS-SD device URI resolution: Cleaned API for upcoming libcups3
  support For resolving DNS-SD-service-name-based device URIs for IPP
  printers as CUPS uses them, libcups2 does not offer any useful API
  and we therefore ended up implementing 2 workarounds. As libcups3
  has an API function for it we have adapted our API for its use
  (libcups3 support will get actually added in version 2.1.0).

- Cleaned up the image scaling and rotation code in
  cfFilterImage...(), removing duplicate code and doing some
  simplification.

- cfFilterImageToPDF() could crash when an image could not get loaded
  and print-scaling was set to fill or none, as a part of the image
  processing was not covered by the NULL check.

- cfFilterUniversal(): Added NULL checks for parameters, so that the
  function can get called without parameters.

- Added "-std=c++17" C++ compiler flag (Pull request #18) Needed to
  build libcupsfilters with QPDF 11.

- Removed unneeded #include entries of libcups


## CHANGES IN V2.0b4 (23rd February 2023)

- Do not free `cf_image_t` data structure in `_cfImageZoomDelete()`
  (cups-filters issue #507)
  The library-internal `_cfImageZoom...()` API does not create the
  `cf_image_t` data structure, so it should not free it, to avoid
  double-free crashes. This made the `cfFilterImageToRaster()` filter
  function (`imagetoraster` CUPS filter) crash.

- `cfImageOpenFP()`: Removed leftover `HAVE_LIBZ` conditionals
  In the 3rd beta we have removed the dependency on libz from the
  build system as there is no explicit dependency on it in
  libcupsfilters. Having forgotten to remove `HAVE_LIBZ` from the
  conditionals in `cfImageOpenFP()` PNG images were rendered as blank
  pages. See cups-filters issue #465.

- Compatibility with QPDF 11 and later
  * Replaced deprecated `PointerHolder` with `shared_ptr` (PR #13)
  * `cfFilterPDFToPDF()`: Replaced deprecated QPDF function name
    `replaceOrRemoveKey` by `replaceKey`.
  * Set `CXXFLAGS="-DPOINTERHOLDER_TRANSITION=0"` to silence QPDF warnings.

- Coverity check done by Zdenek Dohnal for the inclusion of libppd in
  Fedora and Red Hat. Zdenek has fixed all the issues: Missing `free()`,
  files not closed, potential string overflows, ... Thanks a lot!

- `configure.ac`: Change deprecated `AC_PROG_LIBTOOL` for `LT_INIT` (PR #12)

- `INSTALL`: Explain dependencies (PR #10)


## CHANGES IN V2.0b3 (31st January 2023)

- cfFilterGhostscript(): Select correct ICC profile for PCL-XL.
  When using the cfFilterGhostscript() filter function to generate
  PCL-XL (PXL, PCL 6, Ghostscript output devices "pxlmono",
  "pxlcolor") output, always the color IPP profile srgb.c was used,
  also for monochrome output ("pxlmono") and this makes Ghostscript
  error out. Now we correctly select sgray.icc for monochrome output.

- cfGetPrinterAttributes(): Poll "media-col-database" separately if needed
  Some printers are not able to handle a get-printer-attributes
  request querying both the "all" group attribute and the
  "nedia-col-database" atrribute, so query the latter with a separate
  call in such cases.

- cfGenerateSizes(): Also parse the "media-col-ready" IPP attribute
  for page sizes and margins. This often reveals extra margin
  variants, like borderless.

- Removed public cfPDFOut...() API (cupsfilters/pdfutils.h)
  This API only makes sense if the API of fontembed is also public,
  but this we made private earlier.

- Build system, README.md: Remove unnecessary dependencies overlooked
  during the separation: zlib (only needed by libppd), Freetype (not
  needed any more after removal of pdftoopvp), Avahi and GLib (both
  only needed by cups-browsed). Thanks a lot, Zdenek Dohnal (Pull
  request #7).

- COPYING, NOTICE: Simplification for autotools-generated files
  autotools-generated files can be included under the license of the
  upstream code, and FSF copyright added to upstream copyright
  list. Simplified COPYING appropriately.

- COPYING, NOTICE: Added copyright year 2023

- COPYING, NOTICE, AUTHORS: Added Jai Luthra and Vikrant Malik

- Makefile.am: Include LICENSE in distribution tarball


## CHANGES IN V2.0b2 (8th January 2023)

- Manage page dimensions when no printer properties are given.
  If a filter function is called without printer IPP attributes
  (classic CUPS filter wrapper without PPD file) any page size/media
  attributes given are accepted, when no page dimensions are given, US
  Letter is used, and when no margins are given, non-zero default
  margins are used.

- cfFilterTextToPDF(): If no output page dimensions specified, use US
  Letter. Before, the page dimensions were set to 0x0, ending up with
  one empty page per character in the input file being produced.

- cfFilterPDFToPDF(): Initialize output page dimensions to easily
  identify if no dimensions were supplied, to fall back to default
  size Letter.  Otherwise we get invalid PDF output if we do not
  specify the output page dimensions (no printer IPP attributes) but
  need them (for print-scaling=fit/fill, numper-up, booklet).

- cfFilterGhostscript(): Never supply "-dDEVICEWIDTHPOINTS=0
  -dDEVICEHEIGHTPOINTS=0", if no page size got requested with the job
  (page dimensions are zero in raster header) skip these arguments so
  that Ghostscript uses the page dimensions of the input page.

- cfCatalog...() API: Add support to specify the UI
  language/locale. Before, the functions only served for getting
  English human-readable strings for options and choices of options,
  not translations in a requested language.  As the human-readable
  strings are taken from the translation tables of CUPS and also of
  IPP printers, language support is trivial, only adding a parameter
  to supply the desired language (in `xx` or `xx_YY` format) (PR #2,
  #3).

- cfCatalog...() API: Removed "const" qualifier from output string
  pointers as these strings get allocated by the functions.

- Let API header files catalog.h and ipp.h not include config.h.

- Fixed building libcupsfilters with and without libpoppler. By
  default, libpoppler is used, to not use it, supply the
  "--disable-poppler" option to "./configure".

- Without libpoppler the cfFilterUniversal requires Ghostscript to
  turn PDF input into any of the Raster formats, built with both
  "--disable-poppler" and "--disable-ghostscript", the universal
  filter function is not able to rasterize PDF.

- The cfFilterPDFToRaster() gets non-functional when building
  libcupsfilters without libpoppler. Calling it simply produces an
  error message in the log.

- Unnecessary "./configure" options for the former PDF-to-PostScript
  filter function (has moved to libppd as ppdFilterPDFToPS) which were
  forgotten during the separation, are removed now.

- All "AC_DEFINE" macro calls in configure.ac got corrected, setting
  the macros in config.h really to 1 after testing positive.

- libcupsfilters does not use glib, removed the check in configure.ac.

- libcupsfilters.pc.in: Added libqpdf under "Libs.private".

- Makefile.am: Include NOTICE in distribution tarball

- configure.ac: Added "foreign" to to AM_INIT_AUTOMAKE() call. Makes
  automake not require a file named README.

- Cleaned up .gitignore

- Tons of fixes in the source code documentation: README.md, INSTALL,
  DEVELOPING.md, CONTRIBUTING.md, COPYING, NOTICE, ... Adapted to the
  libcupsfilters component, added links.


## CHANGES IN V2.0b1 (17th November 2022)

- Introduced the filter functions concept converting filter
  executables into library functions with common call scheme, moving
  their core functionality into libcupsfilters and allowing easier use
  by Printer Applications. Common data about the printer and the job
  are supplied via a data structure, which is the same for each filter
  in a chain of filters. The data structure can be extended via named
  extensions.

- Converted nearly all filters to filter functions, only exceptions
  are rastertoescpx, rastertopclx, commandtoescpx, commandtopclx, and
  foomatic-rip. The latter is deeply involved with Foomatic PPDs and
  the others are legacy printer drivers. Filter functions which
  output PostScript are implemented in libppd.

- Converted CUPS' rastertopwg filter into the cfFilterRasterToPWG()
  filter function.

- Created new cfFilterPWGToRaster() filter function primarily to print
  raster input (PWG Raster, Apple Raster, images) to CUPS Raster
  drivers in driver-retro-fitting Printer Applications.

- Converted all filter functions to completely work without PPD files,
  using only printer and job IPP attributes and an option list for
  options not mappable to IPP attributes. For some filter functions
  there are also wrappers for a more comprehensive PPD file support in
  libppd.

- Added concept of callback functions for logging, for the filter
  functions not spitting their log to stderr.

- Added concept of callback functions for telling that a job is
  cancelled so that filter functions can return early. This change is
  to get more flexibility and especially to support the
  papplJobIsCanceled() of PAPPL.

- Added new streaming mode triggered by the boolean
  "filter-streaming-mode" option. In this mode a filter (function) is
  supposed to avoid everything which prevents the job data from
  streaming, as loading the whole job (or good part of it) into a
  temporary file or into memory, interpreting PDF, pre-checking input
  file type or zero-page jobs, ... This is mainly to be used by
  Printer Applications when they do raster printing in streaming mode,
  to run with lowest resources possible. Currently
  cfFilterGhostscript() and cfFilterPDFToPDF() got a streaming
  mode. In streaming mode PostScript (not PDF) is assumed as input and
  no zero-page-job check is done, and all QPDF processing (page
  management, page size adjustment, ...) is skipped and only JCL
  according to the PPD added.

- Added raster-only PDF and PCLm output support to the ghostscript()
  filter function. Note that PCLm does not support back side
  orientation for duplex.

- cfFilterPDFToPDF(): Introduced new "input-page-ranges" attribute
  (Issue #365, Pull request #444, #445).

- Added cfFilterChain() filter function to run several filter
  functions in a chain.

- Added filterPOpen() and filterPClose() functions which similar to
  popen() and pclose() create a file descriptor which either takes
  data to feed into a filter function or provides data coming out of a
  filter function.

- Added new cfFilterExternal() filter function which calls an external
  CUPS filter (or backend) executable specified in the parameters, for
  example a legacy or proprietary printer driver which cannot be
  converted into a filter function. Backends can be run both for job
  execution or in their discovery mode. The environment in which the
  external executable is running is resembling CUPS as best as
  possible.

- Added support for the back and side channels which CUPS uses for
  additional communication between the filters and the backend into
  the filter function infrastructure. Now filter functions can use
  these channels and also CUPS filters or backends called via the
  cfFilterExternal() function. Printer Applications can easily create
  the needed pipes via the new function cfFilterOpenBackAndSidePipes()
  and close them via cfFilterCloseBackAndSidePipes() and filter
  functions used as classic CUPS filters get the appropriate file
  descriptors supplied by ihe ppdFilterCUPSWrapper() function of
  libppd.

- Added the cfFilterUniversal() filter function which allows a single
  CUPS filter executable which auto-creates chains of filter function
  calls to convert any input data format into any other output data
  format. So CUPS can call a single filter for any conversion, taking
  up less resources. Thanks to Pranshu Kharkwal for this excellent
  GSoC project (Pull request #421).

- Added functions to read out IPP attributes from the job and check
  them against the IPP attributes (capabilities) of the printer:
  cfIPPAttrEnumValForPrinter(), cfIPPAttrIntValForPrinter(),
  cfIPPAttrResolutionForPrinter()

- Added functions cfGenerateSizes() and cfGetPageDimensions() to match
  input page sizes with the page sizes available on the printer
  according to printer IPP attributes.

- Moved IEEE1284-device-ID-related functions into the public API of
  libcupsfilters, also made the internal functions public and renamed
  them all to cfIEEE1284...(), moved test1284 to cupsfilters/.

- Extended cfIEEE1284NormalizeMakeAndModel() to a universal function
  to clean up and normalize make/model strings (also device IDs) to
  get human-readable, machine-readable, or easily sortable make/model
  strings. Also allow supplying a regular expression to extract driver
  information when the input string is a PPD's *NickName.

- When calling filters without having printer attributes, improved
  understanding of color mode options/attributes. Options
  "output-mode", "OutputMode", "print-color-mode", and choices "auto",
  "color", "auto-monochrome", "process-monochrome", and "bi-level" are
  supported now and default color mode is RGB 8-bit color and not
  1-bit monochrome.

- When parsing IPP attributes/options map the color spaces the same
  way as in the PPD generator (Issue #326, Pull request #327).

- Added new oneBitToGrayLine() API function which converts a line of
  1-bit monochrome pixels into 8-bit grayscale format
  (cupsfilters/bitmap.[ch]).

- Removed support for asymmetric image resolutions ("ppi=XXXxYYY") in
  cfFilterImageToPDF() and cfFilterImageToRaster() as CUPS does not
  support this (Issue #347, Pull request #361, OpenPrinting CUPS issue
  #115).

- Removed now obsolete apply_filters() API function to call a chain of
  external CUPS filter executables, as we have filter functions now
  and even can call one or another filter executable (or even backend)
  via cfFilterExternal().

- Build system, README: Require CUPS 2.2.2+ and QPDF 10.3.2+.  Removed
  now unneeded ./configure switches for PCLm support in QPDF and for
  use of the urftopdf filter for old CUPS versions.

- Renamed function/data type/constant names to get a consistent API:
  Functions start with "cf" and the name is in camel-case, data types
  start with "cf_" and are all-lowercase with "_" separators,
  constants start with "CF_" and are all-uppercase, also with "_"
  separators.

- Bumped soname to 2, as we have a new API now.

- Build system: Remove '-D_PPD_DEPRECATED=""' from the compiling
  command lines of the source files which use libcups. The flag is not
  supported any more for longer times already and all the PPD-related
  functions deprecated by CUPS have moved into libppd now.

- Older versions of libcups (< 2.3.1) had the enum name for
  fold-accordion finishings mistyped.  Added a workaround.

- Added support for Sharp-proprietary "ARDuplex" PPD option name for
  double-sided printing.

- Build system: Add files in gitignore that are generated by
  "autogen.sh", "configure", and "make" (Pull request #336).

- Fixed possible crash bug in oneBitLine() function.

- In ghostscript() pass on LD_LIBRARY_PATH to Ghostscript
