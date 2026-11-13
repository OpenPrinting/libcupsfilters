//
// Checker for test-pdftopdf-copies.sh: read back the PDF that
// cfFilterPDFToPDF() produced and check its page count and the copies
// and collate comments in its header.  Every page is loaded, which also
// verifies the cross-reference table pdftopdf has rewritten behind the
// inserted comments.
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
// 'main()' - Check the number of pages and the pdftopdf comments.
//

int					// O - 0 on success, 1 on failure
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  pdfio_file_t	*pdf;			// Output PDF from pdftopdf
  pdfio_obj_t	*page;			// Current page
  pdfio_stream_t *st;			// Content stream
  FILE		*fp;			// Output PDF as text
  char		line[256],		// Header line
		*value;			// Comment value
  size_t	i,			// Looping var
		expected;		// Expected number of pages
  int		copies = 0;		// "%%PDFTOPDFNumCopies" value
  const char	*collate = NULL;	// "%%PDFTOPDFCollate" value


  if (argc != 5)
  {
    fputs("Usage: test-pdftopdf-copies OUTPUT.pdf PAGES COPIES COLLATE\n", stderr);
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

  if ((fp = fopen(argv[1], "r")) == NULL)
  {
    fprintf(stderr, "Unable to open \"%s\".\n", argv[1]);
    return (1);
  }

  for (i = 0; i < 10 && fgets(line, sizeof(line), fp); i ++)
  {
    if ((value = strchr(line, ':')) == NULL)
      continue;

    value ++;
    while (*value == ' ')
      value ++;

    if (!strncmp(line, "%%PDFTOPDFNumCopies", 19))
      copies = atoi(value);
    else if (!strncmp(line, "%%PDFTOPDFCollate", 17))
      collate = strncmp(value, "true", 4) ? "false" : "true";
  }

  fclose(fp);

  if (copies != atoi(argv[3]))
  {
    fprintf(stderr, "Expected %%%%PDFTOPDFNumCopies : %s, got %d.\n", argv[3], copies);
    return (1);
  }

  if (!collate || strcmp(collate, argv[4]))
  {
    fprintf(stderr, "Expected %%%%PDFTOPDFCollate : %s, got %s.\n", argv[4], collate ? collate : "nothing");
    return (1);
  }

  return (0);
}
