//
// Regression check: a landscape page that pdftopdf turns onto portrait
// paper must be centered in the printable area, exactly like the same page
// in portrait orientation, which pdftopdf places without turning it.
//
// Usage:
//
//   test-pdftopdf-landscape-centered make INPUT.pdf WIDTH HEIGHT
//       Write a one-page test document of WIDTH x HEIGHT points showing
//       "Page 1".  The content itself contains no "cm" operator.
//
//   test-pdftopdf-landscape-centered box OUTPUT.pdf WIDTH HEIGHT
//       Print the rectangle "x0 y0 x1 y1" that pdftopdf's OUTPUT.pdf places
//       the WIDTH x HEIGHT input page on: the corners of the input page
//       mapped through the "cm" operators that are in effect where the
//       input page's content starts on the first output page: at the first
//       "BT" operator (pdftopdf copies the content inline) or "Do" operator.
//
// Copyright © 2026 by OpenPrinting
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
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
           double     width,		// I - Page width in points
           double     height)		// I - Page height in points
{
  pdfio_rect_t	media;			// Page size
  pdfio_file_t	*pdf;			// Output file
  pdfio_obj_t	*font;			// Helvetica
  pdfio_dict_t	*dict;			// Page dictionary
  pdfio_stream_t *st;			// Page content


  media.x1 = 0.0;
  media.y1 = 0.0;
  media.x2 = width;
  media.y2 = height;

  if ((pdf = pdfioFileCreate(filename, "1.7", &media, &media, NULL, NULL)) == NULL)
  {
    fprintf(stderr, "Unable to create '%s'\n", filename);
    return (1);
  }

  if ((font = pdfioFileCreateFontObjFromBase(pdf, "Helvetica")) == NULL)
  {
    fprintf(stderr, "Unable to create the font\n");
    pdfioFileClose(pdf);
    return (1);
  }

  dict = pdfioDictCreate(pdf);
  pdfioPageDictAddFont(dict, "F1", font);

  if ((st = pdfioFileCreatePage(pdf, dict)) == NULL)
  {
    fprintf(stderr, "Unable to create the page\n");
    pdfioFileClose(pdf);
    return (1);
  }

  pdfioContentTextBegin(st);
  pdfioContentSetTextFont(st, "F1", 36.0);
  pdfioContentTextMoveTo(st, 72.0, height - 108.0);
  pdfioContentTextShow(st, false, "Page 1");
  pdfioContentTextEnd(st);
  pdfioStreamClose(st);

  return (pdfioFileClose(pdf) ? 0 : 1);
}


//
// 'concat()' - Concatenate a "cm" matrix to the current matrix.
//

static void
concat(double       ctm[6],		// IO - Current transformation matrix
       const double m[6])		// I  - Operands of the "cm" operator
{
  double	r[6];			// Result


  // CTM' = M x CTM (PDF 32000-1, 8.4.4)
  r[0] = m[0] * ctm[0] + m[1] * ctm[2];
  r[1] = m[0] * ctm[1] + m[1] * ctm[3];
  r[2] = m[2] * ctm[0] + m[3] * ctm[2];
  r[3] = m[2] * ctm[1] + m[3] * ctm[3];
  r[4] = m[4] * ctm[0] + m[5] * ctm[2] + ctm[4];
  r[5] = m[4] * ctm[1] + m[5] * ctm[3] + ctm[5];

  memcpy(ctm, r, sizeof(r));
}


//
// 'print_box()' - Print where pdftopdf's output places the input page.
//

