//
// Color mode regression test for cfRasterPrepareHeader() (Issues #298, #350).
//
// Copyright © 2026 by OpenPrinting.
//
// Licensed under Apache License v2.0. See the file "LICENSE" for more
// information.
//

#include <config.h>
#include <cupsfilters/filter.h>
#include <cupsfilters/raster.h>
#include <stdio.h>
#include <string.h>


//
// One test case: a printer, the options of a job, and the PWG Raster
// header the job has to get.
//

typedef struct
{
  int		ppd_header;		// Start from the sample header of a
					// driverless PPD?
  const char	*raster_types;		// pwg-raster-document-type-supported,
					// comma-separated, NULL for no
					// printer attributes
  const char	*default_mode;		// print-color-mode-default or NULL
  const char	*color_model;		// ColorModel option or NULL
  const char	*print_color_mode;	// print-color-mode option or NULL
  int		bits;			// Expected bits per color
  cups_cspace_t	color_space;		// Expected color space
} test_case_t;


//
// 'run_test()' - Run cfRasterPrepareHeader() for one test case.
//

static int				// O - 1 if passed, 0 if failed
run_test(const test_case_t *test)	// I - Test case
{
  cf_filter_data_t	data;		// Filter data
  cups_page_header_t	h,		// Raster header
			sample;		// Sample header
  cups_cspace_t		cspace = -1;	// Color space, auto
  ipp_t			*printer_attrs = NULL;
					// Printer attributes
  char			types[256],	// Copy of the raster types
			*type;		// Current raster type
  const char		*values[16];	// Raster types
  int			num_values = 0;	// Number of raster types
  int			ret;		// Return value
  static const char * const modes[] =	// print-color-mode-supported
  {
    "bi-level", "monochrome", "color"
  };


  memset(&data, 0, sizeof(data));
  memset(&h, 0, sizeof(h));

  if (test->ppd_header)
  {
    //
    // With a PPD, libppd's ppdFilterLoadPPD() supplies a sample header
    // made from the PPD's option settings.  The ColorModel choices of a
    // driverless PPD have no code, so this header always is 1-bit black,
    // whatever the printer supports, and cfRasterPrepareHeader() has to
    // replace color space and depth...
    //

    memset(&sample, 0, sizeof(sample));
    sample.HWResolution[0]  = 300;
    sample.HWResolution[1]  = 300;
    sample.PageSize[0]      = 595;
    sample.PageSize[1]      = 842;
    sample.cupsPageSize[0]  = 595.0f;
    sample.cupsPageSize[1]  = 842.0f;
    sample.cupsBitsPerColor = 1;
    sample.cupsBitsPerPixel = 1;
    sample.cupsColorSpace   = CUPS_CSPACE_K;
    sample.cupsNumColors    = 1;
    data.header = &sample;
  }

  if (test->raster_types)
  {
    snprintf(types, sizeof(types), "%s", test->raster_types);
    for (type = strtok(types, ",");
	 type && num_values < (int)(sizeof(values) / sizeof(values[0]));
	 type = strtok(NULL, ","))
      values[num_values ++] = type;

    printer_attrs = ippNew();
    ippAddStrings(printer_attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
		  "pwg-raster-document-type-supported", num_values, NULL,
		  values);
    ippAddStrings(printer_attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
		  "print-color-mode-supported",
		  (int)(sizeof(modes) / sizeof(modes[0])), NULL, modes);
    if (test->default_mode)
      ippAddString(printer_attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
		   "print-color-mode-default", NULL, test->default_mode);
    data.printer_attrs = printer_attrs;
  }

  if (test->color_model)
    data.num_options = cupsAddOption("ColorModel", test->color_model,
				     data.num_options, &data.options);
  if (test->print_color_mode)
    data.num_options = cupsAddOption("print-color-mode",
				     test->print_color_mode,
				     data.num_options, &data.options);

  ret = cfRasterPrepareHeader(&h, &data, CF_FILTER_OUT_FORMAT_PWG_RASTER,
			      CF_FILTER_OUT_FORMAT_PWG_RASTER, 0, &cspace);

  cupsFreeOptions(data.num_options, data.options);
  ippDelete(printer_attrs);

  if (ret == 0 && (int)h.cupsBitsPerColor == test->bits &&
      h.cupsColorSpace == test->color_space)
    return (1);

  printf("FAIL: %sprinter %s (default %s), ColorModel=%s, "
	 "print-color-mode=%s: expected %d bit in color space %d, got %d bit "
	 "in color space %d, return value %d\n",
	 test->ppd_header ? "PPD header, " : "",
	 test->raster_types ? test->raster_types : "without attributes",
	 test->default_mode ? test->default_mode : "none",
	 test->color_model ? test->color_model : "(not set)",
	 test->print_color_mode ? test->print_color_mode : "(not set)",
	 test->bits, (int)test->color_space, (int)h.cupsBitsPerColor,
	 (int)h.cupsColorSpace, ret);

  return (0);
}


