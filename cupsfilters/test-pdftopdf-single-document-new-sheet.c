//
// Regression check: with multiple-document-handling=single-document-new-
// sheet, every output path must agree on how many sheets an odd page
// selection needs.
//
// prepare_pages() pads the output page count to even here so a later
// document (or job sheet) always starts on a fresh sheet, but it left the
// padding slot's "scheduled blank page" unmarked (outpage->pdf unset).
// The layout path drew it anyway, but the fast-copy path
// (print-scaling=none) only emits a page for a slot that is marked, so it
// silently dropped the padding sheet -- the two paths disagreed on the
// sheet count for the exact same job.
//
// Usage:
//
//   test-pdftopdf-single-document-new-sheet make INPUT.pdf PAGES
//       Write a Letter-size test document with PAGES pages, each showing
//       "Page N".
//
//   test-pdftopdf-single-document-new-sheet check OUTPUT.pdf PAGES
//       Check that pdftopdf's OUTPUT.pdf has exactly PAGES pages.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pdfio.h>
#include <pdfio-content.h>


//
// 'make_input()' - Create the test document.
//

static int				// O - Exit status
make_input(const char *filename,	// I - Output filename
           int        pages)		// I - Number of pages
{
  pdfio_rect_t	media = { 0.0, 0.0, 612.0, 792.0 };
					// US Letter
  pdfio_file_t	*pdf;			// Output file
  pdfio_obj_t	*font;			// Helvetica
  int		i;			// Looping var


  if ((pdf = pdfioFileCreate(filename, "1.7", &media, &media, NULL, NULL)) == NULL)
  {
    fprintf(stderr, "Unable to create '%s'\n", filename);
    return (1);
  }

  font = pdfioFileCreateFontObjFromBase(pdf, "Helvetica");

  for (i = 1; i <= pages; i ++)
  {
    pdfio_dict_t	*dict = pdfioDictCreate(pdf);
					// Page dictionary
    pdfio_stream_t	*st;		// Page content

    pdfioPageDictAddFont(dict, "F1", font);

    if ((st = pdfioFileCreatePage(pdf, dict)) == NULL)
    {
      fprintf(stderr, "Unable to create page %d\n", i);
      pdfioFileClose(pdf);
      return (1);
    }

    pdfioContentTextBegin(st);
    pdfioContentSetTextFont(st, "F1", 36.0);
    pdfioContentTextMoveTo(st, 72.0, 690.0);
    pdfioContentTextShowf(st, false, "Page %d", i);
    pdfioContentTextEnd(st);
    pdfioStreamClose(st);
  }

  return (pdfioFileClose(pdf) ? 0 : 1);
}


//
// 'check_output()' - Check pdftopdf's output page count.
//

static int				// O - Exit status
check_output(const char *filename,	// I - pdftopdf output
             int        pages)		// I - Expected number of pages
{
  pdfio_file_t	*pdf;			// Output file
  size_t	num_pages;		// Number of pages


  if ((pdf = pdfioFileOpen(filename, NULL, NULL, NULL, NULL)) == NULL)
  {
    fprintf(stderr, "pdftopdf output could not be opened\n");
    return (1);
  }

  num_pages = pdfioFileGetNumPages(pdf);

  pdfioFileClose(pdf);

  if (num_pages != (size_t)pages)
  {
    fprintf(stderr, "pdftopdf output has %u pages, expected %d\n", (unsigned)num_pages, pages);
    return (1);
  }

  return (0);
}


//
// 'main()' - Make the test input or check pdftopdf's output.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line args
     char *argv[])			// I - Command-line arguments
{
  if (argc == 4 && !strcmp(argv[1], "make"))
    return (make_input(argv[2], atoi(argv[3])));
  else if (argc == 4 && !strcmp(argv[1], "check"))
    return (check_output(argv[2], atoi(argv[3])));

  fprintf(stderr, "Usage: %s make INPUT.pdf PAGES\n"
                  "       %s check OUTPUT.pdf PAGES\n", argv[0], argv[0]);
  return (1);
}
