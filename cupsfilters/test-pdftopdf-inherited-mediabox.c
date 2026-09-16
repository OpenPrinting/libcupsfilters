//
// Checker for test-pdftopdf-inherited-mediabox.sh: read back the PDF that
// cfFilterPDFToPDF() produced from test_files/inherited_mediabox.pdf and
// inspect the transformation applied to the input page.
//
// The input page is 2970x2100 pt with its MediaBox inherited from the
// /Pages node; printed on Letter with print-scaling=auto it has to be
// scaled down (by about 0.26) and rotated by 90 degrees.  When the
// inherited MediaBox is ignored, pdftopdf assumes the input page is already
// Letter-sized and emits a ~0.97 scale without rotation.
//
// Copyright © 2026 by OpenPrinting
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <pdfio.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//
// 'main()' - Check the first "cm" operator of page 1.
//

int					// O - 0 on success, 1 on failure
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  pdfio_file_t	*pdf;			// Output PDF from pdftopdf
  pdfio_obj_t	*page;			// First page
  pdfio_stream_t *st;			// Content stream
  size_t	i,			// Looping var
		num_streams;		// Number of content streams
  char		token[256],		// Current token
		*end;			// End of numeric token
  double	value,			// Numeric token value
		matrix[6];		// Last six numbers seen
  int		num_values = 0,		// Number of numbers in the window
		found = 0;		// Found a "cm" operator?
  double	scale;			// Scale factor of the matrix


  if (argc != 2)
  {
    fputs("Usage: test-pdftopdf-inherited-mediabox OUTPUT.pdf\n", stderr);
    return (1);
  }

  if ((pdf = pdfioFileOpen(argv[1], NULL, NULL, NULL, NULL)) == NULL)
  {
    fprintf(stderr, "Unable to open \"%s\".\n", argv[1]);
    return (1);
  }

  if (pdfioFileGetNumPages(pdf) != 1)
  {
    fprintf(stderr, "Expected 1 output page, got %u.\n", (unsigned)pdfioFileGetNumPages(pdf));
    return (1);
  }

  page        = pdfioFileGetPage(pdf, 0);
  num_streams = pdfioPageGetNumStreams(page);

  for (i = 0; i < num_streams && !found; i ++)
  {
    if ((st = pdfioPageOpenStream(page, i, true)) == NULL)
    {
      fprintf(stderr, "Unable to open content stream %u.\n", (unsigned)i);
      return (1);
    }

    while (pdfioStreamGetToken(st, token, sizeof(token)))
    {
      value = strtod(token, &end);

      if (end > token && !*end)
      {
        // Number: keep the last six...
        if (num_values == 6)
          memmove(matrix, matrix + 1, 5 * sizeof(double));
        else
          num_values ++;

        matrix[num_values - 1] = value;
      }
      else if (!strcmp(token, "cm") && num_values == 6)
      {
        found = 1;
        break;
      }
      else
        num_values = 0;
    }

    pdfioStreamClose(st);
  }

  pdfioFileClose(pdf);

  if (!found)
  {
    fputs("No \"cm\" operator found on page 1.\n", stderr);
    return (1);
  }

  scale = hypot(matrix[0], matrix[1]);

  printf("cm = [%g %g %g %g %g %g], scale = %g\n", matrix[0], matrix[1], matrix[2], matrix[3], matrix[4], matrix[5], scale);

  if (scale < 0.1 || scale > 0.5)
  {
    fputs("Input page was not scaled to fit: inherited MediaBox ignored?\n", stderr);
    return (1);
  }

  if (fabs(matrix[0]) > 1e-6 || fabs(matrix[3]) > 1e-6)
  {
    fputs("Input page was not rotated to fit: inherited MediaBox ignored?\n", stderr);
    return (1);
  }

  return (0);
}
