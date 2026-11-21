//
// Regression test: cfFilterPDFToPDF() must take the output order and duplex
// mode from the printer's defaults when the job does not ask for them.
//
// The QPDF-based pdftopdf (up to 2.1.1) used cfIPPReverseOutput() and the
// printer's "sides" default for jobs without an output order or duplex
// mode.  The PDFio-based rewrite only looked at the job's options, so
// printers with a face-up output bin stacked every job last page on top,
// and queues defaulting to duplex printed documents and copies with odd
// page counts without padding them to whole sheets.
//
// Each test runs a 3-page document whose pages are tagged "/CFPage1" to
// "/CFPage3" through cfFilterPDFToPDF() and checks the order of the pages
// in the output, with 0 standing for a blank page inserted by pdftopdf.
//
// Copyright © 2026 by OpenPrinting.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <config.h>
#include <cupsfilters/filter.h>
#include <fcntl.h>
#include <pdfio.h>
#include <pdfio-content.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


//
// Printer output bins...
//

typedef enum
{
  BIN_NONE,				// No printer attributes
  BIN_STACKING_LAST_TO_FIRST,		// Face-up bin that reports
					// "stackingorder=lastToFirst" (as an
					// Epson ET-4950 does)
  BIN_FACE_UP,				// "face-up" bin without
					// "printer-output-tray"
  BIN_FACE_DOWN				// "face-down" bin
} bin_t;


//
// One test case...
//

typedef struct
{
  const char	*name;			// Description
  bin_t		bin;			// Printer's default output bin
  const char	*sides_default;		// Printer's "sides-default" or NULL
  const char	*option;		// Job option ("name=value") or NULL
  int		copies;			// Number of copies
  const char	*expected;		// Expected pages in output order
} test_case_t;


//
// 'make_input()' - Create the 3-page test document.
//

static int				// O - 1 on success, 0 on failure
make_input(const char *filename)	// I - Output filename
{
  pdfio_rect_t	media = { 0.0, 0.0, 612.0, 792.0 };
					// US Letter
  pdfio_file_t	*pdf;			// Output file
  pdfio_obj_t	*font;			// Helvetica
  int		i;			// Looping var


  if ((pdf = pdfioFileCreate(filename, "1.7", &media, &media, NULL, NULL)) == NULL)
    return (0);

  font = pdfioFileCreateFontObjFromBase(pdf, "Helvetica");

  for (i = 1; i <= 3; i ++)
  {
    pdfio_dict_t	*dict = pdfioDictCreate(pdf);
					// Page dictionary
    pdfio_stream_t	*st;		// Page content

    pdfioPageDictAddFont(dict, "F1", font);

    if ((st = pdfioFileCreatePage(pdf, dict)) == NULL)
    {
      pdfioFileClose(pdf);
      return (0);
    }

    pdfioStreamPrintf(st, "/CFPage%d BMC\n", i);
    pdfioContentTextBegin(st);
    pdfioContentSetTextFont(st, "F1", 36.0);
    pdfioContentTextMoveTo(st, 72.0, 690.0);
    pdfioContentTextShowf(st, false, "Page %d", i);
    pdfioContentTextEnd(st);
    pdfioStreamPuts(st, "EMC\n");
    pdfioStreamClose(st);
  }

  return (pdfioFileClose(pdf));
}


//
// 'find_tag()' - Find the "/CFPageN" tag in a stream.
//

static int				// O - Page number or 0 if none
find_tag(pdfio_stream_t *st)		// I - Stream
{
  char		buffer[65536];		// Stream data
  size_t	total = 0;		// Bytes read
  ssize_t	bytes;			// Bytes read this time
  char		*ptr;			// Tag in buffer


  if (!st)
    return (0);

  while (total < sizeof(buffer) - 1 &&
         (bytes = pdfioStreamRead(st, buffer + total, sizeof(buffer) - 1 - total)) > 0)
    total += (size_t)bytes;

  pdfioStreamClose(st);

  buffer[total] = '\0';

  if ((ptr = strstr(buffer, "/CFPage")) != NULL)
    return (atoi(ptr + 7));

  return (0);
}


//
// 'page_number()' - Get the input page number shown on an output page.
//
// The page content and the form XObjects used by the page are searched.
//

