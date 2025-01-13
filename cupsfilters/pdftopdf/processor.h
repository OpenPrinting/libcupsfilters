//
// Copyright 2024 Uddhav Phatak <uddhavabhijeet@gmail.com>
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef C_PDFTOPDF_PROCESSOR_PRIVATE_H
#define C_PDFTOPDF_PROCESSOR_PRIVATE_H

#include "pptypes-private.h"
#include "pdfio.h"
#include "pptypes-private.h"
#include "nup-private.h"
#include "pdftopdf-private.h"
#include "intervalset-private.h"
#include <stdio.h>
#include <stdbool.h>
#define HASH_TABLE_SIZE 2048 

typedef struct KeyValuePair {
    char *key;  // Key (string)
    pdfio_obj_t *value;  // Value (PDF object handle)
    struct KeyValuePair *next;  // For handling collisions (chaining)
} KeyValuePair;

typedef struct HashTable {
    KeyValuePair *buckets[HASH_TABLE_SIZE];  // Array of pointers to key-value pairs
    int count;  // Number of filled elements in the hash table
} HashTable;

HashTable *hashCreate_hash_table();
void hashInsert(HashTable *table, const char *key, pdfio_obj_t *value);
pdfio_obj_t *hashGet(HashTable *table, const char *key);
int hashGet_filled_count(HashTable *table);
void hashFree_hash_table(HashTable *table);

typedef struct _cfPDFToPDFPageHandle{
  // 1st mode: existing
  pdfio_obj_t *page;  
  int no;

  // 2nd mode: create new
  HashTable *xobjs;    
  char *content;      

  pdftopdf_rotation_e rotation;
}_cfPDFToPDFPageHandle;

void _cfPDFToPDFPageHandle_existingMode(_cfPDFToPDFPageHandle *handle,
			    	     	pdfio_obj_t *page,
					int orig_no); // 1st mode:existing

void _cfPDFToPDFPageHandle_create_newMode(_cfPDFToPDFPageHandle *handle,
	       			 	  pdfio_file_t *pdf, 
					  float width, float height); // 2nd mode:create new

void _cfPDFToPDFPageHandle_destroy(_cfPDFToPDFPageHandle *handle);

void _cfPDFToPDFPageHandle_debug(_cfPDFToPDFPageHandle *handle, 
				 const _cfPDFToPDFPageRect *rect, 
				 float xpos, float ypos);

bool _cfPDFToPDFPageHandle_is_existing(_cfPDFToPDFPageHandle *handle); 

pdfio_obj_t* _cfPDFToPDFPageHandle_get(_cfPDFToPDFPageHandle *handle); 


_cfPDFToPDFPageRect _cfPDFToPDFPageHandle_get_rect(const _cfPDFToPDFPageHandle *handle);

void _cfPDFToPDFPageHandle_add_border_rect(_cfPDFToPDFPageHandle *handle, 
					   pdfio_file_t *pdf,
					   const _cfPDFToPDFPageRect rect, 
					   pdftopdf_border_type_e border, 
					   float fscale); 


pdftopdf_rotation_e _cfPDFToPDFPageHandle_crop(_cfPDFToPDFPageHandle *handle, 
					       const _cfPDFToPDFPageRect *cropRect, 
					       pdftopdf_rotation_e orientation, 
					       pdftopdf_rotation_e param_orientation, 
					       pdftopdf_position_e xpos, 
					       pdftopdf_position_e ypos, 
					       bool scale, bool autorotate, 
					       pdftopdf_doc_t *doc);


void _cfPDFToPDFPageHandle_add_subpage(_cfPDFToPDFPageHandle *handle, 
				       _cfPDFToPDFPageHandle *sub, 
				       pdfio_file_t *pdf,
				       float xpos, float ypos, float scale, 
				       const _cfPDFToPDFPageRect *crop);

bool _cfPDFToPDFPageHandle_is_landscape(const _cfPDFToPDFPageHandle *handle, 
					pdftopdf_rotation_e orientation); 

void _cfPDFToPDFPageHandle_mirror(_cfPDFToPDFPageHandle *handle,
				  pdfio_file_t *pdf);

void _cfPDFToPDFPageHandle_rotate(_cfPDFToPDFPageHandle *handle, pdftopdf_rotation_e rot);

void _cfPDFToPDFPageHandle_add_label(_cfPDFToPDFPageHandle *handle,
				     pdfio_file_t *pdf,
				     const _cfPDFToPDFPageRect *rect, 
				     const char *label);

typedef enum pdftopdf_arg_ownership_e {
  CF_PDFTOPDF_WILL_STAY_ALIVE,
  CF_PDFTOPDF_MUST_DUPLICATE,
  CF_PDFTOPDF_TAKE_OWNERSHIP
} pdftopdf_arg_ownership_e;

typedef struct _cfPDFToPDF_PDFioProcessor{
    // Other members
    _cfPDFToPDFPageHandle *pageHandle; 

    pdfio_file_t *pdf;      // Equivalent to std::unique_ptr<QPDF>
    pdfio_obj_t **orig_pages; // Equivalent to std::vector<QPDFObjectHandle>
    size_t orig_pages_size;   // Current number of pages

    bool hasCM;
    char *extraheader;
} _cfPDFToPDF_PDFioProcessor;

