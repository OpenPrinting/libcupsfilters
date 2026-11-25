//
// pdftopdf regression test for pages that PDFio cannot copy (Issue #287).
//
// pdfjam wraps every input page in a Form XObject whose /PTEX.InfoDict
// points to the original document information dictionary.  When that
// dictionary contains a null value (pdfCropMargins writes "/Title null"),
// PDFio fails to copy the XObject.  pdftopdf ignored the failure, dropped
// the first page (or its content) and still reported success.
//
// The filter must either reproduce every page with its content or fail
// with an error, never silently drop content.
//
// Copyright © 2026 by OpenPrinting.
//
// Licensed under Apache License v2.0. See the file "LICENSE" for more
// information.
//

#include <config.h>
#include <cupsfilters/filter.h>
#include <pdfio.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


//
// 'write_pdf()' - Write a two-page pdfjam-like PDF.
//
// Objects 4 and 6 are the per-page Form XObjects, both referencing the
// information dictionary (object 7) that contains null values.
//

static int				// O - 0 on success, 1 on error
write_pdf(const char *filename)		// I - PDF file to write
{
  FILE		*fp;			// PDF file
  long		offsets[10],		// Object offsets
		xref;			// Cross-reference table offset
  int		i,			// Looping var
		failed;			// Write failed?
  static const char * const form[2] =	// Form XObject contents
  {
    "0 0 1 rg 10 10 50 50 re f\n",
    "1 0 0 rg 20 20 50 50 re f\n"
  };
  static const char * const content[2] =// Page contents
  {
    "q /Im1 Do Q\n",
    "q /Im2 Do Q\n"
  };


  if ((fp = fopen(filename, "wb")) == NULL)
    return (1);

  fputs("%PDF-1.5\n", fp);
  offsets[1] = ftell(fp);
  fputs("1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n", fp);
  offsets[2] = ftell(fp);
  fputs("2 0 obj\n<< /Type /Pages /Kids [3 0 R 5 0 R] /Count 2 >>\nendobj\n", fp);

  for (i = 0; i < 2; i ++)
  {
    int page = 3 + 2 * i;		// Page object number

    offsets[page] = ftell(fp);
    fprintf(fp, "%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
                "/Resources << /ProcSet [/PDF] /XObject << /Im%d %d 0 R >> >> "
                "/Contents %d 0 R >>\nendobj\n", page, i + 1, page + 1, 8 + i);
    offsets[page + 1] = ftell(fp);
    fprintf(fp, "%d 0 obj\n<< /Type /XObject /Subtype /Form /FormType 1 "
                "/BBox [0 0 100 100] /PTEX.PageNumber %d /PTEX.InfoDict 7 0 R "
                "/Length %zu >>\nstream\n%sendstream\nendobj\n",
            page + 1, i + 1, strlen(form[i]), form[i]);
  }

  offsets[7] = ftell(fp);
  fputs("7 0 obj\n<< /Producer (test) /Title null /Author null >>\nendobj\n", fp);

  for (i = 0; i < 2; i ++)
  {
    offsets[8 + i] = ftell(fp);
    fprintf(fp, "%d 0 obj\n<< /Length %zu >>\nstream\n%sendstream\nendobj\n",
            8 + i, strlen(content[i]), content[i]);
  }

  xref = ftell(fp);
  fputs("xref\n0 10\n0000000000 65535 f \n", fp);
  for (i = 1; i < 10; i ++)
    fprintf(fp, "%010ld 00000 n \n", offsets[i]);
  fprintf(fp, "trailer\n<< /Size 10 /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n", xref);

  failed = ferror(fp);
  if (fclose(fp))
    failed = 1;

  return (failed);
}


//
// 'count_xobjects()' - Count the pages and the XObjects drawn on them.
//

