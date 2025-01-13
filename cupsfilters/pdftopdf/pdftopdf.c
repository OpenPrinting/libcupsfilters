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
#include "processor.h"

#include <stdarg.h>

static bool 
optGetInt(const char *name, 
	  int num_options, 
	  cups_option_t *options, 
	  int *ret)  // {{{
{
  const char *val = cupsGetOption(name, num_options, options);
  if (val) 
  {
    *ret = atoi(val);
    return true;
  }
  return false;
}
// }}}

static bool 
optGetFloat(const char *name, 
	    int num_options, 
	    cups_option_t *options, 
	    float *ret)  // {{{
{
  const char *val = cupsGetOption(name, num_options, options);
  if (val) 
  {
    *ret = atof(val);
    return true;
  }
  return false;
}
// }}}

static bool 
is_false(const char *value) // {{{
{
  if (!value) 
    return false;
  return ((strcasecmp(value, "no") == 0) || 
	  (strcasecmp(value, "off") == 0) || 
	  (strcasecmp(value, "false") == 0));
}
// }}}

static bool
is_true(const char *value) // {{{
{
  if (!value) 
    return false;
  return ((strcasecmp(value, "yes") == 0) || 
	  (strcasecmp(value, "on") == 0) || 
	  (strcasecmp(value, "true") == 0));
}
// }}}

static bool 
parsePosition(const char *value, 
	      pdftopdf_position_e *xpos, 
	      pdftopdf_position_e *ypos) // {{{
{
  // ['center','top','left','right','top-left','top-right','bottom',
  //  'bottom-left','bottom-right']
  *xpos = CENTER;
  *ypos = CENTER;
  int next = 0;

  if (strcasecmp(value, "center") == 0) 
    return true;
  else if (strncasecmp(value, "top", 3) == 0) 
  {
    *ypos = TOP;
    next = 3;
  } 
  else if (strncasecmp(value, "bottom", 6) == 0) {
    *ypos = BOTTOM;
    next = 6;
  }

  if (next) 
  {
    if (value[next] == 0) 
      return true;
    else if (value[next] != '-')
      return false;
    value += next + 1;
  }
  if (strcasecmp(value, "left") == 0) 
    *xpos = LEFT;
  else if (strcasecmp(value, "right") == 0) 
    *xpos = RIGHT;
  else
    return false;

  return true;
}
// }}}

static void 
parseRanges(const char *range, _cfPDFToPDFIntervalSet *ret) // {{{
{
  _cfPDFToPDFIntervalSet_clear(ret);
  
  if (!range) 
  {
    _cfPDFToPDFIntervalSet_add(ret, 1, 1);  
    _cfPDFToPDFIntervalSet_finish(ret);
    return;
  }

  int lower, upper;
  while (*range) 
  {
    if (*range == '-') 
    {
      range++;
      upper = strtol(range, (char **)&range, 10);
      if (upper >= 2147483647) 
        _cfPDFToPDFIntervalSet_add(ret, 1, 1);  
      else 
        _cfPDFToPDFIntervalSet_add(ret, 1, upper+1);  
    } 
    else 
    {
      lower = strtol(range, (char **)&range, 10);
      if (*range == '-') 
      { 
	range++;
       	if (!isdigit(*range)) 
          _cfPDFToPDFIntervalSet_add(ret, lower, lower);  
	else 
	{
	  upper = strtol(range, (char **)&range, 10);
	  if (upper >= 2147483647) 
            _cfPDFToPDFIntervalSet_add(ret, lower, lower);  
	  else 
            _cfPDFToPDFIntervalSet_add(ret, lower, upper+1);  
        }
      } 
      else 
      {
        _cfPDFToPDFIntervalSet_add(ret, lower, lower+1);  
      }
    }
    if (*range != ',') 
      break;
    range++;
  }
  _cfPDFToPDFIntervalSet_finish(ret);
}
// }}}