//_cfPDFToPDFQPDFProcessor functions


void _cfPDFToPDF_PDFioProcessor_close_file(_cfPDFToPDF_PDFioProcessor *processor);

bool _cfPDFToPDF_PDFioProcessor_load_file(_cfPDFToPDF_PDFioProcessor *processor,
					   FILE *f, pdftopdf_doc_t *doc, 
					   pdftopdf_arg_ownership_e take, int flatten_forms);

bool _cfPDFToPDF_PDFioProcessor_load_filename(_cfPDFToPDF_PDFioProcessor *processor,
	       				      const char *name, 
					      pdftopdf_doc_t *doc, 
					      int flatten_forms);

void _cfPDFToPDF_PDFioProcessor_start(_cfPDFToPDF_PDFioProcessor *processor, 
				      int flatten_forms);

bool _cfPDFToPDF_PDFioProcessor_check_print_permissions(_cfPDFToPDF_PDFioProcessor *processor,
							pdftopdf_doc_t *doc);

_cfPDFToPDFPageHandle** _cfPDFToPDF_PDFioProcessor_get_pages(_cfPDFToPDF_PDFioProcessor *handle,
						   	     pdftopdf_doc_t *doc, 
							     size_t *out_len);

_cfPDFToPDFPageHandle* _cfPDFToPDF_PDFioProcessor_new_page(_cfPDFToPDF_PDFioProcessor *handle, 
							   float width, float height, 
							   pdftopdf_doc_t *doc);

//void _cfPDFToPDF_PDFioProcessor_add_page(_cfPDFToPDF_PDFioProcessor *handle,
//					 _cfPDFToPDFPageHandle *page,
//					 bool front);
					 

void _cfPDFToPDF_PDFioProcessor_multiply(_cfPDFToPDF_PDFioProcessor *handle,
	       				 int copies, bool collate);

void _cfPDFToPDF_PDFioProcessor_auto_rotate_all(_cfPDFToPDF_PDFioProcessor *handle,
	      			 		bool dst_lscape,
					       	pdftopdf_rotation_e normal_landscape);

void _cfPDFToPDF_PDFioProcessor_add_cm(_cfPDFToPDF_PDFioProcessor *handle,
				       const char *defaulticc, const char *outputicc);

void _cfPDFToPDF_PDFioProcessor_set_comments(_cfPDFToPDF_PDFioProcessor *handle,
	       			 	     char **comments, int num_comments);
void _cfPDFToPDF_PDFioProcessor_emit_file(_cfPDFToPDF_PDFioProcessor *handle,
	       		 	  	  FILE *f, pdftopdf_doc_t *doc, 
					  pdftopdf_arg_ownership_e take);

void _cfPDFToPDF_PDFioProcessor_emit_filename(_cfPDFToPDF_PDFioProcessor *handle,
				              const char *name, pdftopdf_doc_t *doc);

bool _cfPDFToPDF_PDFioProcessor_has_acro_form(_cfPDFToPDF_PDFioProcessor *handle);

typedef enum {
  CF_PDFTOPDF_BOOKLET_OFF,
  CF_PDFTOPDF_BOOKLET_ON,
  CF_PDFTOPDF_BOOKLET_JUST_SHUFFLE
} pdftopdf_booklet_mode_e;

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
  _cfPDFToPDFPageRect *page;  
  pdftopdf_rotation_e orientation, normal_landscape;  // normal_landscape (i.e. default direction) is e.g. needed for number-up=2
  bool paper_is_landscape;
  bool duplex;
  pdftopdf_border_type_e border;
  _cfPDFToPDFNupParameters *nup;
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

//_cfPDFToPDFQPDFPageHandle functions
//inherited functions

//C-pdftopdf-processor-private functions

void _cfPDFToPDFProcessingParameters_init(_cfPDFToPDFProcessingParameters *processingParams);

void _cfPDFToPDFProcessingParameters_free(_cfPDFToPDFProcessingParameters *processingParams);

bool _cfPDFToPDFProcessingParameters_even_odd_page(const _cfPDFToPDFProcessingParameters *self,
						   int outno);

bool _cfPDFToPDFProcessingParameters_with_page(const _cfPDFToPDFProcessingParameters *self,
	       			               int outno);
  
bool _cfPDFToPDFProcessingParameters_have_page(const _cfPDFToPDFProcessingParameters *self, 
					       int pageno);
void _cfPDFToPDFProcessingParameters_dump(const _cfPDFToPDFProcessingParameters *self, 
					  pdftopdf_doc_t *doc);

int* _cfPDFToPDFBookletShuffle(int numPages, int signature, int* ret_size); 

bool _cfProcessPDFToPDF( _cfPDFToPDF_PDFioProcessor *proc,
		   	_cfPDFToPDFProcessingParameters *param, 
			pdftopdf_doc_t *doc);
#endif // C_PDFTOPDF_PROCESSOR_PRIVATE_H
