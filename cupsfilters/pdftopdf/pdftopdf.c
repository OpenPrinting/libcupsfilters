//
// Copyright 2024 Uddhav Phatak <uddhavabhijeet@gmail.com>
//
// Copyright (c) 6-2011, BBR Inc.  All rights reserved.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <config.h>

#include <stdio.h>
#include <ctype.h>
#include <cupsfilters/debug-internal.h>
#include <cupsfilters/raster.h>
#include <cupsfilters/ipp.h>
#include <cupsfilters/libcups2-private.h>
#include <cups/cups.h>
#include <cups/pwg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "pdftopdf-private.h"

#include <stdarg.h>

static const char       *Prefix;
static int              Verbosity = 0;

#define XFORM_TEXT_SIZE         10.0    // Point size of plain text output
#define XFORM_TEXT_HEIGHT       12.0    // Point height of plain text output
#define XFORM_TEXT_WIDTH        0.6     // Width of monospaced characters


//
// 'pdfio_start_page()' - Start a page.
//
// This function applies a transform on the back side pages.
//

static pdfio_stream_t *                 // O - Page content stream
pdfio_start_page(xform_prepare_t *p,    // I - Preparation data
                 pdfio_dict_t    *dict) // I - Page dictionary
{
  pdfio_stream_t        *st;            // Page content stream


  if ((st = pdfioFileCreatePage(p->pdf, dict)) != NULL)
  {
    if (p->use_duplex_xform && !(pdfioFileGetNumPages(p->pdf) & 1))
    {
      pdfioContentSave(st);
      pdfioContentMatrixConcat(st, p->duplex_xform);
    }
  }

  return (st);
}

//
// 'pdfio_end_page()' - End a page.
//
// This function restores graphics state when ending a back side page.
//

static void
pdfio_end_page(xform_prepare_t *p,      // I - Preparation data
               pdfio_stream_t  *st)     // I - Page content stream
{
  if (p->use_duplex_xform && !(pdfioFileGetNumPages(p->pdf) & 1))
    pdfioContentRestore(st);

  pdfioStreamClose(st);
}

//
// 'generate_job_error_sheet()' - Generate a job error sheet.
//

static bool				// O - `true` on success, `false` on failure
generate_job_error_sheet(
    xform_prepare_t  *p)		// I - Preparation data
{
  pdfio_stream_t *st;			// Page stream
  pdfio_obj_t	*courier;		// Courier font
  pdfio_dict_t	*dict;			// Page dictionary
  size_t	i,			// Looping var
		count;			// Number of pages
  const char	*msg;			// Current message
  size_t	mcount;			// Number of messages


  // Create a page dictionary with the Courier font...
  courier = pdfioFileCreateFontObjFromBase(p->pdf, "Courier");
  dict    = pdfioDictCreate(p->pdf);

  pdfioPageDictAddFont(dict, "F1", courier);

  // Figure out how many impressions to produce...
  if (!strcmp(p->options->sides, "one-sided"))
    count = 1;
  else
    count = 2;

  // Create pages...
  for (i = 0; i < count; i ++)
  {
    // Create the error sheet...
    st = pdfio_start_page(p, dict);

    // The job error sheet is a banner with the following information:
    //
    //   Errors:
    //     ...
    //
    //   Warnings:
    //     ...

    pdfioContentSetFillColorDeviceGray(st, 0.0);
    pdfioContentTextBegin(st);
    pdfioContentTextMoveTo(st, p->crop.x1, p->crop.y2 - 2.0 * XFORM_TEXT_SIZE);
    pdfioContentSetTextFont(st, "F1", 2.0 * XFORM_TEXT_SIZE);
    pdfioContentSetTextLeading(st, 2.0 * XFORM_TEXT_HEIGHT);
    pdfioContentTextShow(st, false, "Errors:\n");

    pdfioContentSetTextFont(st, "F1", XFORM_TEXT_SIZE);
    pdfioContentSetTextLeading(st, XFORM_TEXT_HEIGHT);

    for (msg = (const char *)cupsArrayGetFirst(p->errors), mcount = 0; msg; msg = (const char *)cupsArrayGetNext(p->errors))
    {
      if (*msg == 'E')
      {
	pdfioContentTextShowf(st, false, "  %s\n", msg + 1);
	mcount ++;
      }
    }

    if (mcount == 0)
      pdfioContentTextShow(st, false, "  No Errors\n");

    pdfioContentSetTextFont(st, "F1", 2.0 * XFORM_TEXT_SIZE);
    pdfioContentSetTextLeading(st, 2.0 * XFORM_TEXT_HEIGHT);
    pdfioContentTextShow(st, false, "\n");
    pdfioContentTextShow(st, false, "Warnings:\n");

    pdfioContentSetTextFont(st, "F1", XFORM_TEXT_SIZE);
    pdfioContentSetTextLeading(st, XFORM_TEXT_HEIGHT);

    for (msg = (const char *)cupsArrayGetFirst(p->errors), mcount = 0; msg; msg = (const char *)cupsArrayGetNext(p->errors))
    {
      if (*msg == 'I')
      {
	pdfioContentTextShowf(st, false, "  %s\n", msg + 1);
	mcount ++;
      }
    }

    if (mcount == 0)
      pdfioContentTextShow(st, false, "  No Warnings\n");

    pdfioContentTextEnd(st);
    pdfio_end_page(p, st);
  }

  return (true);
}


//
// 'generate_job_sheets()' - Generate a job banner sheet.
//