static bool 
_cfPDFToPDFParseBorder(const char *val, 
		       pdftopdf_border_type_e *ret) // {{{
{
  if (strcasecmp(val, "none") == 0) 
    *ret = NONE;
  else if (strcasecmp(val, "single") == 0) 
    *ret = ONE_THIN;
  else if (strcasecmp(val, "single-thick") == 0)
    *ret = ONE_THICK;
  else if (strcasecmp(val, "double") == 0) 
    *ret = TWO_THIN;
  else if (strcasecmp(val, "double-thick") == 0) 
    *ret = TWO_THICK;
  else 
    return false;
  return true;
}
// }}}

void 
getParameters(cf_filter_data_t *data, 
	      int num_options, 
	      cups_option_t *options, 
	      _cfPDFToPDFProcessingParameters *param, 
	      pdftopdf_doc_t *doc) // {{{
{
  char *final_content_type = data->final_content_type;
  ipp_t *printer_attrs = data->printer_attrs;
  ipp_t *job_attrs = data->job_attrs;
  ipp_attribute_t *attr;
  const char *val;
  int ipprot;
  int nup;
  char rawlabel[256];
  char *classification;
  char cookedlabel[256];

  if ((val = cupsGetOption("copies", num_options, options)) != NULL ||
      (val = cupsGetOption("Copies", num_options, options)) != NULL ||
      (val = cupsGetOption("num-copies", num_options, options)) != NULL ||
      (val = cupsGetOption("NumCopies", num_options, options)) != NULL) 
  {
    int copies = atoi(val);
    if (copies > 0) 
      param->num_copies = copies;
  }

  if (param->num_copies == 0) 
    param->num_copies = 1;

  if (printer_attrs != NULL &&
      (attr = ippFindAttribute(printer_attrs, 
			       "landscape-orientation-requested-preferred", 
			       IPP_TAG_ZERO)) != NULL &&
      ippGetInteger(attr, 0) == 5) 
    param->normal_landscape = ROT_270;
  else 
    param->normal_landscape = ROT_90;

  param->orientation = ROT_0;
  param->no_orientation = false;
  if (optGetInt("orientation-requested", num_options, options, &ipprot)) 
  {
    // IPP orientation values are:
    //   3: 0 degrees,  4: 90 degrees,  5: -90 degrees,  6: 180 degrees
    
    if ((ipprot < 3) || (ipprot > 6)) 
    {
      if (ipprot && doc->logfunc)
        doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR, 
		     "cfFilterPDFToPDF: Bad value (%d) for "
		     "orientation-requested, using 0 degrees", 
		     ipprot);
      param->no_orientation = true;
    } 
    else 
    {
      static const pdftopdf_rotation_e 
	ipp2rot[4] = {ROT_0, ROT_90, ROT_270, ROT_180};
      param->orientation = ipp2rot[ipprot - 3]; 
    }
  } 
  else if ((val = cupsGetOption("landscape", num_options, options)) != NULL) 
  {
    if (!is_false(val)) 
      param->orientation = param->normal_landscape;
  } 
  else
    param->no_orientation = true;
   
  param->pagesize_requested = 
    (cfGetPageDimensions(printer_attrs, job_attrs, num_options, options, NULL, 
			 0,
			 &(param->page->width), &(param->page->height), 
			 &(param->page->left), &(param->page->bottom), 
			 &(param->page->right), &(param->page->top), 
			 NULL, NULL) >= 1);
   
  cfSetPageDimensionsToDefault(&(param->page->width), &(param->page->height), 
		  	       &(param->page->left), &(param->page->bottom), 
			       &(param->page->right), &(param->page->top), 
			       doc->logfunc, doc->logdata);

  param->page->right = param->page->width - param->page->right;
  param->page->top = param->page->height - param->page->top;

  param->paper_is_landscape = (param->page->width > param->page->height);

  _cfPDFToPDFPageRect *tmp = (_cfPDFToPDFPageRect *)malloc(sizeof(_cfPDFToPDFPageRect)); 
  _cfPDFToPDFPageRect_init(tmp);

  optGetFloat("page-top", num_options, options, &tmp->top);
  optGetFloat("page-left", num_options, options, &tmp->left);
  optGetFloat("page-right", num_options, options, &tmp->right);
  optGetFloat("page-bottom", num_options, options, &tmp->bottom);

  if ((val = cupsGetOption("media-top-margin", num_options, options)) 
      != NULL)
    tmp->top = atof(val) * 72.0 / 2540.0;
  if ((val = cupsGetOption("media-left-margin", num_options, options)) 
      != NULL) 
    tmp->left = atof(val) * 72.0 / 2540.0;
  if ((val = cupsGetOption("media-right-margin", num_options, options)) 
      != NULL) 
    tmp->right = atof(val) * 72.0 / 2540.0;
  if ((val = cupsGetOption("media-bottom-margin", num_options, options)) 
      != NULL) 
    tmp->bottom = atof(val) * 72.0 / 2540.0;

  if ((param->orientation == ROT_90) || (param->orientation == ROT_270)) 
  { // unrotate page
    // NaN stays NaN
    tmp->right = param->page->height - tmp->right;
    tmp->top = param->page->width - tmp->top;

    _cfPDFToPDFPageRect_rotate_move(tmp, param->orientation, param->page->height, param->page->width);
  } 
  else 
  {
    tmp->right = param->page->width - tmp->right;
    tmp->top = param->page->height - tmp->top;

    _cfPDFToPDFPageRect_rotate_move(tmp, param->orientation, param->page->width, param->page->height);
  }
  _cfPDFToPDFPageRect_set(param->page, tmp);

  if ((val = cfIPPAttrEnumValForPrinter(printer_attrs, job_attrs, "sides")) != 
      NULL && 
      strncmp(val, "two-sided-", 10) == 0)
    param->duplex = true;
  else if (is_true(cupsGetOption("Duplex", num_options, options))) 
  {
    param->duplex = true;
    param->set_duplex = true;
  } 
  else if ((val = cupsGetOption("sides", num_options, options)) != NULL) 
  {
    if ((strcasecmp(val, "two-sided-long-edge") == 0) || 
	(strcasecmp(val, "two-sided-short-edge") == 0)) 
    {
      param->duplex = true;
      param->set_duplex = true;
    } 
    else if (strcasecmp(val, "one-sided") != 0) 
    {
      if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR, 
		      		     "cfFilterPDFToPDF: Unsupported sides value %s, using sides=one-sided!", 
				     val);
    }
  }

  // default nup is 1
  nup = 1;
  if (optGetInt("number-up", num_options, options, &nup)) 
  {
    if (!_cfPDFToPDFNupParameters_possible(nup)) 
    {
      if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR, 
		      		     "cfFilterPDFToPDF: Unsupported number-up value %d, using number-up=1!", 
				     nup);
      nup = 1;
    }
    _cfPDFToPDFNupParameters_preset(nup, param->nup);
  }

  if ((val = cupsGetOption("number-up-layout", num_options, options)) != NULL) 
  {
    if (!_cfPDFToPDFParseNupLayout(val, param->nup)) 
    {
      if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR, 
		       		     "cfFilterPDFToPDF: Unsupported number-up-layout %s, using number-up-layout=lrtb!",
				     val);
      param->nup->first = X;
      param->nup->xstart = LEFT;
      param->nup->ystart = TOP;
    }
  }

  if ((val = cupsGetOption("page-border", num_options, options)) != NULL) 
  {
    if (!_cfPDFToPDFParseBorder(val, &(param->border))) 
    {
      if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR, 
		      		     "cfFilterPDFToPDF: Unsupported page-border value %s, using page-border=none!", 
				     val);
      param->border = NONE;
    }
  }

  if ((val = cupsGetOption("OutputOrder", num_options, options)) != NULL ||
      (val = cupsGetOption("output-order", num_options, options)) != NULL ||
      (val = cupsGetOption("page-delivery", num_options, options)) != NULL) 
  {
    param->reverse = (strcasecmp(val, "Reverse") == 0 || 
		      strcasecmp(val, "reverse-order") == 0);
  } 
  else 
  {
    param->reverse = cfIPPReverseOutput(printer_attrs, job_attrs);
  }

  classification = getenv("CLASSIFICATION");
  if (classification) 
    strcpy(rawlabel, classification);

  if ((val = cupsGetOption("page-label", num_options, options)) != NULL) 
  {
    if (strlen(rawlabel) > 0) strcat(rawlabel, " - ");
    strcat(rawlabel, cupsGetOption("page-label", num_options, options));
  }

  char *rawptr = rawlabel;
  char *cookedptr = cookedlabel;
  while (*rawptr) 
  {
    if (*rawptr < 32 || *rawptr > 126) 
    {
      sprintf(cookedptr, "\\%03o", (unsigned int)*rawptr);
      cookedptr += 4;
    } 
    else 
    {
      *cookedptr++ = *rawptr;
    }
    rawptr++;
  }
  *cookedptr = '\0';
  param->page_label = strdup(cookedlabel);

  if ((val = cupsGetOption("page-set", num_options, options)) != NULL) 
  {
    if (strcasecmp(val, "even") == 0)
      param->odd_pages = false;
    else if (strcasecmp(val, "odd") == 0) 
      param->even_pages = false;
    else if (strcasecmp(val, "all") != 0) 
    {
      if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR, 
		      		     "cfFilterPDFToPDF: Unsupported page-set value %s, using page-set=all!", 
				     val);
    }
  }

  if ((val = cupsGetOption("page-ranges", num_options, options)) != NULL) 
    parseRanges(val, param->page_ranges);
  if ((val = cupsGetOption("input-page-ranges", num_options, options)) != NULL) 
    parseRanges(val, param->input_page_ranges);

  if ((val = cupsGetOption("mirror", num_options, options)) != NULL ||
      (val = cupsGetOption("mirror-print", num_options, options)) != NULL) 
    param->mirror = is_true(val);

  param->booklet = CF_PDFTOPDF_BOOKLET_OFF;
  if ((val = cupsGetOption("booklet", num_options, options)) != NULL) 
  {
    if (strcasecmp(val, "shuffle-only") == 0) 
      param->booklet = CF_PDFTOPDF_BOOKLET_JUST_SHUFFLE;
    else if (is_true(val))
      param->booklet = CF_PDFTOPDF_BOOKLET_ON;
    else if (!is_false(val)) 
    {
      if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR, 
		      		     "cfFilterPDFToPDF: Unsupported booklet value %s, using booklet=off!", 
				     val);
    }
  }
  param->book_signature = -1;
  if (optGetInt("booklet-signature", num_options, options, &(param->book_signature))) 
  {
    if (param->book_signature == 0) 
    {
      if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR, 
		      		     "cfFilterPDFToPDF: Unsupported booklet-signature value, using booklet-signature=-1 (all)!", 
				     val);
      param->book_signature = -1;
    }
  }

  if ((val = cupsGetOption("position", num_options, options)) != NULL) 
  {
    if (!parsePosition(val, &(param->xpos), &(param->ypos))) 
    {
      if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR,
		     		     "cfFilterPDFToPDF: Unrecognized position value %s, using position=center!", 
				     val);
      param->xpos = CENTER;
      param->ypos = CENTER;
    }
  }

  if (is_true(cupsGetOption("Collate", num_options, options))) 
    param->collate = true;
  else if ((val = cupsGetOption("sheet-collate", num_options, options)) != NULL) 
    param->collate = (strcasecmp(val, "uncollated") != 0);
  else if (((val = cupsGetOption("multiple-document-handling", 
				 num_options, options)) != NULL &&
	    (strcasecmp(val, "separate-documents-collated-copies") == 0 ||
             strcasecmp(val, "separate-documents-uncollated-copies") == 0 ||
             strcasecmp(val, "single-document") == 0 ||
             strcasecmp(val, "single-document-new-sheet") == 0)) ||
           (val = cfIPPAttrEnumValForPrinter(printer_attrs, job_attrs, 
					     "multiple-document-handling")) != 
	   NULL) 
  {
    param->collate = 
      (strcasecmp(val, "separate-documents-uncollated-copies") != 0);
  }

