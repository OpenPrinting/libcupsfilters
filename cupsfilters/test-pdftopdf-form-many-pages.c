//
// Helper for test-pdftopdf-form-many-pages.sh.
//
// Usage:
//
//   test-pdftopdf-form-many-pages make INPUT.pdf PAGES
//       Write a document of PAGES small pages whose catalog has an
//       AcroForm with one text field, so that pdftopdf flattens every
//       page before printing it.
//
//   test-pdftopdf-form-many-pages check OUTPUT.pdf PAGES
//       Check that pdftopdf's OUTPUT.pdf has PAGES pages and that every
//       page can be loaded.
//
// Copyright © 2026 by OpenPrinting
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <pdfio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//
// 'make_input()' - Create the test document.
//

static int				// O - Exit status
make_input(const char *filename,	// I - Output filename
           size_t     pages)		// I - Number of pages
{
  pdfio_rect_t	media;			// Page size
  pdfio_file_t	*pdf;			// Output file
  pdfio_dict_t	*field;			// Form field dictionary
  pdfio_array_t	*fields;		// Form field array
  pdfio_dict_t	*acroform;		// AcroForm dictionary
  pdfio_dict_t	*catalog;		// Catalog dictionary
  pdfio_stream_t *st;			// Page content
  size_t	i;			// Looping var


  media.x1 = 0.0;
  media.y1 = 0.0;
  media.x2 = 72.0;
  media.y2 = 72.0;

  if ((pdf = pdfioFileCreate(filename, "1.7", &media, &media, NULL, NULL)) == NULL)
  {
    fprintf(stderr, "Unable to create \"%s\".\n", filename);
    return (1);
  }

  field = pdfioDictCreate(pdf);
  pdfioDictSetName(field, "FT", "Tx");
  pdfioDictSetString(field, "T", "field");

  fields = pdfioArrayCreate(pdf);
  pdfioArrayAppendObj(fields, pdfioFileCreateObj(pdf, field));

  acroform = pdfioDictCreate(pdf);
  pdfioDictSetArray(acroform, "Fields", fields);

  if ((catalog = pdfioFileGetCatalog(pdf)) == NULL)
  {
    fputs("Unable to get the catalog.\n", stderr);
    pdfioFileClose(pdf);
    return (1);
  }

  pdfioDictSetDict(catalog, "AcroForm", acroform);

  for (i = 0; i < pages; i ++)
  {
    if ((st = pdfioFileCreatePage(pdf, NULL)) == NULL)
    {
      fprintf(stderr, "Unable to create page %u.\n", (unsigned)i + 1);
      pdfioFileClose(pdf);
      return (1);
    }

    pdfioStreamClose(st);
  }

  return (pdfioFileClose(pdf) ? 0 : 1);
}


//
// 'check_output()' - Check the number of pages of pdftopdf's output.
//

static int				// O - Exit status
check_output(const char *filename,	// I - pdftopdf output
             size_t     pages)		// I - Expected number of pages
{
  pdfio_file_t	*pdf;			// Output PDF from pdftopdf
  pdfio_obj_t	*page;			// Current page
  size_t	i;			// Looping var


  if ((pdf = pdfioFileOpen(filename, NULL, NULL, NULL, NULL)) == NULL)
  {
    fprintf(stderr, "Unable to open \"%s\".\n", filename);
    return (1);
  }

  if (pdfioFileGetNumPages(pdf) != pages)
  {
    fprintf(stderr, "Expected %u output pages, got %u.\n", (unsigned)pages, (unsigned)pdfioFileGetNumPages(pdf));
    pdfioFileClose(pdf);
    return (1);
  }

  for (i = 0; i < pages; i ++)
  {
    if ((page = pdfioFileGetPage(pdf, i)) == NULL || !pdfioObjGetDict(page))
    {
      fprintf(stderr, "Unable to load page %u.\n", (unsigned)i + 1);
      pdfioFileClose(pdf);
      return (1);
    }
  }

  pdfioFileClose(pdf);

  return (0);
}


//
// 'main()' - Make the input or check the output.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  size_t	pages;			// Number of pages


  if (argc != 4)
  {
    fputs("Usage: test-pdftopdf-form-many-pages make|check FILE.pdf PAGES\n", stderr);
    return (1);
  }

  pages = (size_t)strtoul(argv[3], NULL, 10);

  if (!strcmp(argv[1], "make"))
    return (make_input(argv[2], pages));
  else if (!strcmp(argv[1], "check"))
    return (check_output(argv[2], pages));

  fprintf(stderr, "Unknown command \"%s\".\n", argv[1]);
  return (1);
}