static bool				// O - `true` on success, `false` on failure
generate_job_sheets(
    xform_prepare_t  *p)		// I - Preparation data
{
  pdfio_stream_t *st;			// Page stream
  pdfio_obj_t	*courier;		// Courier font
  pdfio_dict_t	*dict;			// Page dictionary
  size_t	i,			// Looping var
		count;			// Number of pages

  // Create a page dictionary with the Courier font...
  courier = pdfioFileCreateFontObjFromBase(p->pdf, "Courier");
  dict    = pdfioDictCreate(p->pdf);

  pdfioPageDictAddFont(dict, "F1", courier);

  // Figure out how many impressions to produce...
  if (!strcmp(p->options->sides, "one-sided"))
    count = 1;
  else
    count = 2;

  // Create pages...
  for (i = 0; i < count; i ++)
  {
    st = pdfio_start_page(p, dict);

    // The job sheet is a banner with the following information:
    //
    //     Title: job-title
    //      User: job-originating-user-name
    //     Pages: job-media-sheets
    //   Message: job-sheet-message

    pdfioContentTextBegin(st);
    pdfioContentSetTextFont(st, "F1", 2.0 * XFORM_TEXT_SIZE);
    pdfioContentSetTextLeading(st, 2.0 * XFORM_TEXT_HEIGHT);
    pdfioContentTextMoveTo(st, p->media.x2 / 8.0, p->media.y2 / 2.0 + 2 * (XFORM_TEXT_HEIGHT + XFORM_TEXT_SIZE));
    pdfioContentSetFillColorDeviceGray(st, 0.0);

    pdfioContentTextShowf(st, false, "  Title: %s\n", p->options->job_name);
    pdfioContentTextShowf(st, false, "   User: %s\n", p->options->job_originating_user_name);
    pdfioContentTextShowf(st, false, "  Pages: %u\n", (unsigned)(p->num_outpages / count));
    if (p->options->job_sheet_message[0])
      pdfioContentTextShowf(st, false, "Message: %s\n", p->options->job_sheet_message);

    pdfioContentTextEnd(st);
    pdfio_end_page(p, st);
  }

  return (true);
}



//
// 'media_to_rect()' - Convert `cups_media_t` to `pdfio_rect_t` for media and crop boxes.
//

static void
media_to_rect(cups_media_t *size,       // I - CUPS media (size) information
              pdfio_rect_t *media,      // O - PDF MediaBox value
              pdfio_rect_t *crop)       // O - PDF CropBox value
{
  // cups_media_t uses hundredths of millimeters, pdf_rect_t uses points...
  media->x1 = 0.0;
  media->y1 = 0.0;
  media->x2 = 72.0 * size->width / 2540.0;
  media->y2 = 72.0 * size->length / 2540.0;

  crop->x1  = 72.0 * size->left / 2540.0;
  crop->y1  = 72.0 * size->bottom / 2540.0;
  crop->x2  = 72.0 * (size->width - size->right) / 2540.0;
  crop->y2  = 72.0 * (size->length - size->top) / 2540.0;
}

//
// 'prepare_log()' - Log an informational or error message while preparing
//                   documents for printing.
//

static void
prepare_log(xform_prepare_t *p,         // I - Preparation data
            bool            error,      // I - `true` for error, `false` for info
            const char      *message,   // I - Printf-style message string
            ...)                        // I - Addition arguments as needed
{
  va_list       ap;                     // Argument pointer
  char          buffer[1024];           // Output buffer


  va_start(ap, message);
  vsnprintf(buffer + 1, sizeof(buffer) - 1, message, ap);
  va_end(ap);

  buffer[0] = error ? 'E' : 'I';

  cupsArrayAdd(p->errors, buffer);

  /*
  if (error)
    fprintf(stderr, _("%s: %s"), Prefix, buffer + 1);
  else
    fprintf(stderr, "INFO: %s\n", buffer + 1);
    */
}

//
// 'pdfio_error_cb()' - Log an error from the PDFio library.
//

static bool                             // O - `false` to stop
pdfio_error_cb(pdfio_file_t *pdf,       // I - PDF file (unused)
               const char   *message,   // I - Error message
               void         *cb_data)   // I - Preparation data
{
  xform_prepare_t       *p = (xform_prepare_t *)cb_data;
                                        // Preparation data


  if (pdf != p->pdf)
    prepare_log(p, true, "Input Document %d: %s", p->document, message);
  else
    prepare_log(p, true, "Output Document: %s", message);

  return (false);
}

//
// 'resource_dict_cb()' - Merge resource dictionaries from multiple input pages.
//
// This function detects resource conflicts and maps conflicting names as
// needed.
//

static bool
resource_dict_cb(
    pdfio_dict_t *dict,			// I - Dictionary
    const char   *key,			// I - Dictionary key
    xform_page_t *outpage)		// I - Output page
{
  pdfio_array_t	*arrayval;		// Array value
  pdfio_dict_t	*dictval;		// Dictionary value
  const char	*nameval,		// Name value
		*curname;		// Current name value
  pdfio_obj_t	*objval;		// Object value
  char		mapname[256];		// Mapped resource name


  fprintf(stderr, "DEBUG: resource_dict_cb(dict=%p, key=\"%s\", outpage=%p)\n", (void *)dict, key, (void *)outpage);

  snprintf(mapname, sizeof(mapname), "%c%s", (int)('a' + outpage->layout), key);

  switch (pdfioDictGetType(dict, key))
  {
    case PDFIO_VALTYPE_ARRAY : // Array
        arrayval = pdfioDictGetArray(dict, key);
        if (pdfioDictGetArray(outpage->restype, key))
        {
	  if (!outpage->resmap[outpage->layout])
	    outpage->resmap[outpage->layout] = pdfioDictCreate(outpage->pdf);

	  pdfioDictSetName(outpage->resmap[outpage->layout], pdfioStringCreate(outpage->pdf, key), pdfioStringCreate(outpage->pdf, mapname));
	  key = mapname;
	}

        pdfioDictSetArray(outpage->restype, pdfioStringCreate(outpage->pdf, key), pdfioArrayCopy(outpage->pdf, arrayval));
        break;

    case PDFIO_VALTYPE_DICT : // Dictionary
        dictval = pdfioDictGetDict(dict, key);
        if (pdfioDictGetDict(outpage->restype, key))
        {
	  if (!outpage->resmap[outpage->layout])
	    outpage->resmap[outpage->layout] = pdfioDictCreate(outpage->pdf);

	  pdfioDictSetName(outpage->resmap[outpage->layout], pdfioStringCreate(outpage->pdf, key), pdfioStringCreate(outpage->pdf, mapname));
	  key = mapname;
	}

        pdfioDictSetDict(outpage->restype, pdfioStringCreate(outpage->pdf, key), pdfioDictCopy(outpage->pdf, dictval));
        break;

    case PDFIO_VALTYPE_NAME : // Name
        nameval = pdfioDictGetName(dict, key);
        if ((curname = pdfioDictGetName(outpage->restype, key)) != NULL)
        {
          if (!strcmp(nameval, curname))
            break;

	  if (!outpage->resmap[outpage->layout])
	    outpage->resmap[outpage->layout] = pdfioDictCreate(outpage->pdf);

	  pdfioDictSetName(outpage->resmap[outpage->layout], pdfioStringCreate(outpage->pdf, key), pdfioStringCreate(outpage->pdf, mapname));
	  key = mapname;
	}

        pdfioDictSetName(outpage->restype, pdfioStringCreate(outpage->pdf, key), pdfioStringCreate(outpage->pdf, nameval));
        break;

    case PDFIO_VALTYPE_INDIRECT : // Object reference
        objval = pdfioDictGetObj(dict, key);
        if (pdfioDictGetObj(outpage->restype, key))
        {
	  if (!outpage->resmap[outpage->layout])
	    outpage->resmap[outpage->layout] = pdfioDictCreate(outpage->pdf);

	  pdfioDictSetName(outpage->resmap[outpage->layout], pdfioStringCreate(outpage->pdf, key), pdfioStringCreate(outpage->pdf, mapname));
	  key = mapname;
	}

        pdfioDictSetObj(outpage->restype, pdfioStringCreate(outpage->pdf, key), pdfioObjCopy(outpage->pdf, objval));
        break;

    default :
        break;
  }

  return (true);
}

