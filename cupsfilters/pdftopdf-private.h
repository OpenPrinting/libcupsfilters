//
// Copyright 2020 by Jai Luthra.
// Copyright 2024-2025 Uddhav Phatak <uddhavphatak@gmail.com>
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _CUPS_FILTERS_PDFTOPDF_PDFTOPDF_H
#define _CUPS_FILTERS_PDFTOPDF_PDFTOPDF_H

#include <cupsfilters/filter.h>
#include <pdfio.h>
#include <pdfio-content.h>
#include "ipp-options-private.h"
#define XFORM_MAX_PAGES         10000
#define XFORM_MAX_LAYOUT        16

typedef enum {
    an_invisible = 1 << 0,      // Annotation is invisible
    an_hidden = 1 << 1,          // Annotation is hidden
    an_print = 1 << 2,           // Annotation should be printed
    an_no_zoom = 1 << 3,         // Don't zoom annotation
    an_no_rotate = 1 << 4,       // Don't rotate with page
    an_no_view = 1 << 5,         // Don't show on screen
    an_read_only = 1 << 6,       // Read-only annotation
    an_locked = 1 << 7,          // Locked annotation
    an_toggle_no_view = 1 << 8,  // Toggle visibility
    an_locked_contents = 1 << 9  // Locked contents
} pdf_annotation_flag;

typedef struct                               // **** Document information ****
{
  cf_logfunc_t logfunc;                      // Log function
  void *logdata;                             // Log data
  cf_filter_iscanceledfunc_t iscanceledfunc; // Function returning 1 when
                                             // job is canceled, NULL for not
                                             // supporting stop on cancel
  void *iscanceleddata;                      // User data for is-canceled
                                             // function, can be NULL
} pdftopdf_doc_t;

typedef struct {
    pdfio_obj_t *obj; // Track object pointers for cycle detection
} VisitedObj;

typedef struct xform_document_s         // Document information                            
{ 
  const char    *filename,              // Document filename                               
                *format;                // Document format
  char          tempfile[1024];         // Temporary PDF file, if any         
  const char    *pdf_filename;          // PDF filename
  pdfio_file_t  *pdf;                   // PDF file for document
  int           first_page,             // First page number in document
                last_page,              // Last page number in document
                num_pages;              // Number of pages to print in document            
} xform_document_t;       

typedef struct xform_page_s             // Output page
{
  pdfio_file_t  *pdf;                   // Output PDF file
  size_t        layout;                 // Current layout cell
  pdfio_obj_t   *input[XFORM_MAX_LAYOUT];
                                        // Input page objects
  pdfio_dict_t  *pagedict;              // Page dictionary
  pdfio_dict_t  *resdict;               // Resource dictionary
  pdfio_dict_t  *resmap[XFORM_MAX_LAYOUT];
                                        // Resource name map
  pdfio_dict_t  *restype;               // Current resource type dictionary
  pdfio_stream_t *output;               // Output page stream
} xform_page_t;

typedef struct xform_page_ext_s             // Output page
{
  pdfio_file_t  *pdf;                   // Output PDF file
  pdfio_obj_t   **input;
                                        // Input page objects
  pdfio_dict_t  *pagedict;              // Page dictionary
  pdfio_dict_t  *resdict;               // Resource dictionary
  pdfio_dict_t  *resmap[XFORM_MAX_LAYOUT];
                                        // Resource name map
  pdfio_dict_t  *restype;               // Current resource type dictionary
  pdfio_stream_t *output;               // Output page stream
} xform_page_ext_t;


typedef struct xform_prepare_s          // Preparation data
{
  cf_filter_options_t *options;               // Print options
  cups_array_t  *errors;                // Error messages
  int           document,               // Current document
                num_inpages;            // Number of input pages
  pdfio_file_t  *pdf;                   // Output PDF file
  pdfio_file_t  *inpdf;			// INPUT PDF FILE
  pdfio_rect_t  media;                  // Default media box
  pdfio_rect_t  crop;                   // Default crop box
  size_t        num_outpages;           // Number of output pages
  xform_page_t  outpages[XFORM_MAX_PAGES];
                                        // Output pages
  size_t        num_layout;             // Number of layout rectangles
  pdfio_rect_t  layout[XFORM_MAX_LAYOUT];
                                        // Layout rectangles
  bool          use_duplex_xform;       // Use the back side transform matrix?
  pdfio_matrix_t duplex_xform;          // Back side transform matrix
  bool 		has_form;    		// does PDF have Acroform(is flattening required?)
  bool 		has_annotations;   	// does PDF have annotations in Acroform(is flattenning required?)
  bool 		needAppearances;	// does PDF need appearances?
  char          **annotation_contents; // Annotation contents to write to page streams
  size_t        num_annotations;       // Number of annotations to write
} xform_prepare_t;

#endif // !_CUPS_FILTERS_PDFTOPDF_PDFTOPDF_H
