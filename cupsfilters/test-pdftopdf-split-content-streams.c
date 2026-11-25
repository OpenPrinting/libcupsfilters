//
// Checker for test-pdftopdf-split-content-streams.sh: read back a PDF that
// cfFilterPDFToPDF() produced from test_files/split_content_streams*.pdf
// and make sure no operators were fused at content stream boundaries.
//
// The input page's /Contents is an array of three streams, "q\nq",
// "0.5 0 0 0.5 0 0 cm ... Q" and "Q", none of which ends in whitespace.
// When they are concatenated without a separator the output contains the
// invalid tokens "q0.5" and "QQ" (Issue #293).
//
// Copyright © 2026 by OpenPrinting
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <pdfio.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>


//
// 'main()' - Scan the page content for fused operators.
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
		*ptr;			// Pointer into token
  int		num_save = 0,		// Number of "q" operators
		num_restore = 0,	// Number of "Q" operators
		bad = 0;		// Number of fused tokens
  int		has_alpha,		// Token has letters?
		has_digit;		// Token has digits?


  if (argc != 2)
  {
    fputs("Usage: test-pdftopdf-split-content-streams OUTPUT.pdf\n", stderr);
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

  for (i = 0; i < num_streams; i ++)
  {
    if ((st = pdfioPageOpenStream(page, i, true)) == NULL)
    {
      fprintf(stderr, "Unable to open content stream %u.\n", (unsigned)i);
      return (1);
    }

    while (pdfioStreamGetToken(st, token, sizeof(token)))
    {
      if (!strcmp(token, "q"))
      {
        num_save ++;
      }
      else if (!strcmp(token, "Q"))
      {
        num_restore ++;
      }
      else if (token[0] != '/' && token[0] != '(' && token[0] != '<' && token[0] != '[')
      {
        // Operators are all letters, numbers have no letters...
        for (ptr = token, has_alpha = 0, has_digit = 0; *ptr; ptr ++)
        {
          if (isalpha(*ptr & 255))
            has_alpha = 1;
          else if (isdigit(*ptr & 255))
            has_digit = 1;
        }

        if ((has_alpha && has_digit) || !strcmp(token, "QQ") || !strcmp(token, "qq") || !strcmp(token, "qQ") || !strcmp(token, "Qq"))
        {
          fprintf(stderr, "Fused token \"%s\" in content stream %u.\n", token, (unsigned)i);
          bad ++;
        }
      }
    }

    pdfioStreamClose(st);
  }

  pdfioFileClose(pdf);

  printf("q = %d, Q = %d, fused = %d\n", num_save, num_restore, bad);

  if (bad)
    return (1);

  if (num_save != num_restore)
  {
    fputs("Unbalanced q/Q operators.\n", stderr);
    return (1);
  }

  if (num_save < 3)
  {
    fputs("The input page's q/Q operators are missing from the output.\n", stderr);
    return (1);
  }

  return (0);
}