#if 0 
  if ((val = cupsGetOption("scaling", num_options, options)) != 0)
  {
    scalint = atoi(val) * 0.01;
    fitplot = true
  }
  else if (fitplot)
    scaling = 1.0;

  if ((val = cupsGetOption("natural-scaling", num_options, options)) != 0)
    naturalScaling = atoi(val) * 0.01;
#endif
  
  // Make pages a multiple of two (only considered when duplex is on).
  // i.e. printer has hardware-duplex, but needs pre-inserted filler pages
  // FIXME? pdftopdf also supports it as cmdline option (via checkFeature())
  param->even_duplex = 
    (param->duplex && 
     is_true(cupsGetOption("even-duplex", num_options, options)));

  param->auto_rotate = param->no_orientation;
  if ((val = cupsGetOption("pdftopdfAutoRotate", 
 	     		   num_options, options)) != NULL || 
      (val = cupsGetOption("pdfAutoRotate", num_options, options)) != NULL)
    param->auto_rotate = !is_false(val);

  if ((val = cupsGetOption("ipp-attribute-fidelity", num_options, options)) != 
      NULL) 
  {
    if (!strcasecmp(val, "true") || !strcasecmp(val, "yes") || 
	!strcasecmp(val, "on"))
      param->fidelity = true;
  }

  if (printer_attrs == NULL && !param->pagesize_requested && 
      param->booklet == CF_PDFTOPDF_BOOKLET_OFF && 
      param->nup->nupX == 1 && param->nup->nupY == 1)
    param->cropfit = true;

  else if ((val = cupsGetOption("print-scaling", num_options, options)) != NULL) 
  {
    // Standard IPP attribute
    if (!strcasecmp(val, "auto")) 
      param->autoprint = true;
    else if (!strcasecmp(val, "auto-fit")) 
      param->autofit = true;
    else if (!strcasecmp(val, "fill")) 
      param->fillprint = true;
    else if (!strcasecmp(val, "fit"))
      param->fitplot = true;
    else if (!strcasecmp(val, "none")) 
      param->cropfit = true;
    else 
      param->autoprint = true;
  } 
  else 
  {
    // Legacy CUPS attributes
    if ((val = cupsGetOption("fitplot", num_options, options)) == NULL) 
    {
      if ((val = cupsGetOption("fit-to-page", num_options, options)) == NULL)
        val = cupsGetOption("ipp-attribute-fidelity", num_options, options);
    }
    // TODO?  pstops checks == "true", pdftops !is_false  ... pstops says:
    // fitplot only for PS (i.e. not for PDF, cmp. cgpdftopdf)
    param->fitplot = (val && !is_false(val));

    if ((val = cupsGetOption("fill", num_options, options)) != 0) 
    {
      if (!strcasecmp(val, "true") || !strcasecmp(val, "yes"))
        param->fillprint = true;
    }
    if ((val = cupsGetOption("crop-to-fit", num_options, options)) != NULL) 
    {
      if (!strcasecmp(val, "true") || !strcasecmp(val, "yes"))
        param->cropfit = 1;
    }
    if (!param->autoprint && !param->autofit && !param->fitplot && 
	!param->fillprint && !param->cropfit)
      param->autoprint = true;
  }

  // Certain features require a given page size for the page to be
  // printed or all pages of the document being the same size. Here we
  // set param.pagesize_requested so that the default page size is used
  // when no size got specified by the user.
  if (param->fitplot || param->fillprint || param->autoprint || param->autofit || 
      param->booklet != CF_PDFTOPDF_BOOKLET_OFF || 
      param->nup->nupX > 1 || param->nup->nupY > 1)
    param->pagesize_requested = true;

  //
  // Do we have to do the page logging in page_log?
  //
  // CUPS standard is that the last filter (not the backend, usually the
  // printer driver) does page logging in the /var/log/cups/page_log file
  // by outputting "PAGE: <# of current page> <# of copies>" to stderr.
  //
  // cfFilterPDFToPDF() would have to do this only for PDF printers as
  // in this case cfFilterPDFToPDF() is the last filter, but some of
  // the other filters are not able to do the logging because they do
  // not have access to the number of pages of the file to be printed,
  // so cfFilterPDFToPDF() overtakes their logging duty.
  //

  // Check whether page logging is forced or suppressed by the options

  if ((val = cupsGetOption("pdf-filter-page-logging", 
			   num_options, options)) != NULL) 
  {
    if (strcasecmp(val, "auto") == 0) 
    {
      param->page_logging = -1;
      if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG, 
		      		     "cfFilterPDFToPDF: Automatic page logging selected by options.");
    } 
    else if (is_true(val)) 
    {
      param->page_logging = 1;
      if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG, 
		      		     "cfFilterPDFToPDF: Forced page logging selected by options.");
    } 
    else if (is_false(val)) 
    {
      param->page_logging = 0;
      if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG, 
		                     "cfFilterPDFToPDF: Suppressed page logging selected by options.");
    } 
    else 
    {
      if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR, 
		      		     "cfFilterPDFToPDF: Unsupported page logging setting \"pdf-filter-page-logging=%s\", using \"auto\"!", 
				     val);
       param->page_logging = -1;
    }
  }

  if (param->page_logging == -1) 
  {
    // We determine whether to log pages or not
    // using the output data MIME type. log pages only when the output is
    // either pdf or PWG Raster
    if (final_content_type && 
	(strcasestr(final_content_type, "/pdf") || 
	 strcasestr(final_content_type, "/vnd.cups-pdf") || 
	 strcasestr(final_content_type, "/pwg-raster")))
      param->page_logging = 1;
    else 
      param->page_logging = 0;

    // If final_content_type is not clearly available we are not sure whether
    // to log pages or not
    if (!final_content_type || 
	final_content_type[0] == '\0')
      param->page_logging = -1;

    if (doc->logfunc) 
    {
      doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG, 
		   "cfFilterPDFToPDF: Determined whether to log pages or not using output data type.");
      doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG, 
		   "final_content_type = %s => page_logging = %d", 
		   final_content_type ? final_content_type : "NULL", 
		   param->page_logging);
    }

      if (param->page_logging == -1) 
	param->page_logging = 0;
  }
  free(tmp);
}