static int				// O - 0 on success, 1 on error
count_xobjects(const char *filename,	// I - PDF file
               size_t     *num_pages,	// O - Number of pages
               size_t     *num_xobjects)// O - Number of XObjects
{
  pdfio_file_t	*pdf;			// PDF file
  size_t	i;			// Looping var


  *num_pages    = 0;
  *num_xobjects = 0;

  if ((pdf = pdfioFileOpen(filename, NULL, NULL, NULL, NULL)) == NULL)
    return (1);

  *num_pages = pdfioFileGetNumPages(pdf);

  for (i = 0; i < *num_pages; i ++)
  {
    pdfio_dict_t *resources,		// Page resources
		 *xobjects;		// XObject resources

    resources = pdfioDictGetDict(pdfioObjGetDict(pdfioFileGetPage(pdf, i)), "Resources");
    if (!resources)
      resources = pdfioObjGetDict(pdfioDictGetObj(pdfioObjGetDict(pdfioFileGetPage(pdf, i)), "Resources"));

    if ((xobjects = pdfioDictGetDict(resources, "XObject")) != NULL)
      *num_xobjects += pdfioDictGetNumPairs(xobjects);
  }

  pdfioFileClose(pdf);

  return (0);
}


//
// 'run_case()' - Run pdftopdf with the given options and check the output.
//

static int				// O - 0 on success, 1 on failure
run_case(const char *input,		// I - Input PDF
         const char *output,		// I - Output PDF
         const char *options,		// I - Job options
         size_t     expected_pages)	// I - Expected number of output pages
{
  cf_filter_data_t data;		// Filter data
  int		inputfd,		// Input file
		outputfd,		// Output file
		status,			// Filter status
		failed;			// Test failed?
  size_t	num_pages,		// Number of output pages
		num_xobjects;		// Number of XObjects in output


  if ((inputfd = open(input, O_RDONLY)) < 0)
    return (1);

  if ((outputfd = open(output, O_CREAT | O_TRUNC | O_WRONLY, 0600)) < 0)
  {
    close(inputfd);
    return (1);
  }

  memset(&data, 0, sizeof(data));
  data.copies             = 1;
  data.content_type       = "application/pdf";
  data.final_content_type = "application/vnd.cups-pdf";
  data.back_pipe[0]       = data.back_pipe[1] = -1;
  data.side_pipe[0]       = data.side_pipe[1] = -1;
  data.num_options        = cupsParseOptions(options, 0, &data.options);

  status = cfFilterPDFToPDF(inputfd, outputfd, 1, &data, NULL);

  close(inputfd);
  close(outputfd);
  cupsFreeOptions(data.num_options, data.options);

  if (status)
  {
    // Failing with an error is acceptable when PDFio cannot copy the page...
    printf("%s: status=%d (reported failure): PASS\n", options, status);
    unlink(output);
    return (0);
  }

  failed = count_xobjects(output, &num_pages, &num_xobjects) ||
           num_pages != expected_pages || num_xobjects != 2;

  printf("%s: status=0, pages=%u (expected %u), xobjects=%u (expected 2): %s\n",
         options, (unsigned)num_pages, (unsigned)expected_pages,
         (unsigned)num_xobjects, failed ? "FAIL" : "PASS");

  unlink(output);

  return (failed);
}


//
// 'main()' - Main entry.
//

int					// O - Exit status
main(void)
{
  char	directory[] = "pdftopdf-null-value.XXXXXX",
	input[1024],			// Input PDF
	output[1024];			// Output PDF
  int	failed = 0;			// Test failed?
  int	major = 0,			// PDFio version numbers
	minor = 0,
	patch = 0;


  // PDFio before 1.6.5 crashes in pdfioPageCopy() when a value cannot be
  // copied, which pdftopdf cannot prevent, so skip the test there...
  sscanf(PDFIO_VERSION, "%d.%d.%d", &major, &minor, &patch);
  if (major < 1 || (major == 1 && (minor < 6 || (minor == 6 && patch < 5))))
  {
    printf("SKIP: PDFio %s crashes on failed page copies, need 1.6.5 or later.\n", PDFIO_VERSION);
    return (77);
  }

  if (!mkdtemp(directory))
    return (1);

  snprintf(input, sizeof(input), "%s/input.pdf", directory);
  snprintf(output, sizeof(output), "%s/output.pdf", directory);

  if (write_pdf(input))
  {
    unlink(input);
    rmdir(directory);
    return (1);
  }

  failed |= run_case(input, output, "PageSize=Letter", 2);
  failed |= run_case(input, output, "PageSize=Letter number-up=2", 1);

  unlink(input);
  if (rmdir(directory))
    failed = 1;

  return (failed);
}