static int				// O - Input page number or 0 if blank
page_number(pdfio_obj_t *page)		// I - Output page
{
  size_t	i;			// Looping var
  int		number;			// Page number
  pdfio_dict_t	*resources,		// Page resources
		*xobjects;		// Form XObjects
  pdfio_obj_t	*xobj;			// Current XObject


  for (i = 0; i < pdfioPageGetNumStreams(page); i ++)
  {
    if ((number = find_tag(pdfioPageOpenStream(page, i, true))) > 0)
      return (number);
  }

  if ((resources = pdfioDictGetDict(pdfioObjGetDict(page), "Resources")) == NULL ||
      (xobjects = pdfioDictGetDict(resources, "XObject")) == NULL)
    return (0);

  for (i = 0; i < pdfioDictGetNumPairs(xobjects); i ++)
  {
    if ((xobj = pdfioDictGetObj(xobjects, pdfioDictGetKey(xobjects, i))) != NULL &&
        (number = find_tag(pdfioObjOpenStream(xobj, true))) > 0)
      return (number);
  }

  return (0);
}


//
// 'printer_attributes()' - Create the printer attributes for a test case.
//

static ipp_t *				// O - Printer attributes or NULL
printer_attributes(
    const test_case_t *test)		// I - Test case
{
  ipp_t		*attrs;			// Printer attributes
  static const char * const sides[] =	// "sides-supported" values
  {
    "one-sided",
    "two-sided-long-edge",
    "two-sided-short-edge"
  };
  static const char tray[] =		// "printer-output-tray" value
    "type=unRemovableBin;maxcapacity=30;remaining=-3;status=0;"
    "name=Face-up Tray;stackingorder=lastToFirst;pagedelivery=faceUp;";


  if (test->bin == BIN_NONE)
    return (NULL);

  attrs = ippNew();

  ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "output-bin-default", NULL, test->bin == BIN_FACE_DOWN ? "face-down" : "face-up");
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "output-bin-supported", NULL, test->bin == BIN_FACE_DOWN ? "face-down" : "face-up");

  if (test->bin == BIN_STACKING_LAST_TO_FIRST)
    ippAddOctetString(attrs, IPP_TAG_PRINTER, "printer-output-tray", tray, sizeof(tray) - 1);

  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "sides-supported", 3, NULL, sides);
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "sides-default", NULL, test->sides_default ? test->sides_default : "one-sided");

  return (attrs);
}


//
// 'run_test()' - Run cfFilterPDFToPDF() for one test case.
//

static int				// O - 1 if passed, 0 if failed
run_test(const test_case_t *test,	// I - Test case
         const char        *input,	// I - Input document
         const char        *output)	// I - Output document
{
  cf_filter_data_t	data;		// Filter data
  ipp_t			*printer_attrs;	// Printer attributes
  int			infd,		// Input file
			outfd,		// Output file
			ret;		// Filter status
  pdfio_file_t		*pdf;		// Output document
  size_t		i;		// Looping var
  char			got[256] = "";	// Pages in output order


  memset(&data, 0, sizeof(data));

  printer_attrs           = printer_attributes(test);
  data.printer            = (char *)"test";
  data.job_id             = 1;
  data.job_user           = (char *)"user";
  data.job_title          = (char *)"title";
  data.copies             = test->copies;
  data.content_type       = (char *)"application/pdf";
  data.final_content_type = (char *)"image/urf";
  data.printer_attrs      = printer_attrs;
  data.back_pipe[0]       = data.back_pipe[1] = -1;
  data.side_pipe[0]       = data.side_pipe[1] = -1;

  if (test->option)
  {
    char	name[256],		// Option name
		*value;			// Option value

    snprintf(name, sizeof(name), "%s", test->option);
    if ((value = strchr(name, '=')) != NULL)
    {
      *value++ = '\0';
      data.num_options = cupsAddOption(name, value, data.num_options, &data.options);
    }
  }

  infd  = open(input, O_RDONLY);
  outfd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);

  ret = cfFilterPDFToPDF(infd, outfd, 1, &data, NULL);

  close(infd);
  close(outfd);
  cupsFreeOptions(data.num_options, data.options);
  ippDelete(printer_attrs);

  if (ret)
  {
    printf("FAIL: %s: cfFilterPDFToPDF() returned %d\n", test->name, ret);
    return (0);
  }

  if ((pdf = pdfioFileOpen(output, NULL, NULL, NULL, NULL)) == NULL)
  {
    printf("FAIL: %s: unable to open output\n", test->name);
    return (0);
  }

  for (i = 0; i < pdfioFileGetNumPages(pdf); i ++)
    snprintf(got + strlen(got), sizeof(got) - strlen(got), "%s%d", i ? "," : "", page_number(pdfioFileGetPage(pdf, i)));

  pdfioFileClose(pdf);

  if (strcmp(got, test->expected))
  {
    printf("FAIL: %s: expected pages %s, got %s\n", test->name, test->expected, got);
    return (0);
  }

  printf("PASS: %s\n", test->name);
  return (1);
}