void 
calculate(int num_options, 
	  cups_option_t *options, 
	  _cfPDFToPDFProcessingParameters *param, 
	  char *final_content_type) 
{
  const char	*val;
  bool 		hw_copies = false, 
		hw_collate = false;

  // Check options for caller's instructions about hardware copies/collate
  if ((val = cupsGetOption("hardware-copies", 
			   num_options, options)) != NULL)
    // Use hardware copies according to the caller's instructions
    hw_copies = is_true(val);
  else
    // Caller did not tell us whether the printer does Hardware copies
    // or not, so we assume hardware copies on PDF printers, and software
    // copies on other (usually raster) printers or if we do not know the
    // final output format.
    hw_copies = (final_content_type && 
		 (strcasestr(final_content_type, "/pdf") || 
		  strcasestr(final_content_type, "/vnd.cups-pdf")));

  if (hw_copies) 
  {
    if ((val = cupsGetOption("hardware-collate",
			     num_options, options)) != NULL)
      // Use hardware collate according to the caller's instructions
      hw_collate = is_true(val);
    else
      hw_collate = (final_content_type && 
		    (strcasestr(final_content_type, "/pdf") || 
		     strcasestr(final_content_type, "/vnd.cups-pdf") || 
		     strcasestr(final_content_type, "/pwg-raster") || 
		     strcasestr(final_content_type, "/urf") || 
		     strcasestr(final_content_type, "/PCLm")));
  }

  if (param->reverse && param->duplex) 
    // Enable even_duplex or the first page may be empty.
    param->even_duplex = true; // disabled later, if non-duplex

  if (param->num_copies == 1) 
  {
    param->device_copies = 1;
    // collate is never needed for a single copy
    param->collate = false; // (does not make a big difference for us)
  } 
  else if (hw_copies) 
  { // hw copy generation available
    param->device_copies = param->num_copies;
    if (param->collate) 
    {
      param->device_collate = hw_collate;
      if (!param->device_collate) 
	// printer can't hw collate -> we must copy collated in sw
	param->device_copies = 1;
    } // else: printer copies w/o collate and takes care of duplex/even_duplex
  } 
  else 
  { // sw copies
    param->device_copies = 1;
    if (param->duplex) 
    { // &&(num_copies>1)
      // sw collate + even_duplex must be forced to prevent copies on the
      // back sides
      param->collate = true;
      param->device_collate = false;
    }
  }

  if (param->device_copies != 1) 
    param->num_copies = 1;

  if (param->duplex && 
      param->collate && !param->device_collate) 
    param->even_duplex = true;

  if (!param->duplex) 
    param->even_duplex = false;
}