//
// 'page_dict_cb()' - Merge page dictionaries from multiple input pages.
//
// This function detects resource conflicts and maps conflicting names as
// needed.
//

static bool				// O - `true` to continue, `false` to stop
page_dict_cb(pdfio_dict_t *dict,	// I - Dictionary
             const char   *key,		// I - Dictionary key
             xform_page_t *outpage)	// I - Output page
{
  pdfio_array_t	*arrayres;		// Array resource
  pdfio_array_t	*arrayval = NULL;	// Array value
  pdfio_dict_t	*dictval = NULL;	// Dictionary value
  pdfio_obj_t	*objval;		// Object value


  fprintf(stderr, "DEBUG: page_dict_cb(dict=%p, key=\"%s\", outpage=%p), type=%d\n", (void *)dict, key, (void *)outpage, pdfioDictGetType(dict, key));

  if (strcmp(key, "ColorSpace") && strcmp(key, "ExtGState") && strcmp(key, "Font") && strcmp(key, "Pattern") && strcmp(key, "ProcSet") && strcmp(key, "Properties") && strcmp(key, "Shading") && strcmp(key, "XObject"))
    return (true);

  switch (pdfioDictGetType(dict, key))
  {
    case PDFIO_VALTYPE_ARRAY : // Array resource
        arrayval = pdfioDictGetArray(dict, key);
        break;

    case PDFIO_VALTYPE_DICT : // Dictionary resource
        dictval = pdfioDictGetDict(dict, key);
        break;

    case PDFIO_VALTYPE_INDIRECT : // Object reference to dictionary
        objval   = pdfioDictGetObj(dict, key);
        arrayval = pdfioObjGetArray(objval);
        dictval  = pdfioObjGetDict(objval);

        fprintf(stderr, "DEBUG: page_dict_cb: objval=%p(%u), arrayval=%p, dictval=%p\n", (void *)objval, (unsigned)pdfioObjGetNumber(objval), (void *)arrayval, (void *)dictval);
        break;

    default :
        break;
  }

  if (arrayval)
  {
    // Copy/merge an array resource...
    if ((arrayres = pdfioDictGetArray(outpage->resdict, key)) == NULL)
    {
      // Copy array
      pdfioDictSetArray(outpage->resdict, pdfioStringCreate(outpage->pdf, key), pdfioArrayCopy(outpage->pdf, arrayval));
    }
    else if (!strcmp(key, "ProcSet"))
    {
      // Merge ProcSet array
      size_t		i, j,		// Looping var
			ic, jc;		// Counts
      const char	*iv, *jv;	// Values

      for (i = 0, ic = pdfioArrayGetSize(arrayval); i < ic; i ++)
      {
	if ((iv = pdfioArrayGetName(arrayval, i)) == NULL)
	  continue;

	for (j = 0, jc = pdfioArrayGetSize(arrayres); j < jc; j ++)
	{
	  if ((jv = pdfioArrayGetName(arrayres, j)) == NULL)
	    continue;

	  if (!strcmp(iv, jv))
	    break;
	}

	if (j >= jc)
	  pdfioArrayAppendName(arrayres, pdfioStringCreate(outpage->pdf, iv));
      }
    }
  }
  else if (dictval)
  {
    // Copy/merge dictionary...
    if ((outpage->restype = pdfioDictGetDict(outpage->resdict, key)) == NULL)
      pdfioDictSetDict(outpage->resdict, pdfioStringCreate(outpage->pdf, key), pdfioDictCopy(outpage->pdf, dictval));
    else
      pdfioDictIterateKeys(dictval, (pdfio_dict_cb_t)resource_dict_cb, outpage);
  }

  return (true);
}



//
// 'pdfio_password_cb()' - Return the password, if any, for the input document.
//

static const char *                     // O - Document password
pdfio_password_cb(void       *cb_data,  // I - Document number
                  const char *filename) // I - Filename (unused)
{
  int   document = *((int *)cb_data);   // Document number
  char  name[128];                      // Environment variable name


  (void)filename;

  if (document > 1)
  {
    snprintf(name, sizeof(name), "IPP_DOCUMENT_PASSWORD%d", document);
    return (getenv(name));
  }
  else
  {
    return (getenv("IPP_DOCUMENT_PASSWORD"));
  }
}




//
// 'prepare_number_up()' - Prepare the layout rectangles based on the number-up and orientation-requested values.
//

