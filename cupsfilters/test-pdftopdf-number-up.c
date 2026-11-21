//
// Checker for test-pdftopdf-number-up.sh: read back the PDF that
// cfFilterPDFToPDF() produced and check its page count.  Every page is
// loaded to make sure the output is a usable PDF.
//
// Copyright © 2026 by OpenPrinting
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <pdfio.h>
#include <stdio.h>
#include <stdlib.h>


//
// 'main()' - Check the number of pages.
//

int					// O - 0 on success, 1 on failure
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  pdfio_file_t	*pdf;			// Output PDF from pdftopdf
  pdfio_obj_t	*page;			// Current page
  pdfio_stream_t *st;			// Content stream
  size_t	i,			// Looping var
		expected;		// Expected number of pages


  if (argc != 3)
  {
    fputs("Usage: test-pdftopdf-number-up OUTPUT.pdf PAGES\n", stderr);
    return (1);
  }

  expected = (size_t)strtoul(argv[2], NULL, 10);

  if ((pdf = pdfioFileOpen(argv[1], NULL, NULL, NULL, NULL)) == NULL)
  {
    fprintf(stderr, "Unable to open \"%s\".\n", argv[1]);
    return (1);
  }

  if (pdfioFileGetNumPages(pdf) != expected)
  {
    fprintf(stderr, "Expected %u output pages, got %u.\n", (unsigned)expected, (unsigned)pdfioFileGetNumPages(pdf));
    return (1);
  }

  for (i = 0; i < expected; i ++)
  {
    if ((page = pdfioFileGetPage(pdf, i)) == NULL || !pdfioObjGetDict(page) ||
        (st = pdfioPageOpenStream(page, 0, true)) == NULL)
    {
      fprintf(stderr, "Unable to load page %u.\n", (unsigned)i + 1);
      return (1);
    }

    pdfioStreamClose(st);
  }

  pdfioFileClose(pdf);

  return (0);
}
