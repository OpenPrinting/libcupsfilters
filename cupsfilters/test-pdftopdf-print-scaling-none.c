//
// Regression check for issue #258: with print-scaling=none, pdftopdf must
// not scale a page that has annotations.  The first "cm" operator in the
// output page content is the page transform, and its x/y scale must be 1.
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


  if (argc != 2)
  {
    fprintf(stderr, "Usage: %s file.pdf\n", argv[0]);
    return (1);
  }

  if ((pdf = pdfioFileOpen(argv[1], NULL, NULL, NULL, NULL)) == NULL)
  {
    fprintf(stderr, "pdftopdf output could not be opened\n");
    return (1);
  }

  if ((page = pdfioFileGetPage(pdf, 0)) == NULL ||
      (st = pdfioPageOpenStream(page, 0, true)) == NULL)
  {
    fprintf(stderr, "pdftopdf output has no readable page content\n");
    pdfioFileClose(pdf);
    return (1);
  }

  bytes = pdfioStreamRead(st, buffer, sizeof(buffer) - 1);
  pdfioStreamClose(st);
  pdfioFileClose(pdf);

  if (bytes <= 0)
  {
    fprintf(stderr, "pdftopdf output page content is empty\n");
    return (1);
  }
  buffer[bytes] = '\0';

  for (tok = strtok_r(buffer, " \t\r\n", &save); tok; tok = strtok_r(NULL, " \t\r\n", &save))
  {
    if (!strcmp(tok, "cm"))
    {
      if (have < 6)
      {
        fprintf(stderr, "found cm operator without six operands\n");
        return (1);
      }

      if (fabs(last[0] - 1.0) > 0.001 || fabs(last[3] - 1.0) > 0.001)
      {
        fprintf(stderr, "page was scaled: cm scale is %g x %g, expected 1 x 1\n", last[0], last[3]);
        return (1);
      }

      return (0);
    }

    memmove(last, last + 1, 5 * sizeof(double));
    last[5] = strtod(tok, &end);
    if (end == tok || *end)
      have = 0;
    else if (have < 6)
      have ++;
  }

  fprintf(stderr, "no cm operator found in output page content\n");
  return (1);
}