static void
prepare_number_up(xform_prepare_t *p)	// I - Preparation data
{
  size_t	i,			// Looping var
		cols,			// Number of columns
		rows;			// Number of rows
  pdfio_rect_t	*r;			// Current layout rectangle...
  double	width,			// Width of layout rectangle
		height;			// Height of layout rectangle


  if (!strcmp(p->options->imposition_template, "booklet"))
  {
    // "imposition-template" = 'booklet' forces 2-up output...
    p->num_layout   = 2;
    p->layout[0]    = p->media;
    p->layout[0].y2 = p->media.y2 / 2.0;
    p->layout[1]    = p->media;
    p->layout[1].y1 = p->media.y2 / 2.0;

    if (p->options->number_up != 1)
      prepare_log(p, false, "Ignoring \"number-up\" = '%d'.", p->options->number_up);

    return;
  }
  else
  {
    p->num_layout = (size_t)p->options->number_up;
  }

  // Figure out the number of rows and columns...
  switch (p->num_layout)
  {
    default : // 1-up or unknown
	if (p->options->number_up != 1)
	  prepare_log(p, false, "Ignoring \"number-up\" = '%d'.", p->options->number_up);

        p->num_layout   = 1;
        p->layout[0]    = p->crop;
        return;

    case 2 : // 2-up
        cols = 1;
        rows = 2;
        break;
    case 4 : // 4-up
        cols = 2;
        rows = 2;
        break;
    case 6 : // 6-up
        cols = 2;
        rows = 3;
        break;
    case 9 : // 9-up
        cols = 3;
        rows = 3;
        break;
    case 12 : // 12-up
        cols = 3;
        rows = 4;
        break;
    case 16 : // 16-up
        cols = 4;
        rows = 4;
        break;
  }

  // Then arrange the page rectangles evenly across the page...
  width  = (p->crop.x2 - p->crop.x1) / cols;
  height = (p->crop.y2 - p->crop.y1) / rows;

  switch (p->options->orientation_requested)
  {
    default : // Portrait or "none"...
        for (i = 0, r = p->layout; i < p->num_layout; i ++, r ++)
        {
          r->x1 = p->crop.x1 + width * (i % cols);
          r->y1 = p->crop.y1 + height * (rows - 1 - i / cols);
          r->x2 = r->x1 + width;
          r->y2 = r->y1 + height;
        }
        break;

    case IPP_ORIENT_LANDSCAPE : // Landscape
        for (i = 0, r = p->layout; i < p->num_layout; i ++, r ++)
        {
          r->x1 = p->crop.x1 + width * (cols - 1 - i / rows);
          r->y1 = p->crop.y1 + height * (rows - 1 - (i % rows));
          r->x2 = r->x1 + width;
          r->y2 = r->y1 + height;
        }
        break;

    case IPP_ORIENT_REVERSE_PORTRAIT : // Reverse portrait
        for (i = 0, r = p->layout; i < p->num_layout; i ++, r ++)
        {
          r->x1 = p->crop.x1 + width * (cols - 1 - (i % cols));
          r->y1 = p->crop.y1 + height * (i / cols);
          r->x2 = r->x1 + width;
          r->y2 = r->y1 + height;
        }
        break;

    case IPP_ORIENT_REVERSE_LANDSCAPE : // Reverse landscape
        for (i = 0, r = p->layout; i < p->num_layout; i ++, r ++)
        {
          r->x1 = p->crop.x1 + width * (i / rows);
          r->y1 = p->crop.y1 + height * (i % rows);
          r->x2 = r->x1 + width;
          r->y2 = r->y1 + height;
        }
        break;
  }
}

//
// 'prepare_pages()' - Prepare the pages for the output document.
//

static void
prepare_pages(
    xform_prepare_t  *p,		// I - Preparation data
    size_t           num_documents,	// I - Number of documents
    xform_document_t *documents)	// I - Documents
{
  int		page;			// Current page number in output
  size_t	i,			// Looping var
		current,		// Current output page index
		layout;			// Current layout cell
  xform_page_t	*outpage;		// Current output page
  xform_document_t *d;			// Current document
  bool		use_page;		// Use this page?


  if (!strcmp(p->options->imposition_template, "booklet"))
  {
    // Booklet printing arranges input pages so that the folded output can be
    // stapled along the midline...
    p->num_outpages = (size_t)(p->num_inpages + 1) / 2;
    if (p->num_outpages & 1)
      p->num_outpages ++;

    for (current = 0, layout = 0, page = 1, i = num_documents, d = documents; i > 0; i --, d ++)
    {
      while (page <= d->last_page)
      {
	if (p->options->multiple_document_handling < IPPOPT_HANDLING_SINGLE_DOCUMENT)
	  use_page = ippOptionsCheckPage(p->options, page - d->first_page + 1);
	else
	  use_page = ippOptionsCheckPage(p->options, page);

        if (use_page)
        {
	  if (current < p->num_outpages)
	    outpage = p->outpages + current;
	  else
	    outpage = p->outpages + 2 * p->num_outpages - current - 1;

	  outpage->pdf           = p->pdf;
	  outpage->input[layout] = pdfioFileGetPage(d->pdf, (size_t)(page - d->first_page));
	  layout = 1 - layout;
	  current ++;
	}

        page ++;
      }

      if (p->options->multiple_document_handling < IPPOPT_HANDLING_SINGLE_DOCUMENT)
        page = 1;
    }
  }
  else
  {
    // Normal printing lays out N input pages on each output page...
    for (current = 0, outpage = p->outpages, layout = 0, page = 1, i = num_documents, d = documents; i > 0; i --, d ++)
    {
      while (page <= d->last_page)
      {
	if (p->options->multiple_document_handling < IPPOPT_HANDLING_SINGLE_DOCUMENT)
	  use_page = ippOptionsCheckPage(p->options, page - d->first_page + 1);
	else
	  use_page = ippOptionsCheckPage(p->options, page);

        if (use_page)
        {
          outpage->pdf           = p->pdf;
          outpage->input[layout] = pdfioFileGetPage(d->pdf, (size_t)(page - d->first_page));

          if (Verbosity)
	    fprintf(stderr, "DEBUG: Using page %d (%p) of document %d, cell=%u/%u, current=%u\n", page, (void *)outpage->input[layout], (int)(d - documents + 1), (unsigned)layout + 1, (unsigned)p->num_layout, (unsigned)current);

          layout ++;
          if (layout == p->num_layout)
          {
            current ++;
            outpage ++;
            layout = 0;
          }
        }

        page ++;
      }

      if (p->options->multiple_document_handling < IPPOPT_HANDLING_SINGLE_DOCUMENT)
      {
        page = 1;

        if (layout)
        {
	  current ++;
	  outpage ++;
	  layout = 0;
        }
      }
      else if (p->options->multiple_document_handling == IPPOPT_HANDLING_SINGLE_NEW_SHEET && (current & 1))
      {
	current ++;
	outpage ++;
	layout = 0;
      }
    }

    if (layout)
      current ++;

    p->num_outpages = current;
  }
}

//
// 'copy_page()' - Copy the input page to the output page.
//

