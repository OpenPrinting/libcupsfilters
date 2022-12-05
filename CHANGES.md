# CHANGES - OpenPrinting libcupsfilters v2.0b1 - 2022-11-17

## CHANGES IN V2.0b2 (TBA)

- cfFilterPDFToPDF(): Initialize output page dimensions to easily
  identify if no dimensions were supplied, to fall back to default
  size Letter.  Otherwise we get invalid PDF output if we do not
  specify the output page dimensions (no printer IPP attributes) but
  need them (for print-scaling=fit/fill, numper-up, booklet).

- cfFilterGhostscript(): Never supply "-dDEVICEWIDTHPOINTS=0
  -dDEVICEHEIGHTPOINTS=0", if no page size got requested with the job
  (page dimensions are zero in raster header) skip these arguments so
  that Ghostscript uses the page dimensions of the input page.

- libcupsfilters.pc.in: Added libqpdf under "Libs.private".

- configure.ac: Added "foreign" to to AM_INIT_AUTOMAKE() call. Makes
  automake not require a file named README.

- libcupsfilters does not use glib, removed the check in configure.ac.

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
