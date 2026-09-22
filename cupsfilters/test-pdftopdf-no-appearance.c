//
// Regression check for issue #246: a PDF whose only annotations are links
// with no appearance stream must come back out of pdftopdf as a readable
// one-page PDF.
//

#include <stdio.h>
#include <pdfio.h>

int
main(int  argc,
     char *argv[])
{
  pdfio_file_t	*pdf;
  size_t	pages;


  if (argc != 2)
  {
    fprintf(stderr, "Usage: %s file.pdf\n", argv[0]);
    return (1);
  }

  pdf = pdfioFileOpen(argv[1], NULL, NULL, NULL, NULL);
  if (!pdf)
  {
    fprintf(stderr, "pdftopdf output could not be opened\n");
    return (1);
  }

  pages = pdfioFileGetNumPages(pdf);
  pdfioFileClose(pdf);

  if (pages < 1)
  {
    fprintf(stderr, "pdftopdf output has no pages\n");
    return (1);
  }

  return (0);
}