static void
copy_page(xform_prepare_t *p,		// I - Preparation data
          xform_page_t    *outpage,	// I - Output page
          size_t          layout)	// I - Layout cell
{
  pdfio_rect_t	*cell = p->layout + layout;
					// Layout cell
  pdfio_dict_t	*idict;			// Input page dictionary
  pdfio_rect_t	irect;			// Input page rectangle
  double	cwidth,			// Cell width
		cheight,		// Cell height
		iwidth,			// Input page width
		iheight,		// Input page height
		scaling;		// Scaling factor
  bool		rotate;			// Rotate 90 degrees?
  pdfio_matrix_t cm;			// Cell transform matrix
  size_t	i,			// Looping var
		count;			// Number of input page streams
  pdfio_stream_t *st;			// Input page stream
  char		buffer[65536],		// Copy buffer
		*bufptr,		// Pointer into buffer
		*bufstart,		// Start of current sequence
		*bufend,		// End of buffer
		name[256],		// Name buffer
		*nameptr;		// Pointer into name
  ssize_t	bytes;			// Bytes read
  const char	*resname;		// Resource name


  // Skip if this cell is empty...
  if (!outpage->input[layout])
    return;

  // Save state for this cell...
  pdfioContentSave(outpage->output);

  // Clip to cell...
  if (getenv("IPPTRANSFORM_DEBUG"))
  {
    // Draw a box around the cell for debugging...
    pdfioContentSetStrokeColorDeviceGray(outpage->output, 0.0);
    pdfioContentPathRect(outpage->output, cell->x1, cell->y1, cell->x2 - cell->x1, cell->y2 - cell->y1);
    pdfioContentStroke(outpage->output);
  }

  pdfioContentPathRect(outpage->output, cell->x1, cell->y1, cell->x2 - cell->x1, cell->y2 - cell->y1);
  pdfioContentClip(outpage->output, false);
  pdfioContentPathEnd(outpage->output);

  // Transform input page to output cell...
  idict = pdfioObjGetDict(outpage->input[layout]);

  if (!pdfioDictGetRect(idict, "CropBox", &irect))
  {
    // No crop box, use media box...
    if (!pdfioDictGetRect(idict, "MediaBox", &irect))
    {
      // No media box, use output page size...
      irect = p->media;
    }
  }

  cwidth  = cell->x2 - cell->x1;
  cheight = cell->y2 - cell->y1;

  iwidth  = irect.x2 - irect.x1;
  iheight = irect.y2 - irect.y1;

  if ((iwidth > iheight && cwidth < cheight) || (iwidth < iheight && cwidth > cheight))
  {
    // Need to rotate...
    rotate  = true;
    iwidth  = irect.y2 - irect.y1;
    iheight = irect.x2 - irect.x1;
  }
  else
  {
    // No rotation...
    rotate = false;
  }

  fprintf(stderr, "DEBUG: iwidth=%g, iheight=%g, cwidth=%g, cheight=%g, rotate=%s\n", iwidth, iheight, cwidth, cheight, rotate ? "true" : "false");

  scaling = cwidth / iwidth;
  if (p->options->print_scaling == IPPOPT_SCALING_FILL)
  {
    // Scale to fill...
    if ((iheight * scaling) < cheight)
      scaling = cheight / iheight;
  }
  else
  {
    // Scale to fit...
    if ((iheight * scaling) > cheight)
      scaling = cheight / iheight;
  }

  if (rotate)
  {
    cm[0][0] = 0.0;
    cm[0][1] = -scaling;
    cm[1][0] = scaling;
    cm[1][1] = 0.0;
    cm[2][0] = cell->x1 + 0.5 * (cwidth - iwidth * scaling);
    cm[2][1] = cell->y2 + 0.5 * (cheight - iheight * scaling);
  }
  else
  {
    cm[0][0] = scaling;
    cm[0][1] = 0.0;
    cm[1][0] = 0.0;
    cm[1][1] = scaling;
    cm[2][0] = cell->x1 + 0.5 * (cwidth - iwidth * scaling);
    cm[2][1] = cell->y1 + 0.5 * (cheight - iheight * scaling);
  }

  if (Verbosity)
    fprintf(stderr, "DEBUG: Page %u, cell %u/%u, cm=[%g %g %g %g %g %g], input=%p\n", (unsigned)(outpage - p->outpages + 1), (unsigned)layout + 1, (unsigned)p->num_layout, cm[0][0], cm[0][1], cm[1][0], cm[1][1], cm[2][0], cm[2][1], (void *)outpage->input[layout]);

  pdfioContentMatrixConcat(outpage->output, cm);

  // Copy content streams...
  for (i = 0, count = pdfioPageGetNumStreams(outpage->input[layout]); i < count; i ++)
  {
    fprintf(stderr, "DEBUG: Opening content stream %u/%u...\n", (unsigned)i + 1, (unsigned)count);

    if ((st = pdfioPageOpenStream(outpage->input[layout], i, true)) != NULL)
    {
      fprintf(stderr, "DEBUG: Opened stream %u, resmap[%u]=%p\n", (unsigned)i + 1, (unsigned)layout, (void *)outpage->resmap[layout]);
      if (outpage->resmap[layout])
      {
        // Need to map resource names...
	while ((bytes = pdfioStreamRead(st, buffer, sizeof(buffer))) > 0)
	{
	  for (bufptr = buffer, bufstart = buffer, bufend = buffer + bytes; bufptr < bufend;)
	  {
	    if (*bufptr == '/')
	    {
	      // Start of name...
	      bool done = false;		// Done with name?

	      bufptr ++;
	      pdfioStreamWrite(outpage->output, bufstart, (size_t)(bufptr - bufstart));

              nameptr = name;

              do
              {
		if (bufptr >= bufend)
		{
		  // Read another buffer's worth...
		  if ((bytes = pdfioStreamRead(st, buffer, sizeof(buffer))) <= 0)
		    break;

                  bufptr = buffer;
                  bufend = buffer + bytes;
		}

		if (strchr("<>(){}[]/% \t\n\r", *bufptr))
		{
		  // Delimiting character...
		  done = true;
		}
		else if (*bufptr == '#')
		{
		  int	j,			// Looping var
			ch = 0;			// Escaped character

                  for (j = 0; j < 2; j ++)
                  {
		    bufptr ++;
		    if (bufptr >= bufend)
		    {
		      // Read another buffer's worth...
		      if ((bytes = pdfioStreamRead(st, buffer, sizeof(buffer))) <= 0)
			break;

		      bufptr = buffer;
		      bufend = buffer + bytes;
		    }

		    if (!isxdigit(*bufptr & 255))
		      break;
		    else if (isdigit(*bufptr))
		      ch = (ch << 4) | (*bufptr - '0');
		    else
		      ch = (ch << 4) | (tolower(*bufptr) - 'a' + 10);
		  }

                  if (nameptr < (name + sizeof(name) - 1))
                  {
                    // Save escaped character...
                    *nameptr++ = (char)ch;
		    bufptr ++;
		  }
		  else
		  {
		    // Not enough room...
		    break;
		  }
		}
		else if (nameptr < (name + sizeof(name) - 1))
		{
		  // Save literal character...
		  *nameptr++ = *bufptr++;
		}
		else
		{
		  // Not enough room...
		  break;
		}
	      }
	      while (!done);

              bufstart = bufptr;
	      *nameptr = '\0';

	      // See if it needs to be mapped...
	      if ((resname = pdfioDictGetName(outpage->resmap[layout], name)) == NULL)
		resname = name;

	      pdfioStreamPuts(outpage->output, resname);
	    }
	    else if (buffer[0] == '(')
	    {
	      // Skip string...
	      bool	done = false;		// Are we done yet?
	      int	parens = 0;		// Number of parenthesis

	      do
	      {
		bufptr ++;
		if (bufptr >= bufend)
		{
		  // Save what has been skipped so far...
		  pdfioStreamWrite(outpage->output, bufstart, (size_t)(bufptr - bufstart));

                  // Read another buffer's worth...
		  if ((bytes = pdfioStreamRead(st, buffer, sizeof(buffer))) <= 0)
		    break;

		  bufptr   = buffer;
		  bufstart = buffer;
		  bufend   = buffer + bytes;
		}

		if (*bufptr == ')')
		{
		  if (parens > 0)
		    parens --;
		  else
		    done = true;

		  bufptr ++;
		}
		else if (*bufptr == '(')
		{
		  parens ++;
		  bufptr ++;
		}
		else if (*bufptr == '\\')
		{
		  // Skip escaped character...
		  bufptr ++;

		  if (bufptr >= bufend)
		  {
		    // Save what has been skipped so far...
		    pdfioStreamWrite(outpage->output, bufstart, (size_t)(bufptr - bufstart));

		    // Read another buffer's worth...
		    if ((bytes = pdfioStreamRead(st, buffer, sizeof(buffer))) <= 0)
		      break;

		    bufptr   = buffer;
		    bufstart = buffer;
		    bufend   = buffer + bytes;
		  }

                  bufptr ++;
		}
		else
		{
		  bufptr ++;
		}
	      }
	      while (!done);
	    }
	    else
	    {
	      // Skip one character...
	      bufptr ++;
	    }
	  }

	  if (bufptr > bufstart)
	  {
	    // Write the remainder...
	    pdfioStreamWrite(outpage->output, bufstart, (size_t)(bufptr - bufstart));
	  }
	}
      }
      else
      {
        // Copy page stream verbatim...
        while ((bytes = pdfioStreamRead(st, buffer, sizeof(buffer))) > 0)
          pdfioStreamWrite(outpage->output, buffer, (size_t)bytes);
      }

      pdfioStreamClose(st);
    }
  }

  // Restore state...
  pdfioStreamPuts(outpage->output, "\n");
  pdfioContentRestore(outpage->output);
}