// check whether a given file is empty
bool 
is_empty(FILE *f) 
{
  char buf[1];
  if (fread(buf, 1, 1, f) == 0) 
    return true;
  rewind(f);
  return false;
}

// coping inputfp data to temp_fp, so that we have a filename, as it is required in pdfioFileOpen API
int 
copy_fd_to_tempfile(int inputfd, 
		    FILE *temp_file, 
		    pdftopdf_doc_t *doc) 
{
  char buffer[BUFSIZ]; 
  ssize_t bytes_read, bytes_written;

  while ((bytes_read = read(inputfd, buffer, sizeof(buffer))) > 0) 
  {
    bytes_written = fwrite(buffer, 1, bytes_read, temp_file);
    if (bytes_written != bytes_read) 
    {
      if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR, "write to temporary file failed");
      return -1; 
    }
  }

  if (bytes_read == -1) 
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR, "Read from inputfd failed");
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

  pdftopdf_doc_t 	doc;
  char 			*final_content_type = data->final_content_type;
  FILE 			*inputfp, 
       			*outputfp;
  const char 		*t;
  int 			streaming = 0;
  size_t 		bytes;
  char 			buf[BUFSIZ];
  cf_logfunc_t 		log = data->logfunc;
  void 			*ld = data->logdata;
  cf_filter_iscanceledfunc_t iscanceled = data->iscanceledfunc;
  void 			*icd = data->iscanceleddata;
  int 			num_options = 0;
  cups_option_t 	*options = NULL;

  _cfPDFToPDFProcessingParameters *param = (_cfPDFToPDFProcessingParameters *)malloc(sizeof(_cfPDFToPDFProcessingParameters));
  _cfPDFToPDFProcessingParameters_init(param);

  
  fprintf(stderr, "_cfPDFToPDFProcessingParameters_init has been done\n");
  
  param->job_id = data->job_id;
  param->user = data->job_user;
  param->title = data->job_title;
  param->num_copies = data->copies;
  param->copies_to_be_logged = data->copies;
  param->page->width = param->page->height = 0;
  param->page->left = param->page->bottom = -1;
  param->page->right = param->page->top = -1;

  doc.logfunc = log;
  doc.logdata = ld;
  doc.iscanceledfunc = iscanceled;
  doc.iscanceleddata = icd;

  num_options = cfJoinJobOptionsAndAttrs(data, num_options, &options);

  getParameters(data, num_options, options, param, &doc);
  calculate(num_options, options, param, final_content_type);

