//
// Regression check: pdftopdf must not transform the back sides of duplex
// sheets.
//
// As in the former QPDF-based implementation, the back side orientation
// that some printers need (pwg-raster-document-sheet-back, urf-supported
// DM2-DM4, see cfGetBackSideOrientation()) is applied by the rasterizing
// filters, and only for raster output.  pdftopdf used to wrap even output
// pages in a flip or 180 degree rotation itself, and pages with annotations
// got it twice, once while flattening the annotations and once when
// composing the output page, so that it cancelled out on those pages only.
//
// Usage:
//
//   test-pdftopdf-duplex-backside make INPUT.pdf PAGES links|plain
//       Write a Letter-size test document with PAGES pages, each showing
//       "Page N", with or without a link annotation on every page.  The
//       content itself contains no "cm" operator.
//
//   test-pdftopdf-duplex-backside check OUTPUT.pdf PAGES
//       Check that pdftopdf's OUTPUT.pdf has PAGES pages and that no page's
//       content applies a mirroring transform, i.e. that no "cm" operator
//       has a negative horizontal or vertical scale.  A flipped back side
//       ([-1 0 0 1 w 0] or [1 0 0 -1 0 h]) or a rotated one
//       ([-1 0 0 -1 w h]) is reported with its page number.
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
           int        pages,		// I - Number of pages
           int        links)		// I - Add link annotations?
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

    if (links)
    {
      pdfio_dict_t	*annot = pdfioDictCreate(pdf),
					// Link annotation
			*action = pdfioDictCreate(pdf);
					// URI action
      pdfio_rect_t	rect = { 72.0, 680.0, 300.0, 730.0 };
					// Link rectangle
      pdfio_array_t	*annots = pdfioArrayCreate(pdf),
					// Annotations array
			*border = pdfioArrayCreate(pdf);
					// No visible border
      pdfio_obj_t	*obj;		// Annotation object

      pdfioDictSetName(action, "S", "URI");
      pdfioDictSetString(action, "URI", "https://openprinting.github.io/");

      pdfioArrayAppendNumber(border, 0.0);
      pdfioArrayAppendNumber(border, 0.0);
      pdfioArrayAppendNumber(border, 0.0);

      pdfioDictSetName(annot, "Type", "Annot");
      pdfioDictSetName(annot, "Subtype", "Link");
      pdfioDictSetRect(annot, "Rect", &rect);
      pdfioDictSetArray(annot, "Border", border);
      pdfioDictSetDict(annot, "A", action);

      obj = pdfioFileCreateObj(pdf, annot);
      pdfioObjClose(obj);
      pdfioArrayAppendObj(annots, obj);
      pdfioDictSetArray(dict, "Annots", annots);
    }

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
// 'check_output()' - Check pdftopdf's output for back side transforms.
//

static int				// O - Exit status
check_output(const char *filename,	// I - pdftopdf output
             int        pages)		// I - Expected number of pages
{
  pdfio_file_t	*pdf;			// Output file
  size_t	num_pages,		// Number of pages
		i, j;			// Looping vars
  int		status = 0;		// Exit status


  if ((pdf = pdfioFileOpen(filename, NULL, NULL, NULL, NULL)) == NULL)
  {
    fprintf(stderr, "pdftopdf output could not be opened\n");
    return (1);
  }

  if ((num_pages = pdfioFileGetNumPages(pdf)) != (size_t)pages)
  {
    fprintf(stderr, "pdftopdf output has %u pages, expected %d\n", (unsigned)num_pages, pages);
    pdfioFileClose(pdf);
    return (1);
  }

  for (i = 0; i < num_pages; i ++)
  {
    pdfio_obj_t	*page = pdfioFileGetPage(pdf, i);
					// Current page
    char	*content = NULL,	// Page content
		*tok,			// Current token
		*save,			// strtok_r state
		*end;			// End of number
    size_t	length = 0;		// Length of content
    double	last[6] = { 0.0 };	// Last six numeric operands
    int		have = 0;		// Number of valid operands in last[]

    // Read all content streams of the page...
    for (j = 0; page && j < pdfioPageGetNumStreams(page); j ++)
    {
      pdfio_stream_t	*st;		// Content stream
      char		buffer[8192];	// Read buffer
      ssize_t		bytes;		// Bytes read

      if ((st = pdfioPageOpenStream(page, j, true)) == NULL)
        continue;

      while ((bytes = pdfioStreamRead(st, buffer, sizeof(buffer))) > 0)
      {
        char *temp = realloc(content, length + (size_t)bytes + 2);

        if (!temp)
        {
          fprintf(stderr, "Out of memory\n");
          free(content);
          pdfioStreamClose(st);
          pdfioFileClose(pdf);
          return (1);
        }

        content = temp;
        memcpy(content + length, buffer, (size_t)bytes);
        length += (size_t)bytes;
      }

      pdfioStreamClose(st);

      if (content)
        content[length ++] = '\n';
    }

    if (!content)
    {
      fprintf(stderr, "page %u: no readable content\n", (unsigned)i + 1);
      status = 1;
      continue;
    }

    content[length] = '\0';

    // Look at the operands of every "cm" operator...
    for (tok = strtok_r(content, " \t\r\n", &save); tok; tok = strtok_r(NULL, " \t\r\n", &save))
    {
      if (!strcmp(tok, "cm"))
      {
        if (have == 6 && (last[0] < 0.0 || last[3] < 0.0))
        {
          fprintf(stderr, "page %u: back side transformed by [%g %g %g %g %g %g] cm\n", (unsigned)i + 1, last[0], last[1], last[2], last[3], last[4], last[5]);
          status = 1;
        }

        have = 0;
        continue;
      }

      memmove(last, last + 1, 5 * sizeof(double));
      last[5] = strtod(tok, &end);
      if (end == tok || *end)
        have = 0;
      else if (have < 6)
        have ++;
    }

    free(content);
  }

  pdfioFileClose(pdf);

  return (status);
}


//
// 'main()' - Make the test input or check pdftopdf's output.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line args
     char *argv[])			// I - Command-line arguments
{
  if (argc == 5 && !strcmp(argv[1], "make"))
    return (make_input(argv[2], atoi(argv[3]), !strcmp(argv[4], "links")));
  else if (argc == 4 && !strcmp(argv[1], "check"))
    return (check_output(argv[2], atoi(argv[3])));

  fprintf(stderr, "Usage: %s make INPUT.pdf PAGES links|plain\n"
                  "       %s check OUTPUT.pdf PAGES\n", argv[0], argv[0]);
  return (1);
}
