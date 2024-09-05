//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.

#include "C-pdftopdf-processor-private.h"
#include "pdfio-pdftopdf-processor-private.h"
#include <stdio.h>
#include "cupsfilters/debug-internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

void 
_cfPDFToPDFProcessingParameters_init(_cfPDFToPDFProcessingParameters *processingParams) // {{{ 
{

  processingParams->job_id = 0;
  processingParams->num_copies = 1;
  processingParams->user = NULL;
  processingParams->title = NULL;
  processingParams->pagesize_requested = false;
  processingParams->fitplot = false;
  processingParams->fillprint = false;  // print-scaling = fill
  processingParams->cropfit = false;
  processingParams->autoprint = false;
  processingParams->autofit = false;
  processingParams->fidelity = false;
  processingParams->no_orientation = false;
  processingParams->orientation = ROT_0;
  processingParams->normal_landscape = ROT_270;
  processingParams->paper_is_landscape = false;
  processingParams->duplex = false;
  processingParams->border = NONE;
  processingParams->reverse = false; 
    
  processingParams->page_label = NULL;
  processingParams->even_pages = true;
  processingParams->odd_pages = true;

  processingParams->mirror = false;

  processingParams->xpos = CENTER;
  processingParams->ypos = CENTER;

  processingParams->collate = false;
  processingParams->even_duplex = false;
 
  processingParams->booklet = CF_PDFTOPDF_BOOKLET_OFF;
  processingParams->book_signature = -1;

  processingParams->auto_rotate = false;

  processingParams->device_copies = 1;
  processingParams->device_collate = false;
  processingParams->set_duplex = false;

  processingParams->page_logging = -1;
  processingParams->copies_to_be_logged = 0;
  
  processingParams->page.width = 612.0;   // Letter size width in points
  processingParams->page.height = 792.0;  // Letter size height in points
  processingParams->page.top = processingParams->page.height - 36.0;
  processingParams->page.bottom = 36.0;
  processingParams->page.left = 18.0;
  processingParams->page.right = processingParams->page.width - 18.0;

  _cfPDFToPDFIntervalSet_add_single(processingParams->input_page_ranges, 1);
  _cfPDFToPDFIntervalSet_finish(processingParams->input_page_ranges);
  _cfPDFToPDFIntervalSet_add_single(processingParams->page_ranges, 1);
  _cfPDFToPDFIntervalSet_finish(processingParams->page_ranges);
}
// }}}

void 
BookletMode_dump(pdftopdf_booklet_mode_e bkm, 
		 pdftopdf_doc_t *doc) // {{{
{
  static const char *bstr[3] = {"Off", "On", "Shuffle-Only"};

  if ((bkm < CF_PDFTOPDF_BOOKLET_OFF) || 
      (bkm > CF_PDFTOPDF_BOOKLET_JUST_SHUFFLE)) 
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                                   "cfFilterPDFToPDF: Booklet mode: (Bad booklet mode: %d)",
                                   bkm);
  } 
  else 
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                                   "cfFilterPDFToPDF: Booklet mode: %s",
                                   bstr[bkm]);
  }
}
// }}}

bool 
_cfPDFToPDFProcessingParameters_with_page(const _cfPDFToPDFProcessingParameters *processingParams, 
				 	  int outno) // {{{
{
  if (outno % 2 == 0) 
  {
    if (!processingParams->even_pages)
      return false;
  } 
  else if (!processingParams->odd_pages) 
  {
    return false;
  }
  return _cfPDFToPDFIntervalSet_contains(processingParams->page_ranges, outno);
}
// }}}

bool 
_cfPDFToPDFProcessingParameters_have_page(const _cfPDFToPDFProcessingParameters *processingParams, 
					  int pageno) // {{{
{
  return _cfPDFToPDFIntervalSet_contains(processingParams->input_page_ranges, pageno);
}
// }}}

void 
_cfPDFToPDFProcessingParameters_dump(const _cfPDFToPDFProcessingParameters *processingParams, 
				     pdftopdf_doc_t *doc) // {{{
{
  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                                 "cfFilterPDFToPDF: job_id: %d, num_copies: %d",
                                 processingParams->job_id, processingParams->num_copies);
  
  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
		                 "cfFilterPDFToPDF: user: %s, title: %s",
				 (processingParams->user) ? processingParams->user : "(null)",
				 (processingParams->title) ? processingParams->title : "(null)");
  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG, 
		  		 "cfFilterPDFToPDF: fitplot: %s",
		 		 (processingParams->fitplot) ? "true" : "false");

  _cfPDFToPDFPageRect_dump(&processingParams->page, doc);
  _cfPDFToPDFRotationDump(processingParams->orientation, doc);

  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                    		 "cfFilterPDFToPDF: paper_is_landscape: %s",
				 (processingParams->paper_is_landscape) ? "true" : "false");

  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
		 		 "cfFilterPDFToPDF: duplex: %s",
				 (processingParams->duplex) ? "true" : "false"); 
  
  _cfPDFToPDFBorderTypeDump(processingParams->border, doc); 
  _cfPDFToPDFNupParameters_dump(&processingParams->nup, doc);

  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
		 		"cfFilterPDFToPDF: reverse: %s",
			       	(processingParams->reverse) ? "true" : "false");

  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
	                        "cfFilterPDFToPDF: even_pages: %s, odd_pages: %s",
			       	(processingParams->even_pages) ? "true" : "false",
			       	(processingParams->odd_pages) ? "true" : "false");
 
  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
   	                        "cfFilterPDFToPDF: input page range:");
 
  _cfPDFToPDFIntervalSet_dump(processingParams->input_page_ranges, doc);
 
  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
	                         "cfFilterPDFToPDF: page range:");
 
  _cfPDFToPDFIntervalSet_dump(processingParams->page_ranges, doc);
 
  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                     "cfFilterPDFToPDF: mirror: %s",
                     (processingParams->mirror) ? "true" : "false");

  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                     	         "cfFilterPDFToPDF: Position:");
 
  _cfPDFToPDFPositionAndAxisDump(processingParams->xpos, X, doc);
  _cfPDFToPDFPositionAndAxisDump(processingParams->ypos, Y, doc);

  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                                 "cfFilterPDFToPDF: collate: %s",
				 (processingParams->collate) ? "true" : "false");

  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                                 "cfFilterPDFToPDF: even_duplex: %s",
                                 (processingParams->even_duplex) ? "true" : "false");
  
  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
	      		         "cfFilterPDFToPDF: page_label: %s",
				 processingParams->page_label ? processingParams->page_label : "(none)");

  BookletMode_dump(processingParams->booklet, doc);

  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                                 "cfFilterPDFToPDF: booklet signature: %d",
				 processingParams->book_signature);

  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                                 "cfFilterPDFToPDF: auto_rotate: %s",
				 (processingParams->auto_rotate) ? "true" : "false");

  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                     		 "cfFilterPDFToPDF: device_copies: %d",
				 processingParams->device_copies);
 
  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                                 "cfFilterPDFToPDF: device_collate: %s",
				 (processingParams->device_collate) ? "true" : "false");
  
  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
		 		 "cfFilterPDFToPDF: set_duplex: %s",
				 (processingParams->set_duplex) ? "true" : "false");
}
// }}}

