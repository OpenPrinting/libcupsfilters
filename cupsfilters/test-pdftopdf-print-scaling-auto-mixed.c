//
// Fixture generator for the print-scaling=auto regression test: creates
// a two-page PDF with mixed page sizes, page 1 A3 and page 2 A4.  With
// page-ranges=2 only the A4 page gets printed, so the document must not
// be considered "too large" for A4 paper; with page-ranges=1 the A3 page
// gets printed and the document must be scaled to fit.
//

#include <stdio.h>
#include <stdlib.h>
#include <pdfio.h>

int
main(int  argc,
     char *argv[])
{
  pdfio_file_t	*pdf;			// Output PDF file
  pdfio_dict_t	*dict;			// Page dictionary
  pdfio_stream_t *st;			// Page content stream
  pdfio_rect_t	box;			// Page rectangle
  double	sizes[2][2] =		// Page sizes: A3, A4
		{
		  { 842.0, 1191.0 },
		  { 595.28, 841.89 }
		};
  int		i;			// Looping var


  if (argc != 2)
  {
    fprintf(stderr, "Usage: %s file.pdf\n", argv[0]);
    return (1);
  }

  box.x1 = 0.0;
  box.y1 = 0.0;
  box.x2 = 595.28;
  box.y2 = 841.89;

  if ((pdf = pdfioFileCreate(argv[1], "1.7", &box, &box, NULL, NULL)) == NULL)
  {
    fprintf(stderr, "Unable to create %s\n", argv[1]);
    return (1);
  }

  for (i = 0; i < 2; i ++)
  {
    box.x1 = 0.0;
    box.y1 = 0.0;
    box.x2 = sizes[i][0];
    box.y2 = sizes[i][1];

    dict = pdfioDictCreate(pdf);
    pdfioDictSetRect(dict, "MediaBox", &box);
    pdfioDictSetRect(dict, "CropBox", &box);
    pdfioDictSetName(dict, "Type", "Page");

    st = pdfioFileCreatePage(pdf, dict);
    pdfioStreamPuts(st, "0 0 0 RG 2 w 100 100 200 200 re S\n");
    pdfioStreamClose(st);
  }

  pdfioFileClose(pdf);
  return (0);
}