//
// 'main()' - Run the tests.
//

int					// O - Exit status
main(void)
{
  int		i,			// Looping var
		failed = 0;		// Number of failed tests
  char		input[] = "/tmp/test-pdftopdf-printer-defaults-in.XXXXXX",
		output[] = "/tmp/test-pdftopdf-printer-defaults-out.XXXXXX";
  int		fd;			// Temporary file
  static const test_case_t tests[] =	// Test cases
  {
    // No output order requested: the printer's default output bin decides
    { "lastToFirst bin, no options", BIN_STACKING_LAST_TO_FIRST, NULL, NULL, 1, "3,2,1" },
    { "face-up bin, no options", BIN_FACE_UP, NULL, NULL, 1, "3,2,1" },
    { "face-down bin, no options", BIN_FACE_DOWN, NULL, NULL, 1, "1,2,3" },
    { "no printer attributes, no options", BIN_NONE, NULL, NULL, 1, "1,2,3" },

    // Requested output order, page delivery, or output bin wins
    { "lastToFirst bin, outputorder=normal", BIN_STACKING_LAST_TO_FIRST, NULL, "outputorder=normal", 1, "1,2,3" },
    { "face-down bin, outputorder=reverse", BIN_FACE_DOWN, NULL, "outputorder=reverse", 1, "3,2,1" },
    { "face-up bin, output-bin=face-down", BIN_FACE_UP, NULL, "output-bin=face-down", 1, "1,2,3" },
    { "face-down bin, output-bin=face-up", BIN_FACE_DOWN, NULL, "output-bin=face-up", 1, "3,2,1" },
    { "face-up bin, page-delivery=same-order-face-down", BIN_FACE_UP, NULL, "page-delivery=same-order-face-down", 1, "1,2,3" },

    // No duplex mode requested: the printer's "sides" default decides,
    // documents and copies are padded to whole sheets
    { "duplex default, face-down bin", BIN_FACE_DOWN, "two-sided-long-edge", NULL, 1, "1,2,3,0" },
    { "duplex default, lastToFirst bin", BIN_STACKING_LAST_TO_FIRST, "two-sided-long-edge", NULL, 1, "0,3,2,1" },
    { "duplex default, face-down bin, 2 copies", BIN_FACE_DOWN, "two-sided-long-edge", NULL, 2, "1,2,3,0,1,2,3,0" },
    { "duplex default, sides=one-sided", BIN_FACE_DOWN, "two-sided-long-edge", "sides=one-sided", 1, "1,2,3" },
    { "duplex default, Duplex=None", BIN_FACE_DOWN, "two-sided-long-edge", "Duplex=None", 1, "1,2,3" },
    { "simplex default, sides=two-sided-long-edge", BIN_FACE_DOWN, NULL, "sides=two-sided-long-edge", 1, "1,2,3,0" }
  };


  if ((fd = mkstemp(input)) < 0)
  {
    perror(input);
    return (1);
  }
  close(fd);

  if ((fd = mkstemp(output)) < 0)
  {
    perror(output);
    unlink(input);
    return (1);
  }
  close(fd);

  if (!make_input(input))
  {
    puts("FAIL: unable to create the test document");
    failed ++;
  }
  else
  {
    for (i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); i ++)
    {
      if (!run_test(tests + i, input, output))
        failed ++;
    }
  }

  unlink(input);
  unlink(output);

  return (failed ? 1 : 0);
}
