//
// Checker for test-pdftopdf-job-sheets.sh: read back the PDF that
// cfFilterPDFToPDF() produced and check its page count and, optionally,
// that the first page contains a given string (the banner text).
//
// Copyright © 2026 by OpenPrinting
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <pdfio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//
// 'main()' - Check the number of pages and the text of page 1.
//

int					// O - 0 on success, 1 on failure
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  pdfio_file_t	*pdf;			// Output PDF from pdftopdf
  pdfio_obj_t	*page;			// First page
  pdfio_stream_t *st;			// Content stream
  size_t	i,			// Looping var
		num_streams,		// Number of content streams
		expected;		// Expected number of pages
  char		buffer[65536];		// Content of page 1
  size_t	used = 0;		// Bytes used in buffer
  ssize_t	bytes;			// Bytes read


  if (argc < 3 || argc > 4)
  {
    fputs("Usage: test-pdftopdf-job-sheets OUTPUT.pdf PAGES [TEXT]\n", stderr);
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

  if (argc == 4)
  {
    page        = pdfioFileGetPage(pdf, 0);
    num_streams = pdfioPageGetNumStreams(page);

    for (i = 0; i < num_streams; i ++)
    {
      if ((st = pdfioPageOpenStream(page, i, true)) == NULL)
      {
        fprintf(stderr, "Unable to open content stream %u.\n", (unsigned)i);
        return (1);
      }

      while (used < sizeof(buffer) - 1 && (bytes = pdfioStreamRead(st, buffer + used, sizeof(buffer) - 1 - used)) > 0)
        used += (size_t)bytes;

      pdfioStreamClose(st);
    }

    buffer[used] = '\0';

    if (!strstr(buffer, argv[3]))
    {
      fprintf(stderr, "\"%s\" not found on page 1.\n", argv[3]);
      return (1);
    }
  }

  pdfioFileClose(pdf);

  return (0);
}