int
main(void)
{
  int	i;				// Looping var
  int	failed = 0;			// Number of failed tests
  static const test_case_t tests[] =
  {
    //
    // FastGray and ProcessFastGray are the ColorModel choices for bi-level
    // and process-bi-level, they get 1-bit black if the printer has
    // black_1 (Issue #298)...
    //
    // PPD  raster types       default       ColorModel         print-color-mode

    { 0, "black_1,sgray_8",  "monochrome", "FastGray",        NULL,
      1, CUPS_CSPACE_K },
    { 0, "black_1,sgray_8",  NULL,         "FastGray",        NULL,
      1, CUPS_CSPACE_K },
    { 0, "black_1,sgray_8",  "monochrome", "ProcessFastGray", NULL,
      1, CUPS_CSPACE_K },
    { 0, NULL,               NULL,         "FastGray",        NULL,
      1, CUPS_CSPACE_K },
    { 0, "black_1,sgray_8",  "monochrome", NULL,              "bi-level",
      1, CUPS_CSPACE_K },
    { 1, "black_1,sgray_8",  "monochrome", "FastGray",        NULL,
      1, CUPS_CSPACE_K },

    //
    // ...everything else stays 8-bit sGray on that printer, also if
    // print-color-mode is set explicitly next to ColorModel=FastGray...
    //

    { 0, "black_1,sgray_8",  "monochrome", "Gray",            NULL,
      8, CUPS_CSPACE_SW },
    { 0, "black_1,sgray_8",  "monochrome", NULL,              NULL,
      8, CUPS_CSPACE_SW },
    { 0, "black_1,sgray_8",  "monochrome", "FastGray",        "monochrome",
      8, CUPS_CSPACE_SW },
    { 1, "black_1,sgray_8",  "monochrome", NULL,              NULL,
      8, CUPS_CSPACE_SW },

    //
    // ...and with sgray_1 instead of black_1 it is 1-bit sGray.
    //

    { 0, "sgray_1,sgray_8",  "monochrome", "FastGray",        NULL,
      1, CUPS_CSPACE_SW },

    //
    // A printer which only has black_N types gets black in a depth it
    // supports, for every job.  With a PPD the 1-bit sample header must
    // not get through, and without black_1 the bi-level modes must not
    // give 1 bit either (Issue #350)...
    //

    { 1, "black_8",          "monochrome", NULL,              NULL,
      8, CUPS_CSPACE_K },
    { 1, "black_8",          "monochrome", "Gray",            NULL,
      8, CUPS_CSPACE_K },
    { 1, "black_8",          "monochrome", "FastGray",        NULL,
      8, CUPS_CSPACE_K },
    { 1, "black_8",          "monochrome", NULL,              "bi-level",
      8, CUPS_CSPACE_K },
    { 1, "black_16",         "monochrome", NULL,              NULL,
      16, CUPS_CSPACE_K },
    { 0, "black_8",          "monochrome", "Gray",            NULL,
      8, CUPS_CSPACE_K },
    { 0, "black_8",          "monochrome", "FastGray",        NULL,
      8, CUPS_CSPACE_K },
    { 0, "black_16",         "monochrome", "Gray",            NULL,
      16, CUPS_CSPACE_K },

    //
    // ...and with black_1 and black_8 grayscale is 8 bit and bi-level is
    // 1 bit.
    //

    { 1, "black_1,black_8",  "monochrome", NULL,              NULL,
      8, CUPS_CSPACE_K },
    { 1, "black_1,black_8",  "monochrome", "FastGray",        NULL,
      1, CUPS_CSPACE_K },
    { 0, "black_1,black_8",  "monochrome", "Gray",            NULL,
      8, CUPS_CSPACE_K },

    //
    // A ColorModel which the printer has no raster type for falls back to
    // the printer's default color mode.
    //

    { 0, "sgray_8",          "monochrome", "RGB",             NULL,
      8, CUPS_CSPACE_SW },
    { 1, "sgray_8",          "monochrome", "RGB",             NULL,
      8, CUPS_CSPACE_SW },
    { 0, "sgray_8",          "monochrome", "DeviceGray",      NULL,
      8, CUPS_CSPACE_SW }
  };


  for (i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); i ++)
  {
    if (!run_test(tests + i))
      failed ++;
  }

  if (failed)
    return (1);

  puts("PASS: cfRasterPrepareHeader color mode tests passed.");
  return (0);
}
