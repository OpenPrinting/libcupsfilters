#ifndef C_PDFTOPDF_PROCESSOR_PRIVATE_H
#define C_PDFTOPDF_PROCESSOR_PRIVATE_H


#include "C-pptypes-private.h"
#include "C-nup-private.h"
#include "C-pdftopdf-private.h"
#include "C-intervalset-private.h"
#include <stdio.h>
#include <stdbool.h>

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




typedef enum {
  CF_PDFTOPDF_WILL_STAY_ALIVE,
  CF_PDFTOPDF_MUST_DUPLICATE,
  CF_PDFTOPDF_TAKE_OWNERSHIP
} pdftopdf_arg_ownership_e;

/*
// Example function to initialize the struct (constructor equivalent)
typedef struct _cfPDFToPDFProcessor _cfPDFToPDFProcessor;

struct _cfPDFToPDFProcessor {
  void (*destroy)(_cfPDFToPDFProcessor *self);
    
  bool (*load_file)(_cfPDFToPDFProcessor *self, 
		    FILE *f, pdftopdf_doc_t *doc, 
		    pdftopdf_arg_ownership_e take, 
		    int flatten_forms);

  bool (*load_filename)(_cfPDFToPDFProcessor *self, 
		        const char *name, 
			pdftopdf_doc_t *doc, 
			int flatten_forms);

  bool (*check_print_permissions)(_cfPDFToPDFProcessor *self, 
		  		  pdftopdf_doc_t *doc);

  void (*get_pages)(_cfPDFToPDFProcessor *self, 
		    pdftopdf_doc_t *doc, 
		    _cfPDFToPDFPageHandle **pages, 
		    size_t *count);

  _cfPDFToPDFPageHandle *(*new_page)(_cfPDFToPDFProcessor *self, 
		  		     float width, float height, 
				     pdftopdf_doc_t *doc);

  void (*add_page)(_cfPDFToPDFProcessor *self, 
		   _cfPDFToPDFPageHandle *page, 
		   bool front);

  void (*multiply)(_cfPDFToPDFProcessor *self, 
		   int copies, bool collate);

  void (*auto_rotate_all)(_cfPDFToPDFProcessor *self, 
		  	  bool dst_lscape, 
			  pdftopdf_rotation_e normal_landscape);

  void (*add_cm)(_cfPDFToPDFProcessor *self, 
		 const char *defaulticc, 
		 const char *outputicc);

  void (*set_comments)(_cfPDFToPDFProcessor *self, 
		       const char **comments,
		       size_t count);

  void (*emit_file)(_cfPDFToPDFProcessor *self, 
		    FILE *dst, pdftopdf_doc_t *doc, 
		    pdftopdf_arg_ownership_e take);

  void (*emit_filename)(_cfPDFToPDFProcessor *self, 
		        const char *name, 
			pdftopdf_doc_t *doc);

  bool (*has_acro_form)(_cfPDFToPDFProcessor *self);
};

// Example function to initialize the struct (constructor equivalent)
void _cfPDFToPDFProcessor_init(_cfPDFToPDFProcessor *self) {
    // Initialize function pointers and any necessary data members
}

_cfPDFToPDFProcessor* _cfPDFToPDFFactory_processor(void);
*/

#endif // C_PDFTOPDF_PROCESSOR_PRIVATE_H
