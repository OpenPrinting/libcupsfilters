//
// Helper for test-pdftopdf-split-content-streams.sh (Issue #293).
//
//   test-pdftopdf-split-content-streams --write INPUT.pdf [--annot]
//
// writes the test input: a single page whose /Contents is an array of three
// streams, "q\nq", "0.5 0 0 0.5 0 0 cm ... Q" and "Q", none of which ends
// in whitespace.  With --annot the page also gets an annotation with an
// appearance stream, so that pdftopdf flattens it (flatten_pdf()) before
// laying it out (copy_page()).
//
//   test-pdftopdf-split-content-streams OUTPUT.pdf
//
// reads back the PDF that cfFilterPDFToPDF() produced and makes sure no
// operators were fused at content stream boundaries.  When the streams are
// concatenated without a separator the output contains the invalid tokens
// "q0.5" and "QQ".
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
// 'write_pdf()' - Write the test input PDF.
//

static int				// O - 0 on success, 1 on error
write_pdf(const char *filename,		// I - PDF file to write
          int        annot)		// I - Add an annotation?
{
  FILE		*fp;			// PDF file
  long		offsets[9],		// Object offsets
		xref;			// Cross-reference table offset
  int		i,			// Looping var
		num_objs = annot ? 9 : 7;// Number of objects + 1
  static const char * const streams[] =	// Page content streams
  {
    "q\nq",
    "0.5 0 0 0.5 0 0 cm\n1 0 0 rg\n144 144 576 288 re f\nQ",
    "Q"
  };


  if ((fp = fopen(filename, "wb")) == NULL)
  {
    perror(filename);
    return (1);
  }

  fputs("%PDF-1.4\n", fp);

  offsets[1] = ftell(fp);
  fputs("1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n", fp);

  offsets[2] = ftell(fp);
  fputs("2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n", fp);

  offsets[3] = ftell(fp);
  fprintf(fp, "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> /Contents [4 0 R 5 0 R 6 0 R]%s >>\nendobj\n", annot ? " /Annots [7 0 R]" : "");

  // The streams don't end in whitespace, the newline before "endstream" is
  // not part of the stream data...
  for (i = 0; i < 3; i ++)
  {
    offsets[4 + i] = ftell(fp);
    fprintf(fp, "%d 0 obj\n<< /Length %u >>\nstream\n%s\nendstream\nendobj\n", 4 + i, (unsigned)strlen(streams[i]), streams[i]);
  }

  if (annot)
  {
    offsets[7] = ftell(fp);
    fputs("7 0 obj\n<< /Type /Annot /Subtype /Square /Rect [400 600 450 650] /F 4 /AP << /N 8 0 R >> >>\nendobj\n", fp);

    offsets[8] = ftell(fp);
    fputs("8 0 obj\n<< /Type /XObject /Subtype /Form /BBox [0 0 50 50] /Length 23 >>\nstream\n0 0 1 rg\n0 0 50 50 re f\nendstream\nendobj\n", fp);
  }

  xref = ftell(fp);
  fprintf(fp, "xref\n0 %d\n0000000000 65535 f \n", num_objs);
  for (i = 1; i < num_objs; i ++)
    fprintf(fp, "%010ld 00000 n \n", offsets[i]);
  fprintf(fp, "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n", num_objs, xref);

  if (fclose(fp))
  {
    perror(filename);
    return (1);
  }

  return (0);
}


//
// 'main()' - Write the test input or scan the output for fused operators.
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


  if (argc >= 3 && argc <= 4 && !strcmp(argv[1], "--write"))
  {
    if (argc == 4 && strcmp(argv[3], "--annot"))
    {
      fputs("Usage: test-pdftopdf-split-content-streams --write INPUT.pdf [--annot]\n", stderr);
      return (1);
    }

    return (write_pdf(argv[2], argc == 4));
  }

  if (argc != 2)
  {
    fputs("Usage: test-pdftopdf-split-content-streams --write INPUT.pdf [--annot]\n"
          "       test-pdftopdf-split-content-streams OUTPUT.pdf\n", stderr);
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