//
// 'prepare_documents()' - Prepare one or more documents for printing.
//
// This function generates a single PDF file containing the union of the input
// documents and any job sheets.
//

static bool				// O - `true` on success, `false` on failure
prepare_documents(
    size_t           num_documents,	// I - Number of input documents
    xform_document_t *documents,	// I - Input documents
    ipp_options_t    *options,		// I - IPP options
    const char       *sheet_back,	// I - Back side transform
    char             *outfile,		// I - Output filename buffer
    size_t           outsize,		// I - Output filename buffer size
    const char       *outformat,	// I - Output format
    unsigned         *outpages,		// O - Number of pages
    bool             generate_copies)	// I - Generate copies in output PDF?
{
  bool			ret = false;	// Return value
  int			copies;		// Number of copies
  size_t		i;		// Looping var
  xform_prepare_t	p;		// Preparation data
  xform_document_t	*d;		// Current document
  xform_page_t		*outpage;	// Current output page
  int			outdir;		// Output direction
  bool			reverse_order;	// Should output be in reverse order?
  size_t		layout;		// Layout cell
  int			document;	// Document number
  int			page;		// Current page number
  bool			duplex = !strncmp(options->sides, "two-sided-", 10);
					// Doing two-sided printing?

  fprintf(stderr, "prepdocs check_______________\n");
  // Initialize data for preparing input files for transform...
  memset(&p, 0, sizeof(p));
  p.options = options;
  p.errors  = cupsArrayNew(NULL, NULL, NULL, 0, (cups_acopy_cb_t)strdup, (cups_afree_cb_t)free);

  media_to_rect(&options->media, &p.media, &p.crop);
  prepare_number_up(&p);

  if (!strncmp(options->sides, "two-sided-", 10) && sheet_back && strcmp(sheet_back, "normal"))
  {
    // Need to do a transform on the back side of pages...
    if (!strcmp(sheet_back, "flipped"))
    {
      if (!strcmp(options->sides, "two-sided-short-edge"))
      {
	p.use_duplex_xform   = true;
        p.duplex_xform[0][0] = -1.0;
        p.duplex_xform[0][1] = 0.0;
        p.duplex_xform[1][0] = 0.0;
        p.duplex_xform[1][1] = 1.0;
        p.duplex_xform[2][0] = p.media.x2;
        p.duplex_xform[2][1] = 0.0;
      }
      else
      {
	p.use_duplex_xform   = true;
        p.duplex_xform[0][0] = 1.0;
        p.duplex_xform[0][1] = 0.0;
        p.duplex_xform[1][0] = 0.0;
        p.duplex_xform[1][1] = -1.0;
        p.duplex_xform[2][0] = 0.0;
        p.duplex_xform[2][1] = p.media.y2;
      }
    }
    else if ((!strcmp(sheet_back, "manual-tumble") && !strcmp(options->sides, "two-sided-short-edge")) || (!strcmp(sheet_back, "rotated") && !strcmp(options->sides, "two-sided-long-edge")))
    {
      p.use_duplex_xform   = true;
      p.duplex_xform[0][0] = -1.0;
      p.duplex_xform[0][1] = 0.0;
      p.duplex_xform[1][0] = 0.0;
      p.duplex_xform[1][1] = -1.0;
      p.duplex_xform[2][0] = p.media.x2;
      p.duplex_xform[2][1] = p.media.y2;
    }
  }

  if ((p.pdf = pdfioFileCreateTemporary(outfile, outsize, "1.7", &p.media, &p.media, pdfio_error_cb, &p)) == NULL)
    return (false);

  // Loop through the input documents to count pages, etc.
  for (i = num_documents, d = documents, document = 1, page = 1; i > 0; i --, d ++, document ++)
  {
    if (Verbosity)
      fprintf(stderr, "DEBUG: Preparing document %d: '%s' (%s)\n", document, d->filename, d->format);

    if (!strcmp(d->format, "application/pdf"))
    {
      // PDF file...
      d->pdf_filename = d->filename;
    }

    if (Verbosity)
      fprintf(stderr, "DEBUG: Opening prepared document %d: '%s'.\n", document, d->pdf_filename);

    d->pdf = pdfioFileOpen(d->pdf_filename, pdfio_password_cb, NULL, pdfio_error_cb, NULL);
    size_t num_pages = pdfioFileGetNumPages(d->pdf);

    if ((d->pdf = pdfioFileOpen(d->pdf_filename, pdfio_password_cb, &document, pdfio_error_cb, &p)) == NULL)
      goto done;

    if (options->multiple_document_handling < IPPOPT_HANDLING_SINGLE_DOCUMENT)
    {
      d->first_page = 1;
      d->last_page  = (int)pdfioFileGetNumPages(d->pdf);
    }
    else
    {
      d->first_page = page;
      d->last_page  = page + (int)pdfioFileGetNumPages(d->pdf) - 1;
    }

    if (Verbosity)
      fprintf(stderr, "DEBUG: Document %d: pages %d to %d.\n", document, d->first_page, d->last_page);

    while (page <= d->last_page)
    {
      if (options->multiple_document_handling < IPPOPT_HANDLING_SINGLE_DOCUMENT)
      {
        if (ippOptionsCheckPage(options, page - d->first_page + 1))
          d->num_pages ++;
      }
      else if (ippOptionsCheckPage(options, page))
      {
        d->num_pages ++;
      }

      page ++;
    }

    if ((d->last_page & 1) && duplex && options->multiple_document_handling != IPPOPT_HANDLING_SINGLE_DOCUMENT)
    {
      d->last_page ++;
      page ++;
    }

    if ((d->num_pages & 1) && duplex && options->multiple_document_handling != IPPOPT_HANDLING_SINGLE_DOCUMENT)
      d->num_pages ++;

    p.num_inpages += d->num_pages;
  }

  // When doing N-up or booklet printing, the default is to scale to fit unless
  // fill is explicitly chosen...
  if (p.num_layout > 1 && options->print_scaling != IPPOPT_SCALING_FILL)
    options->print_scaling = IPPOPT_SCALING_FIT;

  // Prepare output pages...
  prepare_pages(&p, num_documents, documents);

  // Add job-sheets content...
  if (options->job_sheets[0] && strcmp(options->job_sheets, "none"))
    generate_job_sheets(&p);

  // Copy pages to the output file...
  for (copies = generate_copies ? options->copies : 1; copies > 0; copies --)
  {
    reverse_order = !strcmp(options->output_bin, "face-up");
    if (options->page_delivery >= IPPOPT_DELIVERY_REVERSE_ORDER_FACE_DOWN)
      reverse_order = !reverse_order;

    if (reverse_order)
    {
      outpage = p.outpages + p.num_outpages - 1;
      outdir  = -1;
    }
    else
    {
      outpage = p.outpages;
      outdir  = 1;
    }

    if (p.num_layout == 1 && options->print_scaling == IPPOPT_SCALING_NONE && strcasecmp(outformat, "image/pwg-raster") && strcasecmp(outformat, "image/urf"))
    {
      // Simple path - no layout/scaling/rotation of pages so we can just copy the pages quickly.
      if (Verbosity)
	fputs("DEBUG: Doing fast copy of pages.\n", stderr);

      for (i = p.num_outpages; i > 0; i --, outpage += outdir)
      {
	if (outpage->input[0])
	  pdfioPageCopy(p.pdf, outpage->input[0]);
      }
    }
    else
    {
      // Layout path - merge page resources and do mapping of resources as needed
      if (Verbosity)
	fprintf(stderr, "DEBUG: Doing full layout of %u pages.\n", (unsigned)p.num_outpages);

      for (i = p.num_outpages; i > 0; i --, outpage += outdir)
      {
	// Create a page dictionary that merges the resources from each of the
	// input pages...
	if (Verbosity)
	  fprintf(stderr, "DEBUG: Laying out page %u/%u.\n", (unsigned)(outpage - p.outpages + 1), (unsigned)p.num_outpages);

	outpage->pagedict = pdfioDictCreate(p.pdf);
	outpage->resdict  = pdfioDictCreate(p.pdf);

	pdfioDictSetRect(outpage->pagedict, "CropBox", &p.media);
	pdfioDictSetRect(outpage->pagedict, "MediaBox", &p.media);
	pdfioDictSetDict(outpage->pagedict, "Resources", outpage->resdict);
	pdfioDictSetName(outpage->pagedict, "Type", "Page");

	for (layout = 0; layout < p.num_layout; layout ++)
	{
	  pdfio_dict_t	*pagedict,	// Page dictionary
			*resdict;	// Resources dictionary
	  pdfio_obj_t	*resobj;	// Resources object

	  if (!outpage->input[layout])
	    continue;

	  outpage->layout  = layout;
	  outpage->restype = NULL;

          pagedict = pdfioObjGetDict(outpage->input[layout]);
          if ((resdict = pdfioDictGetDict(pagedict, "Resources")) != NULL)
	    pdfioDictIterateKeys(resdict, (pdfio_dict_cb_t)page_dict_cb, outpage);
	  else if ((resobj = pdfioDictGetObj(pagedict, "Resources")) != NULL)
	    pdfioDictIterateKeys(pdfioObjGetDict(resobj), (pdfio_dict_cb_t)page_dict_cb, outpage);
          else if (Verbosity)
            fprintf(stderr, "DEBUG: No Resources for cell %u.\n", (unsigned)layout);

	  // TODO: Handle inherited resources from parent page objects...
	}

	// Now copy the content streams to build the composite page, using the
	// resource map for any named resources...
	outpage->output = pdfio_start_page(&p, outpage->pagedict);

	for (layout = 0; layout < p.num_layout; layout ++)
	  copy_page(&p, outpage, layout);

	pdfio_end_page(&p, outpage->output);
	outpage->output = NULL;
      }
    }
  }

  // Add final job-sheets content...
  if (options->job_sheets[0] && strcmp(options->job_sheets, "none"))
    generate_job_sheets(&p);

  // Add job-error-sheet content as needed...
  if (options->job_error_sheet.report == IPPOPT_ERROR_REPORT_ALWAYS || (options->job_error_sheet.report == IPPOPT_ERROR_REPORT_ON_ERROR && cupsArrayGetCount(p.errors) > 0))
    generate_job_error_sheet(&p);

  ret = true;

  *outpages = (unsigned)pdfioFileGetNumPages(p.pdf);

  // Finalize the output and return...
  done:
  cupsArrayDelete(p.errors);

  for (i = p.num_outpages, outpage = p.outpages; i > 0; i --, outpage ++)
  {
    if (outpage->output)
      pdfioStreamClose(outpage->output);
  }

  if (!pdfioFileClose(p.pdf))
    ret = false;

  if (!ret)
  {
    // Remove temporary file...
    unlink(outfile);
    *outfile = '\0';
  }

  // Close and delete intermediate files...
  for (i = num_documents, d = documents; i > 0; i --, d ++)
  {
    pdfioFileClose(d->pdf);
    if (d->tempfile[0])
      unlink(d->tempfile);
  }

  // Return success/fail status...
  return (ret);
}

