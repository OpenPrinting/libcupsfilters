//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "processor.h"
#include <stdio.h>
#include "cupsfilters/debug-internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#define MAX(a, b) ((a) > (b) ? (a) : (b))

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

  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG, "cfFilterPDFToPDF: booklet signature: %d", processingParams->book_signature);
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

int* 
_cfPDFToPDFBookletShuffle(int numPages, int signature, int* ret_size) 
{
  if (signature < 0) {
    signature = (numPages + 3) & ~0x3; // Round up to the nearest multiple of 4
  }

  int maxSize = numPages + signature - 1;
  int* ret = (int*)malloc(maxSize * sizeof(int));
  if (ret == NULL) 
  {
    *ret_size = 0;
    return NULL; // Handle memory allocation failure
  }

  int curpage = 0;
  int index = 0; // Keeps track of the current index in the result array
  while (curpage < numPages) 
  {
    int firstpage = curpage;
    int lastpage = curpage + signature - 1;

    while (firstpage < lastpage) 
    {
      ret[index++] = lastpage--;
      ret[index++] = firstpage++;
      ret[index++] = firstpage++;
      ret[index++] = lastpage--;
    }
    curpage += signature;
  }

  *ret_size = index; // Set the size of the result
  return ret;
}

bool 
_cfProcessPDFToPDF( _cfPDFToPDF_PDFioProcessor *proc,
		   _cfPDFToPDFProcessingParameters *param,
		   pdftopdf_doc_t *doc) 
{
  if(!_cfPDFToPDF_PDFioProcessor_check_print_permissions(proc, doc))
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG, 
		    		   "cfFilterPDFToPDF: Not allowed to print");		
    return false;
  }

  const bool dst_lscape =
	  (param->paper_is_landscape ==
	    ((param->orientation == ROT_0) || (param->orientation == ROT_180)));

  if (param->paper_is_landscape)
  {
    int temp = param->nup.nupX;
    param->nup.nupX = param->nup.nupY;
    param->nup.nupY = temp;
  }

  if (param->auto_rotate)
    _cfPDFToPDF_PDFioProcessor_auto_rotate_all(proc, dst_lscape, param->normal_landscape);

  int *num_page;
  _cfPDFToPDFPageHandle **pages = _cfPDFToPDF_PDFioProcessor_get_pages(proc, doc, num_page);

  _cfPDFToPDFPageHandle **input_page_range_list = malloc((*num_page) * sizeof(_cfPDFToPDFPageHandle*));
  int input_page_range_size = 0;

  for (int i = 1; i <= *num_page; i++) 
  {
    if (_cfPDFToPDFProcessingParameters_with_page(param, i)) 
    {
      input_page_range_list[input_page_range_size++] = pages[i - 1];
      input_page_range_size++;
    }
  }
 
  const int numOrigPages = input_page_range_size;

  int* shuffle = NULL;
  int* shuffle_size;
  
  if (param->booklet != CF_PDFTOPDF_BOOKLET_OFF)
  {
    shuffle = _cfPDFToPDFBookletShuffle(numOrigPages, param->book_signature, shuffle_size); 
    if (param->booklet == CF_PDFTOPDF_BOOKLET_ON)
    {
      _cfPDFToPDFNupParameters_preset(2, &param->nup);
    }
  }
  else
  {
    int* shuffle = malloc(numOrigPages * sizeof(numOrigPages));
    for (int i = 0; i < numOrigPages; i++) 
    {
      shuffle[i] = i;
    }
  }
  
  const int numPages = MAX(*shuffle_size, input_page_range_size);

  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                                 "cfFilterPDFToPDF: \"print-scaling\" IPP attribute: %s",
                                 (param->autoprint ? "auto" :
                                  (param->autofit ? "auto-fit" :
                                   (param->fitplot ? "fit" :
                                    (param->fillprint ? "fill" :
                                     (param->cropfit ? "none" :
                                      "Not defined, should never happen"))))));
  
  if (param->autoprint || param->autofit)
  {
    bool margin_defined = true;
    bool document_large = false;
    int pw = param->page.right - param->page.left;
    int ph = param->page.top - param->page.bottom;
    
    if ((param->page.width == pw) && (param->page.height == ph))
      margin_defined = false;

    for (int i = 0; i < input_page_range_size; i ++)
    {
      _cfPDFToPDFPageRect r = _cfPDFToPDFPageHandle_get_rect(input_page_range_list[i]);
      int w = r.width * 100 / 102; // 2% of tolerance
      int h = r.height * 100 / 102;
      if ((w > param->page.width || h > param->page.height) &&
          (h > param->page.width || w > param->page.height))
      {
        if (doc->logfunc)
          doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                       "cfFilterPDFToPDF: Page %d too large for output page size, scaling pages to fit.",
                       i + 1);
        document_large = true;
      }
    }

    if (param->fidelity && doc->logfunc)
      doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                   "cfFilterPDFToPDF: \"ipp-attribute-fidelity\" IPP attribute is set, scaling pages to fit.");
   
    if (param->autoprint)
    {
      if (param->fidelity || document_large)
      {
        if (margin_defined)
          param->fitplot = true;
        else
          param->fillprint = true;
      }
      else
        param->cropfit = true;
    }
    
    else
    {
      if (param->fidelity || document_large)
        param->fitplot = true;
      else
        param->cropfit = true;
    }
  }

  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                                 "cfFilterPDFToPDF: Print scaling mode: %s",
                                 (param->fitplot ?
                                  "Scale to fit printable area" :
                                  (param->fillprint ?
                                   "Scale to fill page and crop" :
                                   (param->cropfit ?
                                    "Do not scale, center, crop if needed" :
                                    "Not defined, should never happen"))));

  if (param->cropfit)
  {
    param->page.left = 0;
    param->page.bottom = 0;
    param->page.right = param->page.width;
    param->page.top = param->page.height;
  }

  if (param->pagesize_requested && (param->fillprint || param->cropfit))
  {
    for (int i = 0; i < input_page_range_size; i ++)
    {
      _cfPDFToPDFPageHandle *page = input_page_range_list[i];
      pdftopdf_rotation_e orientation;
      if(_cfPDFToPDFPageHandle_is_landscape(page, param->orientation))
        orientation = param->normal_landscape;
      else
        orientation = ROT_0;
      _cfPDFToPDFPageHandle_crop(page,
		      		 &param->page,
				 orientation,
				 param->orientation,
				 param->xpos,
				 param->ypos,
				 !param->cropfit,
				 !param->auto_rotate,
				 doc);
    }
    if (param->fillprint)
      param->fitplot = true;
  }

  _cfPDFToPDFPageHandle *curpage;
  int outputpage = 0;
  int outputno = 0;

  if ((param->nup.nupX == 1) && (param->nup.nupY == 1) && !param->fitplot)
  {
    param->nup.width = param->page.width;
    param->nup.height = param->page.height;
  }
  else
  {
    param->nup.width = param->page.right - param->page.left;
    param->nup.height = param->page.top - param->page.bottom;
  }

  if ((param->orientation == ROT_90) || (param->orientation == ROT_270))
  {
    int temp = param->nup.nupX;
    param->nup.nupX = param->nup.nupY;
    param->nup.nupY = temp;
    
    param->nup.landscape = !param->nup.landscape;
    param->orientation = param->orientation - param->normal_landscape;
  }

  double xpos = 0, ypos = 0;
  if(param->nup.landscape)
  {
    param->orientation = param->orientation + param->normal_landscape;
    if (param->nup.nupX != 1 || param->nup.nupY != 1 || param->fitplot)
    {
      xpos = param->page.height - param->page.top;
      ypos = param->page.left; 
    }
    int temp = param->page.width;
    param->page.width = param->page.height;
    param->page.height = temp;

    temp = param->nup.width;
    param->nup.width = param->nup.height;
    param->nup.height = temp;
  }
  else
  {
    if (param->nup.nupX != 1 || param->nup.nupY != 1 || param->fitplot)
    {
      xpos = param->page.left;
      ypos = param->page.bottom; 
    }
  }
  
  _cfPDFToPDFNupState *nupState;
  _cfPDFToPDFNupState_init(nupState, &param->nup);
  
  _cfPDFToPDFNupPageEdit pgedit;

  for (int iA = 0; iA < numPages; iA++)
  {
    _cfPDFToPDFPageHandle *page;
    if(shuffle[iA] >= numOrigPages)
      page = _cfPDFToPDF_PDFioProcessor_new_page(proc, param->page.width, param->page.height, doc);
    else
      page = input_page_range_list[shuffle[iA]];  
   
    _cfPDFToPDFPageRect rect = _cfPDFToPDFPageHandle_get_rect(page);  

    if (!param->pagesize_requested)
    {
      param->page.width = param->page.right = rect.width;
      param->page.height = param->page.top = rect.height;
    }

    bool newPage = _cfPDFToPDFNupState_next_page(nupState, rect.width, rect.height, &pgedit);
    if (newPage)
    {
      if((curpage) && (_cfPDFToPDFProcessingParameters_with_page(param, outputpage)))
      {
	_cfPDFToPDFPageHandle_rotate(curpage, param->orientation);
	if (param->mirror)
	  _cfPDFToPDFPageHandle_mirror(curpage, proc->pdf);

//	_cfPDFToPDF_PDFioProcessor_add_page(proc, curpage, param->reverse);
	// Log page in /var/log/cups/page_log
	
	outputno ++;
	if (param->page_logging == 1)
	  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_CONTROL,
					 "PAGE: %d %d", outputno,
					 param->copies_to_be_logged);
      }
      curpage = _cfPDFToPDF_PDFioProcessor_new_page(proc, param->page.width, param->page.height, doc);
      outputpage++;
    }

    if (shuffle[iA] >= numOrigPages)
      continue;

    if (param->border != NONE)
      _cfPDFToPDFPageHandle_add_border_rect(page, proc->pdf, rect, param->border, 1.0 / pgedit.scale);

    if (param->page_label[0] != '\0')
    {
      _cfPDFToPDFPageHandle_add_label(page, proc->pdf, &param->page, param->page_label); 
    }

    if(param->cropfit)
    {
      if ((param->nup.nupX == 1) && (param->nup.nupY == 1))
      {
        double xpos2, ypos2;
	_cfPDFToPDFPageRect get_rect_height = _cfPDFToPDFPageHandle_get_rect(page);
	_cfPDFToPDFPageRect get_rect_width = _cfPDFToPDFPageHandle_get_rect(page);
	
	if ((param->page.height - param->page.width) *
	    (get_rect_height.height - get_rect_width.width) < 0)
	{
	  xpos2 = (param->page.width - (get_rect_height.height)) / 2;
	  ypos2 = (param->page.height - (get_rect_width.width)) / 2;
	  _cfPDFToPDFPageHandle_add_subpage(curpage, page, proc->pdf, ypos2 + xpos, xpos2 + ypos, 1, NULL);
	}
	else
	{
	  xpos2 = (param->page.width - get_rect_width.width) / 2;
	  ypos2 = (param->page.height - get_rect_height.height) /2;
	  _cfPDFToPDFPageHandle_add_subpage(curpage, page, proc->pdf, xpos2 + xpos, ypos2 + ypos, 1, NULL);
	}
      }
      else
      {
	_cfPDFToPDFPageHandle_add_subpage(curpage, page, proc->pdf, pgedit.xpos + xpos, pgedit.ypos + ypos, pgedit.scale, NULL);
      }
    }
    else
      _cfPDFToPDFPageHandle_add_subpage(curpage, page, proc->pdf, pgedit.xpos + xpos, pgedit.ypos + ypos, pgedit.scale, NULL);
   
#ifdef DEBUG
    _cfPDFToPDFPageHandle *dbg = (_cfPDFToPDFPageHandle *)curpage;
    if (dbg && dbg->debug) 
    {
      _cfPDFToPDFPageHandle_debug(dbg, sub, xpos,ypos);
    }
#endif
  }
  
  if((curpage) && (_cfPDFToPDFProcessingParameters_with_page(param, outputpage)))
  {
    _cfPDFToPDFPageHandle_rotate(curpage, param->orientation);
    if(param->mirror)
      _cfPDFToPDFPageHandle_mirror(curpage, proc->pdf);

    // need to output empty page to not confuse duplex
//    _cfPDFToPDF_PDFioProcessor_add_page(proc, _cfPDFToPDF_PDFioProcessor_new_page(proc,
//		  			      param->page.width, param->page.height, doc), param->reverse);
    
    // Log page in /var/log/cups/page_log
    if(param->page_logging == 1)
      if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_CONTROL,
				     "PAGE: %d %d", outputno + 1,
				     param->copies_to_be_logged);
  }

  _cfPDFToPDF_PDFioProcessor_multiply(proc, param->num_copies, param->collate);
  return true;
}
