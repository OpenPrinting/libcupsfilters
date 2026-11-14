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
    const char *one_bit_type,		// I - 1-bit raster type of the printer
					//     ("black_1", "sgray_1") or NULL
					//     for no printer attributes
    int        with_default,		// I - Supply print-color-mode-default?
    const char *color_model,		// I - ColorModel option or NULL
    const char *print_color_mode,	// I - print-color-mode option or NULL
    int        *color_space)		// O - Color space of the header
{
  cf_filter_data_t	data;		// Filter data
  cups_page_header_t	h;		// Raster header
  cups_cspace_t		cspace = -1;	// Color space, auto
  ipp_t			*printer_attrs = NULL;
					// Printer attributes
  const char		*types[2];	// pwg-raster-document-type-supported
  static const char * const modes[] =	// print-color-mode-supported
  {
    "bi-level", "monochrome"
  };


  memset(&data, 0, sizeof(data));
  memset(&h, 0, sizeof(h));

  if (one_bit_type)
  {
    types[0] = one_bit_type;
    types[1] = "sgray_8";

    printer_attrs = ippNew();
    ippAddStrings(printer_attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
		  "pwg-raster-document-type-supported",
		  (int)(sizeof(types) / sizeof(types[0])), NULL, types);
    ippAddStrings(printer_attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
		  "print-color-mode-supported",
		  (int)(sizeof(modes) / sizeof(modes[0])), NULL, modes);
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

  *color_space = (int)h.cupsColorSpace;

  return ((int)h.cupsBitsPerColor);
}


int
main(void)
{
  int	i;				// Looping var
  int	bits,				// Bits per color
	color_space;			// Color space
  static const struct
  {
    const char	*name;			// Test name
    const char	*one_bit_type;		// 1-bit raster type or NULL
    int		with_default;		// print-color-mode-default?
    const char	*color_model,		// ColorModel option
		*print_color_mode;	// print-color-mode option
    int		bits,			// Expected bits per color
		color_space;		// Expected color space
  } tests[] =
  {
    { "ColorModel=FastGray, printer default monochrome",
      "black_1", 1, "FastGray", NULL, 1, CUPS_CSPACE_K },
    { "ColorModel=FastGray, no printer default",
      "black_1", 0, "FastGray", NULL, 1, CUPS_CSPACE_K },
    { "ColorModel=ProcessFastGray, printer default monochrome",
      "black_1", 1, "ProcessFastGray", NULL, 1, CUPS_CSPACE_K },
    { "ColorModel=FastGray, no printer attributes",
      NULL, 0, "FastGray", NULL, 1, CUPS_CSPACE_K },
    { "ColorModel=FastGray, printer with sgray_1 instead of black_1",
      "sgray_1", 1, "FastGray", NULL, 1, CUPS_CSPACE_SW },
    { "ColorModel=Gray, printer default monochrome",
      "black_1", 1, "Gray", NULL, 8, CUPS_CSPACE_SW },
    { "print-color-mode=bi-level",
      "black_1", 1, NULL, "bi-level", 1, CUPS_CSPACE_K },
    { "print-color-mode=monochrome wins over ColorModel=FastGray",
      "black_1", 1, "FastGray", "monochrome", 8, CUPS_CSPACE_SW },
    { "no options, printer default monochrome",
      "black_1", 1, NULL, NULL, 8, CUPS_CSPACE_SW }
  };


  for (i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); i ++)
  {
    bits = prepare_header(tests[i].one_bit_type, tests[i].with_default,
			  tests[i].color_model, tests[i].print_color_mode,
			  &color_space);
    if (bits != tests[i].bits || color_space != tests[i].color_space)
    {
      printf("FAIL: %s: expected %d bit, color space %d, got %d bit, "
	     "color space %d\n", tests[i].name, tests[i].bits,
	     tests[i].color_space, bits, color_space);
      return (1);
    }
  }

  puts("PASS: cfRasterPrepareHeader issue #298 color mode tests passed.");
  return (0);
}