#ifdef DEBUG
  _cfPDFToPDFProcessingParameters_dump(param, &doc);
#endif

  // If we are in streaming mode we only apply JCL and do not run the
  // job through QPDL (so no page management, form flattening,
  // page size/orientation adjustment, ...)
  if ((t = cupsGetOption("filter-streaming-mode", 
	   		 num_options, options)) != NULL && 
      (strcasecmp(t, "false") && strcasecmp(t, "off") && 
       strcasecmp(t, "no"))) 
  {
    streaming = 1;
    if (log) log(ld, CF_LOGLEVEL_DEBUG, 
	     	   "cfFilterPDFToPDF: Streaming mode: No PDF processing, only adding of JCL");
  }

  cupsFreeOptions(num_options, options);

  _cfPDFToPDF_PDFioProcessor proc;

  char temp_filename[] = "/tmp/tempfileXXXXXX";
  int temp_fd = mkstemp(temp_filename);
  if (temp_fd == -1) 
  {
    if (log) log(ld, CF_LOGLEVEL_ERROR, "tempfilename wasn't created");
    return 1;
  }

  // Convert the temp_fd to a FILE* stream
  inputfp = fdopen(temp_fd, "wb+");
  if (!inputfp) {
      if (log) log(ld, CF_LOGLEVEL_ERROR, "Couldn't convert temp_fd to FILE* stream");
      close(temp_fd);
      close(inputfd);
      unlink(temp_filename);
      return 1;
  }

  if (copy_fd_to_tempfile(inputfd, inputfp, &doc) == -1) {
        fprintf(stderr, "Failed to copy inputfd to temp file\n");
        fclose(inputfp);
        close(inputfd);
        unlink(temp_filename); // Clean up temporary file
        return 1;
    }
  
  rewind(inputfp); // Rewind the temp_file for reading
 
  if (!streaming) 
  {
    if (is_empty(inputfp)) 
    {
      fclose(inputfp);
      close(inputfd);
      unlink(temp_filename); 
      if (log) log(ld, CF_LOGLEVEL_DEBUG, 
		   "cfFilterPDFToPDF: Input is empty, outputting empty file.");
      return 0;
    }
    
    //test
    if (log) log(ld, CF_LOGLEVEL_DEBUG, 
		 " cfFilterPDFToPDF: Processing PDF input with PDFio: Page-ranges, page-set, number-up, booklet, size adjustment, ...");

    // Load the PDF input data into PDFio
    if (!_cfPDFToPDF_PDFioProcessor_load_filename(&proc, temp_filename, &doc, 1))
    {
      if (log) log(ld, CF_LOGLEVEL_DEBUG, "cfFilterPDFToPDF: error in _cfPDFToPDF_PDFioProcessor_load_filename");
      fclose(inputfp);
      close(inputfd);
      unlink(temp_filename); 
      return 1;
    }

    // Process the PDF input data
    if (!_cfProcessPDFToPDF(&proc, param, &doc))
    {
      if (log) log(ld, CF_LOGLEVEL_DEBUG, "cfFilterPDFToPDF: error in _cfProcessPDFToPDF");
      return 2;
    }

    // Pass information to subsequent filters via PDF comments
    char *output[10];
    int output_len = 0;

    output[output_len++] = "% This file was generated by pdftopdf";

    if (param->device_copies > 0) 
    {
      char buf[256];
      snprintf(buf, sizeof(buf), "%d", param->device_copies);
      output[output_len++] = strdup(buf);

      if (param->device_collate)
        output[output_len++] = "%%PDFTOPDFCollate : true";
      else
        output[output_len++] = "%%PDFTOPDFCollate : false";
    }

    _cfPDFToPDF_PDFioProcessor_set_comments(&proc, output, output_len);
  }

  char temp_output_filename[] = "/tmp/outputfilenameXXXXXX";

  int temp_output_fd = mkstemp(temp_output_filename);
  if (temp_output_fd == -1) {
    if (log) log(ld, CF_LOGLEVEL_ERROR, "Failed to create temporary output file");
    return 1;
  }

  outputfp = fdopen(temp_output_fd, "wb+");
  if (!outputfp) {
    if (log) log(ld, CF_LOGLEVEL_ERROR, "Failed to open temporary output file stream");
    close(temp_output_fd);
    unlink(temp_output_filename);
    return 1;
  }

  if (!streaming) 
  {
    _cfPDFToPDF_PDFioProcessor_emit_filename(&proc, temp_output_filename, &doc); 
  } 
  else 
  {
    if (log) log(ld, CF_LOGLEVEL_DEBUG, 
		 "cfFilterPDFToPDF: Passing on unchanged PDF data from input");
    while ((bytes = fread(buf, 1, sizeof(buf), inputfp)) > 0)
      if (fwrite(buf, 1, bytes, outputfp) != bytes)
        break;
    fclose(inputfp);
    close(inputfd);
    unlink(temp_filename); 
  }

  fflush(outputfp);
  rewind(outputfp);
  char buffer[8192];
  ssize_t bytes_read;
  while ((bytes_read = read(temp_output_fd, buffer, sizeof(buffer))) > 0) 
  {
  if (write(outputfd, buffer, bytes_read) != bytes_read) {
    if (log) log(ld, CF_LOGLEVEL_ERROR, "Failed to write to output_fd");
    fclose(outputfp);
    close(outputfd);
    unlink(temp_output_filename);
    return 1;
  }
}

// free Param
  _cfPDFToPDFProcessingParameters_free(param);

// Clean up the temporary file
  unlink(temp_output_filename);
  fclose(outputfp);
  close(inputfd);
  unlink(temp_filename); 
  return 0;
}
// }}}
