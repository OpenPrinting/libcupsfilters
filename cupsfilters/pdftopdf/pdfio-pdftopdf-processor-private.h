//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "C-pdftopdf-processor-private.h"
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

//_cfPDFToPDFQPDFPageHandle functions
void _cfPDFToPDF_PDFioProcessor_existingMode(_cfPDFToPDF_PDFioProcessor *handle,
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
						 pdfio_file_t *pdf,	
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
					     pdfio_file_t *pdf, 
					     float xpos, float ypos, float scale, 
					     const _cfPDFToPDFPageRect *crop);

bool _cfPDFToPDF_PDFioProcessor_is_landscape(const _cfPDFToPDF_PDFioProcessor *handle, 
					     pdftopdf_rotation_e orientation); 

void _cfPDFToPDF_PDFioProcessor_mirror(_cfPDFToPDF_PDFioProcessor *handle, pdfio_file_t *pdf);

void _cfPDFToPDF_PDFioProcessor_rotate(_cfPDFToPDF_PDFioProcessor *handle, pdftopdf_rotation_e rot);

void _cfPDFToPDF_PDFioProcessor_add_label(_cfPDFToPDF_PDFioProcessor *handle,
					  pdfio_file_t *pdf, 
					  const _cfPDFToPDFPageRect *rect, 
					  const char *label);


