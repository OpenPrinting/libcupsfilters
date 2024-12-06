//
// Copyright 2024 Uddhav Phatak <uddhavabhijeet@gmail.com>
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "processor.h"
#include "C-pptypes-private.h"
#include "pdfio.h"
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

/*
typedef struct _cfPDFToPDF_PDFioProcessor{

    // 1st mode: existing
    pdfio_obj_t *page;      // Equivalent to PDFioObjectHandle
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
*/
//_cfPDFToPDFPDFioPageHandle functions

void _cfPDFToPDF_PDFioProcessor_existingMode(_cfPDFToPDF_PDFioProcessor *handle,
					     pdfio_file_t *pdf, 
			    	     	     pdfio_obj_t *page,
					     int orig_no); // 1st mode:existing

void _cfPDFToPDF_PDFioProcessor_create_newMode(_cfPDFToPDF_PDFioProcessor *handle,
	       			 	       pdfio_file_t *pdf, 
					       float width, float height); // 2nd mode:create new

void _cfPDFToPDF_PDFioProcessor_debug(_cfPDFToPDF_PDFioProcessor *handle, 
				      const _cfPDFToPDFPageRect *rect, 
				      float xpos, float ypos);

bool _cfPDFToPDF_PDFioProcessor_is_existing(struct _cfPDFToPDF_PDFioProcessor *self); 

pdfio_obj_t* _cfPDFToPDF_PDFioProcessor_get(struct _cfPDFToPDF_PDFioProcessor *handle); 

//inherited functions
void _cfPDFToPDF_PDFioProcessor_destroy(_cfPDFToPDF_PDFioProcessor *handle);

_cfPDFToPDFPageRect _cfPDFToPDF_PDFioProcessor_get_rect(const _cfPDFToPDF_PDFioProcessor *handle);

void _cfPDFToPDF_PDFioProcessor_add_border_rect(_cfPDFToPDF_PDFioProcessor *handle, 
						 const _cfPDFToPDFPageRect rect, 
						 pdftopdf_border_type_e border, 
						 float fscale); 


pdftopdf_rotation_e _cfPDFToPDF_PDFioProcessor_crop(_cfPDFToPDF_PDFioProcessor *handle, 
						    const _cfPDFToPDFPageRect *cropRect, 
						    pdftopdf_rotation_e orientation, 
						    pdftopdf_rotation_e param_orientation, 
						    pdftopdf_position_e xpos, 
						    pdftopdf_position_e ypos, 
						    bool scale, bool autorotate, 
						    pdftopdf_doc_t *doc);


void _cfPDFToPDF_PDFioPageHandle_add_subpage(_cfPDFToPDF_PDFioProcessor *handle, 
					     _cfPDFToPDF_PDFioProcessor *sub, 
					     float xpos, float ypos, float scale, 
					     const _cfPDFToPDFPageRect *crop);

bool _cfPDFToPDF_PDFioProcessor_is_landscape(const _cfPDFToPDF_PDFioProcessor *handle, 
					     pdftopdf_rotation_e orientation); 

void _cfPDFToPDF_PDFioProcessor_mirror(_cfPDFToPDF_PDFioProcessor *handle);

void _cfPDFToPDF_PDFioProcessor_rotate(_cfPDFToPDF_PDFioProcessor *handle, pdftopdf_rotation_e rot);

void _cfPDFToPDF_PDFioProcessor_add_label(_cfPDFToPDF_PDFioProcessor *handle,
					  const _cfPDFToPDFPageRect *rect, 
					  const char *label);


//_cfPDFToPDFPDFioProcessor functions
void _cfPDFToPDF_PDFioProcessor_close_file(_cfPDFToPDF_PDFioProcessor *processor);

bool _cfPDFToPDF_PDFioProcessor_load_file(_cfPDFToPDF_PDFioProcessor *processor,
					   FILE *f, pdftopdf_doc_t *doc, 
					   pdftopdf_arg_ownership_e take, int flatten_forms);

bool _cfPDFToPDF_PDFioProcessor_load_filename(_cfPDFToPDF_PDFioProcessor *processor,
	       				      const char *name, 
					      pdftopdf_doc_t *doc, 
					      int flatten_forms);

void _cfPDFToPDF_PDFioProcessor_start(_cfPDFToPDF_PDFioProcessor *processor, int flatten_forms);

bool _cfPDFToPDF_PDFioProcessor_check_print_permissions(_cfPDFToPDF_PDFioProcessor *processor,
							pdftopdf_doc_t *doc);

_cfPDFToPDF_PDFioProcessor** _cfPDFToPDF_PDFioProcessor_get_pages(_cfPDFToPDF_PDFioProcessor *handle,
								  pdftopdf_doc_t *doc, 
								  int *out_len);

_cfPDFToPDF_PDFioProcessor* _cfPDFToPDF_PDFioProcessor_new_page(_cfPDFToPDF_PDFioProcessor *handle, 
								float width, float height, 
								pdftopdf_doc_t *doc);

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