// coping inputfp data to temp_fp, so that we have a filename, as it is required in pdfioFileOpen API
int 
copy_fd_to_tempfile(int inputfd, 
		    FILE *temp_file) 
{
  char buffer[BUFSIZ]; 
  ssize_t bytes_read, bytes_written;

  while ((bytes_read = read(inputfd, buffer, sizeof(buffer))) > 0) 
  {
    bytes_written = fwrite(buffer, 1, bytes_read, temp_file);
    if (bytes_written != bytes_read) 
    {
      fprintf(stderr, "write to temporary file failed");
      return -1; 
    }
  }

  if (bytes_read == -1) 
  {
    fprintf(stderr, "Read from inputfd failed");
    return -1; 
  }

  return 0; 
}

int 
cfFilterPDFToPDF(int inputfd, 
		 int outputfd,
		 int inputseekable, 
		 cf_filter_data_t *data, 
		 void *parameters) 
{

  char               *final_content_type = data->final_content_type;
  cf_logfunc_t       log = data->logfunc;
  void               *ld = data->logdata;
  cf_filter_iscanceledfunc_t iscanceled = data->iscanceledfunc;
  void               *icd = data->iscanceleddata;

  // New variables from prepare_documents
  // confirmed required
  char          pdf_file[1024];
  const char	*output_type = "application/pdf";
  ipp_options_t *ipp_options;
  xform_document_t file;
  
  //not confirmed
  xform_document_t   *documents = NULL;
  int                copies;
  bool               duplex = false;
  bool               reverse_order = false;
  size_t             layout = 1;
  xform_page_t       *outpage = NULL;
  const char         *sheet_back = "rotated";
  unsigned      	pdf_pages;

  memset(&file, 0, sizeof(file));

  ipp_options = ippOptionsNew(data->num_options, data->options);
  
  char temp_filename[] = "/tmp/tempfileXXXXXX";
  int temp_fd = mkstemp(temp_filename);
  if (temp_fd == -1)
  {
    if (log) log(ld, CF_LOGLEVEL_ERROR, "tempfilename wasn't created");
    return 1;
  }

  // Convert the temp_fd to a FILE* stream
  FILE *inputfp = fdopen(temp_fd, "wb+");
  if (!inputfp) {
      if (log) log(ld, CF_LOGLEVEL_ERROR, "Couldn't convert temp_fd to FILE* stream");
      close(temp_fd);
      close(inputfd);
      unlink(temp_filename);
      return 1;
  }
  if (copy_fd_to_tempfile(inputfd, inputfp) == -1) {
        fprintf(stderr, "Failed to copy inputfd to temp file\n");
        fclose(inputfp);
        close(inputfd);
        unlink(temp_filename); // Clean up temporary file
        return 1;
    }

  rewind(inputfp); // Rewind the temp_file for reading

  file.filename = temp_filename;
  file.format = "application/pdf";
  file.pdf_filename = temp_filename;

  if (!prepare_documents(1, &file, ipp_options, sheet_back, pdf_file, sizeof(pdf_file), output_type, &pdf_pages, !strcasecmp(output_type, "application/pdf")))
  {
    // Unable to prepare documents, exit...
    ippOptionsDelete(ipp_options);
    return (1);
  }

  // After processing, close and unlink the initial temp file
fclose(inputfp); // Ensure the FILE* is closed
close(temp_fd);  // Close the underlying file descriptor
unlink(temp_filename); // Remove the initial temporary file

// ...

// Copy the generated PDF to outputfd
int tempfd = open(pdf_file, O_RDONLY);
if (tempfd < 0) {
    perror("open tempfile for reading");
    return 1; // Error
}

char buffer[8192];
ssize_t bytes_read, bytes_written, total_written;

while ((bytes_read = read(tempfd, buffer, sizeof(buffer))) > 0) {
    if (bytes_read < 0) {
        perror("read");
        close(tempfd);
        return 1;
    }

    total_written = 0;
    while (total_written < bytes_read) {
        bytes_written = write(outputfd, buffer + total_written, bytes_read - total_written);
        if (bytes_written < 0) {
            perror("write");
            close(tempfd);
            return 1;
        }
        total_written += bytes_written;
    }
}

close(tempfd);
unlink(pdf_file); // Remove the generated PDF temp file

return 0; // Success
}
// }}}