static int				// O - Exit status
print_box(const char *filename,		// I - pdftopdf output
          double     width,		// I - Input page width
          double     height)		// I - Input page height
{
  pdfio_file_t	*pdf;			// Output file
  pdfio_obj_t	*page;			// First page
  char		*content = NULL,	// Page content
		*tok,			// Current token
		*save,			// strtok_r state
		*end;			// End of number
  size_t	length = 0,		// Length of content
		i;			// Looping var
  double	last[6] = { 0.0 },	// Last six numeric operands
		ctm[6] = { 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 },
					// Current transformation matrix
		stack[16][6],		// Saved matrices (q/Q)
		corners[4][2],		// Corners of the input page
		x0, y0, x1, y1;		// Placed rectangle
  int		have = 0,		// Number of valid operands in last[]
		depth = 0,		// q/Q nesting depth
		found = 0;		// Found the "BT" or "Do" operator?


  if ((pdf = pdfioFileOpen(filename, NULL, NULL, NULL, NULL)) == NULL)
  {
    fprintf(stderr, "pdftopdf output could not be opened\n");
    return (1);
  }

  if ((page = pdfioFileGetPage(pdf, 0)) == NULL)
  {
    fprintf(stderr, "pdftopdf output has no pages\n");
    pdfioFileClose(pdf);
    return (1);
  }

  // Read all content streams of the page...
  for (i = 0; i < pdfioPageGetNumStreams(page); i ++)
  {
    pdfio_stream_t	*st;		// Content stream
    char		buffer[8192];	// Read buffer
    ssize_t		bytes;		// Bytes read

    if ((st = pdfioPageOpenStream(page, i, true)) == NULL)
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

    // Separate the streams; realloc() above always leaves room for this
    // newline and the final nul, as long as the stream added some bytes...
    if (content && length > 0 && content[length - 1] != '\n')
      content[length ++] = '\n';
  }

  pdfioFileClose(pdf);

  if (!content)
  {
    fprintf(stderr, "pdftopdf output page has no readable content\n");
    return (1);
  }

  content[length] = '\0';

  // Follow q, Q and cm up to the first BT or Do...
  for (tok = strtok_r(content, " \t\r\n", &save); tok; tok = strtok_r(NULL, " \t\r\n", &save))
  {
    if (!strcmp(tok, "q"))
    {
      if (depth >= 16)
      {
        fprintf(stderr, "q operators nested too deeply in output page content\n");
        free(content);
        return (1);
      }

      memcpy(stack[depth ++], ctm, sizeof(ctm));
      have = 0;
    }
    else if (!strcmp(tok, "Q"))
    {
      if (depth > 0)
        memcpy(ctm, stack[-- depth], sizeof(ctm));
      have = 0;
    }
    else if (!strcmp(tok, "cm"))
    {
      if (have == 6)
        concat(ctm, last);
      have = 0;
    }
    else if (!strcmp(tok, "BT") || !strcmp(tok, "Do"))
    {
      found = 1;
      break;
    }
    else
    {
      memmove(last, last + 1, 5 * sizeof(double));
      last[5] = strtod(tok, &end);
      if (end == tok || *end)
        have = 0;
      else if (have < 6)
        have ++;
    }
  }

  free(content);

  if (!found)
  {
    fprintf(stderr, "no BT or Do operator found in output page content\n");
    return (1);
  }

  corners[0][0] = 0.0;   corners[0][1] = 0.0;
  corners[1][0] = width; corners[1][1] = 0.0;
  corners[2][0] = 0.0;   corners[2][1] = height;
  corners[3][0] = width; corners[3][1] = height;

  x0 = y0 = 1.0e9;
  x1 = y1 = -1.0e9;

  for (i = 0; i < 4; i ++)
  {
    double x = ctm[0] * corners[i][0] + ctm[2] * corners[i][1] + ctm[4],
	   y = ctm[1] * corners[i][0] + ctm[3] * corners[i][1] + ctm[5];

    if (x < x0)
      x0 = x;
    if (x > x1)
      x1 = x;
    if (y < y0)
      y0 = y;
    if (y > y1)
      y1 = y;
  }

  printf("%.3f %.3f %.3f %.3f\n", x0, y0, x1, y1);

  return (0);
}


//
// 'main()' - Make the test input or report where the page was placed.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line args
     char *argv[])			// I - Command-line arguments
{
  if (argc == 5 && !strcmp(argv[1], "make"))
    return (make_input(argv[2], atof(argv[3]), atof(argv[4])));
  else if (argc == 5 && !strcmp(argv[1], "box"))
    return (print_box(argv[2], atof(argv[3]), atof(argv[4])));

  fprintf(stderr, "Usage: %s make INPUT.pdf WIDTH HEIGHT\n"
                  "       %s box OUTPUT.pdf WIDTH HEIGHT\n", argv[0], argv[0]);
  return (1);
}
