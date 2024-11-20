//
// Copyright 2024 Uddhav Phatak <uddhavabhijeet@gmail.com>
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef C_PDFTOPDF_PROCESSOR_PRIVATE_H
#define C_PDFTOPDF_PROCESSOR_PRIVATE_H

#include "C-pptypes-private.h"
#include "C-nup-private.h"
#include "C-pdftopdf-private.h"
#include "pdfio-pdftopdf-processor-private.h"
#include "C-intervalset-private.h"
#include <stdio.h>
#include <stdbool.h>

typedef enum {
  CF_PDFTOPDF_BOOKLET_OFF,
  CF_PDFTOPDF_BOOKLET_ON,
  CF_PDFTOPDF_BOOKLET_JUST_SHUFFLE
} pdftopdf_booklet_mode_e;

typedef struct _cfPDFToPDF_PDFioProcessor{

    // 1st mode: existing
    pdfio_obj_t *page;      // Equivalent to QPDFObjectHandle
    int no;

    // 2nd mode: create new
    HashTable *xobjs;       // Pointer to a single HashTable

    char *content;          // Equivalent to std::string content

    pdftopdf_rotation_e rotation;

    // Other members
    pdfio_file_t *pdf;      // Equivalent to std::unique_ptr<QPDF>
    pdfio_obj_t **orig_pages; // Equivalent to std::vector<QPDFObjectHandle>
    size_t orig_pages_size;   // Current number of pages
    size_t orig_pages_capacity; // Capacity for page array

    bool hasCM;
    char *extraheader;
} _cfPDFToPDF_PDFioProcessor;


typedef struct {
  int job_id, num_copies;
  const char *user, *title;
  bool pagesize_requested;
  bool fitplot;
  bool fillprint;   // print-scaling = fill
  bool cropfit;     // -o crop-to-fit
  bool autoprint;   // print-scaling = auto
  bool autofit;     // print-scaling = auto-fit
  bool fidelity;
  bool no_orientation;
  _cfPDFToPDFPageRect page;  
  pdftopdf_rotation_e orientation, normal_landscape;  // normal_landscape (i.e. default direction) is e.g. needed for number-up=2
  bool paper_is_landscape;
  bool duplex;
  pdftopdf_border_type_e border;
  _cfPDFToPDFNupParameters nup;
  bool reverse;

  char *page_label;
  bool even_pages, odd_pages;
  _cfPDFToPDFIntervalSet *page_ranges;
  _cfPDFToPDFIntervalSet *input_page_ranges;

  bool mirror;

  pdftopdf_position_e xpos, ypos;

  bool collate;
  
  bool even_duplex; // make number of pages a multiple of 2
  
  pdftopdf_booklet_mode_e booklet;
  int book_signature;
 
  bool auto_rotate;

  int device_copies;
  bool device_collate;
  bool set_duplex;

  int page_logging;
  int copies_to_be_logged;
} _cfPDFToPDFProcessingParameters;

void _cfPDFToPDFProcessingParameters_init(_cfPDFToPDFProcessingParameters *processingParams);

bool _cfPDFToPDFProcessingParameters_with_page(const _cfPDFToPDFProcessingParameters *self,
	       			               int outno);
  
bool _cfPDFToPDFProcessingParameters_have_page(const _cfPDFToPDFProcessingParameters *self, 
					       int pageno);
void _cfPDFToPDFProcessingParameters_dump(const _cfPDFToPDFProcessingParameters *self, 
					  pdftopdf_doc_t *doc);





int* _cfPDFToPDFBookletShuffle(int numPages, int signature, int* ret_size); 

bool _cfProcessPDFToPDF(_cfPDFToPDF_PDFioProcessor *proc,
		   	_cfPDFToPDFProcessingParameters *param, 
			pdftopdf_doc_t *doc);
#endif // C_PDFTOPDF_PROCESSOR_PRIVATE_H
