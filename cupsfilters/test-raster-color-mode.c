//
// Color mode regression test for cfRasterPrepareHeader() (Issue #298).
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
// 'prepare_header()' - Run cfRasterPrepareHeader() for PWG Raster output
//                      and return the bits per color of the header.
//

static int				// O - Bits per color, -1 on error
prepare_header(
    int        with_printer,		// I - Supply printer attributes?
    int        with_default,		// I - Supply print-color-mode-default?
    const char *color_model,		// I - ColorModel option or NULL
    const char *print_color_mode)	// I - print-color-mode option or NULL
{
  cf_filter_data_t	data;		// Filter data
  cups_page_header_t	h;		// Raster header
  cups_cspace_t		cspace = -1;	// Color space, auto
  ipp_t			*printer_attrs = NULL;
					// Printer attributes
  static const char * const types[] =	// pwg-raster-document-type-supported
  {
    "black_1", "sgray_8"
  };
  static const char * const modes[] =	// print-color-mode-supported
  {
    "bi-level", "monochrome"
  };


  memset(&data, 0, sizeof(data));
  memset(&h, 0, sizeof(h));
  data.content_type = "application/pdf";

  if (with_printer)
  {
    printer_attrs = ippNew();
    ippAddStrings(printer_attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
		  "pwg-raster-document-type-supported", 2, NULL, types);
    ippAddStrings(printer_attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
		  "print-color-mode-supported", 2, NULL, modes);
    if (with_default)
      ippAddString(printer_attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
		   "print-color-mode-default", NULL, "monochrome");
    ippAddResolution(printer_attrs, IPP_TAG_PRINTER,
		     "pwg-raster-document-resolution-supported",
		     IPP_RES_PER_INCH, 300, 300);
    data.printer_attrs = printer_attrs;
  }

  if (color_model)
    data.num_options = cupsAddOption("ColorModel", color_model,
				     data.num_options, &data.options);
  if (print_color_mode)
    data.num_options = cupsAddOption("print-color-mode", print_color_mode,
				     data.num_options, &data.options);

  if (cfRasterPrepareHeader(&h, &data, CF_FILTER_OUT_FORMAT_PWG_RASTER,
			    CF_FILTER_OUT_FORMAT_PWG_RASTER, 0, &cspace) < 0)
    h.cupsBitsPerColor = (unsigned)-1;

  cupsFreeOptions(data.num_options, data.options);
  ippDelete(printer_attrs);

  return ((int)h.cupsBitsPerColor);
}


int
main(void)
{
  int	i;				// Looping var
  int	bits;				// Bits per color
  static const struct
  {
    const char	*name;			// Test name
    int		with_printer,		// Printer attributes?
		with_default;		// print-color-mode-default?
    const char	*color_model,		// ColorModel option
		*print_color_mode;	// print-color-mode option
    int		bits;			// Expected bits per color
  } tests[] =
  {
    { "ColorModel=FastGray, printer default monochrome",
      1, 1, "FastGray", NULL, 1 },
    { "ColorModel=FastGray, no printer default",
      1, 0, "FastGray", NULL, 1 },
    { "ColorModel=ProcessFastGray, printer default monochrome",
      1, 1, "ProcessFastGray", NULL, 1 },
    { "ColorModel=FastGray, no printer attributes",
      0, 0, "FastGray", NULL, 1 },
    { "ColorModel=Gray, printer default monochrome",
      1, 1, "Gray", NULL, 8 },
    { "print-color-mode=bi-level",
      1, 1, NULL, "bi-level", 1 },
    { "print-color-mode=monochrome wins over ColorModel=FastGray",
      1, 1, "FastGray", "monochrome", 8 },
    { "no options, printer default monochrome",
      1, 1, NULL, NULL, 8 }
  };


  for (i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); i ++)
  {
    bits = prepare_header(tests[i].with_printer, tests[i].with_default,
			  tests[i].color_model, tests[i].print_color_mode);
    if (bits != tests[i].bits)
    {
      printf("FAIL: %s: expected %d bit, got %d bit\n", tests[i].name,
	     tests[i].bits, bits);
      return (1);
    }
  }

  puts("PASS: cfRasterPrepareHeader issue #298 color mode tests passed.");
  return (0);
}
