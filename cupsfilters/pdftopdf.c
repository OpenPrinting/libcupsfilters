//
// Copyright (c) 6-2011, BBR Inc.  All rights reserved.
// Copyright 2024-2025 Uddhav Phatak <uddhavphatak@gmail.com>
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <config.h>

#include <stdio.h>
#include <float.h>
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

#define MAX_VISITED 20

static int              Verbosity = 0;

#define XFORM_TEXT_SIZE         10.0    // Point size of plain text output
#define XFORM_TEXT_HEIGHT       12.0    // Point height of plain text output
#define XFORM_TEXT_WIDTH        0.6     // Width of monospaced characters


//
// 'pdfio_start_page()' - Start a page.
//
// This function applies a transform on the back side pages.
//

static pdfio_stream_t * 		// O - Page content stream
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

static int
merge_resources_cb(pdfio_dict_t *dict,
	       	   const char *key,
		   pdfio_obj_t *value,
		   void *data)
{
    pdfio_dict_t *dest_dict = (pdfio_dict_t *)data;
    pdfioDictSetObj(dest_dict, key, pdfioObjCopy(NULL, value));
    return 1; // Continue iteration
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
// 'page_ext_dict_cb()' - Merge page dictionaries from multiple input pages.
//
// This function detects resource conflicts and maps conflicting names as
// needed.
//

static bool				// O - `true` to continue, `false` to stop
page_ext_dict_cb(pdfio_dict_t *dict,	// I - Dictionary
             const char   *key,		// I - Dictionary key
             xform_page_ext_t *outpage)	// I - Output page
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

static const char * // O - Document password
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
	cols = 1;
	rows = 1;
	break;
    case 2 : // 2-up
        cols = 1;
        rows = 2;
        break;
    case 3 : // 3-up
        cols = 1;
        rows = 3;
        break;
    case 4 : // 4-up
        cols = 2;
        rows = 2;
        break;
    case 6 : // 6-up
        cols = 2;
        rows = 3;
        break;
    case 8 : // 8-up
        cols = 2;
        rows = 4;
        break;
    case 9 : // 9-up
        cols = 3;
        rows = 3;
        break;
    case 10 : // 10-up
        cols = 2;
        rows = 5;
        break;
    case 12 : // 12-up
        cols = 3;
        rows = 4;
        break;
    case 15 : // 15-up
        cols = 3;
        rows = 5;
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

    case CF_FILTER_ORIENT_LANDSCAPE : // Landscape
        for (i = 0, r = p->layout; i < p->num_layout; i ++, r ++)
        {
          r->x1 = p->crop.x1 + width * (cols - 1 - i / rows);
          r->y1 = p->crop.y1 + height * (rows - 1 - (i % rows));
          r->x2 = r->x1 + width;
          r->y2 = r->y1 + height;
        }
        break;

    case CF_FILTER_ORIENT_REVERSE_PORTRAIT : // Reverse portrait
        for (i = 0, r = p->layout; i < p->num_layout; i ++, r ++)
        {
          r->x1 = p->crop.x1 + width * (cols - 1 - (i % cols));
          r->y1 = p->crop.y1 + height * (i / cols);
          r->x2 = r->x1 + width;
          r->y2 = r->y1 + height;
        }
        break;

    case CF_FILTER_ORIENT_REVERSE_LANDSCAPE : // Reverse landscape
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
	if (p->options->multiple_document_handling < CF_FILTER_HANDLING_SINGLE_DOCUMENT)
	  use_page = cfFilterOptionsIsPageInRange(p->options, page - d->first_page + 1);
	else
	  use_page = cfFilterOptionsIsPageInRange(p->options, page);

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

      if (p->options->multiple_document_handling < CF_FILTER_HANDLING_SINGLE_DOCUMENT)
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
	if (p->options->multiple_document_handling < CF_FILTER_HANDLING_SINGLE_DOCUMENT)
	  use_page = cfFilterOptionsIsPageInRange(p->options, page - d->first_page + 1);
	else
	  use_page = cfFilterOptionsIsPageInRange(p->options, page);

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

      if (p->options->multiple_document_handling < CF_FILTER_HANDLING_SINGLE_DOCUMENT)
      {
        page = 1;

        if (layout)
        {
	  current ++;
	  outpage ++;
	  layout = 0;
        }
      }
      else if (p->options->multiple_document_handling == CF_FILTER_HANDLING_SINGLE_NEW_SHEET && (current & 1))
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

void
getArrayAsMatrix(pdfio_array_t *array,
		 pdfio_matrix_t cm)
{
  size_t array_size = pdfioArrayGetSize(array);
  if(array_size != 6)
    return;
  double items[6];
  for(size_t i = 0; i<array_size; i++)
  {
    items[i] = pdfioArrayGetNumber(array, i);
  }

  cm[0][0] = items[0];
  cm[0][1] = items[1];
  cm[1][0] = items[2];
  cm[1][1] = items[3];
  cm[2][0] = items[4];
  cm[2][1] = items[5];
  return;
}

double
get_flags(pdfio_dict_t *annots_dict)
{
  double val = pdfioDictGetNumber(annots_dict, "F");
  return val;
}

static inline void matrix_set_identity(pdfio_matrix_t m) {
  m[0][0] = 1.0; m[0][1] = 0.0;   // a b
  m[1][0] = 0.0; m[1][1] = 1.0;   // c d
  m[2][0] = 0.0; m[2][1] = 0.0;   // e f
}


static inline void matrix_apply_point(const pdfio_matrix_t m, double x, double y, double *ox, double *oy) {
  *ox = m[0][0]*x + m[1][0]*y + m[2][0];
  *oy = m[0][1]*x + m[1][1]*y + m[2][1];
}



static inline void matrix_translate(pdfio_matrix_t m, double tx, double ty) {
  matrix_set_identity(m);
  m[2][0] = tx;  // e
  m[2][1] = ty;  // f
}

static char *matrix_unparse(const pdfio_matrix_t m) 
{
  // Emit "a b c d e f" with common PDF precision
  char *s = (char*)malloc(128);
  if (!s) 
    return NULL;
  snprintf(s, 128, "%.6f %.6f %.6f %.6f %.6f %.6f",
           m[0][0], m[0][1],  // a b
           m[1][0], m[1][1],  // c d
           m[2][0], m[2][1]); // e f
  return s;
}

static inline void 
matrix_rotatex90(pdfio_matrix_t m, 
		 int degrees) 
{
  int k = ((degrees % 360) + 360) % 360;
  matrix_set_identity(m);
  switch (k) 
  {
    case 90:   
      m[0][0]=0;  
      m[0][1]=1;  
      m[1][0]=-1; 
      m[1][1]=0;  
      break;  		// [0 1 -1 0 0 0]
    case 180:  
      m[0][0]=-1; 
      m[0][1]=0;  
      m[1][0]=0;  
      m[1][1]=-1; 
      break;  		// [-1 0 0 -1 0 0]
    case 270:  
      m[0][0]=0; 
      m[0][1]=-1;
      m[1][0]=1;  
      m[1][1]=0; 
      break;  		// [0 -1 1 0 0 0]
    default: /* 0° */ 
      break;
  }
}

static inline void 
matrix_scale(pdfio_matrix_t m,
	     double sx, double sy) 
{
  matrix_set_identity(m);
  m[0][0] = sx;  // a
  m[1][1] = sy;  // d
}

static inline void 
matrix_copy(pdfio_matrix_t dst, 
	    const pdfio_matrix_t src) 
{
  dst[0][0]=src[0][0]; 
  dst[0][1]=src[0][1];
  dst[1][0]=src[1][0]; 
  dst[1][1]=src[1][1];
  dst[2][0]=src[2][0]; 
  dst[2][1]=src[2][1];
}

static inline void 
matrix_concat(pdfio_matrix_t out, 
	      const pdfio_matrix_t L, 
	      const pdfio_matrix_t R) 
{
  pdfio_matrix_t t;
  t[0][0] = L[0][0]*R[0][0] + L[1][0]*R[0][1];  // a' = La*Ra + Lc*Rb
  t[0][1] = L[0][1]*R[0][0] + L[1][1]*R[0][1];  // b' = Lb*Ra + Ld*Rb
  t[1][0] = L[0][0]*R[1][0] + L[1][0]*R[1][1];  // c' = La*Rc + Lc*Rd
  t[1][1] = L[0][1]*R[1][0] + L[1][1]*R[1][1];  // d' = Lb*Rc + Ld*Rd
  t[2][0] = L[0][0]*R[2][0] + L[1][0]*R[2][1] + L[2][0]; // e' = La*Re + Lc*Rf + Le
  t[2][1] = L[0][1]*R[2][0] + L[1][1]*R[2][1] + L[2][1]; // f' = Lb*Re + Ld*Rf + Lf
  matrix_copy(out, t);
}

static inline pdfio_rect_t 
matrix_transform_rect(const pdfio_matrix_t m, 
		      pdfio_rect_t r) 
{
  double X[4], Y[4];
  matrix_apply_point(m, r.x1, r.y1, &X[0], &Y[0]);
  matrix_apply_point(m, r.x2, r.y1, &X[1], &Y[1]);
  matrix_apply_point(m, r.x2, r.y2, &X[2], &Y[2]);
  matrix_apply_point(m, r.x1, r.y2, &X[3], &Y[3]);

  double minx=X[0], maxx=X[0], miny=Y[0], maxy=Y[0];
  for (int i=1;i<4;i++) 
  {
    if (X[i]<minx) 
      minx=X[i]; 

    if (X[i]>maxx) 
      maxx=X[i];

    if (Y[i]<miny) 
      miny=Y[i]; 
    
    if (Y[i]>maxy) 
      maxy=Y[i];
  }
  pdfio_rect_t out = { minx, miny, maxx, maxy };
  return out;
}

static void
transform_point(const pdfio_matrix_t matrix,
		double x, double y,
		double *xp, double *yp)
{
    *xp = matrix[0][0] * x + matrix[1][0] * y + matrix[2][0];
    *yp = matrix[0][1] * x + matrix[1][1] * y + matrix[2][1];
}

pdfio_rect_t
transform_rectangle(const pdfio_matrix_t matrix,
		    pdfio_rect_t r)
{
  double min_x = DBL_MAX, min_y = DBL_MAX;
  double max_x = -DBL_MAX, max_y = -DBL_MAX;

  const double corners[4][2] = 
  {
    {r.x1, r.y1},  // Lower-left
    {r.x1, r.y2},  // Upper-left
    {r.x2, r.y1},  // Lower-right
    {r.x2, r.y2}   // Upper-right
  };

  for (int i = 0; i < 4; i++)
  {
    double x, y;
    transform_point(matrix, corners[i][0], corners[i][1], &x, &y);

    if (x < min_x) 
      min_x = x;
    if (x > max_x) 
      max_x = x;
    if (y < min_y) 
      min_y = y;
    if (y > max_y) 
      max_y = y;
  }

  pdfio_rect_t transformed = {min_x, min_y, max_x, max_y};
  return transformed;
}

char*
unparse_matrix(pdfio_matrix_t matrix)
{
  double comps[6] = {matrix[0][0], matrix[0][1], matrix[1][0],
                     matrix[1][1], matrix[2][0], matrix[2][1]};

  char parts[6][32];
  size_t total_len = 0;

  for (int i = 0; i < 6; i++)
  {
    if (fabs(comps[i]) < 1e-15) comps[i] = 0.0;

    char buf[32];
    snprintf(buf, sizeof(buf), "%.5f", comps[i]);

    char *dot = strchr(buf, '.');
    if (dot)
    {
      char *end = buf + strlen(buf) - 1;
      while (end > dot && *end == '0')
	*end-- = '\0';
      if (end == dot)
	*dot = '\0';
    }

    strcpy(parts[i], buf);
    total_len += strlen(parts[i]);
  }

  total_len += 5 + 1;

  char *result = malloc(total_len);
  if (!result)
    return NULL;

  char *ptr = result;
  for (int i = 0; i < 6; i++)
  {
    size_t part_len = strlen(parts[i]);
    memcpy(ptr, parts[i], part_len);
    ptr += part_len;

    if (i < 5)
      *ptr++ = ' ';
  }
  *ptr = '\0';
  return result;
}

pdfio_obj_t*
getAppearance(pdfio_dict_t *Annot_dict, char* value)
{
  pdfio_dict_t *AP_dict = pdfioDictGetDict(Annot_dict, "AP");
  pdfio_obj_t* appearance_obj = pdfioDictGetObj(AP_dict, value);

  if(appearance_obj)
    return appearance_obj;

  const char* desired_state = pdfioDictGetName(Annot_dict, "AS");
  if (desired_state)
  {
    pdfio_dict_t *N_dict = pdfioDictGetDict(AP_dict, value);
    appearance_obj = pdfioDictGetObj(N_dict, desired_state);
    return appearance_obj;
  }
  else
  {
    fprintf(stderr, "Key '%s' not found.\n", value);
  }

  return appearance_obj;
}

static pdfio_dict_t*
dict_get_stream_dict(pdfio_dict_t *d, 
		    const char *key) 
{
  if (!d || !key)
    return NULL;

  pdfio_dict_t *AP = pdfioDictGetDict(d, "AP");
  if (!AP)  
    return NULL;

  const char *as = pdfioDictGetName(d, "AS");  // e.g. "On", "Off", "Yes"
  if (as) 
  {
    pdfio_dict_t *W_dict = pdfioDictGetDict(AP, key);
    if (W_dict) 
    {
      pdfio_obj_t *obj = pdfioDictGetObj(W_dict, as);
      if (obj) 
	return pdfioObjGetDict(obj);
    }
  }

  {
    pdfio_obj_t *obj = pdfioDictGetObj(AP, key);
    if (obj) 
      return pdfioObjGetDict(obj);
  }

  return NULL;
}

static int 
get_pdf_matrix(pdfio_dict_t *d, 
	       const char *key, 
	       pdfio_matrix_t out) 
{
  pdfio_valtype_t v;
  v = pdfioDictGetType(d, key);
  if (v != PDFIO_VALTYPE_ARRAY) 
    return 0;
  pdfio_array_t *a = pdfioDictGetArray(d, key);
  double n[6];
  for (int i=0;i<6;i++) 
  {
    pdfio_valtype_t ai;
    ai = pdfioArrayGetType(a, i);
    if (ai != PDFIO_VALTYPE_NUMBER) 
      return 0;
    n[i] = pdfioArrayGetNumber(a, i);
  }
  // Map [a b c d e f] -> m
  out[0][0] = n[0]; out[0][1] = n[1];
  out[1][0] = n[2]; out[1][1] = n[3];
  out[2][0] = n[4]; out[2][1] = n[5];
  return 1;
}

char*
special_pdfio_annotation_get_content(pdfio_obj_t  *annot,
	       			     const char   *name, 
				     int           page_rotate, 
				     int           forbidden_flags,
				     int           required_flags)
{
  if (!annot || !name) 
  {
    fprintf(stderr, "ERROR: No Annot Obj or XObject name\n"); 
    return NULL;
  }

  pdfio_dict_t *annot_dict = pdfioObjGetDict(annot);
  if (!annot_dict) 
  {
    fprintf(stderr, "ERROR: Annot Obj does not have a Dict\n"); 
    return NULL;
  }

  pdfio_rect_t *rect = (pdfio_rect_t *)malloc(sizeof(pdfio_rect_t));
  if(!pdfioDictGetRect(annot_dict, "Rect", rect))
  {
    fprintf(stderr, "ERROR: Annotation dict doesn't have a rect\n"); 
    return NULL;
  }
    
  int flags = (int)get_flags(annot_dict);

  if (flags & forbidden_flags)
  {
    fprintf(stderr, "ERROR: forbidden Flags found\n");
    return NULL;
  }

  if ((flags & required_flags) != required_flags)
  {
    fprintf(stderr, "ERROR: missing required flags\n");
    return NULL;
  }

  pdfio_rect_t BBox;
  BBox.x1 = 0;
  BBox.y1 = 0;
  BBox.x2 = rect->x2 - rect->x1;
  BBox.y2 = rect->y2 - rect->y1;
  
  pdfio_matrix_t M;
  matrix_set_identity(M);
  
  pdfio_rect_t T = matrix_transform_rect(M, BBox);
  double Tw = T.x2 - T.x1, Th = T.y2 - T.y1;
  if (Tw == 0.0 || Th == 0.0) 
    return NULL;

  double rw = rect->x2 - rect->x1, rh = rect->y2 - rect->y1;

  pdfio_matrix_t AA, t, s;
  matrix_set_identity(AA);
  
  matrix_translate(t, rect->x1, rect->y1);
  matrix_concat(AA, t, AA);

  matrix_scale(s, rw / Tw, rh / Th);
  matrix_concat(AA, s, AA);

  matrix_translate(t, -T.x1, -T.y1);
  matrix_concat(AA, t, AA);

  char *AA_s = matrix_unparse(AA);
  if (!AA_s) 
    return NULL;

  const char *fmt = "q\n%s cm\n/%s Do\nQ\n"; 
  size_t need = strlen(AA_s) + strlen(name) + 20;
  char *out = (char*)malloc(need);
  if (!out) 
  {  
    free(AA_s);
    return NULL; 
  }

  snprintf(out, need, fmt, AA_s, name);
  free(AA_s);
  return out;
}

char*
pdfio_annotation_get_content(pdfio_obj_t  *annot,
			     const char   *name, 
			     int           page_rotate,
			     int           forbidden_flags,
			     int           required_flags)
{
  if (!annot || !name) 
  {
    fprintf(stderr, "ERROR: No annot obj or XObject name\n"); 
    return NULL;
  }

  pdfio_dict_t *annot_dict = pdfioObjGetDict(annot);
  if (!annot_dict) 
  {
    fprintf(stderr, "ERROR: Annot Obj does not have a Dict\n"); 
    return NULL;
  }

  pdfio_rect_t *rect = (pdfio_rect_t *)malloc(sizeof(pdfio_rect_t));
  if(!pdfioDictGetRect(annot_dict, "Rect", rect))
  {
    fprintf(stderr, "ERROR: Annotation dict doesn't have a rect\n"); 
    return NULL;
  }
    
  int flags = (int)get_flags(annot_dict);

  if (flags & forbidden_flags)
  {
    fprintf(stderr, "ERROR: forbidden Flags found\n");
    return NULL;
  }

  if ((flags & required_flags) != required_flags)
  {
    fprintf(stderr, "ERROR: missing required flags\n");
    return NULL;
  }

  pdfio_dict_t *appearance_N_dict = dict_get_stream_dict(annot_dict, "N");
  if (!appearance_N_dict) 
    return NULL;

  pdfio_rect_t BBox;
  if(!pdfioDictGetRect(appearance_N_dict, "BBox", &BBox))
  {
    BBox.x1 = 0;
    BBox.y1 = 0;
    BBox.x2 = rect->x2 - rect->x1;
    BBox.y2 = rect->y2 - rect->y1;
  }
  
  pdfio_matrix_t M;
  matrix_set_identity(M);
  pdfio_matrix_t tmp;
  if (get_pdf_matrix(appearance_N_dict, "Matrix", tmp)) 
  {
    matrix_copy(M, tmp);
  }

  int do_rotate = (page_rotate != 0) && (flags & an_no_rotate);
  if (do_rotate) 
  {
    pdfio_matrix_t R;
    matrix_rotatex90(R, page_rotate);
    pdfio_matrix_t MR;
    matrix_concat(MR, R, M);
    matrix_copy(M, MR);

    double rw = rect->x2 - rect->x1, rh = rect->y2 - rect->y1;
    pdfio_rect_t *r2 = rect;
    switch (((page_rotate % 360) + 360) % 360) 
    {
      case 90:
        r2->x1 = rect->x1;       
	r2->y1 = rect->y2;
        r2->x2 = rect->x1 + rh;  
	r2->y2 = rect->y2 + rw;
        break;
      case 180:
        r2->x1 = rect->x1 - rw;  
	r2->y1 = rect->y2;
        r2->x2 = rect->x1;       
	r2->y2 = rect->y2 + rh;
        break;
      case 270:
        r2->x1 = rect->x1 - rh;  
	r2->y1 = rect->y2 - rw;
        r2->x2 = rect->x1;       
	r2->y2 = rect->y2;
        break;
      default:
        break;
    }
    rect = r2;
  }

  pdfio_rect_t T = matrix_transform_rect(M, BBox);
  double Tw = T.x2 - T.x1, Th = T.y2 - T.y1;
  if (Tw == 0.0 || Th == 0.0) 
    return NULL;

  double rw = rect->x2 - rect->x1, rh = rect->y2 - rect->y1;

  pdfio_matrix_t AA, t, s;
  matrix_set_identity(AA);
  
  matrix_translate(t, rect->x1, rect->y1);
  matrix_concat(AA, t, AA);

  matrix_scale(s, rw / Tw, rh / Th);
  matrix_concat(AA, s, AA);

  matrix_translate(t, -T.x1, -T.y1);
  matrix_concat(AA, t, AA);

  if (do_rotate) 
  {
    pdfio_matrix_t R;
    matrix_rotatex90(R, page_rotate);
    matrix_concat(AA, R, AA);
  }

  char *AA_s = matrix_unparse(AA);
  if (!AA_s) return NULL;

  const char *fmt = "q %s cm %s Do Q\n"; 

  size_t need = strlen(fmt) + strlen(AA_s) + strlen(name) + 1;
  char *out = (char*)malloc(need);
  if (!out) 
  {  
    free(AA_s);
    return NULL; 
  }

  snprintf(out, need, fmt, AA_s, name);
  free(AA_s);
  return out;
}

void 
merge_resources(pdfio_dict_t *dest, 
		pdfio_dict_t *source)
{
  pdfioDictIterateKeys(source, (pdfio_dict_cb_t)merge_resources_cb, dest);
}

bool
extractFontDetails(const char *da,
                   char *font_key, size_t keylen,
                   double *font_size)
{
  if (!da || !font_key || keylen == 0 || !font_size)
    return false;

  *font_size = 10.0;

  const char *p = da;
  char last_name[128] = {0};
  bool found_key = false;

  while (*p)
  {
    while (*p && isspace((unsigned char)*p))
      p++;

    if (*p == '/')
    {
      p++;
      const char *start = p;
      while (*p && !isspace((unsigned char)*p))
        p++;
      size_t n = (size_t)(p - start);
      if (n > 0 && n < sizeof(last_name))
      {
        memcpy(last_name, start, n);
       	last_name[n] = '\0';
      }
      continue;
    }
   
    const char *op_start = p;
    while (*p && !isspace((unsigned char)*p))
      p++;
   
    if ((p - op_start) == 2 && op_start[0] == 'T' && op_start[1] == 'f')
    {
      if (last_name[0])
      {
        strncpy(font_key, last_name, keylen - 1);
       	font_key[keylen - 1] = '\0';
       	found_key = true;
       
        const char *num_start = op_start - 1;
        while (num_start > da && isspace((unsigned char)*num_start))
        {
          num_start--;
        }
        const char *num_end = num_start + 1;
        while (num_start > da && (isdigit((unsigned char)*num_start) || *num_start == '.'))
       	{
          num_start--;
        }
        if (num_start != num_end)
        {
          *font_size = atof(num_start);
        }
        return true;
      }
    }
  }
  return found_key;
}

static void
flatten_pdf(xform_prepare_t *p,			// I - Preparation data
	    xform_page_ext_t *outpage,          // I - Output page
	    size_t 	    pg,			// Page number
	    int 	    required_flags,	//Required_flag
	    int		    forbidden_flags)	//forbidden_flag
{
  pdfio_dict_t	*idict;			// Input page dictionary
  pdfio_array_t *annotsArray;		// Input page Annotation Array
  size_t	i,			// Looping var
		count;			// Number of input page streams
  pdfio_stream_t *st;			// Input page stream
  char		buffer[65536];		// Copy buffer
  ssize_t	bytes;			// Bytes read
  int		next_fx=1,		// Image number for converted Annot Objects
		rotate_val=0,		// Page Rotate value
		noAppearanceobjectCount;// Number of Annot objects with no Appearance(e.g. Btn with state /off)
					
  idict = pdfioObjGetDict(outpage->input[pg]); 
  
  annotsArray = pdfioDictGetArray(idict, "Annots"); 
  rotate_val = (int)pdfioDictGetNumber(idict, "Rotate"); 
  count = pdfioArrayGetSize(annotsArray); 
  
  p->annotation_contents = (char**)malloc(count * sizeof(char*)); 
  
  int* noAppearanceobjectIndex = (int *)malloc(count * sizeof(int)); 
  noAppearanceobjectCount = 0; 
  
  for(i = 0; i<count; i++) 
  { 
    pdfio_obj_t	  *Annot_obj, 		// Annot object
   		  *N_object; 		// Appearance N-Object
    pdfio_dict_t  *Annot_dict,		// Annot Dictionary
    		  *Appearance_dict,	// Appearance_dict
		  *N_object_dict; 	// Appearance N-Dict
    pdfio_stream_t *N_stream;		// Appearance N-Stream
    bool 	  is_widget,		// is the Annot a Widget?
		  process;		// does the annotation require Appearances?
    size_t 	  obj_no;		// what Annot number are we on?

    Annot_obj = pdfioArrayGetObj(annotsArray, i); 
    obj_no = pdfioObjGetNumber(Annot_obj); 
    fprintf(stderr, "DEBUG: Opening field stream %u/%u... with obj number %u\n", (unsigned)i + 1, (unsigned)count, (unsigned)obj_no); 

    Annot_dict = pdfioObjGetDict(Annot_obj); 
    Appearance_dict = pdfioDictGetDict(Annot_dict, "AP"); 
    N_object = getAppearance(Annot_dict, "N"); 

    N_object_dict = pdfioObjGetDict(N_object);
    N_stream = pdfioObjOpenStream(N_object, true);
    is_widget = (pdfioObjGetSubtype(Annot_obj) &&
                 strcmp(pdfioObjGetSubtype(Annot_obj), "/Widget") == 0);

    process = true;
    if(p->needAppearances && is_widget) 
    { 
      fprintf(stderr, "skip widget need appearances\n"); 
      process = false; 
    } 
    
    if(process && N_stream) 
    { 
      char 		*content;

      if(is_widget) 
      { 
	pdfio_obj_t 	*as_resources_obj;	// Appearance State Object
	pdfio_dict_t 	*as_resources_dict,	// Appearance State Resource Dictionary
			*catalog,		// PDF Catalog Dictionary
			*acroform,		// Acroform Dictionary
			*acroform_dr;		// Acroform Default Resources Dictionary

        fprintf(stderr, "DEBUG: Merge DR\n");
       	as_resources_obj = pdfioDictGetObj(N_object_dict, "Resources");
       	as_resources_dict = pdfioObjGetDict(as_resources_obj);
       	if (pdfioDictGetType(N_object_dict, "Resources") == PDFIO_VALTYPE_INDIRECT) 
	{ 
	  pdfio_dict_t *new_resources = pdfioDictCreate(outpage->pdf);
	  merge_resources(new_resources, as_resources_dict);
	  pdfioDictSetDict(outpage->pagedict, "Resources", new_resources);
	  as_resources_dict = new_resources;
	} 

	catalog = pdfioFileGetCatalog(p->inpdf);
	acroform = pdfioDictGetDict(catalog, "AcroForm");
	acroform_dr = acroform ? pdfioDictGetDict(acroform, "DR") : NULL;
	if (acroform_dr) 
	{ 
	  merge_resources(as_resources_dict, acroform_dr);
	} 
      } 
      else 
      { 
        fprintf(stderr, "DEBUG: Non-widget Annotation\n");
      } 
      
      char *name = (char *)malloc(sizeof(char) * 32);
      snprintf(name, 32, "/Fxo%d", next_fx);
      content = pdfio_annotation_get_content(Annot_obj, name, rotate_val, 
		      			     forbidden_flags, required_flags);
      fprintf(stderr, "%s\n", content);
      
      if(content && content[0] != '\0') 
      { 
	pdfio_obj_t 	*form_xobj;
	pdfio_dict_t 	*page_resources,	// Page Resources Dict
			*xobj_dict,		// Page XObject Dict
			*form_xobj_dict,	// New Form Xobject Dict(For New PDF)
			*resources_to_copy;	// temp file used for copying objects 
						// from old PDF to new PDF
	pdfio_array_t 	*bbox,			// BBox Array 
			*matrix;		// Matrix Array
	pdfio_valtype_t resources_type;		// Page Resource Type(indirect obj, ...)
	const char 	*field_type;		// Annot Object type(Tx, Ch, Btn, ...)

      	p->annotation_contents[i-noAppearanceobjectCount] = content;
	page_resources = pdfioDictGetDict(outpage->pagedict, "Resources");

	if (!page_resources) 
	{
	  page_resources = pdfioDictCreate(outpage->pdf); 
	  pdfioDictSetDict(outpage->pagedict, "Resources", page_resources);
	} 

	xobj_dict = pdfioDictGetDict(page_resources, "XObject");
	if (!xobj_dict) 
	{ 
	  xobj_dict = pdfioDictCreate(outpage->pdf);
	  pdfioDictSetDict(page_resources, "XObject", xobj_dict); 
	} 
	
	if (!N_object_dict) 
	{ 
	  fprintf(stderr, "ERROR: Annotation appearance object is missing its dictionary.\n"); 
	  continue;
	} 
	
	form_xobj_dict = pdfioDictCreate(outpage->pdf);
	pdfioDictSetName(form_xobj_dict, "Type", "XObject");
	pdfioDictSetName(form_xobj_dict, "Subtype", "Form");
	  
	bbox = pdfioDictGetArray(N_object_dict, "BBox");
	if (bbox) 
	{ 
	  pdfioDictSetArray(form_xobj_dict, "BBox", pdfioArrayCopy(outpage->pdf, bbox));
	} 
	else 
	{ 
	  fprintf(stderr, "WARNING: Appearance stream is missing required /BBox.\n");
	  pdfio_rect_t rect; 
	    
	  if (pdfioDictGetRect(Annot_dict, "Rect", &rect)) 
	  { 
	    pdfioDictSetRect(form_xobj_dict, "BBox", &rect);
	  } 
	} 
	    
	matrix = pdfioDictGetArray(N_object_dict, "Matrix");
	if (matrix) 
	{ 
	  pdfioDictSetArray(form_xobj_dict, "Matrix", pdfioArrayCopy(outpage->pdf, matrix));
	} 
	resources_type = pdfioDictGetType(N_object_dict, "Resources");
	resources_to_copy = NULL;
	if (resources_type == PDFIO_VALTYPE_INDIRECT) 
	{ 
	  pdfio_obj_t *indirect_resource_obj = pdfioDictGetObj(N_object_dict, "Resources");
	  if (indirect_resource_obj) 
	  { 
	    resources_to_copy = pdfioObjGetDict(indirect_resource_obj);
	  } 
	} 
	else if (resources_type == PDFIO_VALTYPE_DICT) 
	{ 
	  resources_to_copy = pdfioDictGetDict(N_object_dict, "Resources");
	} 
	if (resources_to_copy != NULL) 
	{ 
	  pdfioDictSetDict(form_xobj_dict, "Resources", pdfioDictCopy(outpage->pdf, resources_to_copy));
	} 
	   
	form_xobj = pdfioFileCreateObj(outpage->pdf, form_xobj_dict);
	
	field_type = pdfioDictGetName(Annot_dict, "FT");
	if (field_type && (strcmp(field_type, "Tx") == 0 || strcmp(field_type, "Ch") == 0))
	{ 
	  const char 	*field_value, 		// Annot Object Value
			*da_string;		// Default Appearance string

	  field_value = pdfioDictGetName(Annot_dict, "V");
	  if (!field_value) 
	    field_value = pdfioDictGetString(Annot_dict, "V");

	  da_string = pdfioDictGetName(Annot_dict, "DA");
	  if (!da_string) 
	    da_string = pdfioDictGetString(Annot_dict, "DA");

	  if (field_value && da_string) 
	  { 
	    pdfio_stream_t *dst_stream = pdfioObjCreateStream(form_xobj, PDFIO_FILTER_NONE);
	    if (dst_stream) 
	    { 
	      pdfio_rect_t bbox;
	      pdfioDictGetRect(form_xobj_dict, "BBox", &bbox);
	      double x = bbox.x1 + 2;
	      double y = bbox.y1 + (bbox.y2 - bbox.y1) / 4.0;
	      pdfioStreamPuts(dst_stream, "BT\n"); 
	      pdfioStreamPrintf(dst_stream, "%s\n", da_string);
	      pdfioStreamPrintf(dst_stream, "%.2f %.2f Td\n", x, y);
	      pdfioStreamPrintf(dst_stream, "(%s) Tj\n", field_value);
	      pdfioStreamPrintf(dst_stream, "ET\n");
	      pdfioStreamClose(dst_stream);
	    } 
	  } 
        }
        else 
        {
	  size_t obj_num = pdfioObjGetNumber(N_object);
	  pdfio_dict_t *obj_dict = pdfioObjGetDict(N_object);
	  fprintf(stderr, "DEBUG: Appearance object (N_object) is number %zu.\n", obj_num);
	 
	  if (!obj_dict || !pdfioDictGetType(obj_dict, "Length"))
	  { 
	    fprintf(stderr, "ERROR: Object %zu is not a valid stream object. It is missing its dictionary or the required /Length key.\n", obj_num);
	  }
	  else
	  { 
	    if (!N_stream)
	    {
              fprintf(stderr, "ERROR: pdfioObjOpenStream failed for object %zu. The stream might be malformed or encrypted.\n", obj_num);
            }
	    else
            {
              pdfio_stream_t *dst_stream = pdfioObjCreateStream(form_xobj, PDFIO_FILTER_NONE);
              if (!dst_stream)
              {
                  fprintf(stderr, "ERROR: Failed to create destination stream for Form XObject.\n");
              }
              else
              {
	          pdfio_stream_t *src_stream = N_stream;
                  fprintf(stderr, "DEBUG: Successfully opened source and destination streams. Copying content for object %zu...\n", obj_num);
                  char buffer[4096];
                  ssize_t bytes;
                  size_t total_bytes = 0;
                  while ((bytes = pdfioStreamRead(src_stream, buffer, sizeof(buffer))) > 0)
                  {
                      pdfioStreamWrite(dst_stream, buffer, (size_t)bytes);
                      total_bytes += bytes;
                  }
                  fprintf(stderr, "DEBUG: Copied %zu bytes from appearance stream.\n", total_bytes);
                  pdfioStreamClose(dst_stream);
              }
	    }
	  }
        } 
	
	pdfioDictSetObj(xobj_dict, pdfioStringCreatef(outpage->pdf, "Fxo%u", (unsigned)next_fx), form_xobj);
        next_fx++; 
      }
    } 
    else if(process && Appearance_dict) 
    { 
      // If an annotation has no selected appearance stream, just drop the annotation when 
      // flattening. This can happen for unchecked checkboxes and radio buttons, popup windows 
      // associated with comments that aren't visible, and other types of annotations that 
      // aren't visible. Annotations that have no appearance streams at all, such as Link, 
      // Popup, and Projection, should be preserved. 
      fprintf(stderr, "ignore annotation with no appearance\n");
      noAppearanceobjectCount++;
    } 
    else 
    { 
      char *name = (char *)malloc(sizeof(char) * 32);
      snprintf(name, 32, "/Fxo%d", next_fx);

      pdfio_dict_t *page_resources = pdfioDictGetDict(outpage->pagedict, "Resources");
      if (!page_resources) 
      {
	page_resources = pdfioDictCreate(outpage->pdf); 
	pdfioDictSetDict(outpage->pagedict, "Resources", page_resources);
      } 
      pdfio_dict_t *xobj_dict = pdfioDictGetDict(page_resources, "XObject");
      if (!xobj_dict) 
      { 
	xobj_dict = pdfioDictCreate(outpage->pdf);
       	pdfioDictSetDict(page_resources, "XObject", xobj_dict); 
      }
      
      pdfio_array_t *procset = pdfioArrayCreate(outpage->pdf);
      pdfioArrayAppendName(procset, "PDF");   // adds /PDF
      pdfioArrayAppendName(procset, "Text");  // adds /Text

      pdfio_dict_t *resources = pdfioDictCreate(outpage->pdf);
      pdfioDictSetArray(resources, "ProcSet", procset);

      pdfio_dict_t *form_xobj_dict = pdfioDictCreate(outpage->pdf);
      pdfioDictSetName(form_xobj_dict, "Type", "XObject");
      pdfioDictSetName(form_xobj_dict, "Subtype", "Form");

      char *content = special_pdfio_annotation_get_content(Annot_obj, name, rotate_val, forbidden_flags, required_flags);
      p->annotation_contents[i-noAppearanceobjectCount] = content;

      pdfio_array_t *bbox = pdfioDictGetArray(Annot_dict, "BBox");
      if (bbox) 
      { 
        pdfioDictSetArray(form_xobj_dict, "BBox", pdfioArrayCopy(outpage->pdf, bbox));
      } 
      else 
      { 
	fprintf(stderr, "WARNING: Appearance stream is missing required /BBox.\n");
	pdfio_rect_t rect; 
	    
	if (pdfioDictGetRect(Annot_dict, "Rect", &rect)) 
	{ 
	  pdfio_rect_t Bbox;
	  Bbox.x1 = 0;
	  Bbox.y1 = 0;
	  Bbox.x2 = rect.x2 - rect.x1;
	  Bbox.y2 = rect.y2 - rect.y1;
	  pdfioDictSetRect(form_xobj_dict, "BBox", &Bbox);
	} 
      }
       
      pdfio_obj_t *form_xobj = pdfioFileCreateObj(outpage->pdf, form_xobj_dict);
      
      const char *field_type = pdfioDictGetName(Annot_dict, "FT");
      if (field_type && (strcmp(field_type, "Tx") == 0 || strcmp(field_type, "Ch") == 0)) 
      { 
     	const char *field_value = pdfioDictGetName(Annot_dict, "V");
	if (!field_value) field_value = pdfioDictGetString(Annot_dict, "V");

	const char *da_string = pdfioDictGetName(Annot_dict, "DA");
	if (!da_string) da_string = pdfioDictGetString(Annot_dict, "DA");

	char font_key[64];
	double font_size = 10.0;
	if (extractFontDetails(da_string, font_key, sizeof(font_key), &font_size))
       	{
	  pdfio_obj_t *font_obj = NULL;
	  pdfio_dict_t *page_resource_dict = pdfioDictGetDict(outpage->pagedict, "Resources");
	  pdfio_dict_t *font_dict = page_resource_dict ? pdfioDictGetDict(page_resource_dict, "Font") : NULL;
	 
	  if (font_dict)
	  {
            font_obj = pdfioDictGetObj(font_dict, font_key);
	  } 
	 
	  // If font_obj is not found in the page's resources, check the AcroForm's /DR.
    	  if (!font_obj)
	  {
            pdfio_dict_t *catalog = pdfioFileGetCatalog(p->inpdf);
	    pdfio_dict_t *acroform = catalog ? pdfioDictGetDict(catalog, "AcroForm") : NULL;
	    pdfio_dict_t *acroform_dr = acroform ? pdfioDictGetDict(acroform, "DR") : NULL;
	    pdfio_dict_t *acroform_font_dict = acroform_dr ? pdfioDictGetDict(acroform_dr, "Font") : NULL;
	    if (acroform_font_dict)
	    {
              font_obj = pdfioDictGetObj(acroform_font_dict, font_key);
	    }
	  }
	  if (font_obj)
	  {
	    // 1. Create the dedicated sub-dictionary for fonts.
    	    pdfio_dict_t *font_dict = pdfioDictCreate(outpage->pdf); 
	    pdfioDictSetObj(font_dict, font_key, font_obj); 
	    pdfioDictSetDict(resources, "Font", font_dict);
	    pdfioDictSetDict(form_xobj_dict, "Resources", resources);
	   
	    fprintf(stderr, "SUCCESS: Font /%s correctly nested in /Resources /Font dictionary.\\n", font_key); 
	  } 
	  else
	  {
	    fprintf(stderr, "ERROR: Font %s not found in page or AcroForm resources.\\n", font_key);
	  }
       	}

	if (field_value && da_string) 
	{ 
	  pdfio_stream_t *dst_stream = pdfioObjCreateStream(form_xobj, PDFIO_FILTER_NONE);
	  if (dst_stream)
	  {
            pdfio_rect_t bbox;
            if (pdfioDictGetRect(form_xobj_dict, "BBox", &bbox))
	    {
	      double field_height = bbox.y2 - bbox.y1;
	      double x = 2.0;
	      double y = (field_height / 2.0) - (font_size * 0.35);

	      pdfioStreamPuts(dst_stream, "BT\n");
	      pdfioStreamPrintf(dst_stream, "%s\n", da_string);       // Assuming da_string is safe
	      pdfioStreamPrintf(dst_stream, "%.2f %.2f Td\n", x, y);
	      pdfioStreamPrintf(dst_stream, "(%s) Tj\n", field_value);  // The critical fix
	      pdfioStreamPuts(dst_stream, "ET\n");

	    }
	    pdfioStreamClose(dst_stream);
	  }
	} 
      }
      
      pdfioDictSetObj(xobj_dict, pdfioStringCreatef(outpage->pdf, "Fxo%u", (unsigned)next_fx), form_xobj);
      next_fx++; 

      //Annot_dict
      free(name);

      fprintf(stderr, "DEBUG: special case ignore annotation with no appearance\n");
      noAppearanceobjectIndex[noAppearanceobjectCount] = i;
    } 
    if(N_stream)
      pdfioStreamClose(N_stream);
  } 
  p->num_annotations = count-noAppearanceobjectCount;
 
  // Start output page content
  if ((outpage->output = pdfioFileCreatePage(outpage->pdf, outpage->pagedict)) != NULL)
  {
    if (p->use_duplex_xform && !(pdfioFileGetNumPages(outpage->pdf) & 1))
    {
      pdfioContentSave(outpage->output);
      pdfioContentMatrixConcat(outpage->output, p->duplex_xform);
    }
  }
  else 
  {
    fprintf(stderr, "ERROR: not able to create output page stream\n");
    return;
  }
  pdfioContentSave(outpage->output);

  // Copy content streams...
  for (i = 0, count = pdfioPageGetNumStreams(outpage->input[pg]); i < count; i ++)
  {
    fprintf(stderr, "DEBUG: Opening content stream %u/%u...\n", (unsigned)i + 1, (unsigned)count);

    if ((st = pdfioPageOpenStream(outpage->input[pg], i, true)) != NULL)
    {
      fprintf(stderr, "DEBUG: Opened stream %u, resmap[%u]=%p\n", (unsigned)i + 1, (unsigned)pg, (void *)outpage->resmap[pg]);
      while ((bytes = pdfioStreamRead(st, buffer, sizeof(buffer))) > 0)
      {
        pdfioStreamWrite(outpage->output, buffer, (size_t)bytes);
      }
    }
    pdfioStreamClose(st);
  }


  if (p->num_annotations > 0)
  {
    pdfioStreamPuts(outpage->output, "\n");
    for (i = 0; i < p->num_annotations; i++)
    {
      if (p->annotation_contents[i])
      {
	pdfioContentSave(outpage->output);
       	pdfioStreamPuts(outpage->output, p->annotation_contents[i]);
	pdfioContentRestore(outpage->output);
      }
    }
  }

  // Restore state...
  pdfioStreamPuts(outpage->output, "\n");
  pdfioContentRestore(outpage->output);
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

  if (strcmp(p->options->page_border, "none"))
  {
    pdfioContentSetStrokeColorDeviceGray(outpage->output, 0.0);
    if (!strcmp(p->options->page_border, "single-thick"))
      pdfioContentSetLineWidth(outpage->output, 2.0);
    else if (!strcmp(p->options->page_border, "double"))
    {
      pdfioContentSetLineWidth(outpage->output, 0.5);
      pdfioContentPathRect(outpage->output, cell->x1 + 2, cell->y1 + 2, cell->x2 - cell->x1 - 4, cell->y2 - cell->y1 - 4);
    }
    else if (!strcmp(p->options->page_border, "double-thick"))
    {
      pdfioContentSetLineWidth(outpage->output, 2.0);
      pdfioContentPathRect(outpage->output, cell->x1 + 4, cell->y1 + 4, cell->x2 - cell->x1 - 8, cell->y2 - cell->y1 - 8);
    }
    else // single
      pdfioContentSetLineWidth(outpage->output, 1.0);

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
  if (p->options->print_scaling == CF_FILTER_SCALING_FILL)
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


  if (p->options->mirror)
  {
    pdfio_matrix_t mirror_matrix = { {-1.0, 0.0}, {0.0, 1.0}, {p->media.x2, 0.0} };
    pdfioContentMatrixConcat(outpage->output, mirror_matrix);
  }

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
	      bufptr++;
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
    cf_filter_options_t  *options,	// I - IPP options
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
  pdfio_file_t *flattened_pdf;
  char          flattened_pdf_file[1024];

  // Initialize data for preparing input files for transform...
  memset(&p, 0, sizeof(p));
  p.has_form = false;
  p.has_annotations = false;
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

    if ((d->pdf = pdfioFileOpen(d->pdf_filename, pdfio_password_cb, &document, pdfio_error_cb, &p)) == NULL)
      goto done;

    p.inpdf = d->pdf;

    //code for figuring if the File has AcroForm/Annotations
    pdfio_dict_t *catalogDict = pdfioFileGetCatalog(d->pdf);
    if(catalogDict)
    {
      if(pdfioDictGetType(catalogDict, "AcroForm") != PDFIO_VALTYPE_NONE)
      {
	pdfio_dict_t *Acroform_Dict = pdfioDictGetDict(catalogDict, "AcroForm");
	if(pdfioDictGetBoolean(Acroform_Dict, "NeedAppearances") == true)
	{
	  p.needAppearances = true;
 	  fprintf(stderr, "DEBUG: PDF has Acroform, and NeedAppearances value is true\n");
	}
	else
	{
	  p.needAppearances = false;
 	  fprintf(stderr, "DEBUG: PDF has Acroform, and NeedAppearances value is false\n");
	}

	if(pdfioDictGetType(Acroform_Dict, "Fields") != PDFIO_VALTYPE_NONE)
	{
	  pdfio_array_t *fieldsArray = pdfioDictGetArray(Acroform_Dict, "Fields");
	  if(fieldsArray && pdfioArrayGetSize(fieldsArray) > 0)
	  {
	    p.has_form = true;
 	    fprintf(stderr, "DEBUG: PDF contains interactive form fields\n");
	  }
	}
      }

      size_t npages = pdfioFileGetNumPages(d->pdf);
      for (size_t pg = 0; pg < npages; pg++)
      {
	pdfio_obj_t *page = pdfioFileGetPage(d->pdf, pg);
	pdfio_dict_t *page_dict = pdfioObjGetDict(page);
	pdfio_array_t *annots = pdfioDictGetArray(page_dict, "Annots");
	if (annots && pdfioArrayGetSize(annots) > 0)
	{
          fprintf(stderr, "DEBUG: page %zu: Contains annotations\n", pg+1);
          p.has_annotations = true;
       	}
      }
    }

    if (p.has_form && p.has_annotations)
    {
      if ((flattened_pdf = pdfioFileCreateTemporary(flattened_pdf_file, sizeof(flattened_pdf_file), "1.7", &p.media, &p.media, pdfio_error_cb, &p)) == NULL)
	return (false);

      xform_page_ext_t *flattened_page = (xform_page_ext_t*)malloc(sizeof(xform_page_ext_t));
      flattened_page->pdf = flattened_pdf;
      size_t npages = pdfioFileGetNumPages(d->pdf);

      flattened_page->input = (pdfio_obj_t**)malloc(npages * sizeof(pdfio_obj_t*));

      for (size_t pg = 0; pg < npages; pg++)
      {
        flattened_page->input[pg] = pdfioFileGetPage(d->pdf, (size_t)(pg));
      }

      for (size_t pg = 0; pg < npages; pg++)
      {
	pdfio_dict_t	*pagedict,	// Page dictionary
			*resdict;	// Resources dictionary
	pdfio_obj_t	*resobj;	// Resources object

	flattened_page->pagedict = pdfioDictCreate(flattened_pdf);
	flattened_page->resdict  = pdfioDictCreate(flattened_pdf);

	pdfioDictSetRect(flattened_page->pagedict, "CropBox", &p.media);
	pdfioDictSetRect(flattened_page->pagedict, "MediaBox", &p.media);
	pdfioDictSetDict(flattened_page->pagedict, "Resources", flattened_page->resdict);
	pdfioDictSetName(flattened_page->pagedict, "Type", "Page");

        if (!flattened_page->input[pg])
	  continue;
       
       	flattened_page->restype = NULL;
       
	pagedict = pdfioObjGetDict(flattened_page->input[pg]);
       	if ((resdict = pdfioDictGetDict(pagedict, "Resources")) != NULL)
	    pdfioDictIterateKeys(resdict, (pdfio_dict_cb_t)page_ext_dict_cb, flattened_page);
        else if ((resobj = pdfioDictGetObj(pagedict, "Resources")) != NULL)
	    pdfioDictIterateKeys(pdfioObjGetDict(resobj), (pdfio_dict_cb_t)page_ext_dict_cb, flattened_page);
        else if (Verbosity)
          fprintf(stderr, "DEBUG: No Resources for cell %u.\n", (unsigned)layout);

	flatten_pdf(&p, flattened_page, pg, an_print, 0);

	pdfio_end_page(&p, flattened_page->output);
      }

      pdfioFileClose(d->pdf);
      pdfioFileClose(flattened_pdf);

      if ((d->pdf = pdfioFileOpen(flattened_pdf_file, NULL, &documents, pdfio_error_cb, &p)) == NULL)
      goto done;
      p.inpdf = d->pdf;
    }

    if (options->multiple_document_handling < CF_FILTER_HANDLING_SINGLE_DOCUMENT)
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
      if (options->multiple_document_handling < CF_FILTER_HANDLING_SINGLE_DOCUMENT)
      {
        if (cfFilterOptionsIsPageInRange(options, page - d->first_page + 1))
          d->num_pages ++;
      }
      else if (cfFilterOptionsIsPageInRange(options, page))
      {
        d->num_pages ++;
      }

      page ++;
    }

    if ((d->last_page & 1) && duplex && options->multiple_document_handling != CF_FILTER_HANDLING_SINGLE_DOCUMENT)
    {
      d->last_page ++;
      page ++;
    }

    if ((d->num_pages & 1) && duplex && options->multiple_document_handling != CF_FILTER_HANDLING_SINGLE_DOCUMENT)
      d->num_pages ++;

    p.num_inpages += d->num_pages;
  }

  // When doing N-up or booklet printing, the default is to scale to fit unless
  // fill is explicitly chosen...
  if (p.num_layout > 1 && options->print_scaling != CF_FILTER_SCALING_FILL)
    options->print_scaling = CF_FILTER_SCALING_FIT;

  // Prepare output pages...
  prepare_pages(&p, num_documents, documents);

  // Add job-sheets content...
  if (options->job_sheets[0] && strcmp(options->job_sheets, "none"))
    generate_job_sheets(&p);

  // Copy pages to the output file...
  for (copies = generate_copies ? options->copies : 1; copies > 0; copies --)
  {
    reverse_order = !strcmp(options->output_bin, "face-up");
    if (options->page_delivery >= CF_FILTER_DELIVERY_REVERSE_ORDER_FACE_DOWN)
      reverse_order = !reverse_order;

    if (options->reverse_order)
      reverse_order = true;

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

    if (p.num_layout == 1 && 
        options->print_scaling == CF_FILTER_SCALING_NONE && 
	strcasecmp(outformat, "image/pwg-raster") && 
	strcasecmp(outformat, "image/urf") && 
	!strcmp(options->page_border, "none") && 
	!options->mirror && 
	options->orientation_requested == CF_FILTER_ORIENT_NONE)
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
	{
	  copy_page(&p, outpage, layout);
	}

	pdfio_end_page(&p, outpage->output);
	outpage->output = NULL;
      }
    }
  }

  // Add final job-sheets content...
  if (options->job_sheets[0] && strcmp(options->job_sheets, "none"))
    generate_job_sheets(&p);

  // Add job-error-sheet content as needed...
  if (options->job_error_sheet.report == CF_FILTER_ERROR_REPORT_ALWAYS || (options->job_error_sheet.report == CF_FILTER_ERROR_REPORT_ON_ERROR && cupsArrayGetCount(p.errors) > 0))
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
      fprintf(stderr, "ERROR: write to temporary file failed");
      return -1;
    }
  }

  if (bytes_read == -1)
  {
    fprintf(stderr, "ERROR: Read from inputfd failed");
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
  cf_logfunc_t       log = data->logfunc;
  void               *ld = data->logdata;

  // New variables from prepare_documents
  // confirmed required
  char          pdf_file[1024];
  const char	*output_type = "application/pdf";
  cf_filter_options_t *filter_options;
  xform_document_t file;

  //not confirmed
  const char         *sheet_back = "rotated";
  unsigned           pdf_pages;

  memset(&file, 0, sizeof(file));

  filter_options = cfFilterOptionsCreate(data->num_options, data->options);

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
        fprintf(stderr, "ERROR: Failed to copy inputfd to temp file\n");
        fclose(inputfp);
        close(inputfd);
        unlink(temp_filename); // Clean up temporary file
        return 1;
    }

  rewind(inputfp); // Rewind the temp_file for reading

  file.filename = temp_filename;
  file.format = "application/pdf";
  file.pdf_filename = temp_filename;

  if (!prepare_documents(1, &file, filter_options, sheet_back, pdf_file, sizeof(pdf_file), output_type, &pdf_pages, !strcasecmp(output_type, "application/pdf")))
  {
    // Unable to prepare documents, exit...
    cfFilterOptionsDelete(filter_options);
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
