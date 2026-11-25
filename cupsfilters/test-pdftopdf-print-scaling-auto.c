//
// Regression check for issues #304 and #339: with print-scaling=auto
// (the default) and print-scaling=auto-fit, pdftopdf must not scale a
// document whose pages fit the output media; only a document too large
// for the media (or with ipp-attribute-fidelity set) gets scaled to
// fit.  The first "cm" operator in the output page content is the page
// transform; "noscale" expects scale 1x1, "scaled" expects a shrink.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pdfio.h>

int
main(int  argc,
     char *argv[])
{
  pdfio_file_t   *pdf;
  pdfio_obj_t    *page;
  pdfio_stream_t *st;
  char           buffer[65536],
                 *tok,
                 *save,
                 *end;
  ssize_t        bytes;
  double         last[6] = { 0 };
  int            have = 0;
  int            expect_noscale;


  if (argc != 3 ||
      (strcmp(argv[2], "noscale") && strcmp(argv[2], "scaled")))
  {
    fprintf(stderr, "ERROR: Usage: %s file.pdf noscale|scaled\n", argv[0]);
    return (1);
  }

  expect_noscale = !strcmp(argv[2], "noscale");

  if ((pdf = pdfioFileOpen(argv[1], NULL, NULL, NULL, NULL)) == NULL)
  {
    fprintf(stderr, "ERROR: pdftopdf output could not be opened\n");
    return (1);
  }

  if ((page = pdfioFileGetPage(pdf, 0)) == NULL ||
      (st = pdfioPageOpenStream(page, 0, true)) == NULL)
  {
    fprintf(stderr, "ERROR: pdftopdf output has no readable page content\n");
    pdfioFileClose(pdf);
    return (1);
  }

  bytes = pdfioStreamRead(st, buffer, sizeof(buffer) - 1);
  pdfioStreamClose(st);
  pdfioFileClose(pdf);

  if (bytes <= 0)
  {
    fprintf(stderr, "ERROR: pdftopdf output page content is empty\n");
    return (1);
  }
  buffer[bytes] = '\0';

  for (tok = strtok_r(buffer, " \t\r\n", &save); tok; tok = strtok_r(NULL, " \t\r\n", &save))
  {
    if (!strcmp(tok, "cm"))
    {
      if (have < 6)
      {
        fprintf(stderr, "ERROR: found cm operator without six operands\n");
        return (1);
      }

      if (expect_noscale)
      {
        if (fabs(last[0] - 1.0) > 0.001 || fabs(last[3] - 1.0) > 0.001)
        {
          fprintf(stderr, "ERROR: page was scaled: cm scale is %g x %g, expected 1 x 1\n", last[0], last[3]);
          return (1);
        }

        if (fabs(last[4]) > 1.0 || fabs(last[5]) > 1.0)
        {
          fprintf(stderr, "ERROR: page was moved: cm offset is %g, %g, expected near 0, 0\n", last[4], last[5]);
          return (1);
        }

        return (0);
      }
      else
      {
        if (last[0] > 0.999 || last[3] > 0.999)
        {
          fprintf(stderr, "ERROR: page was not scaled: cm scale is %g x %g, expected a shrink\n", last[0], last[3]);
          return (1);
        }

        return (0);
      }
    }

    memmove(last, last + 1, 5 * sizeof(double));
    last[5] = strtod(tok, &end);
    if (end == tok || *end)
      have = 0;
    else if (have < 6)
      have ++;
  }

  fprintf(stderr, "ERROR: no cm operator found in output page content\n");
  return (1);
}
